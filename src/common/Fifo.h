// FrankyCPP
// Copyright (c) 2018-2021 Frank Kopp
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

#ifndef FRANKYCPP_FIFO_H
#define FRANKYCPP_FIFO_H

#include <queue>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <optional>

/**
 * Synchronized FIFO queue based on std::queue and std::deque
 */
template<class T>
class Fifo {

  mutable std::mutex fifoLock;
  mutable std::condition_variable cv;
  std::queue<T, std::deque<T>> fifo;
  bool closedFlag = false;

public:

  Fifo() = default;
  ~Fifo() = default;

  // copy
  Fifo(Fifo const &other) {
    std::scoped_lock lock{other.fifoLock};
    fifo = other.fifo;
  }

  // copy assignment
  Fifo &operator=(const Fifo &other) {
    std::scoped_lock lock(fifoLock, other.fifoLock);
    fifo = other.fifo;
    return *this;
  }

  // move
  Fifo(Fifo const &&other) noexcept {
    std::scoped_lock lock{other.fifoLock};
    fifo = std::move(other.fifo);
  }

  // move assignment
  Fifo &operator=(const Fifo &&other) noexcept {
    if (this != &other) {
      std::scoped_lock lock(fifoLock, other.fifoLock);
      fifo = std::move(other.fifo);
    }
    return *this;
  }

  /**
   * Pushes an item onto the fifo queue
   */
  void push(T &t) {
    {
      std::scoped_lock<std::mutex> lock{fifoLock};
      fifo.push(t);
    }
    cv.notify_one();
  }

  /**
   * Pushes an item onto the fifo queue using a move reference
   */
  void push(T &&t) {
    {
      std::scoped_lock<std::mutex> lock{fifoLock};
      fifo.push(std::move(t));
    }
    cv.notify_one();
  }

  /**
   * Retrieves an std::optional with the next item from the Fifo and removes
   * the item from the queue.
   * Returns an empty optional if called on empty list.
   */
  std::optional<T> pop() {
    std::scoped_lock<std::mutex> lock{fifoLock};
    if (fifo.empty()) return std::nullopt;
    std::optional<T> t{fifo.front()};
    fifo.pop();
    return t;
  }

  /**
   * Retrieves an std::optional with the next item from the Fifo and removes
   * the item from the queue.
   * Changes the given std::optional reference and returns it.
   * the optional will be an empty optional if called on empty list.
   */
  std::optional<T> pop(std::optional<T>  &t) {
    std::scoped_lock<std::mutex> lock{fifoLock};
    if (fifo.empty()) return std::nullopt;
    t.emplace(fifo.front());
    fifo.pop();
    return t;
  }

  /**
   * Retrieves the next item from the Fifo . Blocks if the Fifo is empty and
   * waits until an item becomes available. Block can be canceled be calling
   * Fifo.close in which case this will return nullptr.
   */
  std::optional<T> pop_wait() {
    std::unique_lock<std::mutex> lock{fifoLock};
    if (closedFlag && fifo.empty()) return std::nullopt;
    cv.wait(lock, [this] { return !fifo.empty() || closedFlag; });
    if (fifo.empty()) return std::nullopt;
    std::optional<T> t{fifo.front()};
    fifo.pop();
    return t;
  }

  /**
   * Retrieves the next item from the Fifo. Blocks if the Fifo is empty and
   * waits until an item becomes available. Block can be canceled be calling
   * Fifo.close in which case this will return nullptr.
   */
  std::optional<T> pop_wait(std::optional<T>  &t) {
    std::unique_lock<std::mutex> lock{fifoLock};
    if (closedFlag && fifo.empty()) return std::nullopt;
    cv.wait(lock, [this] { return !fifo.empty() || closedFlag; });
    if (fifo.empty()) return std::nullopt;
    t.emplace(fifo.front());
    fifo.pop();
    return t;
  }

  /**
   * Wakes up waiting threads and does not wait when calling pop_wait() any
   * longer.
   */
  void close() {
    std::scoped_lock<std::mutex> lock{fifoLock};
    closedFlag = true;
    cv.notify_all();
  }

  /**
   * Allows pop_wait() to wait for new entries into the fifo. This is allowed by
   * default but can be disabled by a call to close().
   */
  void open() {
    std::scoped_lock<std::mutex> lock{fifoLock};
    closedFlag = false;
  }

  /**
   * Checks if a call to pop_wait() will actually wait for new entries.
   * If returning true the call to pop_wait() will not wait but return
   * immediately wither with an item if available or with an empty std::optional
   */
  bool isClosed() {
    std::scoped_lock<std::mutex> lock{fifoLock};
    return closedFlag;
  }

  /**
   * Checks if fif queue is empty.
   */
  bool empty() const {
    std::scoped_lock<std::mutex> lock{fifoLock};
    return fifo.empty();
  }

  /**
   * Number of items in the fifo queue.
   */
  std::size_t size() const {
    std::scoped_lock<std::mutex> lock{fifoLock};
    return fifo.size();
  }

};

#endif //FRANKYCPP_FIFO_H
