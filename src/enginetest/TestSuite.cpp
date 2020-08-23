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

#include <fstream>
#include <iostream>
#include <regex>
#include <fmt/chrono.h>

#include "TestSuite.h"
#include <engine/SearchConfig.h>

#include <boost/algorithm/string.hpp>

TestSuite::TestSuite(const milliseconds& time, Depth searchDepth, const std::string& filePath)
    : searchTime(time), searchDepth(searchDepth), filePath(filePath) {

  LOG__INFO(Logger::get().TSUITE_LOG, "Preparing Test Suite {}", filePath);

  SearchConfig::USE_BOOK = false;

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

  auto startTime = currentTime();

  fprintln("Running Test Suite");
  fprintln("==================================================================");
  fprintln("EPD File:    {}", filePath);
  fprintln("SearchTime:  {}", str(searchTime));
  fprintln("MaxDepth:    {}", searchDepth);
  fprintln("No of tests: {}", testCases.size());
  fprintln("Date:        {:%Y-%m-%d %X}", fmt::localtime(time(nullptr)));
  fprintln("");

  // setup search
  Search search{};
  SearchLimits searchLimits{};
  searchLimits.depth = searchDepth;
  if (searchTime.count()) {
    searchLimits.moveTime    = searchTime;
    searchLimits.timeControl = true;
  }

  // execute all tests and store results in the test instance
  runAllTests(search, searchLimits);

  // count and sum up the results
  lastResult = sumUpTests();

  auto elapsed = elapsedSince(startTime);

  // print report
  fprintln("Results for Test Suite", filePath);
  fprintln("------------------------------------------------------------------------------------------------------------------------------------");
  fprintln("EPD File:   {}", filePath);
  fprintln("SearchTime: {}", str(searchTime));
  fprintln("MaxDepth:   {}", searchDepth);
  fprintln("Date:       {:%Y-%m-%d %X}", fmt::localtime(time(nullptr)));
  fprintln("===================================================================================================================================");
  fprintln(" {:<4s} | {:<10s} | {:<8s} | {:<8s} | {:<18s} | {:s} | {:s}", " Nr.", "Result", "Move", "Value", "Expected Result", "Fen", "Id");
  fprintln("====================================================================================================================================");
  int i = 0;
  for (const auto& t : testCases) {
    i++;
    if (t.type == DM) {
      fprintln(" {:<4d} | {:<10s} | {:<8s} | {:<8s} | {:s} {:<15d} | {:s} | {:s}",
               i, resultTypeStr[t.result], str(t.actualMove), str(t.actualValue), testTypeStr[t.type], t.mateDepth, t.fen, t.id);
    }
    else {
      fprintln(" {:<4d} | {:<10s} | {:<8s} | {:<8s} | {:s} {:<15s} | {:s} | {:s}",
               i, resultTypeStr[t.result], str(t.actualMove), str(t.actualValue), testTypeStr[t.type], str(t.targetMoves), t.fen, t.id);
    }
  }
  fprintln("====================================================================================================================================");
  fprintln("Summary:");
  fprintln("EPD File:   {}", filePath);
  fprintln("SearchTime: {}", str(searchTime));
  fprintln("MaxDepth:   {}", searchDepth);
  fprintln("Date:       {:%Y-%m-%d %X}", fmt::localtime(time(nullptr)));
  fprintln("Successful: {:<3d} ({:d} %)", lastResult.successCounter, 100 * lastResult.successCounter / lastResult.counter);
  fprintln("Failed:     {:<3d} ({:d} %)", lastResult.failedCounter, 100 * lastResult.failedCounter / lastResult.counter);
  fprintln("Skipped:    {:<3d} ({:d} %)", lastResult.skippedCounter, 100 * lastResult.skippedCounter / lastResult.counter);
  fprintln("Not tested: {:<3d} ({:d} %)", lastResult.notTestedCounter, 100 * lastResult.notTestedCounter / lastResult.counter);
  fprintln("Test time:  {:s}", format(elapsed));
  fprintln("\nConfiguration:\n{:s}\n", UciOptions::getInstance()->str());
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

void TestSuite::runAllTests(Search& search, SearchLimits& searchLimits) {
  int i = 0;
  // loop over all test cases and execute the test
  for (auto& test : testCases) {
    fprintln("Test {} of {}\nTest: {} -- Target Result {}",
             ++i, testCases.size(), test.line, str(test.targetMoves));
    auto startTime2 = currentTime();
    runSingleTest(search, searchLimits, test);
    auto elapsedTime = elapsedSince(startTime2);
    test.nodes       = search.getLastSearchResult().nodes;
    test.time        = search.getLastSearchResult().time;
    test.nps         = nps(search.getLastSearchResult().nodes, search.getLastSearchResult().time);
    fprintln("Test finished in {} with result {} ({}) - nps: {:L}\n\n",
             str(elapsedTime), resultTypeStr[test.result], str(test.actualMove), test.nps);
  }
}

void TestSuite::runSingleTest(Search& search, SearchLimits& limits, Test& test) {
  // reset search and search limits
  search.newGame();
  limits.mate = 0;
  Position p{test.fen};
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

void TestSuite::directMateTest(Search& search, SearchLimits& limits, Position& position, Test& test) {
  // get target mate depth
  limits.mate = test.mateDepth;
  // start search
  search.startSearch(position, limits);
  search.waitWhileSearching();
  // check and store result
  if ("mate " + std::to_string(limits.mate) == str(search.getLastSearchResult().bestMoveValue)) {
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

void TestSuite::bestMoveTest(Search& search, SearchLimits& limits, Position& position, Test& test) {
  // do the search
  search.startSearch(position, limits);
  search.waitWhileSearching();
  // get the result
  const Move actual = moveOf(search.getLastSearchResult().bestMove);
  // check against expected moves
  for (Move m : test.targetMoves) {
    if (m == actual) {
      LOG__INFO(Logger::get().TSUITE_LOG, "TestSet: ID \"{}\" SUCCESS", test.id);
      test.actualMove  = search.getLastSearchResult().bestMove;
      test.actualValue = search.getLastSearchResult().bestMoveValue;
      test.result      = SUCCESS;
      return;
    }
    else {
      continue;
    }
  }
  LOG__INFO(Logger::get().TSUITE_LOG, "TestSet: ID \"{}\" FAILED", test.id);
  test.actualMove  = search.getLastSearchResult().bestMove;
  test.actualValue = search.getLastSearchResult().bestMoveValue;
  test.result      = FAILED;
}

void TestSuite::avoidMoveTest(Search& search, SearchLimits& limits, Position& position, Test& test) {
  // do the search
  search.startSearch(position, limits);
  search.waitWhileSearching();
  // get the result
  const Move actual = moveOf(search.getLastSearchResult().bestMove);
  // check against expected moves to avoid
  for (Move m : test.targetMoves) {
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
    boost::split(results, result, [](char c) { return c == ' '; });
    for (auto s : results) {
      boost::trim(s);
      Move m = mg.getMoveFromSan(p, s);
      if (validMove(m)) {
        resultMoves.emplace_back(m);
      }
    }
    if (resultMoves.empty()) {
      LOG__WARN(Logger::get().TSUITE_LOG, "Result moves from EPD {} are invalid on this position {}", result, p.strFen());
      return false;
    }
  }
  else if (testType == DM) {
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
  //  fprintln("{}", line);
  std::regex whiteSpaceTrim(R"(^\s*(.*)\s*$)");
  line = std::regex_replace(line, whiteSpaceTrim, "$1");
  //  fprintln("{}", line);
  std::regex leadCommentTrim(R"(^\s*#.*$)");
  line = std::regex_replace(line, leadCommentTrim, "");
  //  fprintln("{}", line);
  std::regex trailCommentTrim(R"(^(.*)#([^;]*)$)");
  line = std::regex_replace(line, trailCommentTrim, "$1;");
  //  fprintln("{}", line);
  return line;
}

