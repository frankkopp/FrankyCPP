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

#include <random>

#include "init.h"
#include "types/types.h"
#include "common/Logging.h"
#include "common/ThreadPool.h"

#include <gtest/gtest.h>
using testing::Eq;

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
    uint64_t producedNumber;
  };

  struct Result {
    uint64_t producedNumber;
  };

  std::random_device rd;
  std::uniform_int_distribution<unsigned long long> randomU64;

  static Result process(const Product& p) {
    // simulate cpu intense calculation
    uint64_t f = 100000000;
    while (f > 1) f = uint64_t(f/1.00000001);
    const Result result = Result{p.producedNumber + 1'000'000 + f};
//    std::cout << "   >>> Processing product...done: " << result.producedNumber << std::endl;;
    return result;
  }

  Product produceProduct() {
    std::this_thread::sleep_for(std::chrono::milliseconds (10));
    const Product product = Product{randomU64(rd)};
//    std::cout << "<<< Producing product...done: " << product.producedNumber << std::endl;;
    return product;
  }

};

TEST_F(ThreadPoolTest, basic) {
  std::cout << "Producer Worker Test" << std::endl;

  ThreadPool threadPool{6};
  std::vector<std::shared_ptr<std::future<Result>>> results{};

  std::cout << "Queuing and starting work" << std::endl;
  int number = 100;
#ifndef NDEBUG
  number = 30;
#endif

  for (int i = 0; i < number; i++) {
    Product product = produceProduct();
    auto future = std::make_shared<std::future<Result>>(threadPool.enqueue([&]{return process(product);}));
    results.push_back(future);
  }

  std::cout << "Getting results" << std::endl;
  const auto &iterEnd = results.end();
  for (auto iter = results.begin(); iter < iterEnd; iter++) {
    std::cout << "Open tasks: " << threadPool.openTasks() << std::endl;
    const uint64_t producedNumber = iter->get()->get().producedNumber;
    std::cout << "Result: " << producedNumber << std::endl;
  }

  SUCCEED();
}
