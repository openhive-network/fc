/*
 * White-box tests for the cost of thread::notify().
 *
 * A promise does not know who is waiting on it.  The thread keeps ONE flat singly-linked
 * list of every context blocked on any promise (thread_d::blocked), so notify(p) has to
 * ask each blocked context in turn "are you waiting on p?" - and it cannot stop at the
 * first match, because several contexts may wait on the same promise.  Waking a single
 * fiber therefore walks the whole list: O(|blocked|) per notify, regardless of how many
 * fibers that notify actually releases.
 *
 * thread.cpp says as much:  // TODO: store a list of blocked contexts with the promise
 *                           //  to accelerate the lookup.... unless it introduces contention...
 *
 * Both tests below are reporting tests: they print the numbers and assert only a sanity
 * ceiling, because the absolute values depend on the machine.  What matters is the SHAPE
 * printed in the ratio lines.
 *
 * Every fiber here blocks WITHOUT a timeout, so nothing lands in sleep_pqueue and the
 * measurement isolates the blocked-list walk.
 */

#include <boost/test/unit_test.hpp>

#include "scheduler_probe.hpp"

#include <chrono>
#include <vector>

namespace {

/* Park `n` fibers on `park` and one extra fiber on `target`, all on `th`.
 * Returns the futures so the caller can drain them.
 */
std::vector<fc::future<void>> park_fibers( fc::thread& th,
                                           size_t n,
                                           const fc::promise<void>::ptr& park )
{
  std::vector<fc::future<void>> fibers;
  fibers.reserve( n );
  for( size_t i = 0; i < n; ++i )
    fibers.push_back( th.async( [ &park ]() { park->wait(); }, "parked" ) );
  return fibers;
}

double seconds_since( const std::chrono::steady_clock::time_point& t0 )
{
  return std::chrono::duration<double>( std::chrono::steady_clock::now() - t0 ).count();
}

} // namespace

BOOST_AUTO_TEST_SUITE( notify_cost_tests )

/*
 * Cost of waking exactly ONE fiber while N others are blocked on an unrelated promise.
 *
 * The work actually required is constant - one context leaves the list.  The work done is
 * proportional to N.
 *
 * Reported as microseconds per notify():
 *   roughly FLAT as N grows       -> the lookup is indexed by promise
 *   roughly DOUBLING as N doubles -> the lookup is a full walk of `blocked`
 */
BOOST_AUTO_TEST_CASE( notify_cost_scales_with_blocked_list )
{
  const std::vector<size_t> sizes{ 1000, 5000, 10000, 25000, 50000 };

  std::vector<double> notify_us;
  notify_us.reserve( sizes.size() );

  for( size_t n : sizes )
  {
    fc::thread th( "notify_cost" );

    fc::promise<void>::ptr park   = fc::promise<void>::create( "park" );
    fc::promise<void>::ptr target = fc::promise<void>::create( "target" );

    std::atomic<bool> target_woken{ false };

    const auto t_setup = std::chrono::steady_clock::now();
    std::vector<fc::future<void>> fibers = park_fibers( th, n, park );

    // the single fiber we are going to wake, queued last so it sits at the head of `blocked`
    // only after everything else - position in the list must not matter, the walk is full
    fibers.push_back( th.async( [ &target, &target_woken ]() {
        target->wait();
        target_woken = true;
      }, "target" ) );

    const queue_state before = probe( th );
    BOOST_REQUIRE_EQUAL( before.blocked_fibers, n + 1 );
    BOOST_REQUIRE_EQUAL( before.sleeping_fibers, 0u );   // no timeouts -> nothing in sleep_pqueue
    const double setup_s = seconds_since( t_setup );

    // run the wake-up ON th so notify() executes inline (is_current() is true) instead of
    // being posted as a task - the timed region is then exactly the blocked-list walk
    const double us = th.async( [ &target ]() {
        const auto t0 = std::chrono::steady_clock::now();
        target->set_value();
        const auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::micro>( t1 - t0 ).count();
      }, "waker" ).wait();

    while( !target_woken.load() )
      std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );

    BOOST_CHECK_EQUAL( probe( th ).blocked_fibers, n );   // exactly one context left the list
    BOOST_CHECK_LT( us, 30.0 * 1000.0 * 1000.0 );         // sanity ceiling only

    notify_us.push_back( us );
    BOOST_TEST_MESSAGE( "blocked " << n
                        << " | notify() waking 1 fiber: " << us << " us"
                        << " | " << ( us * 1000.0 / static_cast<double>( n ) ) << " ns per blocked fiber"
                        << " | setup " << setup_s << " s" );

    park->set_value();
    for( auto& f : fibers )
      f.wait();

    th.quit();
  }

  for( size_t i = 1; i < notify_us.size(); ++i )
    BOOST_TEST_MESSAGE( "cost ratio " << sizes[ i - 1 ] << " -> " << sizes[ i ]
                        << " (list x" << ( static_cast<double>( sizes[ i ] ) / static_cast<double>( sizes[ i - 1 ] ) ) << ")"
                        << ": x" << ( notify_us[ i ] / notify_us[ i - 1 ] )
                        << "  (~x1 = indexed lookup, ~list factor = full walk)" );
}

