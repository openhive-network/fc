/*
 * White-box tests for fc::thread's sleep_pqueue - the heap of fibers that are
 * suspended with a deadline (a wait() with a timeout, or fc::usleep()).
 *
 * This is the container fc!27 ("Fix thread bottlenecks") rewrites: it replaces
 * the eager scan-and-remove in thread::notify() / thread_d::yield_until() /
 * notify_task_has_been_canceled() with a per-entry sequence number, deferring the
 * physical erase to check_for_timeouts() and compact_sleep_pqueue().
 *
 * The tests below are about that container:
 *   - notified_waiters_leave_no_entry_in_sleep_pqueue  pins the invariant that the
 *     eager removal maintained (a regression test);
 *   - repeated_wake_cycles_do_not_grow_sleep_pqueue    pins the bound that makes
 *     deferred removal safe over time (a regression test);
 *   - wake_cost_per_sleeping_fiber                     measures the cost the MR
 *     is trying to remove (a reporting test, no pass/fail on timing).
 */

#include <boost/test/unit_test.hpp>

#include "scheduler_probe.hpp"

#include <chrono>
#include <vector>

BOOST_AUTO_TEST_SUITE( sleep_queue_tests )

/*
 * INVARIANT: a context belongs in sleep_pqueue only while it is genuinely
 * suspended.  Every wake path removes the entry BEFORE the fiber can run again -
 * thread::notify() scans the heap, removes the entry, and only then puts the
 * context on the ready list.
 *
 * The heap is deliberately set up with two groups:
 *   keepers - EARLIER deadline, never notified, so they stay at the FRONT;
 *   wakers  - LATER deadline, all notified early.
 * That layout is what makes the test discriminating.  Peeling invalidated entries off
 * the front of the heap is not enough on its own: with live keepers parked in front of
 * them the wakers' entries are never reached, which is why deferred removal also needs
 * compact_sleep_pqueue().  (If every entry went stale at once - e.g. one group sharing
 * a deadline - the front-peel loop would drain the whole heap and the difference would
 * be invisible.)
 *
 * After the wake-up both groups then block WITHOUT a timeout, which adds no new
 * entry, so context::reinitialize() never runs while we observe.
 *
 * Expected: sleep_pqueue holds exactly the keepers.  With front-peeling as the only
 * removal it holds keepers + wakers.
 */
BOOST_AUTO_TEST_CASE( notified_waiters_leave_no_entry_in_sleep_pqueue )
{
  constexpr size_t KEEPERS = 50;
  constexpr size_t WAKERS  = 250;

  fc::thread th( "sleep_invariant" );

  fc::promise<void>::ptr keeper_prom = fc::promise<void>::create( "keeper" );
  fc::promise<void>::ptr waker_prom  = fc::promise<void>::create( "waker" );
  fc::promise<void>::ptr park        = fc::promise<void>::create( "park" );

  std::atomic<size_t> woken{ 0 };

  std::vector<fc::future<void>> fibers;
  fibers.reserve( KEEPERS + WAKERS );

  // earlier deadline -> front of the heap, and they stay asleep
  for( size_t i = 0; i < KEEPERS; ++i )
    fibers.push_back( th.async( [ &keeper_prom, &park ]() {
        keeper_prom->wait( fc::seconds( 60 ) );
        park->wait();
      }, "keeper" ) );

  // later deadline -> behind the keepers, and they all get woken early
  for( size_t i = 0; i < WAKERS; ++i )
    fibers.push_back( th.async( [ &waker_prom, &park, &woken ]() {
        waker_prom->wait( fc::seconds( 600 ) );
        ++woken;
        park->wait();                        // no timeout -> no sleep_pqueue entry
      }, "waker" ) );

  // every fiber is posted before the probe and each runs until it blocks, so by
  // the time the probe body executes all of them are parked
  BOOST_REQUIRE_EQUAL( probe( th ).sleeping_fibers, KEEPERS + WAKERS );
  BOOST_REQUIRE_EQUAL( woken.load(), 0u );

  waker_prom->set_value();

  while( woken.load() < WAKERS )
    std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );

  // <<< the regression assertion >>>
  BOOST_CHECK_EQUAL( probe( th ).sleeping_fibers, KEEPERS );

  keeper_prom->set_value();
  park->set_value();
  for( auto& f : fibers )
    f.wait();

  th.quit();
}

/*
 * The same layout, but repeated: the wakers go back to sleep after every wake-up while
 * the keepers hold the front of the heap throughout.
 *
 * One round of the test above can be explained away as a transient - the entries would
 * be dropped "eventually".  This one shows there is no eventually: nothing in the wake
 * path ever reaches an entry buried behind a live one, so without compaction the heap
 * grows by WAKERS per round and the cost of every heap operation grows with it.
 *
 * Expected: bounded by the fibers that are genuinely asleep (with slack for entries not
 * yet swept).  Without compaction: KEEPERS + WAKERS * ROUNDS.
 */
