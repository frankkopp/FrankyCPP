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

#include <random>

#include "common/Logging.h"
#include "common/ThreadPool.h"
#include "init.h"
#include "types/types.h"

#include <gtest/gtest.h>
using testing::Eq;

using namespace common;

class ThreadPoolTest : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
    Logger::get().TEST_LOG->set_level(spdlog::level::debug);
  }

protected:
  void SetUp() override {}
  void TearDown() override {}

  struct Product {
    uint64_t producedNumber{};
    bool processed = false;
  };

  static Product process(Product p) {
    fprintln(">>> Processing product...: {}", p.producedNumber);
    // simulate cpu intense calculation
    uint64_t f = 100000000;
    while (f > 1) f = static_cast<uint64_t>(f / 1.00000001);
    std::this_thread::sleep_for(milliseconds(f));
    p.processed = true;
    fprintln(">>> Processed product...: {}", p.producedNumber);
    return p;
  }

  static Product produceProduct(const uint64_t i) {
    std::this_thread::sleep_for(milliseconds(10));
    Product product{i, false};
    fprintln("<<< Producing product...: {} ", product.producedNumber);
    return product;
  }
};

TEST_F(ThreadPoolTest, basic) {
  fprintln("Producer Worker Test");

  ThreadPool threadPool{4};
  std::vector<std::shared_ptr<std::future<Product>>> results{};

  fprintln("Queuing and starting work");
  constexpr int number = 50;

  for (int i = 0; i < number; i++) {
    Product product = produceProduct(i);
    auto future     = std::make_shared<std::future<Product>>(threadPool.enqueue([=] {
      return process(product);
    }));
    results.push_back(future);
    fprintln("Product queued: {} processed: {}", product.producedNumber, product.processed);
  }

  fprintln("Getting results");
  const auto& iterEnd = results.end();
  for (auto iter = results.begin(); iter < iterEnd; ++iter) {
    fprintln("Open tasks: {}", threadPool.openTasks());
    const auto [producedNumber, processed] = iter->get()->get();
    fprintln("Product finished: {} processed {}", producedNumber, processed);
  }
  SUCCEED();
}

TEST_F(ThreadPoolTest, doubleStopIsSafe) {
  ThreadPool pool{2};
  auto future = pool.enqueue([] { return 42; });
  EXPECT_EQ(future.get(), 42);
  pool.stop(); // First stop
  pool.stop(); // Second stop - should not crash
  EXPECT_TRUE(pool.isStopped());
}

TEST_F(ThreadPoolTest, enqueueAfterStopThrows) {
  ThreadPool pool{2};
  pool.stop();
  EXPECT_THROW(pool.enqueue([] { return 42; }), std::runtime_error);
}

TEST_F(ThreadPoolTest, isStoppedReflectsState) {
  ThreadPool pool{2};
  EXPECT_FALSE(pool.isStopped());
  pool.stop();
  EXPECT_TRUE(pool.isStopped());
}

TEST_F(ThreadPoolTest, taskExceptionPropagates) {
  ThreadPool pool{2};
  auto future = pool.enqueue([]() -> int {
    throw std::runtime_error("Task failed intentionally");
  });
  EXPECT_THROW(future.get(), std::runtime_error);
}

TEST_F(ThreadPoolTest, singleThreadPool) {
  ThreadPool pool{1};
  auto f1 = pool.enqueue([] { return 1; });
  auto f2 = pool.enqueue([] { return 2; });
  auto f3 = pool.enqueue([] { return 3; });
  EXPECT_EQ(f1.get(), 1);
  EXPECT_EQ(f2.get(), 2);
  EXPECT_EQ(f3.get(), 3);
}

