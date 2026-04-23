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

//=============================================================================
// CLI Integration Tests
//=============================================================================
//
// Tests that run the actual FrankyCPP executable and verify CLI behavior.
// These tests validate:
//   - --help option works
//   - --version option works
//   - --show-config with various formats (table, yaml, json)
//   - --ucioptions output
//
// Note: These tests require the executable to be built first.
// They are skipped if the executable is not found.
//
//=============================================================================

#include "TestEnginePath.h"
#include "config/ConfigRegistry.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <string>

namespace {

  /// Execute a command and capture stdout
  /// @param cmd Command to execute
  /// @return Pair of (exit_code, stdout_output)
  std::pair<int, std::string> executeCommand(const std::string& cmd) {
    std::string output;
    std::array<char, 4096> buffer{};

    // Append stderr redirect to capture all output
    const std::string fullCmd = cmd + " 2>&1";
    std::cout << "Executing command: " << fullCmd << std::endl;

#ifdef _WIN32
    FILE* pipe = _popen(fullCmd.c_str(), "r");
#else
    FILE* pipe = popen(fullCmd.c_str(), "r");
#endif

    if (!pipe) {
      return {-1, "Failed to execute command"};
    }

    // ReSharper disable once CppRedundantCastExpression
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
      output += buffer.data();
    }

#ifdef _WIN32
    const int exitCode = _pclose(pipe);
#else
    const int exitCode = pclose(pipe);
#endif

    return {exitCode, output};
  }

  /// Execute a command with stdin input and capture stdout
  /// @param cmd Command to execute
  /// @param stdinInput Input to send to stdin
  /// @return Pair of (exit_code, stdout_output)
  std::pair<int, std::string> executeCommandWithInput(const std::string& cmd, const std::string& stdinInput) {
    std::string output;
    std::array<char, 4096> buffer{};

    // Use echo to pipe input, or use a temp file approach on Windows
    std::cout << "Executing command with input: " << cmd << std::endl;
    std::cout << "Input:\n"
              << stdinInput << std::endl;

#ifdef _WIN32
    // On Windows, use cmd /c with echo and pipe
    // Create a temporary file with the input
    const std::string tempFile = std::filesystem::temp_directory_path().string() + "\\franky_uci_test_input.txt";
    {
      std::ofstream ofs(tempFile);
      ofs << stdinInput;
    }
    // ReSharper disable once CppVariableCanBeMadeConstexpr
    const std::string fullCmd = "cmd /c \"type \"" + tempFile + "\" | " + cmd + "\" 2>&1";
    FILE* pipe                = _popen(fullCmd.c_str(), "r");
#else
    // On Unix, use echo with pipe
    // ReSharper disable once CppVariableCanBeMadeConstexpr
    const std::string fullCmd = "echo '" + stdinInput + "' | " + cmd + " 2>&1";
    FILE* pipe                = popen(fullCmd.c_str(), "r");
#endif

    if (!pipe) {
      return {-1, "Failed to execute command"};
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
      output += buffer.data();
    }

#ifdef _WIN32
    const int exitCode = _pclose(pipe);
    // Clean up temp file
    std::filesystem::remove(tempFile);
#else
    const int exitCode = pclose(pipe);
#endif

    return {exitCode, output};
  }

} // namespace

class CliIntegrationTest : public testing::Test {
protected:
  std::string enginePath;

  void SetUp() override {
    enginePath = getTestEnginePath();
    if (enginePath.empty()) {
      GTEST_SKIP() << "FrankyCPP executable not found - skipping CLI integration tests";
    }
    // Convert to absolute path with native separators for reliable execution
    enginePath = std::filesystem::absolute(enginePath).string();
    std::cout << "Engine path: " << enginePath << std::endl;
  }

  /// Run the engine with given arguments
  std::pair<int, std::string> runEngine(const std::string& args) const {
    // Build command - quotes around path handle spaces in path
    // ReSharper disable once CppVariableCanBeMadeConstexpr
    const std::string cmd = "\"" + enginePath + "\" " + args;
    return executeCommand(cmd);
  }

