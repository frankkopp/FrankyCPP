// FrankyCPP
// Copyright (c) 2018-2026 Frank Kopp
//
// MIT License
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef FRANKYCPP_THREADPOOL_H
#define FRANKYCPP_THREADPOOL_H

//=============================================================================
// ThreadPool.h - Generic Thread Pool Implementation
//=============================================================================
//
// ThreadPool provides a fixed-size pool of worker threads that execute
// tasks asynchronously. Tasks are queued and distributed to available
// workers automatically.
//
// Features:
//   - Fixed thread count (set at construction)
//   - FIFO task queue with automatic load balancing
//   - Future-based result retrieval
//   - Graceful shutdown (completes pending tasks)
//   - RAII resource management
//
// Usage:
//   ThreadPool pool(4);  // Create pool with 4 worker threads
//
//   // Enqueue tasks and get futures
//   auto future1 = pool.enqueue([]() { return computeSomething(); });
//   auto future2 = pool.enqueue([]() { doSomethingElse(); });
//
//   // Wait for results
//   auto result = future1.get();
//
//   // Pool automatically shuts down when destroyed
//
// Thread Safety:
//   - enqueue() is thread-safe (can be called from multiple threads)
//   - Tasks execute concurrently in worker threads
//   - stop() should only be called once (or let destructor handle it)
//
// Exception Handling:
//   - Exceptions thrown in tasks are captured in the returned future
//   - Call future.get() to observe exceptions (will rethrow)
//   - enqueue() throws std::runtime_error if called after stop()
//
// Shutdown Behavior:
//   - stop() signals all threads to finish
//   - Pending tasks in queue are completed before threads exit
//   - Destructor calls stop() automatically (RAII)
//
//=============================================================================

#include <condition_variable>
#include <functional>
#include <future>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

/// Thread pool for asynchronous task execution with a fixed number of workers.
/// Tasks are queued and executed in FIFO order by available worker threads.
namespace common {

  class ThreadPool {
    using Task = std::function<void()>;

    std::vector<std::thread> mThreads{};
    std::condition_variable mEventVar{};
    mutable std::mutex mEventMutex{};
    bool mStopping = false;
    bool mStopped  = false;
    std::queue<Task> mTasks{};

  public:
    /// Creates a thread pool with the given number of worker threads.
    /// Threads are started immediately and wait for tasks to be enqueued.
    /// @param numThreads Number of worker threads to create
    explicit ThreadPool(std::size_t numThreads);

    /// Destructor - stops all threads gracefully (completes pending tasks)
    ~ThreadPool() { stop(); }

    // Non-copyable, non-movable (threads cannot be copied/moved)
    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&)                 = delete;
    ThreadPool& operator=(ThreadPool&&)      = delete;

    /// Enqueues a task for execution by a worker thread.
    /// @param task Callable to execute (typically a lambda)
    /// @return Future for the task's return value
    /// @throws std::runtime_error if called after stop()
    /// @note Thread-safe: can be called from multiple threads concurrently
    template<class T>
    auto enqueue(T task) -> std::future<decltype(task())> {
      auto wrapper = std::make_shared<std::packaged_task<decltype(task())()>>(std::move(task));
      {
        std::unique_lock lock{mEventMutex};
        if (mStopping) {
          throw std::runtime_error("Cannot enqueue on stopped ThreadPool");
        }
        mTasks.emplace([=] { (*wrapper)(); });
      }
      mEventVar.notify_one();
      return wrapper->get_future();
    }

    /// Returns the number of pending (not yet started) tasks in the queue.
    /// @return Number of tasks waiting to be executed
    [[nodiscard]] std::size_t openTasks() const {
      std::unique_lock lock{mEventMutex};
      return mTasks.size();
    }

    /// Returns whether the pool has been stopped.
    /// @return true if stop() has been called
    [[nodiscard]] bool isStopped() const {
      std::unique_lock lock{mEventMutex};
      return mStopped;
    }

    /// Stops all threads gracefully, completing any pending tasks.
    /// Safe to call multiple times - subsequent calls are no-ops.
    /// @note Called automatically by destructor
    void stop();

  private:
    void start(std::size_t numThreads);
  };

} // namespace common

#endif // FRANKYCPP_THREADPOOL_H
