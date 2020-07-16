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

#include "ThreadPool.h"

ThreadPool::ThreadPool(std::size_t numThreads) { start(numThreads); }

// //////////////////////
// PRIVATE

/* Start the given number of threads. Each threads enters a loop waiting for
 * for a condition variable to signal that tasks have been be enqueued into
 * the mTask vector. */
void ThreadPool::start(std::size_t numThreads) {
  mStopping = false;
  for (std::size_t i = 0; i < numThreads; ++i) {
    mThreads.emplace_back([=] {
      while (true) {
        Task task;
        { // lock block
          std::unique_lock<std::mutex> lock{mEventMutex};
          mEventVar.wait(lock, [=] { return mStopping || !mTasks.empty(); });
          if (mStopping && mTasks.empty()) {
            break;
          }
          task = std::move(mTasks.front());
          mTasks.pop();
        }
        task();
      }
    });
  }
}

/* Stops all running threads and waits for them to end before returning */
void ThreadPool::stop() {
  { // lock block
    std::unique_lock<std::mutex> lock{mEventMutex};
    mStopping = true;
  }
  mEventVar.notify_all();
  for (auto &thread : mThreads) {
    thread.join();
  }
}
