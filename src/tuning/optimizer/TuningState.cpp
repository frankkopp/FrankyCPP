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

#include "tuning/optimizer/TuningState.h"

#include "common/Logging.h"
#include "types/globals.h"

#include <chrono>
#include <format>
#include <fstream>
#include <stdexcept>
#include <unordered_map>
#include <yaml-cpp/yaml.h>

namespace tuning {

  // =========================================================================
  // captureFromParams / restoreToParams
  // =========================================================================

  void TuningState::captureFromParams(const std::vector<TuningParameter>& params) {
    paramValues.clear();
    paramValues.reserve(params.size());
    for (const auto& p : params) {
      paramValues.emplace_back(p.name, p.currentValue);
    }
    timestamp = currentTimestamp();
  }

  int TuningState::restoreToParams(std::vector<TuningParameter>& params) const {
    // Build a lookup map from state: name → value
    std::unordered_map<std::string, int> stateMap;
    stateMap.reserve(paramValues.size());
    for (const auto& [name, value] : paramValues) {
      stateMap[name] = value;
    }

    int restored = 0;
    int missing  = 0;
    for (auto& p : params) {
      const auto it = stateMap.find(p.name);
      if (it != stateMap.end()) {
        p.currentValue = it->second;
        ++restored;
      }
      else {
        LOG__WARN(common::Logger::get().TUNING_LOG,
                  "TuningState::restoreToParams: parameter '{}' not found in checkpoint — keeping current value {}",
                  p.name, p.currentValue);
        ++missing;
      }
    }

    if (missing > 0) {
      LOG__WARN(common::Logger::get().TUNING_LOG,
                "TuningState::restoreToParams: {} params restored, {} not found in checkpoint",
                restored, missing);
    }

    // Check for params in checkpoint that don't exist in the current vector
    const int extra = static_cast<int>(paramValues.size()) - restored;
    if (extra > 0) {
      LOG__WARN(common::Logger::get().TUNING_LOG,
                "TuningState::restoreToParams: {} params in checkpoint not present in current parameter vector",
                extra);
    }

    return restored;
  }

  // =========================================================================
  // YAML serialization
  // =========================================================================

  void TuningState::saveToYaml(const std::string& path) const {
    YAML::Emitter out;
    out << YAML::BeginMap;

    // Header
    out << YAML::Key << "format" << YAML::Value << "FrankyCPP_TuningCheckpoint_v1";
    out << YAML::Key << "timestamp" << YAML::Value << timestamp;

    // Tuning metadata
    out << YAML::Key << "completed_passes" << YAML::Value << completedPasses;
    out << YAML::Key << "K" << YAML::Value << YAML::DoublePrecision(12) << K;
    out << YAML::Key << "best_train_mse" << YAML::Value << YAML::DoublePrecision(12) << bestTrainMSE;
    out << YAML::Key << "best_test_mse" << YAML::Value << YAML::DoublePrecision(12) << bestTestMSE;
    out << YAML::Key << "dataset_path" << YAML::Value << datasetPath;

    // Parameter values
    out << YAML::Key << "parameters" << YAML::Value;
    out << YAML::BeginMap;
    for (const auto& [name, value] : paramValues) {
      out << YAML::Key << name << YAML::Value << value;
    }
    out << YAML::EndMap;

    out << YAML::EndMap;

    // Write to file
    std::ofstream file(path);
    if (!file.is_open()) {
      throw std::runtime_error(std::format("TuningState::saveToYaml: cannot open file for writing: {}", path));
    }
    file << out.c_str() << "\n";
    file.close();

    LOG__INFO(common::Logger::get().TUNING_LOG,
              "Checkpoint saved: {} ({} params, pass {}, train MSE {:.10f})",
              path, paramValues.size(), completedPasses, bestTrainMSE);
  }

  TuningState TuningState::loadFromYaml(const std::string& path) {
    YAML::Node root;
    try {
      root = YAML::LoadFile(path);
    }
    catch (const YAML::Exception& e) {
      throw std::runtime_error(
        std::format("TuningState::loadFromYaml: failed to parse '{}': {}", path, e.what()));
    }

    // Validate format marker
    const auto format = root["format"].as<std::string>("");
    if (format != "FrankyCPP_TuningCheckpoint_v1") {
      throw std::runtime_error(
        std::format("TuningState::loadFromYaml: unknown format '{}' in '{}'", format, path));
    }

    TuningState state;
    state.completedPasses = root["completed_passes"].as<int>(0);
    state.K               = root["K"].as<double>(1.0);
    state.bestTrainMSE    = root["best_train_mse"].as<double>(0.0);
    state.bestTestMSE     = root["best_test_mse"].as<double>(0.0);
    state.datasetPath     = root["dataset_path"].as<std::string>("");
    state.timestamp       = root["timestamp"].as<std::string>("");

    // Load parameter values
    const auto paramsNode = root["parameters"];
    if (paramsNode && paramsNode.IsMap()) {
      for (const auto& it : paramsNode) {
        const auto name  = it.first.as<std::string>();
        const auto value = it.second.as<int>();
        state.paramValues.emplace_back(name, value);
      }
    }

    LOG__INFO(common::Logger::get().TUNING_LOG,
              "Checkpoint loaded: {} ({} params, pass {}, train MSE {:.10f})",
              path, state.paramValues.size(), state.completedPasses, state.bestTrainMSE);

    return state;
  }

  // =========================================================================
  // Timestamp utility
  // =========================================================================

  std::string TuningState::currentTimestamp() {
    const auto now    = std::chrono::system_clock::now();
    const auto timeT  = std::chrono::system_clock::to_time_t(now);
    std::tm localTm{};
#ifdef _WIN32
    localtime_s(&localTm, &timeT);
#else
    localtime_r(&timeT, &localTm);
#endif
    return std::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}",
                       localTm.tm_year + 1900, localTm.tm_mon + 1, localTm.tm_mday,
                       localTm.tm_hour, localTm.tm_min, localTm.tm_sec);
  }

} // namespace tuning
