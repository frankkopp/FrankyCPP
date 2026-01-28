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


#include <fstream>
#include <sstream>

#include <yaml-cpp/yaml.h>

#include "ConfigManager.h"
#include "ConfigPaths.h"
#include "common/Logging.h"
#include "types/globals.h"

namespace engine::config {

  ConfigManager& ConfigManager::instance() {
    static ConfigManager inst;
    return inst;
  }

  ConfigManager::ConfigManager() {
    // Initialize current values from a hard-coded fallback
    currentSearch_ = fallbackSearch_;
    currentEval_   = fallbackEval_;

    // Autoload from default YAML paths on the first instantiation.
    autoLoadAttempted_ = true;
    lastLoadOk_        = loadFromFiles();

    // Capture the initially loaded configuration as "defaults" for future resets.
    // If YAML loading failed or was missing, this will equal the fallback values.
    defaultSearch_ = currentSearch_;
    defaultEval_   = currentEval_;


    currentSearch_.CONFIG_SOURCE    = "current";
    currentEval_.EVAL_CONFIG_SOURCE = "current";
  }

  void ConfigManager::resetToDefaults() {
    // Restore to the initially loaded defaults (from YAML at startup, or fallback if unavailable)
    currentSearch_ = defaultSearch_;
    currentEval_   = defaultEval_;
    LOG__INFO(Logger::get().CONFIG_LOG, "Config reset to defaults (initial YAML)");
  }

  static bool file_exists(const std::filesystem::path& p) {
    std::error_code ec;
    return std::filesystem::exists(p, ec) && !ec;
  }

  bool ConfigManager::loadFromFiles(std::optional<std::filesystem::path> searchPath,
                                    std::optional<std::filesystem::path> evalPath) {
    const auto sPath = searchPath.value_or(ConfigPaths::SearchYaml());
    const auto ePath = evalPath.value_or(ConfigPaths::EvalYaml());

    // keep backups for rollback on error
    const auto backupSearch = currentSearch_;
    const auto backupEval   = currentEval_;

    try {
      // Start from hard-coded fallback for both; apply file overrides if present
      SearchConfigData newSearch = fallbackSearch_;
      EvalConfigData newEval     = fallbackEval_;

      // Load search.yaml (missing is OK -> keep fallback)
      if (file_exists(sPath)) {
        LOG__INFO(Logger::get().CONFIG_LOG, "Loading Search config from {}", sPath.string());
        YAML::Node n = YAML::LoadFile(sPath.string());
        if (!n.IsMap()) {
          LOG__WARN(Logger::get().CONFIG_LOG, "Search config at {} is not a map; using fallback", sPath.string());
        }
        else {
          newSearch = n.as<SearchConfigData>();
        }
      }
      else {
        LOG__INFO(Logger::get().CONFIG_LOG, "Search config file not found: {} (using fallback)", sPath.string());
      }

      // Load eval.yaml (missing is OK -> keep fallback)
      if (file_exists(ePath)) {
        LOG__INFO(Logger::get().EVAL_LOG, "Loading Eval config from {}", ePath.string());
        YAML::Node n = YAML::LoadFile(ePath.string());
        if (!n.IsMap()) {
          LOG__WARN(Logger::get().EVAL_LOG, "Eval config at {} is not a map; using fallback", ePath.string());
        }
        else {
          newEval = n.as<EvalConfigData>();
        }
      }
      else {
        LOG__INFO(Logger::get().EVAL_LOG, "Eval config file not found: {} (using fallback)", ePath.string());
      }

      // Commit
      currentSearch_                 = std::move(newSearch);
      currentEval_                   = std::move(newEval);

      lastLoadOk_ = true;
      return true;

    } catch (const YAML::Exception& ex) {
      LOG__ERROR(Logger::get().CONFIG_LOG, "YAML error while loading configs: {}", ex.what());
      // rollback
      currentSearch_ = backupSearch;
      currentEval_   = backupEval;
      lastLoadOk_    = false;
      return false;
    } catch (const std::exception& ex) {
      LOG__ERROR(Logger::get().CONFIG_LOG, "Exception while loading configs: {}", ex.what());
      currentSearch_ = backupSearch;
      currentEval_   = backupEval;
      lastLoadOk_    = false;
      return false;
    }
  }

  std::string ConfigManager::strCurrent() const {
    std::ostringstream os;
    os << "[Search]\n"
       << currentSearch_.str();
    os << "[Eval]\n"
       << currentEval_.str();
    return os.str();
  }

  std::string ConfigManager::strDefaults() const {
    std::ostringstream os;
    os << "[Search]\n"
       << defaultSearch_.str();
    os << "[Eval]\n"
       << defaultEval_.str();
    return os.str();
  }

}// namespace engine::config
