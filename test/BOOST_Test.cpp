/*
 * MIT License
 *
 * Copyright (c) 2020 Frank Kopp
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

#include <gtest/gtest.h>
using testing::Eq;

#include "types/types.h"
#include <chrono>
#include <iostream>

// BOOST timer
#include <boost/timer/timer.hpp>

// BOOST filesystem
#include <boost/filesystem.hpp>
namespace bfs = boost::filesystem;

TEST(BOOST, cpu_timer) {

  std::cout.imbue(deLocale);
  std::cout.imbue(deLocale);

  uint64_t i = 0;
  const uint64_t iterations = 10;
  const int repetitions = 10;
  uint64_t sum = 0;

  boost::timer::cpu_timer timer;
  timer.stop();

  std::cout << timer.format();
  while (i++ < iterations) {
    for (int j = 0; j < repetitions; ++j) {
      uint64_t n = 100000, first = 0, second = 1, next = 0, c = 0;
      timer.resume();
      auto start = std::chrono::high_resolution_clock::now();
      // Fibunacci numbers
      for (c = 0; c < n; c++) {
        if (c <= 1) {
          next = c;
        }
        else {
          next = first + second;
          first = second;
          second = next;
        }
        //std::cout << next << " ";
      }
      timer.stop();
      auto finish = std::chrono::high_resolution_clock::now();
      sum += std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();
      //std::cout << std::endl;
    }
  }
  std::cout << timer.format();
  std::cout << " " << std::setprecision(7) << static_cast<double>(sum) / 1e9 << std::endl;
  SUCCEED();
}

TEST(BOOST, boostFS) {
//  // CYGWIN issue with path
//  std::string filePathStr = FrankyCPP_PROJECT_ROOT;
//  filePathStr += "/books/superbook.pgn";
//  //filePathStr = bfs::current_path().string() + "\\FrankyCPP.log";
//  fprintln("{}", filePathStr);
//  bfs::path p{filePathStr};
//  fprintln("{}", p.string());
//  const boost::filesystem::path &canonical1 = bfs::canonical(p);
//  fprintln("{}", canonical1.string());
//  uint64_t fsize = bfs::file_size(canonical1);
//  fprintln("{}", fsize);
}

//#include <boost/log/core.hpp>
//#include <boost/log/trivial.hpp>
//#include <boost/log/expressions.hpp>
//#include <boost/log/sinks/basic_sink_frontend.hpp>
//#include <boost/log/utility/setup/common_attributes.hpp>
//#include <boost/log/sources/severity_channel_logger.hpp>
//#include <boost/log/sources/record_ostream.hpp>
//
//namespace logging = boost::log;
//namespace src = boost::log::sources;
//namespace sinks = boost::log::sinks;
//namespace keywords = boost::log::keywords;
//
//TEST(SuiteName, Logging) {
//  logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::trace);
//
//  logging::add_common_attributes();
//
//  using namespace logging::trivial;
//  src::severity_channel_logger<severity_level> lg;
//
//  BOOST_LOG_CHANNEL_SEV(lg, "net", trace) << "A trace severity message";
//  BOOST_LOG_CHANNEL_SEV(lg, "uci", trace) << "A trace severity message";
//  BOOST_LOG_SEV(lg, debug) << "A debug severity message";
//  BOOST_LOG_SEV(lg, info) << "An informational severity message";
//  BOOST_LOG_SEV(lg, warning) << "A warning severity message";
//  BOOST_LOG_SEV(lg, error) << "An error severity message";
//  BOOST_LOG_SEV(lg, fatal) << "A fatal severity message";
//}
