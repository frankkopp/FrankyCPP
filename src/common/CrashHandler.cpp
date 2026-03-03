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

#include "CrashHandler.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "DbgHelp.lib")
#else
#include <csignal>
#include <cstdlib>
#include <execinfo.h>
#include <unistd.h>
#endif

namespace crashhandler {

namespace {
  std::string g_dumpPath = ".";
  bool g_installed       = false;

#ifdef _WIN32
  LPTOP_LEVEL_EXCEPTION_FILTER g_previousFilter = nullptr;
#else
  struct sigaction g_previousSigsegv;
  struct sigaction g_previousSigabrt;
  struct sigaction g_previousSigfpe;
#endif

  std::string generateDumpFilename(const std::string& reason) {
    const auto now       = std::chrono::system_clock::now();
    const auto time_t_now = std::chrono::system_clock::to_time_t(now);
    const auto ms        = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) % 1000;

    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif

    std::ostringstream oss;
    oss << g_dumpPath << "/FrankyCPP_crash_"
        << std::put_time(&tm_buf, "%Y%m%d_%H%M%S")
        << "_" << std::setfill('0') << std::setw(3) << ms.count()
        << "_" << reason
#ifdef _WIN32
        << ".dmp";
#else
        << ".txt";
#endif
    return oss.str();
  }

#ifdef _WIN32
  // Windows: Generate minidump using DbgHelp
  void writeMiniDump(EXCEPTION_POINTERS* exceptionInfo, const std::string& filename) {
    const HANDLE hFile = CreateFileA(
      filename.c_str(),
      GENERIC_WRITE,
      0,
      nullptr,
      CREATE_ALWAYS,
      FILE_ATTRIBUTE_NORMAL,
      nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
      std::cerr << "CrashHandler: Failed to create dump file: " << filename << std::endl;
      return;
    }

    MINIDUMP_EXCEPTION_INFORMATION mdei;
    mdei.ThreadId          = GetCurrentThreadId();
    mdei.ExceptionPointers = exceptionInfo;
    mdei.ClientPointers    = FALSE;

    // Include thread and module info, plus full memory if available
    const MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(
      MiniDumpWithDataSegs |
      MiniDumpWithHandleData |
      MiniDumpWithThreadInfo |
      MiniDumpWithUnloadedModules |
      MiniDumpWithFullMemoryInfo);

    const BOOL success = MiniDumpWriteDump(
      GetCurrentProcess(),
      GetCurrentProcessId(),
      hFile,
      dumpType,
      exceptionInfo ? &mdei : nullptr,
      nullptr,
      nullptr);

    CloseHandle(hFile);

    if (success) {
      std::cerr << "CrashHandler: Minidump written to: " << filename << std::endl;
    }
    else {
      std::cerr << "CrashHandler: Failed to write minidump. Error: " << GetLastError() << std::endl;
    }
  }

  const char* exceptionCodeToString(DWORD code) {
    switch (code) {
      case EXCEPTION_ACCESS_VIOLATION: return "ACCESS_VIOLATION";
      case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
      case EXCEPTION_BREAKPOINT: return "BREAKPOINT";
      case EXCEPTION_DATATYPE_MISALIGNMENT: return "DATATYPE_MISALIGNMENT";
      case EXCEPTION_FLT_DENORMAL_OPERAND: return "FLT_DENORMAL_OPERAND";
      case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "FLT_DIVIDE_BY_ZERO";
      case EXCEPTION_FLT_INEXACT_RESULT: return "FLT_INEXACT_RESULT";
      case EXCEPTION_FLT_INVALID_OPERATION: return "FLT_INVALID_OPERATION";
      case EXCEPTION_FLT_OVERFLOW: return "FLT_OVERFLOW";
      case EXCEPTION_FLT_STACK_CHECK: return "FLT_STACK_CHECK";
      case EXCEPTION_FLT_UNDERFLOW: return "FLT_UNDERFLOW";
      case EXCEPTION_ILLEGAL_INSTRUCTION: return "ILLEGAL_INSTRUCTION";
      case EXCEPTION_IN_PAGE_ERROR: return "IN_PAGE_ERROR";
      case EXCEPTION_INT_DIVIDE_BY_ZERO: return "INT_DIVIDE_BY_ZERO";
      case EXCEPTION_INT_OVERFLOW: return "INT_OVERFLOW";
      case EXCEPTION_INVALID_DISPOSITION: return "INVALID_DISPOSITION";
      case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "NONCONTINUABLE_EXCEPTION";
      case EXCEPTION_PRIV_INSTRUCTION: return "PRIV_INSTRUCTION";
      case EXCEPTION_SINGLE_STEP: return "SINGLE_STEP";
      case EXCEPTION_STACK_OVERFLOW: return "STACK_OVERFLOW";
      default: return "UNKNOWN";
    }
  }

  LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo) {
    const DWORD code = exceptionInfo->ExceptionRecord->ExceptionCode;

    std::cerr << "\n========================================" << std::endl;
    std::cerr << "CrashHandler: UNHANDLED EXCEPTION CAUGHT!" << std::endl;
    std::cerr << "Exception Code: 0x" << std::hex << code << std::dec
              << " (" << exceptionCodeToString(code) << ")" << std::endl;
    std::cerr << "Exception Address: 0x" << std::hex
              << reinterpret_cast<uintptr_t>(exceptionInfo->ExceptionRecord->ExceptionAddress)
              << std::dec << std::endl;

    if (code == EXCEPTION_ACCESS_VIOLATION && exceptionInfo->ExceptionRecord->NumberParameters >= 2) {
      const auto accessType = exceptionInfo->ExceptionRecord->ExceptionInformation[0];
      const auto address    = exceptionInfo->ExceptionRecord->ExceptionInformation[1];
      std::cerr << "Access Type: " << (accessType == 0 ? "READ" : (accessType == 1 ? "WRITE" : "DEP"))
                << " at address 0x" << std::hex << address << std::dec << std::endl;
    }
    std::cerr << "========================================\n" << std::endl;

    const std::string filename = generateDumpFilename(exceptionCodeToString(code));
    writeMiniDump(exceptionInfo, filename);

    // Call previous handler if there was one
    if (g_previousFilter) {
      return g_previousFilter(exceptionInfo);
    }

    return EXCEPTION_CONTINUE_SEARCH;
  }

#else
  // Linux: Write stack trace to file
  void writeStackTrace(int sig, const std::string& filename) {
    FILE* f = fopen(filename.c_str(), "w");
    if (!f) {
      std::cerr << "CrashHandler: Failed to create trace file: " << filename << std::endl;
      return;
    }

    fprintf(f, "Signal: %d (%s)\n", sig,
            sig == SIGSEGV ? "SIGSEGV" : sig == SIGABRT ? "SIGABRT"
                                       : sig == SIGFPE  ? "SIGFPE"
                                                        : "UNKNOWN");
    fprintf(f, "\nStack trace:\n");

    void* buffer[128];
    const int nptrs = backtrace(buffer, 128);
    char** symbols  = backtrace_symbols(buffer, nptrs);

    if (symbols) {
      for (int i = 0; i < nptrs; ++i) {
        fprintf(f, "  [%d] %s\n", i, symbols[i]);
      }
      free(symbols);
    }

    fclose(f);
    std::cerr << "CrashHandler: Stack trace written to: " << filename << std::endl;
  }

  void signalHandler(int sig) {
    std::cerr << "\n========================================" << std::endl;
    std::cerr << "CrashHandler: SIGNAL CAUGHT: " << sig << std::endl;
    std::cerr << "========================================\n" << std::endl;

    const char* sigName = sig == SIGSEGV ? "SIGSEGV" : sig == SIGABRT ? "SIGABRT"
                                                     : sig == SIGFPE  ? "SIGFPE"
                                                                      : "UNKNOWN";
    const std::string filename = generateDumpFilename(sigName);
    writeStackTrace(sig, filename);

    // Re-raise the signal with default handler to get core dump if enabled
    signal(sig, SIG_DFL);
    raise(sig);
  }
#endif

}// anonymous namespace

void install(const std::string& dumpPath) {
  if (g_installed) {
    return;
  }

  g_dumpPath = dumpPath;

  // Create dump directory if it doesn't exist
  try {
    std::filesystem::create_directories(dumpPath);
  }
  catch (...) {
    // Ignore - we'll fail later when trying to write
  }

#ifdef _WIN32
  g_previousFilter = SetUnhandledExceptionFilter(unhandledExceptionFilter);
  std::cerr << "CrashHandler: Installed Windows exception handler. Dumps will be written to: "
            << g_dumpPath << std::endl;
#else
  struct sigaction sa {};
  sa.sa_handler = signalHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  sigaction(SIGSEGV, &sa, &g_previousSigsegv);
  sigaction(SIGABRT, &sa, &g_previousSigabrt);
  sigaction(SIGFPE, &sa, &g_previousSigfpe);
  std::cerr << "CrashHandler: Installed signal handlers. Traces will be written to: "
            << g_dumpPath << std::endl;
#endif

  g_installed = true;
}

void uninstall() {
  if (!g_installed) {
    return;
  }

#ifdef _WIN32
  SetUnhandledExceptionFilter(g_previousFilter);
  g_previousFilter = nullptr;
#else
  sigaction(SIGSEGV, &g_previousSigsegv, nullptr);
  sigaction(SIGABRT, &g_previousSigabrt, nullptr);
  sigaction(SIGFPE, &g_previousSigfpe, nullptr);
#endif

  g_installed = false;
  std::cerr << "CrashHandler: Uninstalled." << std::endl;
}

bool isInstalled() {
  return g_installed;
}

void triggerDump(const std::string& reason) {
#ifdef _WIN32
  const std::string filename = generateDumpFilename(reason);
  writeMiniDump(nullptr, filename);
#else
  const std::string filename = generateDumpFilename(reason);
  writeStackTrace(0, filename);
#endif
}

}// namespace crashhandler
