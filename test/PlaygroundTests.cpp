/*
 * MIT License
 *
 * Copyright (c) 2018 Frank Kopp
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

#include <fmt/core.h>

#include <gtest/gtest.h>
using testing::Eq;

#include <iostream>
#include <string>
#include <vector>

TEST(Playground, emplace) {

  struct A {
    std::string s;
    A(std::string str) : s(std::move(str)) {
      std::cout << " constructed\n";
    }
    A(const A& o) : s(o.s) {
      std::cout << " copy constructed\n";
    }
    A(A&& o) : s(std::move(o.s)) {
      std::cout << " move constructed\n";
    }
    A& operator=(const A& other) {
      s = other.s;
      std::cout << " copy assigned\n";
      return *this;
    }
    A& operator=(A&& other) {
      s = std::move(other.s);
      std::cout << " move assigned\n";
      return *this;
    }
    void * operator new(size_t size) {
      std::cout<< "new (allocated size=: " << size << ")" << std::endl;
//      void * p = ::new A("");
      void * p = malloc(size);
      return p;
    }
    void operator delete(void * p) {
      std::cout<< "delete" << std::endl;
      free(p);
    }
  };

  std::vector<A> container;
  // reserve enough place so vector does not have to resize
  container.reserve(10);
  std::cout << "construct 2 times A:\n";
  A two{"two"};
  A three{"three"};

  std::cout << "emplace:\n";
  container.emplace(container.end(), "one");

  std::cout << "emplace with A&:\n";
  container.emplace(container.end(), two);

  std::cout << "emplace with A&&:\n";
  container.emplace(container.end(), std::move(three));

  std::cout << "emplace with new A():\n";
  A* four = new A("four");
  container.emplace(container.end(), *four);

  std::cout << "content:\n";
  for (const auto& obj : container)
    std::cout << ' ' << obj.s;
  std::cout << '\n';
}