  /// Run the engine with stdin input (for UCI communication)
  std::pair<int, std::string> runEngineWithInput(const std::string& stdinInput) const {
    // ReSharper disable once CppVariableCanBeMadeConstexpr
    const std::string cmd = "\"" + enginePath + "\"";
    return executeCommandWithInput(cmd, stdinInput);
  }
};

//=============================================================================
// Basic CLI Tests
//=============================================================================

TEST_F(CliIntegrationTest, HelpOption) {
  auto [exitCode, output] = runEngine("--help");

  EXPECT_EQ(0, exitCode) << "Exit code should be 0 for --help";

  EXPECT_TRUE(output.find("Allowed options") != std::string::npos)
    << "Help output should contain 'Allowed options'";
  EXPECT_TRUE(output.find("--help") != std::string::npos)
    << "Help output should list --help option";
  EXPECT_TRUE(output.find("--show-config") != std::string::npos)
    << "Help output should list --show-config option";
}

TEST_F(CliIntegrationTest, VersionOption) {
  auto [exitCode, output] = runEngine("--version");

  EXPECT_EQ(0, exitCode) << "Exit code should be 0 for --version";

  // Check version string contains expected parts
  std::string expectedVersion = "FrankyCPP";
  expectedVersion
    .append(" v")
    .append(std::to_string(FrankyCPP_VERSION_MAJOR))
    .append(".")
    .append(std::to_string(FrankyCPP_VERSION_MINOR))
    .append(".")
    .append(std::to_string(FrankyCPP_VERSION_PATCH));
#ifdef FRANKYCPP_PRODUCTION
  expectedVersion.append(" (stripped)");
#endif

  EXPECT_TRUE(output.find(expectedVersion) != std::string::npos)
    << "Version output should contain '" << expectedVersion << "'\nActual: " << output;
}

//=============================================================================
// --show-config Tests
//=============================================================================

TEST_F(CliIntegrationTest, ShowConfigTableFormat) {
  auto [exitCode, output] = runEngine("--show-config --format table");

  EXPECT_EQ(0, exitCode) << "Exit code should be 0 for --show-config";

  // Check for expected table structure
  EXPECT_TRUE(output.find("Name") != std::string::npos)
    << "Table should have 'Name' column header";
  EXPECT_TRUE(output.find("Type") != std::string::npos)
    << "Table should have 'Type' column header";
  EXPECT_TRUE(output.find("Default") != std::string::npos)
    << "Table should have 'Default' column header";

  // Check for domain sections
  EXPECT_TRUE(output.find("General") != std::string::npos)
    << "Table should contain 'General' domain";
  EXPECT_TRUE(output.find("Search") != std::string::npos)
    << "Table should contain 'Search' domain";

  // Check for some known config entries
  EXPECT_TRUE(output.find("TT_SIZE_MB") != std::string::npos)
    << "Table should contain TT_SIZE_MB setting";
  EXPECT_TRUE(output.find("USE_BOOK") != std::string::npos)
    << "Table should contain USE_BOOK setting";
}

TEST_F(CliIntegrationTest, ShowConfigYamlFormat) {
  auto [exitCode, output] = runEngine("--show-config --format yaml");

  EXPECT_EQ(0, exitCode) << "Exit code should be 0 for --show-config --format yaml";

  // Check for YAML template structure
  EXPECT_TRUE(output.find("# FrankyCPP Configuration Template") != std::string::npos)
    << "YAML should have header comment";
  EXPECT_TRUE(output.find("# General Settings") != std::string::npos || output.find("General") != std::string::npos)
    << "YAML should have General section";

  // Check for commented settings (YAML template has settings commented out)
  EXPECT_TRUE(output.find("# TT_SIZE_MB") != std::string::npos || output.find("TT_SIZE_MB") != std::string::npos)
    << "YAML should contain TT_SIZE_MB setting";
}

