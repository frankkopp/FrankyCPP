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

#include "EpdParser.h"
#include "common/Logging.h"
#include "engine/SearchStats.h"
#include "engine/UciOptions.h"
#include "types/timeunits.h"

#include <iostream>

using namespace engine;
using namespace chess;
using namespace config;
using namespace common;
using namespace enginetest;

TestSuite::TestSuite(const milliseconds time, const Depth searchDepth, const std::string_view filePath)
    : searchTime_(time), searchDepth_(searchDepth), filePath_(filePath) {

  LOG__INFO(Logger::get().TSUITE_LOG, "Preparing Test Suite {}", filePath_);

  CONFIG_OVERRIDE(s.USE_BOOK = false;);

  // read EPD file using EpdParser
  fprintln("Reading EPD File: ...");
  testCases_ = EpdParser::parseFile(filePath_);
  fprintln("                  ... DONE");
  fprintln("");
}

void TestSuite::runTestSuite() {
  if (testCases_.empty()) {
    LOG__WARN(Logger::get().TSUITE_LOG, "No tests to run in {}", filePath_);
    return;
  }

  printReportHeader();
  const auto startTime = currentTime();


  // execute all tests and store results in the test instance
  runAllTests();

  // count and sum up the results
  lastResult_ = sumUpTests();

  const auto elapsed = elapsedSince(startTime);
  printReport(elapsed);
}

void TestSuite::printReportHeader() const {
  fprintln("Running Test Suite");
  fprintln("==================================================================");
  fprintln("EPD File:    {}", filePath_);
  fprintln("SearchTime:  {}", str(searchTime_));
  fprintln("MaxDepth:    {}", searchDepth_);
  fprintln("No of tests: {}", testCases_.size());
  fprintln("Date:        {}", format_now());
  fprintln("");
}

void TestSuite::printReport(const nanoseconds elapsed) const {
  // print report
  fprintln("Results for Test Suite", filePath_);
  fprintln("------------------------------------------------------------------------------------------------------------------------------------------------");
  fprintln("EPD File:   {}", filePath_);
  fprintln("SearchTime: {}", str(searchTime_));
  fprintln("MaxDepth:   {}", searchDepth_);
  fprintln("Date:       {}", format_now());
  fprintln("================================================================================================================================================");
  fprintln(" {:<4} | {:<10} | {:<8} | {:<8} | {:<18} | {:<18} | {} | {}", " Nr.", "Result", "Move", "Value", "Expected Result", "BetaCuts %", "Fen", "Id");
  fprintln("================================================================================================================================================");
  int i = 0;
  for (const auto& t : testCases_) {
    i++;
    // Format beta cuts percentages as "XX.X/XX.X/XX.X"
    const std::string betaCutsStr = std::format("{:.1f}/{:.1f}/{:.1f}",
                                                t.getBetaCutsPct(0), t.getBetaCutsPct(1), t.getBetaCutsPct(2));
    if (t.getType() == TestType::DM) {
      fprintln(" {:<4d} | {:<10} | {:<8} | {:<8} | {} {:<15d} | {:<18} | {} | {}",
               i, resultTypeToString(t.getResult()), t.getActualMove().str(), t.getActualValue().str(),
               testTypeToString(t.getType()), t.getMateDepth(), betaCutsStr, t.getFen(), t.getId());
    }
    else {
      fprintln(" {:<4d} | {:<10} | {:<8} | {:<8} | {} {:<15} | {:<18} | {} | {}",
               i, resultTypeToString(t.getResult()), t.getActualMove().str(), t.getActualValue().str(),
               testTypeToString(t.getType()), t.getTargetMoves().str(), betaCutsStr, t.getFen(), t.getId());
    }
  }
  fprintln("================================================================================================================================================");
  fprintln("Summary:");
  fprintln("EPD File:   {}", filePath_);
  fprintln("SearchTime: {}", str(searchTime_));
  fprintln("MaxDepth:   {}", searchDepth_);
  fprintln("Date:       {}", format_now());
  fprintln("Successful: {:<3d} ({:d} %)", lastResult_.successCounter, 100 * lastResult_.successCounter / lastResult_.counter);
  fprintln("Failed:     {:<3d} ({:d} %)", lastResult_.failedCounter, 100 * lastResult_.failedCounter / lastResult_.counter);
  fprintln("Skipped:    {:<3d} ({:d} %)", lastResult_.skippedCounter, 100 * lastResult_.skippedCounter / lastResult_.counter);
  fprintln("Not tested: {:<3d} ({:d} %)", lastResult_.notTestedCounter, 100 * lastResult_.notTestedCounter / lastResult_.counter);
  fprintln("Test time:  {}", format(elapsed));
  fprintln("\nConfiguration:\n{}\n", UciOptions::getInstance()->str());
}

TestSuiteResult TestSuite::sumUpTests() const {
  TestSuiteResult tsr{};
  for (const auto& t : testCases_) {
    tsr.counter++;
    switch (t.getResult()) {
      case ResultType::NOT_TESTED:
        tsr.notTestedCounter++;
        break;
      case ResultType::SKIPPED:
        tsr.skippedCounter++;
        break;
      case ResultType::FAILED:
        tsr.failedCounter++;
        break;
      case ResultType::SUCCESS:
        tsr.successCounter++;
        break;
    }
    tsr.nodes += t.getNodes();
    tsr.time += t.getTime();
  }
  return tsr;
}