TEST_F(ThreadPoolTest, openTasksReflectsQueueSize) {
  ThreadPool pool{1};

  // Block the single thread with a slow task
  std::promise<void> blocker;
  const auto blockerFuture = blocker.get_future();
  pool.enqueue([&] { blockerFuture.wait(); });

  // Give thread time to pick up the blocking task
  std::this_thread::sleep_for(milliseconds(50));

  // Queue more tasks while thread is blocked
  auto f1 = pool.enqueue([] { return 1; });
  auto f2 = pool.enqueue([] { return 2; });

  EXPECT_EQ(pool.openTasks(), 2); // Two tasks waiting

  blocker.set_value(); // Unblock
  EXPECT_EQ(f1.get(), 1);
  EXPECT_EQ(f2.get(), 2);
}

TEST_F(ThreadPoolTest, concurrentEnqueue) {
  ThreadPool pool{4};
  std::atomic counter{0};
  std::vector<std::thread> producers;
  std::vector<std::future<void>> futures;
  std::mutex futuresMutex;

  // Multiple threads enqueuing tasks concurrently
  for (int i = 0; i < 4; ++i) {
    producers.emplace_back([&pool, &counter, &futures, &futuresMutex] {
      for (int j = 0; j < 25; ++j) {
        auto f = pool.enqueue([&counter] { ++counter; });
        std::lock_guard lock(futuresMutex);
        futures.push_back(std::move(f));
      }
    });
  }

  // Wait for all producers to finish enqueueing
  for (auto& t : producers) {
    t.join();
  }

  // Wait for all tasks to complete
  for (auto& f : futures) {
    f.get();
  }

  EXPECT_EQ(counter.load(), 100);
}

TEST_F(ThreadPoolTest, voidReturnTask) {
  ThreadPool pool{2};
  std::atomic<bool> executed{false};
  auto future = pool.enqueue([&] { executed = true; });
  future.get(); // Wait for completion
  EXPECT_TRUE(executed);
}

TEST_F(ThreadPoolTest, shutdownCompletesPendingTasks) {
  // Test that destructor (which calls stop()) completes all pending tasks
  // before returning, even when tasks are still queued.
  std::atomic completed{0};

  {
    std::promise<void> blocker;
    auto blockerFuture = blocker.get_future().share();
    ThreadPool pool{1};

    // Signal when blocking task starts
    std::promise<void> taskStarted;
    const auto taskStartedFuture = taskStarted.get_future();

    pool.enqueue([&taskStarted, blockerFuture] {
      taskStarted.set_value(); // Signal that we've started
      blockerFuture.wait();    // Block until test releases us
    });

    // Wait for blocking task to actually start (not just be queued)
    taskStartedFuture.wait();

    // Queue 5 tasks that will be pending when destructor is called
    for (int i = 0; i < 5; ++i) {
      pool.enqueue([&completed] { ++completed; });
    }

    EXPECT_EQ(pool.openTasks(), 5); // 5 tasks pending

    // Release blocker from a separate thread AFTER destructor starts waiting
    // This simulates: destructor is called -> tasks are still pending ->
    // stop() must wait for all tasks to complete
    std::thread releaser([&blocker] {
      std::this_thread::sleep_for(milliseconds(50)); // Let destructor start
      blocker.set_value();                           // Now unblock the worker
    });
    releaser.detach();

    // Destructor is called here with 5 tasks still pending
    // stop() should wait for all tasks to complete
  }
  // If we get here, destructor completed - verify all tasks ran
  EXPECT_EQ(completed.load(), 5);
}


TEST_F(ThreadPoolTest, manyThreadsPool) {
  ThreadPool pool{8};
  std::atomic<int> sum{0};
  std::vector<std::future<int>> futures;

  for (int i = 0; i < 100; ++i) {
    futures.push_back(pool.enqueue([i, &sum] {
      sum += i;
      return i * 2;
    }));
  }

  int doubleSum = 0;
  for (auto& f : futures) {
    doubleSum += f.get();
  }

  // sum of 0..99 = 4950
  EXPECT_EQ(sum.load(), 4950);
  EXPECT_EQ(doubleSum, 9900); // 2 * 4950
}
