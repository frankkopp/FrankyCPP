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

#ifndef FRANKYCPP_STRINGUTIL_H
#define FRANKYCPP_STRINGUTIL_H

// see https://gitlab.com/tbeu/wcx_setfolderdate/-/blob/master/src/splitstring.h
// see https://stackoverflow.com/a/236803/8520615

#include <algorithm>
#include <string>
#include <vector>

// splits a string or string view into a vector of parts at each delimiter
template<typename T>
inline void splitFast(const T& str, std::vector<T>& container, const std::string& delims = " ") {
  for (auto first = str.data(), second = str.data(), last = first + str.size();
       second != last && first != last;
       first = second + 1) {
    second = std::find_first_of(first, last, std::cbegin(delims), std::cend(delims));
    if (first != second) {
      container.emplace_back(first, second - first);
    }
  }
}

// removes whitespace characters from beginning and end of string s
// whitespaces are defined as:  ' ' || '\t' || '\n' || '\v' || '\f' || '\r'
template <typename T>
inline T trimFast(const T& s) {
  const int l = static_cast<int>(s.length());
  int a = 0, b = l - 1;
  char c;
  while (a < l && ((c = s[a]) == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r')) a++;
  while (b > a && ((c = s[b]) == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r')) b--;
  return s.substr(a, 1 + b - a);
}

// removes trailing parts of a string after a given commentMarker
template <typename T>
inline T removeTrailingComments(const T& s, const std::string& commentMarker) {
  const auto firstOf = s.find_first_of(commentMarker);
  if (firstOf != std::string_view::npos) {
    return s.substr(0, firstOf);
  }
  return s;
}

// slower alternatives for trimming
//Round  1 Test  1: 5.684.239.320 ns (   100%) (  5,68423932 sec) ( 56.842,3932 ns avg per test)
//Round  1 Test  2: 5.081.285.270 ns (    89%) (  5,08128527 sec) ( 50.812,8527 ns avg per test)
//Round  1 Test  3:   19.676.920 ns (     0%) (  0,01967692 sec) (    196,7692 ns avg per test)
//Round  1 Test  4:   44.201.860 ns (   224%) (  0,04420186 sec) (    442,0186 ns avg per test)
//Round  1 Test  5:   16.635.990 ns (    37%) (  0,01663599 sec) (    166,3599 ns avg per test)
//Round  1 Test  6:    2.149.750 ns (    12%) (  0,00214975 sec) (     21,4975 ns avg per test)
// 5 = trimFast(string), 6 = trimFast(string_view)
//inline std::string trimRegex(const std::string& toTrim) {
//  const std::regex trimWhiteSpace(R"(^\s+|\s+$)");
//  return std::regex_replace(toTrim, trimWhiteSpace, "");
//}
//
//inline std::string trimRegex(const std::string_view& toTrim) {
//  const std::regex trimWhiteSpace(R"(^\s+|\s+$)");
//  // create a trimmed copy of the string as regex can't handle string_view :(
//  return std::regex_replace(std::string{toTrim}, trimWhiteSpace, "");
//}
//
//inline std::string trimFindNot(const std::string& toTrim,
//                               const std::string& whitespace = " \t\n\v\f\r") {
//  const auto strBegin = toTrim.find_first_not_of(whitespace);
//  if (strBegin == std::string::npos) {
//    return "";// no content
//  }
//  const auto strEnd   = toTrim.find_last_not_of(whitespace);
//  const auto strRange = strEnd - strBegin + 1;
//  return toTrim.substr(strBegin, strRange);
//}
//
//inline std::string& ltrim(std::string& str) {
//  auto it2 =  std::find_if( str.begin() , str.end() , [](char ch){ return !std::isspace<char>(ch , std::locale::classic() ) ; } );
//  str.erase( str.begin() , it2);
//  return str;
//}
//
//inline std::string& rtrim(std::string& str) {
//  auto it1 =  std::find_if( str.rbegin() , str.rend() , [](char ch){ return !std::isspace<char>(ch , std::locale::classic() ) ; } );
//  str.erase( it1.base() , str.end() );
//  return str;
//}
//
//inline std::string& trimFindIf(std::string& str) {
//  return ltrim(rtrim(str));
//}


#endif//FRANKYCPP_STRINGUTIL_H
