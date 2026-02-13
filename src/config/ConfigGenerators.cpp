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

#include "config/ConfigGenerators.h"
#include "config/ConfigRegistry.h"
#include "config/EvalConfigData.h"
#include "config/SearchConfigData.h"

#include <algorithm>
#include <sstream>
#include <vector>

std::string generateConfigString(
    const SearchConfigData& search,
    const EvalConfigData& eval,
    const bool showAll,
    const std::optional<ConfigDomain> domainFilter) {

  const auto& registry = ConfigRegistry::instance();
  std::ostringstream oss;

  // Collect entries to display
  std::vector<const ConfigDef*> entries;
  if (showAll) {
    for (const auto& def : registry.all()) {
      if (!domainFilter.has_value() || def.domain == domainFilter.value()) {
        entries.push_back(&def);
      }
    }
  }
  else {
    for (const auto* def : registry.displayOptions()) {
      if (!domainFilter.has_value() || def->domain == domainFilter.value()) {
        entries.push_back(def);
      }
    }
  }

  // Group by domain for organized output
  // Order: General, Search, Eval, Tuning, Debug
  const std::vector domainOrder = {
      ConfigDomain::General,
      ConfigDomain::Search,
      ConfigDomain::Eval,
      ConfigDomain::Tuning,
      ConfigDomain::Debug};

  for (const auto domain : domainOrder) {
    // Skip if filtering to a different domain
    if (domainFilter.has_value() && domain != domainFilter.value()) {
      continue;
    }

    // Collect entries for this domain
    std::vector<const ConfigDef*> domainEntries;
    for (const auto* def : entries) {
      if (def->domain == domain) {
        domainEntries.push_back(def);
      }
    }

    // Skip empty domains
    if (domainEntries.empty()) {
      continue;
    }

    // Domain header
    oss << "=== " << domainToString(domain) << " ===\n";

    // Output each entry
    for (const auto* def : domainEntries) {
      const std::string value = def->getter(search, eval);
      oss << def->name << ": " << value << "\n";
    }

    oss << "\n";
  }

  return oss.str();
}

std::string generateConfigStringForDomain(
    const SearchConfigData& search,
    const EvalConfigData& eval,
    const ConfigDomain domain) {
  return generateConfigString(search, eval, false, domain);
}

//=============================================================================
// SearchConfigData::str() implementation
//=============================================================================

std::string SearchConfigData::str() const {
  // Use a default EvalConfigData - we only output General and Search domains
  const EvalConfigData defaultEval;
  std::ostringstream oss;

  // Output General domain
  oss << generateConfigStringForDomain(*this, defaultEval, ConfigDomain::General);

  // Output Search domain
  oss << generateConfigStringForDomain(*this, defaultEval, ConfigDomain::Search);

  return oss.str();
}

//=============================================================================
// EvalConfigData::str() implementation
//=============================================================================

std::string EvalConfigData::str() const {
  // Use a default SearchConfigData - we only output Eval domain
  const SearchConfigData defaultSearch;
  return generateConfigStringForDomain(defaultSearch, *this, ConfigDomain::Eval);
}
