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
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
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

#ifndef FRANKYCPP_BASIC_SEMAPHORE_H
#define FRANKYCPP_BASIC_SEMAPHORE_H

#include <mutex>
#include <condition_variable>

/**
 * A simple Semaphore implementation.
 * https://stackoverflow.com/users/369872/david
 */
template <typename Mutex, typename CondVar>
class basic_semaphore {
public:
  using native_handle_type = typename CondVar::native_handle_type;

  // prevent the implicit conversion of constructor parameter
  // with explicit keyword
  explicit basic_semaphore(size_t count = 0);

  // prevent these operations with =delete
  basic_semaphore(const basic_semaphore&) = delete;
  basic_semaphore(basic_semaphore&&) = delete;
  basic_semaphore& operator=(const basic_semaphore&) = delete;
  basic_semaphore& operator=(basic_semaphore&&) = delete;

  // methods
  void reset();
  void release();
  void getOrWait();
  bool get();
  template<class Rep, class Period>
  bool getOrWaitFor(const std::chrono::duration<Rep, Period> &d);
  template<class Clock, class Duration>
  bool getOrWaitUntil(const std::chrono::time_point<Clock, Duration> &t);

  native_handle_type native_handle();

private:
  Mutex   mMutex;
  CondVar mCv;
  size_t  mCount;
};

/**
 * Simple name
 */
using Semaphore = basic_semaphore<std::mutex, std::condition_variable>;

/**
 * Create a semaphore with count permissions.
 * @tparam Mutex
 * @tparam CondVar
 * @param count
 */
template <typename Mutex, typename CondVar>
basic_semaphore<Mutex, CondVar>::basic_semaphore(size_t count) : mCount{count} {}

/**
 * Tries to get a semaphore and returns false if none available
 * @return
 */
template <typename Mutex, typename CondVar>
bool basic_semaphore<Mutex, CondVar>::get() {
  // get the lock - will be released when exiting the block
  // as it is set in the destructor of this class
  std::lock_guard<Mutex> lock{mMutex};
  if (mCount > 0) {
    --mCount;
    return true;
  }
  return false;
}

/**
 * Reset the number of available semaphores to 1
 */
template <typename Mutex, typename CondVar>
void basic_semaphore<Mutex, CondVar>::reset() {
  // get the lock - will be released when exiting the block
  // as it is set in the destructor of this class
  std::lock_guard<Mutex> lock{mMutex};
  mCount = 1;
  mCv.notify_one();
}

/**
 * Release a semaphore for others to grab
 */
template <typename Mutex, typename CondVar>
void basic_semaphore<Mutex, CondVar>::release() {
  // get the lock - will be released when exiting the block
  // as it is set in the destructor of this class
  std::lock_guard<Mutex> lock{mMutex};
  ++mCount;
  mCv.notify_one();
}

/**
 * Retrieves a semaphore and waits if non is available
 * @tparam Mutex
 * @tparam CondVar
 */
template <typename Mutex, typename CondVar>
void basic_semaphore<Mutex, CondVar>::getOrWait() {
  // get the lock - will be released when exiting the block
  // as it is set in the destructor of this class
  std::unique_lock<Mutex> lock{mMutex};
  mCv.wait(lock, [&]{ return mCount > 0; });
  --mCount;
}

/**
 * Waits for and retrieves  a semaphore for a certain amount of time.
 *
 * @tparam Mutex
 * @tparam CondVar
 * @tparam Rep
 * @tparam Period
 * @param d
 * @return
 */
template <typename Mutex, typename CondVar>
template<class Rep, class Period>
bool basic_semaphore<Mutex, CondVar>::getOrWaitFor(const std::chrono::duration<Rep, Period> &d) {
  // get the lock - will be released when exiting the block
  // as it is set in the destructor of this class
  std::unique_lock<Mutex> lock{mMutex};
  auto finished = mCv.wait_for(lock, d, [&]{ return mCount > 0; });
  if (finished) --mCount;
  return finished;
}

/**
 * Waits and retrieves for a semaphore until a certain point in time.
 *
 * @tparam Mutex
 * @tparam CondVar
 * @tparam Clock
 * @tparam Duration
 * @param t
 * @return
 */
template <typename Mutex, typename CondVar>
template<class Clock, class Duration>
bool basic_semaphore<Mutex, CondVar>::getOrWaitUntil(
  const std::chrono::time_point<Clock, Duration> &t) {
  // get the lock - will be released when exiting the block
  // as it is set in the destructor of this class
  std::unique_lock<Mutex> lock{mMutex};
  auto finished = mCv.wait_until(lock, t, [&]{ return mCount > 0; });
  if (finished) --mCount;
  return finished;
}

/**
 *
 * @tparam Mutex
 * @tparam CondVar
 * @return
 */
template <typename Mutex, typename CondVar>
typename basic_semaphore<Mutex, CondVar>::native_handle_type basic_semaphore<Mutex, CondVar>::native_handle() {
  return mCv.native_handle();
}


#endif //FRANKYCPP_BASIC_SEMAPHORE_H