/*
 * The same defect seen the way a busy node meets it: many notifications arriving while a
 * large population stays blocked.
 *
 * Each notify releases one fiber and walks the whole list, so M notifications over a list
 * of N cost O(M*N) - here 1000 * 50000 = 5*10^7 context probes to do 1000 fibers' worth of
 * work.  The thread is single-threaded by construction, so this is time it cannot spend on
 * anything else - in p2p, not on reading from any other peer.
 *
 * The notifications are issued from separate tasks rather than from one loop, which is what
 * "notify from many contexts" looks like in p2p: each peer's read fiber fulfilling its own
 * promise, independently, while every other peer stays parked.
 */
BOOST_AUTO_TEST_CASE( notify_storm_over_a_large_blocked_list )
{
  constexpr size_t PARKED   = 50000;
  constexpr size_t NOTIFIES = 1000;

  fc::thread th( "notify_storm" );

  fc::promise<void>::ptr park = fc::promise<void>::create( "park" );

  std::vector<fc::promise<void>::ptr> targets;
  targets.reserve( NOTIFIES );
  for( size_t i = 0; i < NOTIFIES; ++i )
    targets.push_back( fc::promise<void>::create( "target" ) );

  std::atomic<size_t> woken{ 0 };

  std::vector<fc::future<void>> fibers = park_fibers( th, PARKED, park );
  fibers.reserve( PARKED + NOTIFIES );
  for( size_t i = 0; i < NOTIFIES; ++i )
    fibers.push_back( th.async( [ &targets, i, &woken ]() {
        targets[ i ]->wait();
        ++woken;
      }, "target" ) );

  BOOST_REQUIRE_EQUAL( probe( th ).blocked_fibers, PARKED + NOTIFIES );

  // fire every notification from its own task, all of them inline on th
  const auto t0 = std::chrono::steady_clock::now();
  for( size_t i = 0; i < NOTIFIES; ++i )
    th.async( [ &targets, i ]() { targets[ i ]->set_value(); }, "notifier" );

  while( woken.load() < NOTIFIES )
    std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
  const double total_s = seconds_since( t0 );

  BOOST_CHECK_EQUAL( probe( th ).blocked_fibers, PARKED );
  BOOST_CHECK_LT( total_s, 300.0 );   // sanity ceiling only

  BOOST_TEST_MESSAGE( "parked " << PARKED << " | " << NOTIFIES << " notifications: "
                      << total_s << " s total, "
                      << ( total_s * 1e6 / static_cast<double>( NOTIFIES ) ) << " us each, "
                      << ( static_cast<double>( PARKED ) * NOTIFIES / 1e6 ) << "M context probes" );

  park->set_value();
  for( auto& f : fibers )
    f.wait();

  th.quit();
}

/*
 * The same storm, but the notifications come from ANOTHER thread - which is how most of them
 * arrive in hived: a promise fulfilled by the writer thread while the waiter sleeps on the p2p
 * thread.  thread::notify() sees is_current() == false and posts itself as a priority::max()
 * task, so every one of them lands in the target thread's queue and runs there, one after
 * another, each walking the whole blocked list.
 *
 * The number to watch is how long after the last set_value() the last fiber actually wakes:
 * the sender returns immediately, the cost is paid entirely by the receiver.
 */
BOOST_AUTO_TEST_CASE( cross_thread_notifications_serialize_on_the_target )
{
  constexpr size_t PARKED   = 50000;
  constexpr size_t NOTIFIES = 200;

  fc::thread target_thread( "notify_target" );
  fc::thread sender_thread( "notify_sender" );

  fc::promise<void>::ptr park = fc::promise<void>::create( "park" );

  std::vector<fc::promise<void>::ptr> targets;
  targets.reserve( NOTIFIES );
  for( size_t i = 0; i < NOTIFIES; ++i )
    targets.push_back( fc::promise<void>::create( "target" ) );

  std::atomic<size_t> woken{ 0 };

  std::vector<fc::future<void>> fibers = park_fibers( target_thread, PARKED, park );
  fibers.reserve( PARKED + NOTIFIES );
  for( size_t i = 0; i < NOTIFIES; ++i )
    fibers.push_back( target_thread.async( [ &targets, i, &woken ]() {
        targets[ i ]->wait();
        ++woken;
      }, "target" ) );

  BOOST_REQUIRE_EQUAL( probe( target_thread ).blocked_fibers, PARKED + NOTIFIES );

  const auto t0 = std::chrono::steady_clock::now();
  sender_thread.async( [ &targets ]() {
      for( auto& target : targets )
        target->set_value();
    }, "sender" ).wait();
  const double send_s = seconds_since( t0 );

  while( woken.load() < NOTIFIES )
    std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
  const double drain_s = seconds_since( t0 );

  BOOST_CHECK_LT( drain_s, 300.0 );   // sanity ceiling only

  BOOST_TEST_MESSAGE( "parked " << PARKED << " | " << NOTIFIES << " cross-thread notifications: "
                      << "sender returned after " << send_s << " s, "
                      << "last waiter woke after " << drain_s << " s"
                      << " (backlog " << ( drain_s / ( send_s > 0.0 ? send_s : 1e-9 ) ) << "x the send time)" );

  park->set_value();
  for( auto& f : fibers )
    f.wait();

  target_thread.quit();
  sender_thread.quit();
}

BOOST_AUTO_TEST_SUITE_END()