TEST_F(CliIntegrationTest, ShowConfigJsonFormat) {
  auto [exitCode, output] = runEngine("--show-config --format json");

  EXPECT_EQ(0, exitCode) << "Exit code should be 0 for --show-config --format json";

  // Check for valid JSON structure
  EXPECT_TRUE(output.find("{") != std::string::npos)
    << "JSON should start with '{'";
  EXPECT_TRUE(output.find("\"configVersion\"") != std::string::npos)
    << "JSON should have configVersion field";
  EXPECT_TRUE(output.find("\"settings\"") != std::string::npos)
    << "JSON should have settings array";

  // Check for expected fields in settings
  EXPECT_TRUE(output.find("\"name\"") != std::string::npos)
    << "JSON settings should have 'name' field";
  EXPECT_TRUE(output.find("\"type\"") != std::string::npos)
    << "JSON settings should have 'type' field";
  EXPECT_TRUE(output.find("\"defaultValue\"") != std::string::npos)
    << "JSON settings should have 'defaultValue' field";
  EXPECT_TRUE(output.find("\"domain\"") != std::string::npos)
    << "JSON settings should have 'domain' field";
}

TEST_F(CliIntegrationTest, ShowConfigDomainFilter) {
  auto [exitCode, output] = runEngine("--show-config --domain search");

  EXPECT_EQ(0, exitCode) << "Exit code should be 0 for --show-config --domain search";

  // Check Search domain is present
  EXPECT_TRUE(output.find("Search") != std::string::npos)
    << "Output should contain Search domain";

  // Search-specific settings should be present
  EXPECT_TRUE(output.find("USE_TT") != std::string::npos || output.find("TT_SIZE_MB") != std::string::npos || output.find("USE_NMP") != std::string::npos)
    << "Output should contain search settings";
}

//=============================================================================
// UCI Options Tests
//=============================================================================

TEST_F(CliIntegrationTest, UciOptionsOutput) {
  auto [exitCode, output] = runEngine("--ucioptions");

  EXPECT_EQ(0, exitCode) << "Exit code should be 0 for --ucioptions";

  // Check for UCI protocol elements
  EXPECT_TRUE(output.find("id name FrankyCPP") != std::string::npos)
    << "UCI output should have engine name";
  EXPECT_TRUE(output.find("id author") != std::string::npos)
    << "UCI output should have author";
  EXPECT_TRUE(output.find("option name") != std::string::npos)
    << "UCI output should list options";

  // Check for some expected UCI options
  EXPECT_TRUE(output.find("Hash") != std::string::npos)
    << "UCI output should have Hash option";
}

//=============================================================================
// Perft Tests
//=============================================================================

TEST_F(CliIntegrationTest, PerftBasic) {
  // Run a quick perft test at depth 1-2 only (fast)
  auto [exitCode, output] = runEngine("--perft --startDepth 1 --endDepth 2");

  EXPECT_EQ(0, exitCode) << "Exit code should be 0 for --perft";

  // Check for perft output indicators
  EXPECT_TRUE(output.find("PERFT") != std::string::npos || output.find("perft") != std::string::npos || output.find("Perft") != std::string::npos)
    << "Output should indicate perft test";

  // Perft(1) from startpos should be 20 moves
  EXPECT_TRUE(output.find("20") != std::string::npos)
    << "Perft(1) should show 20 nodes";
}

TEST_F(CliIntegrationTest, PerftWithDepthOption) {
  // Test with explicit depth parameters
  auto [exitCode, output] = runEngine("--perft --startDepth 2 --endDepth 2");

  EXPECT_EQ(0, exitCode) << "Exit code should be 0 for --perft with depth options";

  // Perft(2) from startpos should be 400 moves
  EXPECT_TRUE(output.find("400") != std::string::npos)
    << "Perft(2) should show 400 nodes";
}

//=============================================================================
// Benchmark Tests
//=============================================================================

