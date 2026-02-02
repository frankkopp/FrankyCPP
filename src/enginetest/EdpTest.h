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

#ifndef FRANKYCPP_TEST_H
#define FRANKYCPP_TEST_H

//=============================================================================
// Test.h - Individual Test Case Representation
//=============================================================================
//
// Represents a single EPD test case with its definition and execution results.
//
// Structure:
// - Test definition (immutable after construction): FEN, type, expected moves
// - Test results (mutable during execution): actual move, result, statistics
//
// Construction:
// - Use Test::Builder pattern for clean, explicit construction
// - Builder is primarily used by EpdParser
//
// Usage:
//   Test::Builder builder;
//   Test test = builder.setFen("rnbqkbnr/...")
//                      .setType(TestType::BM)
//                      .setTargetMoves(moves)
//                      .build();
//
// Access:
// - Immutable test definition via const getters
// - Mutable results via setters (used by TestSuite during execution)
//
//=============================================================================

#include "TestTypes.h"
#include "types/types.h"

#include <string>

// Forward declarations
class TestSuite;

/// Represents a single EPD test case.
/// Created by EpdParser, executed by TestSuite.
/// Test definition is immutable after construction; results are populated during execution.
class EpdTest {

  // Test definition (immutable after construction)
  std::string id_{};
  std::string fen_{};
  std::string line_{};                        // Original EPD line
  TestType type_{TestType::NOOP};
  MoveList targetMoves_{};
  Depth mateDepth_{DEPTH_NONE};
  Move expected_{MOVE_NONE};                  // First expected move (for display)

  // Test results (mutable during execution)
  Move actualMove_{MOVE_NONE};
  Value actualValue_{VALUE_NONE};
  ResultType result_{ResultType::NOT_TESTED};
  uint64_t nodes_{0};
  nanoseconds time_{0};
  uint64_t nps_{0};

  // Private default constructor - use Builder
  EpdTest() = default;


  friend class Builder;
  friend class TestSuite;

public:
  class Builder;

  // Move operations (public for std::optional and std::vector support)
  EpdTest(EpdTest&&) noexcept = default;
  EpdTest& operator=(EpdTest&&) noexcept = default;

  // Delete copy operations (tests should not be copied)
  EpdTest(const EpdTest&) = delete;
  EpdTest& operator=(const EpdTest&) = delete;

  // default destructor
  ~EpdTest() = default;

  // Test result mutators (for TestSuite during execution)
  void setActualMove(const Move m) noexcept { actualMove_ = m; }
  void setActualValue(const Value v) noexcept { actualValue_ = v; }
  void setResult(const ResultType r) noexcept { result_ = r; }
  void setNodes(const uint64_t n) noexcept { nodes_ = n; }
  void setTime(const nanoseconds t) noexcept { time_ = t; }
  void setNps(const uint64_t n) noexcept { nps_ = n; }

  // Test definition accessors (immutable)
  [[nodiscard]] const std::string& getId() const noexcept { return id_; }
  [[nodiscard]] const std::string& getFen() const noexcept { return fen_; }
  [[nodiscard]] const std::string& getLine() const noexcept { return line_; }
  [[nodiscard]] TestType getType() const noexcept { return type_; }
  [[nodiscard]] const MoveList& getTargetMoves() const noexcept { return targetMoves_; }
  [[nodiscard]] Depth getMateDepth() const noexcept { return mateDepth_; }
  [[nodiscard]] Move getExpectedMove() const noexcept { return expected_; }

  // Test result accessors
  [[nodiscard]] Move getActualMove() const noexcept { return actualMove_; }
  [[nodiscard]] Value getActualValue() const noexcept { return actualValue_; }
  [[nodiscard]] ResultType getResult() const noexcept { return result_; }
  [[nodiscard]] uint64_t getNodes() const noexcept { return nodes_; }
  [[nodiscard]] nanoseconds getTime() const noexcept { return time_; }
  [[nodiscard]] uint64_t getNps() const noexcept { return nps_; }
};

/// Builder for constructing EpdTest instances.
/// Used by EpdParser to create tests from parsed EPD lines.
///
/// Example:
///   EpdTest::Builder builder;
///   EpdTest test = builder.setId("Test1")
///                      .setFen("rnbq...")
///                      .setType(TestType::BM)
///                      .setTargetMoves(moves)
///                      .build();
class EpdTest::Builder {
public:
  Builder& setId(std::string id) {
    test_.id_ = std::move(id);
    return *this;
  }

  Builder& setFen(std::string fen) {
    test_.fen_ = std::move(fen);
    return *this;
  }

  Builder& setLine(std::string line) {
    test_.line_ = std::move(line);
    return *this;
  }

  Builder& setType(const TestType type) noexcept {
    test_.type_ = type;
    return *this;
  }

  Builder& setTargetMoves(MoveList moves) {
    test_.targetMoves_ = std::move(moves);
    return *this;
  }

  Builder& setMateDepth(const Depth depth) noexcept {
    test_.mateDepth_ = depth;
    return *this;
  }

  Builder& setExpectedMove(const Move m) noexcept {
    test_.expected_ = m;
    return *this;
  }

  /// Builds and returns the EpdTest object.
  /// Builder is moved-from after this call.
  [[nodiscard]] EpdTest build() {
    return std::move(test_);
  }

private:
  EpdTest test_;
};

#endif // FRANKYCPP_TEST_H