BOOST_AUTO_TEST_CASE( repeated_wake_cycles_do_not_grow_sleep_pqueue )
{
  constexpr size_t KEEPERS = 20;
  constexpr size_t WAKERS  = 100;
  constexpr size_t ROUNDS  = 20;

  fc::thread th( "sleep_growth" );

  fc::promise<void>::ptr keeper_prom = fc::promise<void>::create( "keeper" );
  fc::promise<void>::ptr park        = fc::promise<void>::create( "park" );

  // one promise per round; a waker re-blocks on the next one as soon as it is released
  std::vector<fc::promise<void>::ptr> rounds;
  rounds.reserve( ROUNDS );
  for( size_t r = 0; r < ROUNDS; ++r )
    rounds.push_back( fc::promise<void>::create( "round" ) );

  std::atomic<size_t> woken{ 0 };

  std::vector<fc::future<void>> fibers;
  fibers.reserve( KEEPERS + WAKERS );

  for( size_t i = 0; i < KEEPERS; ++i )
    fibers.push_back( th.async( [ &keeper_prom, &park ]() {
        keeper_prom->wait( fc::seconds( 60 ) );
        park->wait();
      }, "keeper" ) );

  for( size_t i = 0; i < WAKERS; ++i )
    fibers.push_back( th.async( [ &rounds, &park, &woken ]() {
        for( auto& round : rounds )
        {
          round->wait( fc::seconds( 600 ) );  // later deadline -> buried behind the keepers
          ++woken;
        }
        park->wait();
      }, "waker" ) );

  BOOST_REQUIRE_EQUAL( probe( th ).sleeping_fibers, KEEPERS + WAKERS );

  for( size_t r = 0; r < ROUNDS; ++r )
  {
    rounds[ r ]->set_value();

    while( woken.load() < ( r + 1 ) * WAKERS )
      std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );

    // <<< the regression assertion >>> - entries of past rounds must not accumulate
    BOOST_REQUIRE_LE( probe( th ).sleeping_fibers, 2 * ( KEEPERS + WAKERS ) );
  }

  BOOST_CHECK_EQUAL( probe( th ).sleeping_fibers, KEEPERS );

  keeper_prom->set_value();
  park->set_value();
  for( auto& f : fibers )
    f.wait();

  th.quit();
}

/*
 * Cost of waking N fibers that are all sleeping with a deadline.
 *
 * thread::notify() walks the blocked list once - O(N), inherent - and for every
 * context it unblocks it then locates that context in sleep_pqueue by linear
 * scan and rebuilds the whole heap with make_heap().  That inner part is O(N)
 * per wake-up, so one notify() that releases N sleepers costs O(N^2).
 *
 * The measurement runs the set_value() ON the probed thread, so thread::notify()
 * executes inline (is_current() is true) rather than being posted as a task.
 * The timed region is therefore exactly the notify walk, with no scheduling or
 * polling noise mixed in.
 *
 * Reported as microseconds per woken fiber:
 *   roughly FLAT as N grows    -> the removal is O(1)/O(log N), total O(N)
 *   roughly DOUBLING as N doubles -> the removal is O(N), total O(N^2)
 *
 * This test deliberately makes no assertion about the shape - that would flip
 * depending on which branch is built.  It only guards against a catastrophic
 * regression and prints the numbers.
 */
BOOST_AUTO_TEST_CASE( wake_cost_per_sleeping_fiber )
{
  const std::vector<size_t> sizes{ 125, 250, 500, 1000 };

  std::vector<double> per_fiber_us;
  per_fiber_us.reserve( sizes.size() );

  for( size_t n : sizes )
  {
    fc::thread th( "wake_cost" );

    fc::promise<void>::ptr release = fc::promise<void>::create( "release" );
    fc::promise<void>::ptr park    = fc::promise<void>::create( "park" );

    std::atomic<size_t> woken{ 0 };

    std::vector<fc::future<void>> sleepers;
    sleepers.reserve( n );
    for( size_t i = 0; i < n; ++i )
      sleepers.push_back( th.async( [ &release, &park, &woken ]() {
          release->wait( fc::seconds( 600 ) );
          ++woken;
          park->wait();
        }, "sleeper" ) );

    BOOST_REQUIRE_EQUAL( probe( th ).sleeping_fibers, n );

    // run the wake-up on `th` itself so notify() is inline, not posted
    const double notify_us = th.async( [ &release ]() {
        const auto t0 = std::chrono::steady_clock::now();
        release->set_value();
        const auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::micro>( t1 - t0 ).count();
      }, "waker" ).wait();

    while( woken.load() < n )
      std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );

    BOOST_CHECK_EQUAL( woken.load(), n );
    // sanity ceiling only - catches a catastrophic regression, not a shape change
    BOOST_CHECK_LT( notify_us, 30.0 * 1000.0 * 1000.0 );

    per_fiber_us.push_back( notify_us / static_cast<double>( n ) );

    BOOST_TEST_MESSAGE( "sleeping fibers " << n
                        << " | notify() " << notify_us << " us"
                        << " | per fiber " << ( notify_us / static_cast<double>( n ) ) << " us" );

    park->set_value();
    for( auto& f : sleepers )
      f.wait();

    th.quit();
  }

  for( size_t i = 1; i < per_fiber_us.size(); ++i )
    BOOST_TEST_MESSAGE( "per-fiber cost ratio " << sizes[ i - 1 ] << " -> " << sizes[ i ]
                        << ": x" << ( per_fiber_us[ i ] / per_fiber_us[ i - 1 ] )
                        << "  (~x1 = linear total, ~x2 = quadratic total)" );
}

BOOST_AUTO_TEST_SUITE_END()
