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

#include "TestSuite.h"

#include "common/Logging.h"
#include "common/stringutil.h"
#include "engine/UciOptions.h"

#include "types/timeunits.h"
#include <boost/algorithm/string.hpp>
#include <chrono>

#include <fstream>
#include <iostream>
#include <regex>

TestSuite::TestSuite(const milliseconds& time, const Depth searchDepth, const std::string& filePath)
    : searchTime(time), searchDepth(searchDepth), filePath(filePath) {

  LOG__INFO(Logger::get().TSUITE_LOG, "Preparing Test Suite {}", filePath);

  CONFIG_OVERRIDE(s.USE_BOOK = false;);

  // read EPD file
  fprintln("Reading EPD File: ...");
  readTestCases(filePath, testCases);
  fprintln("                  ... DONE");
  fprintln("");
}


void TestSuite::runTestSuite() {
  if (testCases.empty()) {
    LOG__WARN(Logger::get().TSUITE_LOG, "No tests to run in {}", filePath);
    return;
  }

  printReportHeader();
  const auto startTime = currentTime();


  // execute all tests and store results in the test instance
  runAllTests();

  // count and sum up the results
  lastResult = sumUpTests();

  const auto elapsed = elapsedSince(startTime);
  printReport(elapsed);
}

void TestSuite::printReportHeader() {
  fprintln("Running Test Suite");
  fprintln("==================================================================");
  fprintln("EPD File:    {}", filePath);
  fprintln("SearchTime:  {}", str(searchTime));
  fprintln("MaxDepth:    {}", searchDepth);
  fprintln("No of tests: {}", testCases.size());
  fprintln("Date:        {}", format_now());
  fprintln("");
}

void TestSuite::printReport(const nanoseconds elapsed) {
  // print report
  fprintln("Results for Test Suite", filePath);
  fprintln("------------------------------------------------------------------------------------------------------------------------------------");
  fprintln("EPD File:   {}", filePath);
  fprintln("SearchTime: {}", str(searchTime));
  fprintln("MaxDepth:   {}", searchDepth);
  fprintln("Date:       {}", format_now());
  fprintln("===================================================================================================================================");
  fprintln(" {:<4} | {:<10} | {:<8} | {:<8} | {:<18} | {} | {}", " Nr.", "Result", "Move", "Value", "Expected Result", "Fen", "Id");
  fprintln("====================================================================================================================================");
  int i = 0;
  for (const auto& t : testCases) {
    i++;
    if (t.type == DM) {
      fprintln(" {:<4d} | {:<10} | {:<8} | {:<8} | {} {:<15d} | {} | {}",
               i, resultTypeStr[t.result], t.actualMove.str(), t.actualValue.str(), testTypeStr[t.type], t.mateDepth, t.fen, t.id);
    }
    else {
      fprintln(" {:<4d} | {:<10} | {:<8} | {:<8} | {} {:<15} | {} | {}",
               i, resultTypeStr[t.result], t.actualMove.str(), t.actualValue.str(), testTypeStr[t.type], t.targetMoves.str(), t.fen, t.id);
    }
  }
  fprintln("====================================================================================================================================");
  fprintln("Summary:");
  fprintln("EPD File:   {}", filePath);
  fprintln("SearchTime: {}", str(searchTime));
  fprintln("MaxDepth:   {}", searchDepth);
  fprintln("Date:       {}", format_now());
  fprintln("Successful: {:<3d} ({:d} %)", lastResult.successCounter, 100 * lastResult.successCounter / lastResult.counter);
  fprintln("Failed:     {:<3d} ({:d} %)", lastResult.failedCounter, 100 * lastResult.failedCounter / lastResult.counter);
  fprintln("Skipped:    {:<3d} ({:d} %)", lastResult.skippedCounter, 100 * lastResult.skippedCounter / lastResult.counter);
  fprintln("Not tested: {:<3d} ({:d} %)", lastResult.notTestedCounter, 100 * lastResult.notTestedCounter / lastResult.counter);
  fprintln("Test time:  {}", format(elapsed));
  fprintln("\nConfiguration:\n{}\n", UciOptions::getInstance()->str());
}

