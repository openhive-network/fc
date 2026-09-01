#pragma once
/*
 * Shared white-box helpers for the fc::thread scheduler tests.
 *
 * Everything here lives in an anonymous namespace on purpose: the explicit
 * template instantiation below defines a free function, and `all_tests` links
 * several of these test translation units together.  Internal linkage gives each
 * one its own copy instead of a duplicate symbol at link time.
 */

#include <fc/thread/future.hpp>
#include <fc/thread/thread.hpp>

// the scheduler queues live in a private header, not in the public API
#include "../../src/thread/thread_d.hpp"

#include <atomic>
#include <chrono>
#include <thread>

namespace {

/*
 * fc::thread::my is private and the tests are not on the friend list.  Rather
 * than patching the public header just for a test, reach it through an explicit
 * template instantiation: [temp.explicit] exempts the names used to specify an
 * explicit instantiation from access checking, so this is well defined - unlike
 * the usual `#define private public` trick, which changes the class in this
 * translation unit only and breaks ODR against the already compiled library.
 */
template< typename Tag, typename Tag::type Member >
struct member_thief
{
  friend typename Tag::type get( Tag ) { return Member; }
};

struct thread_my_tag
{
  using type = fc::thread_d* fc::thread::*;
  friend type get( thread_my_tag );
};

template struct member_thief< thread_my_tag, &fc::thread::my >;

inline fc::thread_d& internals( fc::thread& t ) { return *( t.*get( thread_my_tag() ) ); }

struct queue_state
{
  size_t ready_now = 0;           // task_pqueue.size()    - tasks ready to run
  size_t ready_now_capacity = 0;  // task_pqueue.capacity()
  size_t scheduled = 0;           // task_sch_queue.size()  - tasks waiting for _when
  size_t scheduled_capacity = 0;  // task_sch_queue.capacity()
  size_t ready_fibers = 0;        // ready_heap.size()
  size_t sleeping_fibers = 0;     // sleep_pqueue.size()   - fibers with a deadline
  size_t blocked_fibers = 0;      // length of the `blocked` chain - fibers waiting on a promise
};

/* Only ever call this from a task running on `t` - the containers are owned by
 * that thread's scheduler loop and are not synchronized. */
inline queue_state read_queues( fc::thread& t )
{
  const fc::thread_d& d = internals( t );
  queue_state s;
  s.ready_now = d.task_pqueue.size();
  s.ready_now_capacity = d.task_pqueue.capacity();
  s.scheduled = d.task_sch_queue.size();
  s.scheduled_capacity = d.task_sch_queue.capacity();
  s.ready_fibers = d.ready_heap.size();
  s.sleeping_fibers = d.sleep_pqueue.size();
  for( const fc::context* c = d.blocked; c; c = c->next_blocked )
    ++s.blocked_fibers;
  return s;
}

/* Posting the probe as a task does double duty: it reads the queues from the
 * right thread, and - because it is posted after everything else - it also
 * guarantees that every earlier post has already been through enqueue() by the
 * time the probe body runs. */
inline queue_state probe( fc::thread& t )
{
  return t.async( [ &t ]() { return read_queues( t ); }, "queue_probe" ).wait();
}

/* A task that busy-waits WITHOUT yielding.  While it runs, process_tasks() is
 * stuck inside run_next_task(), so it cannot reach
 * move_newly_scheduled_tasks_to_task_pqueue() and everything posted meanwhile
 * piles up on task_in_queue instead of being consumed one at a time. */
class scheduler_gate
{
public:
  explicit scheduler_gate( fc::thread& t )
  {
    _future = t.async( [ this ]() {
        _entered = true;
        while( !_released.load() )
          std::this_thread::sleep_for( std::chrono::microseconds( 200 ) );
      }, "scheduler_gate" );

    while( !_entered.load() )
      std::this_thread::sleep_for( std::chrono::microseconds( 200 ) );
  }

  void release()
  {
    _released = true;
    _future.wait();
  }

private:
  std::atomic<bool> _entered{ false };
  std::atomic<bool> _released{ false };
  fc::future<void>  _future;
};

} // namespace
