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

#ifndef FRANKYCPP_STACKTRACE_H
#define FRANKYCPP_STACKTRACE_H

//=============================================================================
// stacktrace.h - Platform-specific Stack Trace Utility
//=============================================================================
//
// Provides a simple function to print the current call stack for debugging.
// Useful for tracking down assertion failures, crashes, and unexpected states.
//
// Platform Support:
//   - Windows: Uses DbgHelp API (CaptureStackBackTrace + SymFromAddr)
//   - Linux/Unix: Uses execinfo.h (backtrace + backtrace_symbols)
//   - Other: Prints "not available" message
//
// Usage:
//   #include "common/stacktrace.h"
//
//   if (errorCondition) {
//     std::cerr << "Error occurred!\n";
//     printStackTrace();
//   }
//
// Note: For best results on Windows, ensure debug symbols are available.
//       On Linux, compile with -rdynamic for symbol names.
//
//=============================================================================

#include <iostream>

#ifdef _WIN32
  #include <windows.h>
  #include <dbghelp.h>
  #include <mutex>
  #include <vector>
  #pragma comment(lib, "dbghelp.lib")

namespace debug {

// Mutex to protect DbgHelp calls (they are not thread-safe)
inline std::mutex& getDbgHelpMutex() {
  static std::mutex mtx;
  return mtx;
}

/// Prints the current call stack to stderr (Windows implementation)
/// @param maxFrames Maximum number of stack frames to capture (default: 64)
/// @param skipFrames Number of frames to skip from top of stack (default: 0)
inline void printStackTrace(int maxFrames = 64, int skipFrames = 0) {
  std::vector<void*> stack(maxFrames);

  // Capture stack trace (this part is thread-safe)
  const WORD frames = CaptureStackBackTrace(skipFrames, maxFrames, stack.data(), nullptr);

  // Lock for DbgHelp API calls (not thread-safe)
  std::lock_guard lock(getDbgHelpMutex());

  HANDLE process = GetCurrentProcess();
  SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
  SymInitialize(process, nullptr, TRUE);

  // Allocate buffer for symbol info (name can be up to 256 chars)
  constexpr size_t symbolBufferSize = sizeof(SYMBOL_INFO) + 256 * sizeof(char);
  std::vector<char> symbolBuffer(symbolBufferSize);
  auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer.data());
  symbol->MaxNameLen = 255;
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

  // For line info
  IMAGEHLP_LINE64 line;
  line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
  DWORD displacement = 0;

  std::cerr << "\n=== STACK TRACE (" << frames << " frames) ===\n";
  for (int i = 0; i < frames; i++) {
    const auto address = reinterpret_cast<DWORD64>(stack[i]);

    if (SymFromAddr(process, address, nullptr, symbol)) {
      std::cerr << i << ": " << symbol->Name;

      // Try to get file and line info
      if (SymGetLineFromAddr64(process, address, &displacement, &line)) {
        std::cerr << " (" << line.FileName << ":" << line.LineNumber << ")";
      }

      std::cerr << "\n";
    } else {
      std::cerr << i << ": <unknown> (0x" << std::hex << address << std::dec << ")\n";
    }
  }
  std::cerr << "==========================================\n" << std::flush;
}

} // namespace debug

#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__)
  #include <execinfo.h>
  #include <cstdlib>

namespace debug {

/// Prints the current call stack to stderr (Unix/Linux implementation)
/// @param maxFrames Maximum number of stack frames to capture (default: 64)
/// @param skipFrames Number of frames to skip from top of stack (default: 0)
inline void printStackTrace(int maxFrames = 64, int skipFrames = 0) {
  std::vector<void*> stack(maxFrames);

  const int frames = backtrace(stack.data(), maxFrames);
  char** symbols = backtrace_symbols(stack.data(), frames);

  std::cerr << "\n=== STACK TRACE (" << frames - skipFrames << " frames) ===\n";
  for (int i = skipFrames; i < frames; i++) {
    std::cerr << (i - skipFrames) << ": " << (symbols ? symbols[i] : "<unknown>") << "\n";
  }
  std::cerr << "==========================================\n" << std::flush;

  free(symbols);
}

} // namespace debug

#else

namespace debug {

/// Prints a "not available" message for unsupported platforms
inline void printStackTrace(int /*maxFrames*/ = 64, int /*skipFrames*/ = 0) {
  std::cerr << "\n=== STACK TRACE ===\n";
  std::cerr << "Stack trace not available on this platform\n";
  std::cerr << "===================\n" << std::flush;
}

} // namespace debug

#endif

// Convenience macro for quick debugging
#define PRINT_STACK_TRACE() debug::printStackTrace(64, 1)

#endif // FRANKYCPP_STACKTRACE_H