TestSuiteResult TestSuite::sumUpTests() const {
  TestSuiteResult tsr{};
  for (const auto& t : testCases) {
    tsr.counter++;
    switch (t.result) {
      case NOT_TESTED:
        tsr.notTestedCounter++;
        break;
      case SKIPPED:
        tsr.skippedCounter++;
        break;
      case FAILED:
        tsr.failedCounter++;
        break;
      case SUCCESS:
        tsr.successCounter++;
        break;
    }
    tsr.nodes += t.nodes;
    tsr.time += t.time;
  }
  return tsr;
}

void TestSuite::runAllTests() {
  int i = 0;
  // Ensure evaluator configuration is at defaults for this suite run
  engine::config::ConfigManager::instance().resetToDefaults();
  // loop over all test cases and execute the test
  for (auto& test : testCases) {
    fprintln("Test {} of {}\nTest: {} -- Target Result {}",
             ++i, testCases.size(), test.line, test.targetMoves.str());

    // setup search
    Search search{};
    SearchLimits searchLimits{};
    searchLimits.depth = searchDepth;
    if (searchTime.count()) {
      searchLimits.moveTime    = searchTime;
      searchLimits.timeControl = true;
    }

    const auto startTime2 = currentTime();
    runSingleTest(search, searchLimits, test);
    const auto elapsedTime = elapsedSince(startTime2);
    test.nodes       = search.getLastSearchResult().nodes;
    test.time        = search.getLastSearchResult().time;
    test.nps         = nps(search.getLastSearchResult().nodes, search.getLastSearchResult().time);
    fprintln("Test finished in {} with result {} ({}) - nps: {:L}\n\n",
             format(elapsedTime), resultTypeStr[test.result], test.actualMove.str(), test.nps);
  }
}

void TestSuite::runSingleTest(Search& search, SearchLimits& limits, Test& test) {
  // reset search and search limits
  search.newGame();
  limits.mate = 0;
  const Position p{test.fen};
  // call the appropriate function for the test type
  switch (test.type) {
    case DM:
      directMateTest(search, limits, p, test);
      break;
    case BM:
      bestMoveTest(search, limits, p, test);
      break;
    case AM:
      avoidMoveTest(search, limits, p, test);
      break;
    case NOOP:
      return;
  }
}

void TestSuite::directMateTest(Search& search, SearchLimits& limits, const Position& position, Test& test) {
  // get target mate depth
  limits.mate = test.mateDepth;
  // start search
  search.startSearch(position, limits);
  search.waitWhileSearching();
  // check and store result
  if ("mate " + std::to_string(limits.mate) == search.getLastSearchResult().bestMoveValue.str()) {
    LOG__INFO(Logger::get().TSUITE_LOG, "TestSet: ID \"{}\" SUCCESS", test.id);
    test.result = SUCCESS;
  }
  else {
    LOG__INFO(Logger::get().TSUITE_LOG, "TestSet: ID \"{}\" FAILED", test.id);
    test.result = FAILED;
  }
  test.actualMove  = search.getLastSearchResult().bestMove;
  test.actualValue = search.getLastSearchResult().bestMoveValue;
}

void TestSuite::bestMoveTest(Search& search, const SearchLimits& limits, const Position& position, Test& test) {
  // do the search
  search.startSearch(position, limits);
  search.waitWhileSearching();
  // get the result
  const Move actual = search.getLastSearchResult().bestMove.stripped();
  // check against expected moves
  for (const Move m : test.targetMoves) {
    if (m == actual) {
      LOG__INFO(Logger::get().TSUITE_LOG, "TestSet: ID \"{}\" SUCCESS", test.id);
      test.actualMove  = search.getLastSearchResult().bestMove;
      test.actualValue = search.getLastSearchResult().bestMoveValue;
      test.result      = SUCCESS;
      return;
    }
  }
  LOG__INFO(Logger::get().TSUITE_LOG, "TestSet: ID \"{}\" FAILED", test.id);
  test.actualMove  = search.getLastSearchResult().bestMove;
  test.actualValue = search.getLastSearchResult().bestMoveValue;
  test.result      = FAILED;
}

void TestSuite::avoidMoveTest(Search& search, const SearchLimits& limits, const Position& position, Test& test) {
  // do the search
  search.startSearch(position, limits);
  search.waitWhileSearching();
  // get the result
  const Move actual = search.getLastSearchResult().bestMove.stripped();
  // check against expected moves to avoid
  for (const Move m : test.targetMoves) {
    if (m == actual) {
      LOG__INFO(Logger::get().TSUITE_LOG, "TestSet: ID \"{}\" FAILED", test.id);
      test.actualMove  = search.getLastSearchResult().bestMove;
      test.actualValue = search.getLastSearchResult().bestMoveValue;
      test.result      = FAILED;
      return;
    }
    else {
      continue;
    }
  }
  LOG__INFO(Logger::get().TSUITE_LOG, "TestSet: ID \"{}\" SUCCESS", test.id);
  test.actualMove  = search.getLastSearchResult().bestMove;
  test.actualValue = search.getLastSearchResult().bestMoveValue;
  test.result      = SUCCESS;
}

