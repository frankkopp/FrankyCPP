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

#ifndef FRANKYCPP_SPLITSTRING_H
#define FRANKYCPP_SPLITSTRING_H

// see https://gitlab.com/tbeu/wcx_setfolderdate/-/blob/master/src/splitstring.h
// see https://stackoverflow.com/a/236803/8520615

#pragma once

#include <sstream>
#include <string>
#include <vector>

template<typename T>
using StringType = std::basic_string<T, std::char_traits<T>, std::allocator<T>>;

template<typename T>
using StringStreamType = std::basic_stringstream<T, std::char_traits<T>, std::allocator<T>>;

template<typename T, typename Out>
inline static void splitT(const StringType<T>& s, T delim, Out result) {
  StringStreamType<T> ss(s);
  StringType<T> item;
  while (std::getline(ss, item, delim)) {
    *(result++) = std::move(item);
  }
}

template <typename T>
inline static void splitT(const StringType<T>& s, std::vector<StringType<T>>& elems, T delim) {
  splitT<T, std::back_insert_iterator<std::vector<StringType<T>>>>(s, delim, std::back_inserter(elems));
}

constexpr auto split = splitT<char>;
constexpr auto splitW = splitT<wchar_t>;

#endif//FRANKYCPP_SPLITSTRING_H
