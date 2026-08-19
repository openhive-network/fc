/*
 * White-box tests for the fc::thread task queues.
 *
 * They assert on the scheduler's internal containers rather than on observable
 * behaviour alone, because the question they answer - "does anything accumulate
 * in the queues?" - is not visible from the public API.
 *
 * Containers under test (see src/thread/thread_d.hpp):
 *   task_in_queue  - lock-free (Treiber) singly linked list of freshly posted
 *                    tasks, linked through task_base::_next, drained with a
 *                    single exchange(0) by the owning thread
 *   task_pqueue    - binary heap of tasks ready to run now, ordered by priority
 *                    and then by _posted_num (FIFO within a priority)
 *   task_sch_queue - binary heap of tasks whose _when is still in the future
 */

#include <boost/test/unit_test.hpp>

#include "scheduler_probe.hpp"

#include <algorithm>
#include <numeric>
#include <string>
#include <vector>

namespace {

size_t count_not_run_exactly_once( const std::vector<int>& hits )
{
  return static_cast<size_t>(
    std::count_if( hits.begin(), hits.end(), []( int h ) { return h != 1; } ) );
}

} // namespace

BOOST_AUTO_TEST_SUITE( task_queue_tests )

/*
 * 1000 tasks scheduled for a future point in time, posted in batches so that new
 * future-dated tasks keep arriving across several process_tasks() iterations.
 *
 * What this pins down: task_sch_queue holds exactly the tasks still pending -
 * it never re-absorbs a batch it has already consumed.  The incoming list is
 * detached with task_in_queue.exchange(0), so the next iteration finds it empty
 * and enqueue() is not even called.
 */
BOOST_AUTO_TEST_CASE( scheduled_tasks_do_not_accumulate )
{
  constexpr size_t BATCHES   = 10;
  constexpr size_t PER_BATCH = 100;
  constexpr size_t TOTAL     = BATCHES * PER_BATCH;

  fc::thread th( "sched_queue" );

  BOOST_CHECK_EQUAL( probe( th ).scheduled, 0u );

  std::vector<int> hits( TOTAL, 0 );
  std::vector<fc::future<void>> pending;
  pending.reserve( TOTAL );

  const fc::time_point fire_at = fc::time_point::now() + fc::seconds( 2 );

  for( size_t batch = 0; batch < BATCHES; ++batch )
  {
    for( size_t i = 0; i < PER_BATCH; ++i )
    {
      const size_t idx = batch * PER_BATCH + i;
      pending.push_back(
        th.schedule( [ &hits, idx ]() { ++hits[ idx ]; }, fire_at, "scheduled_task" ) );
    }

    const queue_state s = probe( th );

    // 100, 200, 300 ... - one entry per posted task, no more.  A container that
    // re-walked an already consumed incoming list would read 200, 600, 1400 ...
    BOOST_CHECK_EQUAL( s.scheduled, ( batch + 1 ) * PER_BATCH );
    // nothing is due yet, so nothing may have leaked into the ready heap
    BOOST_CHECK_EQUAL( s.ready_now, 0u );
  }

  // if the machine were slow enough for tasks to start firing while we were
  // still posting, the sizes above would be meaningless - fail loudly instead
  BOOST_REQUIRE_LT( fc::time_point::now().time_since_epoch().count(),
                    fire_at.time_since_epoch().count() );

  // Now keep the scheduler loop turning with nothing new arriving.  Every probe
  // is another trip through move_newly_scheduled_tasks_to_task_pqueue(); the
  // size must not move.
  for( int i = 0; i < 25; ++i )
    BOOST_CHECK_EQUAL( probe( th ).scheduled, TOTAL );

  for( auto& f : pending )
    f.wait();

  BOOST_CHECK_EQUAL( count_not_run_exactly_once( hits ), 0u );

  const queue_state after = probe( th );
  BOOST_CHECK_EQUAL( after.scheduled, 0u );
  BOOST_CHECK_EQUAL( after.ready_now, 0u );

  // informational: vectors keep their high-water-mark capacity for the lifetime
  // of the thread (pop_back/erase never give memory back)
  BOOST_TEST_MESSAGE( "task_sch_queue drained to " << after.scheduled
                      << ", capacity still " << after.scheduled_capacity );

  th.quit();
}

/*
 * 1000 tasks posted for immediate execution.
 *
 * Exercises the whole path: the lock-free task_in_queue list -> a single
 * exchange(0) -> one enqueue() batch -> the task_pqueue heap -> dequeue().
 *
 * The gate keeps the scheduler busy while we post, so the entire batch really
 * does arrive in one go and the observer sees the queue exactly as enqueue()
 * left it.
 */
