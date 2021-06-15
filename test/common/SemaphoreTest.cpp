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

#include <thread>

#include "common/Semaphore.h"

#include <gtest/gtest.h>
using testing::Eq;

void run();

Semaphore mySemaphore;
std::thread myThread;

enum State {
  NOOP, NEW, INITIALIZED, DONE
};

State myState = NOOP;

TEST(SemaphoreTest, basic) {

  // semaphore should not be available
  ASSERT_FALSE(mySemaphore.get());
  ASSERT_EQ(NOOP, myState);

  //std::cout << "Start Thread!\n";
  std::thread myTestThread = std::thread([] { run(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  ASSERT_EQ(NEW, myState);

  //std::cout << "Wait for Thread init!\n";
  mySemaphore.getOrWait();
  ASSERT_FALSE(mySemaphore.get());
  ASSERT_EQ(INITIALIZED, myState);

  //std::cout << "Thread Started\n";

  if (myTestThread.get_id() == std::this_thread::get_id()) { std::cout << "start: NEW THREAD\n"; }
  else { std::cout << "start: OLD THREAD\n"; }

  myTestThread.join();
  ASSERT_EQ(DONE, myState);

  //std::cout << "Thread Ended\n";
}

void run() {
  myState = NEW;
  //std::cout << "New Thread: Started!\n";
  std::this_thread::sleep_for(std::chrono::seconds(2));
  myState = INITIALIZED;
  //std::cout << "New Thread: Init done!\n";
  mySemaphore.release();
  if (myThread.get_id() == std::this_thread::get_id())
    std::cout << "run: NEW THREAD\n";
  else
    std::cout << "run: OLD THREAD\n";
  std::this_thread::sleep_for(std::chrono::seconds(2));
  myState = DONE;
  //std::cout << "New Thread: Finished!\n";
}
