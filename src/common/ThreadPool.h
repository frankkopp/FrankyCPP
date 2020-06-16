/*
 * MIT License
 *
 * Copyright (c) 2018-2020 Frank Kopp
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef FRANKYCPP_THREADPOOL_H
#define FRANKYCPP_THREADPOOL_H

#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <queue>
#include <thread>
#include <vector>

/**
 * ThreadPool implementation for executing functions in asynchronously  with a
 * predetermined number of threads.
 */
class ThreadPool {
  using Task = std::function<void ()>;

  std::vector<std::thread> mThreads{};
  std::condition_variable  mEventVar{};
  std::mutex               mEventMutex{};
  bool                     mStopping = false;
  std::queue<Task>         mTasks{};

public:
  /* Create a thread pool with the given number of threads. Threads are started
   * directly and are waiting for tasks to be enqueued */
  explicit ThreadPool (std::size_t numThreads);

  /* stops the threads and removes the object */
  ~ThreadPool () { stop (); }

  /* Enqueue a task to be executed in a thread. Task is usually provided as
   * a lambda function */
  template <class T>
  auto enqueue (T task) -> std::future<decltype (task ())> {
    auto wrapper = std::make_shared<std::packaged_task<decltype (task ()) ()>> (std::move (task));
    { // lock block
      std::unique_lock<std::mutex> lock{ mEventMutex };
      mTasks.emplace ([=] { (*wrapper) (); });
    }
    mEventVar.notify_one ();
    return wrapper->get_future ();
  }

  /* Return the number of open (not started) tasks */
  auto openTasks () { return mTasks.size (); }

private:
  void start (std::size_t numThreads);
  void stop ();
};

#endif // FRANKYCPP_THREADPOOL_H
