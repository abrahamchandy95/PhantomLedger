#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace PhantomLedger::entities::population {

struct FraudRing {
  std::int32_t id = 0;
  std::vector<std::string> members;
  std::vector<std::string> frauds;
  std::vector<std::string> mules;
  std::vector<std::string> victims;
};

struct Population {
  std::vector<std::string> ids;
  std::unordered_set<std::string> frauds;
  std::unordered_set<std::string> mules;
  std::unordered_set<std::string> victims;
  std::unordered_set<std::string> soloFrauds;
  std::vector<FraudRing> rings;
};

} // namespace PhantomLedger::entities::population
