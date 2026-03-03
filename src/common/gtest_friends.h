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

// FrankyCPP
// Minimal FRIEND_TEST macro for production headers without depending on GoogleTest.
//
// FRIEND_TEST(suite, test) — standard macro for classes at global scope.
//
// FRIEND_TEST_NS(suite, test) — for classes inside a namespace.
//   GoogleTest test classes live at global scope, so the friend declaration
//   must use :: qualification. Requires a matching FRIEND_TEST_FWD_DECL()
//   BEFORE the namespace to forward-declare the test class.
//
// Example (namespaced class):
//   FRIEND_TEST_FWD_DECL(MyTest, myMethod);
//   namespace engine {
//   class MyClass {
//     FRIEND_TEST_NS(MyTest, myMethod);  // refers to ::MyTest_myMethod_Test
//   };
//   }
//
#ifndef FRANKYCPP_GTEST_FRIENDS_H
#define FRANKYCPP_GTEST_FRIENDS_H

#ifndef FRIEND_TEST
#define FRIEND_TEST(test_case_name, test_name) \
  friend class test_case_name##_##test_name##_Test
#endif

// Namespace-safe variant: uses :: to refer to the global-scope test class.
#define FRIEND_TEST_NS(test_case_name, test_name) \
  friend class ::test_case_name##_##test_name##_Test

// Forward-declare a GoogleTest test class at global scope.
// Required before namespace blocks that contain FRIEND_TEST_NS for the same test.
#define FRIEND_TEST_FWD_DECL(test_case_name, test_name) \
  class test_case_name##_##test_name##_Test

#endif// FRANKYCPP_GTEST_FRIENDS_H
