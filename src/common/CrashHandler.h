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

#ifndef FRANKYCPP_CRASHHANDLER_H
#define FRANKYCPP_CRASHHANDLER_H

#include <string>

namespace common::crashhandler {

  /// Installs a crash handler that generates minidumps on unhandled exceptions.
  /// On Windows: Uses SetUnhandledExceptionFilter to catch access violations, etc.
  /// On Linux: Uses signal handlers for SIGSEGV, SIGABRT, etc.
  /// @param dumpPath Directory where minidumps will be written (default: current directory)
  void install(const std::string& dumpPath = ".");

  /// Uninstalls the crash handler, restoring default behavior.
  void uninstall();

  /// Returns true if the crash handler is currently installed.
  bool isInstalled();

  /// Manually trigger a minidump (for testing or diagnostic purposes).
  /// @param reason A description of why the dump was triggered
  void triggerDump(const std::string& reason = "manual");

}// namespace common::crashhandler

#endif// FRANKYCPP_CRASHHANDLER_H