void TestSuite::readTestCases(const std::string& filePathStr, std::vector<Test>& tests) {
  std::ifstream file(filePathStr);
  if (file.is_open()) {
    // read all lines from the file, parse the line into
    // a test case and add it to the list of test cases
    std::string line;
    while (getline(file, line)) {
      Test test{};
      if (readOneEPD(line, test)) {
        tests.push_back(test);
      }
    }
    file.close();
  }
  else {
    LOG__ERROR(Logger::get().TSUITE_LOG, "Could not open file: {}", filePathStr);
    return;
  }
}

bool TestSuite::readOneEPD(std::string& line, Test& test) {
  LOG__DEBUG(Logger::get().TSUITE_LOG, "EPD: {}", line);
  // skip empty lines and comments
  cleanUpLine(line);
  if (line.empty()) {
    return false;
  }
  // Find a EPD line
  std::regex regexPattern(R"(^\s*(.*) (bm|dm|am) (.*?);(.* id \"(.*?)\";)?.*$)");
  std::smatch matcher;
  if (!std::regex_match(line, matcher, regexPattern)) {
    LOG__WARN(Logger::get().TSUITE_LOG, "No EPD match found in {}", line);
    return false;
  }
  // get the parts
  std::string fen    = matcher.str(1);
  std::string type   = matcher.str(2);
  std::string result = matcher.str(3);
  std::string id     = matcher.str(5).empty() ? "no ID" : matcher.str(5);
  LOG__DEBUG(Logger::get().TSUITE_LOG, "Fen: {}    Type: {}    Result: {}    ID: {}", fen, type, result, id);
  // get position
  Position p;
  try {
    p = Position(fen);
  } catch (std::invalid_argument& e) {
    LOG__WARN(Logger::get().TSUITE_LOG, "Invalid fen {} could not create position from: {}", e.what(), line);
    return false;
  }
  // get test type
  TestType testType;
  if (type == "dm") {
    testType = DM;
  }
  else if (type == "bm") {
    testType = BM;
  }
  else if (type == "am") {
    testType = AM;
  }
  else {
    LOG__WARN(Logger::get().TSUITE_LOG, "Invalid TestType {}", type);
    return false;
  }
  // target moves
  MoveList resultMoves{};
  int dmDepth{};
  if (testType == BM || testType == AM) {
    boost::replace_all(result, "!", "");
    boost::replace_all(result, "?", "");
    // check if results are even valid on the position
    // and store the moves into the test
    MoveGenerator mg{};
    std::vector<std::string> results;
    boost::split(results, result, [](const char c) { return c == ' '; });
    for (auto s : results) {
      boost::trim(s);
      Move m = mg.getMoveFromSan(p, s);
      if (m.isValid()) {
        resultMoves.emplace_back(m);
      }
    }
    if (resultMoves.empty()) {
      LOG__WARN(Logger::get().TSUITE_LOG, "Result moves from EPD {} are invalid on this position {}", result, p.strFen());
      return false;
    }
  }
  else { // if (testType == DM)
    std::istringstream(result) >> dmDepth;
    if (!dmDepth) {
      LOG__WARN(Logger::get().TSUITE_LOG, "Direct mate depth from EPD is invalid  {}", result);
      return false;
    }
  }
  // Configure the test
  test.id          = id;
  test.fen         = fen;
  test.type        = testType;
  test.targetMoves = resultMoves;
  test.mateDepth   = static_cast<Depth>(dmDepth);
  test.line        = line;
  return true;
}

std::string& TestSuite::cleanUpLine(std::string& line) {
  line = trimFast(line);
  const std::regex leadCommentTrim(R"(^\s*#.*$)");
  line = std::regex_replace(line, leadCommentTrim, "");
  const std::regex trailCommentTrim(R"(^(.*)#([^;]*)$)");
  line = std::regex_replace(line, trailCommentTrim, "$1;");
  return line;
}
