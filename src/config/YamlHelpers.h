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

#ifndef FRANKYCPP_YAMLHELPERS_H
#define FRANKYCPP_YAMLHELPERS_H

#include <array>
#include <string>
#include <unordered_set>
#include <yaml-cpp/yaml.h>

namespace yaml {

  // If key exists in node, assign to out and record key in seen
  template<typename T>
  void set_if_present(const YAML::Node& n, const char* key, T& out, std::unordered_set<std::string>& seen) {
    if (const auto v = n[key]) {
      out = v.as<T>();
      seen.emplace(key);
    }
  }

  // If key exists and is a sequence, copy up to N ints into out and record key in seen
  template<size_t N>
  void set_array_if_present(const YAML::Node& n, const char* key, std::array<int, N>& out, std::unordered_set<std::string>& seen) {
    if (auto v = n[key]) {
      if (v.IsSequence()) {
        size_t i = 0;
        for (const auto& e : v) {
          if (i >= N) break;
          out[i++] = e.as<int>();
        }
        seen.emplace(key);
      }
    }
  }

}// namespace yaml

#endif // FRANKYCPP_YAMLHELPERS_H
