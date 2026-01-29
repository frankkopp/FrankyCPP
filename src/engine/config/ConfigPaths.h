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

#ifndef FRANKYCPP_CONFIGPATHS_H
#define FRANKYCPP_CONFIGPATHS_H

#include <filesystem>

namespace ConfigPaths {
    // Header-only helpers returning default YAML paths relative to the current working directory.
    // These are copied next to the executable in build/test via post-build commands in CMake.

    // Default path to the search configuration YAML (e.g., ./config/search.yaml)
    inline std::filesystem::path SearchYaml() {
        return std::filesystem::path("config") / "search.yaml";
    }

    // Default path to the evaluation configuration YAML (e.g., ./config/eval.yaml)
    inline std::filesystem::path EvalYaml() {
        return std::filesystem::path("config") / "eval.yaml";
    }
}

#endif // FRANKYCPP_CONFIGPATHS_H