void TestSuite::runAllTests() {
  int i = 0;
  // // Ensure evaluator configuration is at defaults for this suite run
  // FIXME: this should not be here otherwise we never could test different configurations.
  // ConfigManager::instance().resetToDefaults();

  // loop over all test cases and execute the test
  for (auto& test : testCases_) {
    fprintln("Test {} of {}\nTest: {} -- Target Result {}",
             ++i, testCases_.size(), test.getLine(), test.getTargetMoves().str());

    // setup search
    Search search{};
    SearchLimits searchLimits{};
    searchLimits.depth = searchDepth_;
    if (searchTime_.count()) {
      searchLimits.moveTime    = searchTime_;
      searchLimits.timeControl = true;
    }

    const auto startTime2 = currentTime();
    runSingleTest(search, searchLimits, test);
    const auto elapsedTime = elapsedSince(startTime2);
    test.setNodes(search.getLastSearchResult().nodes);
    test.setTime(search.getLastSearchResult().time);
    test.setNps(nps(search.getLastSearchResult().nodes, search.getLastSearchResult().time));

    // Capture beta cutoff percentages
    const auto& stats = search.getSearchStats();
    if (stats.betaCuts > 0) {
      const auto totalCuts = static_cast<double>(stats.betaCuts);
      for (int idx = 0; idx < EpdTest::BETA_CUTS_STORED; ++idx) {
        test.setBetaCutsPct(idx, 100.0 * static_cast<double>(stats.betaCutsByIndex[idx]) / totalCuts);
      }
    }

    fprintln("Test finished in {} with result {} ({}) - nps: {:L}\n\n",
             format(elapsedTime), resultTypeToString(test.getResult()), test.getActualMove().str(), test.getNps());
  }
}

void TestSuite::runSingleTest(Search& search, SearchLimits& limits, EpdTest& test) {
  // reset search and search limits
  search.newGame();
  limits.mate = 0;
  const Position p{test.getFen()};
  // call the appropriate function for the test type
  switch (test.getType()) {
    case TestType::DM:
      directMateTest(search, limits, p, test);
      break;
    case TestType::BM:
      bestMoveTest(search, limits, p, test);
      break;
    case TestType::AM:
      avoidMoveTest(search, limits, p, test);
      break;
    case TestType::NOOP:
      return;
  }
}

void TestSuite::directMateTest(Search& search, SearchLimits& limits, const Position& position, EpdTest& test) {
  // get target mate depth
  limits.mate = test.getMateDepth();
  // start search
  search.startSearch(position, limits);
  search.waitWhileSearching();
  // check and store result
  if ("mate " + std::to_string(limits.mate) == search.getLastSearchResult().bestMoveValue.str()) {
    LOG__INFO(Logger::get().TSUITE_LOG, "TestSet: ID \"{}\" SUCCESS", test.getId());
    test.setResult(ResultType::SUCCESS);
  }
  else {
    LOG__INFO(Logger::get().TSUITE_LOG, "TestSet: ID \"{}\" FAILED", test.getId());
    test.setResult(ResultType::FAILED);
  }
  test.setActualMove(search.getLastSearchResult().bestMove);
  test.setActualValue(search.getLastSearchResult().bestMoveValue);
}

void TestSuite::bestMoveTest(Search& search, const SearchLimits& limits, const Position& position, EpdTest& test) {
  // do the search
  search.startSearch(position, limits);
  search.waitWhileSearching();
  // get the result
  const Move actual = search.getLastSearchResult().bestMove.stripped();
  // check against expected moves
  for (const Move m : test.getTargetMoves()) {
    if (m == actual) {
      LOG__INFO(Logger::get().TSUITE_LOG, "TestSet: ID \"{}\" SUCCESS", test.getId());
      test.setActualMove(search.getLastSearchResult().bestMove);
      test.setActualValue(search.getLastSearchResult().bestMoveValue);
      test.setResult(ResultType::SUCCESS);
      return;
    }
  }
  LOG__INFO(Logger::get().TSUITE_LOG, "TestSet: ID \"{}\" FAILED", test.getId());
  test.setActualMove(search.getLastSearchResult().bestMove);
  test.setActualValue(search.getLastSearchResult().bestMoveValue);
  test.setResult(ResultType::FAILED);
}

void TestSuite::avoidMoveTest(Search& search, const SearchLimits& limits, const Position& position, EpdTest& test) {
  // do the search
  search.startSearch(position, limits);
  search.waitWhileSearching();
  // get the result
  const Move actual = search.getLastSearchResult().bestMove.stripped();
  // check against expected moves to avoid
  for (const Move m : test.getTargetMoves()) {
    if (m == actual) {
      LOG__INFO(Logger::get().TSUITE_LOG, "TestSet: ID \"{}\" FAILED", test.getId());
      test.setActualMove(search.getLastSearchResult().bestMove);
      test.setActualValue(search.getLastSearchResult().bestMoveValue);
      test.setResult(ResultType::FAILED);
      return;
    }
  }
  // No forbidden move was played - SUCCESS
  LOG__INFO(Logger::get().TSUITE_LOG, "TestSet: ID \"{}\" SUCCESS", test.getId());
  test.setActualMove(search.getLastSearchResult().bestMove);
  test.setActualValue(search.getLastSearchResult().bestMoveValue);
  test.setResult(ResultType::SUCCESS);
}
