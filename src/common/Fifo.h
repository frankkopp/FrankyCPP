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

#ifndef FRANKYCPP_FIFO_H
#define FRANKYCPP_FIFO_H

//=============================================================================
// Fifo.h - Thread-Safe FIFO Queue
//=============================================================================
//
// A synchronized FIFO (First-In-First-Out) queue for thread-safe producer/
// consumer patterns. Built on std::queue with mutex and condition variable.
//
// Thread Safety:
//   All operations are thread-safe. Multiple threads can push and pop
//   concurrently without external synchronization.
//
// Blocking Operations:
//   pop_wait() blocks until an item is available or close() is called.
//   Useful for consumer threads that should wait for work.
//
// Close/Open:
//   close() wakes all waiting threads and causes pop_wait() to return
//   immediately with empty optional. open() re-enables waiting.
//
// Usage:
//   Fifo<std::string> queue;
//
//   // Producer thread
//   queue.push("work item");
//
//   // Consumer thread
//   while (auto item = queue.pop_wait()) {
//     process(*item);
//   }
//
//   // Shutdown
//   queue.close();  // Wakes consumer, pop_wait returns nullopt
//
//=============================================================================

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

/// Thread-safe FIFO queue with blocking pop support.
/// @tparam T  Element type stored in the queue
namespace common {

  template<class T>
  class Fifo {

    mutable std::mutex fifoLock;
    mutable std::condition_variable cv;
    std::queue<T> fifo;
    bool closedFlag = false;

  public:
    Fifo()  = default;
    ~Fifo() = default;

    /// Copy constructor (thread-safe).
    Fifo(Fifo const& other) {
      std::scoped_lock lock{other.fifoLock};
      fifo = other.fifo;
    }

    /// Copy assignment operator (thread-safe).
    Fifo& operator=(const Fifo& other) {
      std::scoped_lock lock(fifoLock, other.fifoLock);
      fifo = other.fifo;
      return *this;
    }

    /// Move constructor (thread-safe).
    Fifo(Fifo const&& other) noexcept {
      std::scoped_lock lock{other.fifoLock};
      fifo = std::move(other.fifo);
    }

    /// Move assignment operator (thread-safe).
    Fifo& operator=(Fifo&& other) noexcept {
      if (this != &other) {
        std::scoped_lock lock(fifoLock, other.fifoLock);
        fifo = std::move(other.fifo);
      }
      return *this;
    }

    /// Pushes an item onto the queue (copy).
    /// Wakes one waiting consumer thread.
    /// @param t  Item to push
    void push(T& t) {
      {
        std::scoped_lock lock{fifoLock};
        fifo.push(t);
      }
      cv.notify_one();
    }

    /// Pushes an item onto the queue (move).
    /// Wakes one waiting consumer thread.
    /// @param t  Item to move into queue
    void push(T&& t) {
      {
        std::scoped_lock lock{fifoLock};
        fifo.push(std::move(t));
      }
      cv.notify_one();
    }

    /// Pops an item from the queue (non-blocking).
    /// @return  Item if available, empty optional if queue is empty
    std::optional<T> pop() {
      std::scoped_lock lock{fifoLock};
      if (fifo.empty()) return std::nullopt;
      std::optional<T> t{fifo.front()};
      fifo.pop();
      return t;
    }

    /// Pops an item from the queue into provided optional (non-blocking).
    /// @param t  Optional to populate with item
    /// @return   Same optional (for chaining), empty if queue was empty
    std::optional<T> pop(std::optional<T>& t) {
      std::scoped_lock lock{fifoLock};
      if (fifo.empty()) return std::nullopt;
      t.emplace(fifo.front());
      fifo.pop();
      return t;
    }

    /// Pops an item from the queue, blocking if empty.
    /// Blocks until an item is available or close() is called.
    /// @return  Item if available, empty optional if closed and empty
    std::optional<T> pop_wait() {
      std::unique_lock lock{fifoLock};
      if (closedFlag && fifo.empty()) return std::nullopt;
      cv.wait(lock, [this] { return !fifo.empty() || closedFlag; });
      if (fifo.empty()) return std::nullopt;
      std::optional<T> t{fifo.front()};
      fifo.pop();
      return t;
    }

    /// Pops an item into provided optional, blocking if empty.
    /// Blocks until an item is available or close() is called.
    /// @param t  Optional to populate with item
    /// @return   Same optional, empty if closed and empty
    std::optional<T> pop_wait(std::optional<T>& t) {
      std::unique_lock lock{fifoLock};
      if (closedFlag && fifo.empty()) return std::nullopt;
      cv.wait(lock, [this] { return !fifo.empty() || closedFlag; });
      if (fifo.empty()) return std::nullopt;
      t.emplace(fifo.front());
      fifo.pop();
      return t;
    }

    /// Closes the queue, waking all waiting threads.
    /// After close(), pop_wait() returns immediately with empty optional
    /// if no items are available.
    void close() {
      std::scoped_lock lock{fifoLock};
      closedFlag = true;
      cv.notify_all();
    }

    /// Reopens the queue, allowing pop_wait() to block again.
    /// Reverses the effect of close().
    void open() {
      std::scoped_lock lock{fifoLock};
      closedFlag = false;
    }

    /// Checks if the queue is closed.
    /// @return  True if close() was called and open() was not called since
    bool isClosed() const {
      std::scoped_lock lock{fifoLock};
      return closedFlag;
    }

    /// Checks if the queue is empty.
    /// @return  True if no items in queue
    bool empty() const {
      std::scoped_lock lock{fifoLock};
      return fifo.empty();
    }

    /// Returns the number of items in the queue.
    /// @return  Current queue size
    std::size_t size() const {
      std::scoped_lock lock{fifoLock};
      return fifo.size();
    }
  };

}// namespace common

#endif// FRANKYCPP_FIFO_H