TEST_F(CliIntegrationTest, BenchBasic) {
  // Run a quick benchmark at low depth (fast, ~1-2 seconds)
  // Uses named parameters as defined in main.cpp
  auto [exitCode, output] = runEngine("--bench --benchDepth 4 --benchHash 16");

  EXPECT_EQ(0, exitCode) << "Exit code should be 0 for --bench\nOutput: " << output;

  // Check for benchmark output indicators
  EXPECT_TRUE(output.find("BENCHMARK") != std::string::npos || output.find("Benchmark") != std::string::npos || output.find("benchmark") != std::string::npos || output.find("NPS") != std::string::npos || output.find("nps") != std::string::npos)
    << "Output should indicate benchmark results";

  // Should show some node count or NPS value
  EXPECT_TRUE(output.find("nodes") != std::string::npos || output.find("Nodes") != std::string::npos || output.find("NPS") != std::string::npos)
    << "Benchmark should report nodes or NPS";
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(CliIntegrationTest, InvalidOption) {
  auto [exitCode, output] = runEngine("--invalid-option-that-does-not-exist");

  // Should return non-zero exit code for invalid option
  EXPECT_NE(0, exitCode) << "Exit code should be non-zero for invalid option";

  // Should show some error message
  EXPECT_TRUE(output.find("error") != std::string::npos || output.find("Error") != std::string::npos || output.find("unrecognised") != std::string::npos || output.find("unrecognized") != std::string::npos || output.find("unknown") != std::string::npos)
    << "Should show error message for invalid option.\nActual output: " << output;
}

//=============================================================================
// Short Option Tests (single letter variants)
//=============================================================================

TEST_F(CliIntegrationTest, HelpShortOption) {
  auto [exitCode, output] = runEngine("-?");

  EXPECT_EQ(0, exitCode) << "Exit code should be 0 for -?";
  EXPECT_TRUE(output.find("Allowed options") != std::string::npos)
    << "Help output should contain 'Allowed options'";
}

TEST_F(CliIntegrationTest, VersionShortOption) {
  auto [exitCode, output] = runEngine("-v");

  EXPECT_EQ(0, exitCode) << "Exit code should be 0 for -v";
  EXPECT_TRUE(output.find("FrankyCPP") != std::string::npos)
    << "Version output should contain 'FrankyCPP'";
}

TEST_F(CliIntegrationTest, UciOptionsShortOption) {
  auto [exitCode, output] = runEngine("-u");

  EXPECT_EQ(0, exitCode) << "Exit code should be 0 for -u";
  EXPECT_TRUE(output.find("option name") != std::string::npos)
    << "UCI output should list options";
}

//=============================================================================
// UCI Session Tests
//=============================================================================

TEST_F(CliIntegrationTest, UciSetAllOptionsToDefaults) {
  // Build UCI commands to set all options to their defaults
  // This simulates what Arena does when saving engine configuration

  const auto& registry     = config::ConfigRegistry::instance();
  const auto uciOptionDefs = registry.uciOptions();

  // Build command string: uci + isready + all setoptions + quit
  std::string commands = "uci\nisready\n";

  int optionCount = 0;
  for (const auto* def : uciOptionDefs) {
    const std::string& optionName = def->uciName;
    std::string value             = def->defaultValue;

    // Override SyzygyPath with a path containing backslash to test path handling
    if (optionName == "SyzygyPath") {
      value = "D:\\SYZYGY";
    }

    commands += "setoption name " + optionName + " value " + value + "\n";
    optionCount++;

    std::cout << "  [" << optionCount << "] setoption name " << optionName
              << " value " << value << std::endl;
  }
  commands += "quit\n";

  std::cout << "=== Sending " << optionCount << " setoption commands to engine ===" << std::endl;

  // Execute the engine with all commands
  auto [exitCode, output] = runEngineWithInput(commands);

  std::cout << "=== Engine output ===" << std::endl;
  std::cout << output << std::endl;

  // Verify basic UCI responses
  EXPECT_TRUE(output.find("uciok") != std::string::npos)
    << "Engine should respond with 'uciok'";
  EXPECT_TRUE(output.find("readyok") != std::string::npos)
    << "Engine should respond with 'readyok'";

  // Check for any error messages
  const bool hasError = output.find("error") != std::string::npos || output.find("Error") != std::string::npos || output.find("ERROR") != std::string::npos;
  EXPECT_FALSE(hasError) << "Engine should not report any errors when setting options to defaults";

  // Exit code 0 indicates clean quit
  EXPECT_EQ(0, exitCode) << "Engine should exit cleanly after quit command";

  std::cout << "=== Successfully sent " << optionCount << " setoption commands ===" << std::endl;
}