BOOST_AUTO_TEST_CASE( immediate_tasks_land_in_priority_queue_once )
{
  constexpr size_t TOTAL = 1000;

  fc::thread th( "prio_queue" );

  scheduler_gate gate( th );

  // Posted first within the batch, so enqueue() gives it the lowest _posted_num
  // and it is the first task dequeued once the gate lets the loop run again.
  // (Priority would be the natural way to express "run me first", but the
  // priority argument is currently ignored - see the last test in this file.)
  queue_state at_batch_start;
  fc::future<void> observer =
    th.async( [ &th, &at_batch_start ]() { at_batch_start = read_queues( th ); },
              "batch_observer" );

  std::vector<int>    hits( TOTAL, 0 );
  std::vector<size_t> run_order;
  run_order.reserve( TOTAL );

  std::vector<fc::future<void>> pending;
  pending.reserve( TOTAL );
  for( size_t i = 0; i < TOTAL; ++i )
    pending.push_back( th.async( [ &hits, &run_order, i ]() {
        ++hits[ i ];
        run_order.push_back( i );
      }, "immediate_task" ) );

  gate.release();
  observer.wait();
  for( auto& f : pending )
    f.wait();

  // the whole batch came through a single exchange()/enqueue() pair: 1000 tasks
  // sitting in task_pqueue at the moment the first one is dequeued
  BOOST_CHECK_EQUAL( at_batch_start.ready_now, TOTAL );
  BOOST_CHECK_EQUAL( at_batch_start.scheduled, 0u );

  BOOST_CHECK_EQUAL( count_not_run_exactly_once( hits ), 0u );
  BOOST_REQUIRE_EQUAL( run_order.size(), TOTAL );

  // task_in_queue is a LIFO stack, but enqueue() hands out _posted_num in
  // reverse, so equal-priority tasks still run in post order
  std::vector<size_t> expected( TOTAL );
  std::iota( expected.begin(), expected.end(), size_t( 0 ) );
  BOOST_CHECK_EQUAL_COLLECTIONS( run_order.begin(), run_order.end(),
                                 expected.begin(), expected.end() );

  const queue_state after = probe( th );
  BOOST_CHECK_EQUAL( after.ready_now, 0u );
  BOOST_CHECK_EQUAL( after.scheduled, 0u );

  BOOST_TEST_MESSAGE( "task_pqueue drained to " << after.ready_now
                      << ", capacity still " << after.ready_now_capacity );

  th.quit();
}

/*
 * The one place where the scheduled queue really does hold on to entries:
 * process_canceled_tasks() sits below both early-continues in process_tasks(),
 * so a busy thread never reaps canceled scheduled tasks.  They keep their slot
 * - and their refcount, hence the functor and everything it captured - until
 * the thread finally goes idle (or until their _when arrives).
 *
 * This documents current behaviour; it is not a defect being asserted away.
 */
BOOST_AUTO_TEST_CASE( canceled_scheduled_tasks_are_reaped_only_when_idle )
{
  constexpr size_t TOTAL = 1000;

  fc::thread th( "cancel_reap" );

  const fc::time_point far_future = fc::time_point::now() + fc::seconds( 3600 );

  std::vector<fc::future<void>> pending;
  pending.reserve( TOTAL );
  for( size_t i = 0; i < TOTAL; ++i )
    pending.push_back( th.schedule( []() {}, far_future, "never_runs" ) );

  BOOST_CHECK_EQUAL( probe( th ).scheduled, TOTAL );

  scheduler_gate gate( th );

  // cancel while the scheduler loop is stuck in the gate
  for( auto& f : pending )
    f.cancel( "task_queue_tests" );

  // the only task in this batch, so it necessarily runs before the loop can
  // reach its idle path
  queue_state while_busy;
  fc::future<void> observer =
    th.async( [ &th, &while_busy ]() { while_busy = read_queues( th ); },
              "busy_observer" );

  gate.release();
  observer.wait();

  // cancellation on its own removes nothing from task_sch_queue
  BOOST_CHECK_EQUAL( while_busy.scheduled, TOTAL );

  // let the loop reach its idle path, where process_canceled_tasks() runs
  size_t remaining = TOTAL;
  for( int i = 0; i < 50 && remaining != 0; ++i )
  {
    std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
    remaining = probe( th ).scheduled;
  }

  BOOST_CHECK_EQUAL( remaining, 0u );

  th.quit();
}

/*
 * DEFECT, documented rather than asserted away: the priority argument of
 * async()/schedule() never reaches the task.
 *
 * thread::async_task( task_base* t, const priority& p, const time_point& tp )
 * in src/thread/thread.cpp takes `p` and drops it - task_base::_prio
 * (include/fc/thread/task.hpp) is not assigned anywhere in the library and is
 * not in task_base's constructor initializer list either, so every task runs
 * with priority 0.  task_priority_less() is itself correct, but with a constant
 * priority it degenerates to pure FIFO on _posted_num.
 *
 * Consequences beyond user code: thread::notify() and
 * thread::notify_task_has_been_canceled() both post with priority::max()
 * expecting to jump the queue, and neither does.
 *
 * When _prio is wired up this test will fail; it should then be changed to
 * assert that `high` runs first.
 */
BOOST_AUTO_TEST_CASE( task_priority_argument_is_currently_ignored )
{
  fc::thread th( "prio_ignored" );

  scheduler_gate gate( th );

  std::vector<std::string> run_order;

  // posted low -> high -> normal; a working priority would run high first
  fc::future<void> low = th.async( [ &run_order ]() { run_order.push_back( "low" ); },
                                   "low", fc::priority::min() );
  fc::future<void> high = th.async( [ &run_order ]() { run_order.push_back( "high" ); },
                                    "high", fc::priority::max() );
  fc::future<void> normal = th.async( [ &run_order ]() { run_order.push_back( "normal" ); },
                                      "normal" );

  gate.release();
  low.wait();
  high.wait();
  normal.wait();

  const std::vector<std::string> post_order{ "low", "high", "normal" };
  BOOST_CHECK_EQUAL_COLLECTIONS( run_order.begin(), run_order.end(),
                                 post_order.begin(), post_order.end() );

  th.quit();
}

BOOST_AUTO_TEST_SUITE_END()
