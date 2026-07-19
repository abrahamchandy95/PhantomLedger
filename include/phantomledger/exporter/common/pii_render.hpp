#pragma once

#include "phantomledger/entities/parties/pii.hpp"
#include "phantomledger/synth/pii/pools.hpp"

#include <string>

namespace PhantomLedger::exporter::common::pii_render {

namespace pii = ::PhantomLedger::entity::pii;
namespace synthpii = ::PhantomLedger::synth::pii;

[[nodiscard]] inline std::string name(const synthpii::LocalePool &pool,
                                      pii::Name n) {
  std::string out;
  if (n.firstIdx < pool.firstNames.size()) {
    out += pool.firstNames[n.firstIdx];
  }
  if (n.middleIdx != pii::kNoMiddleIdx &&
      n.middleIdx < pool.middleNames.size()) {
    if (!out.empty()) {
      out.push_back(' ');
    }
    out += pool.middleNames[n.middleIdx];
  }
  if (n.lastIdx < pool.lastNames.size()) {
    if (!out.empty()) {
      out.push_back(' ');
    }
    out += pool.lastNames[n.lastIdx];
  }
  return out;
}

[[nodiscard]] inline std::string birthdate(pii::Dob dob) {
  if (dob.year == 0) {
    return {};
  }
  const auto two = [](unsigned v) {
    std::string s;
    s.push_back(static_cast<char>('0' + (v / 10U) % 10U));
    s.push_back(static_cast<char>('0' + v % 10U));
    return s;
  };
  std::string out = std::to_string(dob.year);
  out.push_back('-');
  out += two(dob.month);
  out.push_back('-');
  out += two(dob.day);
  return out;
}

[[nodiscard]] inline std::string street(const synthpii::LocalePool &pool,
                                        pii::Address addr) {
  if (addr.streetIdx < pool.streets.size()) {
    return pool.streets[addr.streetIdx];
  }
  return {};
}

[[nodiscard]] inline std::string city(const synthpii::LocalePool &pool,
                                      pii::Address addr) {
  if (addr.zipTableIdx < pool.zipTable.size()) {
    return pool.zipTable[addr.zipTableIdx].city;
  }
  return {};
}

[[nodiscard]] inline std::string state(const synthpii::LocalePool &pool,
                                       pii::Address addr) {
  if (addr.zipTableIdx < pool.zipTable.size()) {
    return pool.zipTable[addr.zipTableIdx].adminCode;
  }
  return {};
}

[[nodiscard]] inline std::string postcode(const synthpii::LocalePool &pool,
                                          pii::Address addr) {
  if (addr.zipTableIdx < pool.zipTable.size()) {
    return pool.zipTable[addr.zipTableIdx].postalCode;
  }
  return {};
}

[[nodiscard]] inline std::string name(const synthpii::PoolSet &pools,
                                      const pii::Record &rec) {
  return name(pools.forCountry(rec.country), rec.name);
}
[[nodiscard]] inline std::string birthdate(const pii::Record &rec) {
  return birthdate(rec.dob);
}
[[nodiscard]] inline std::string street(const synthpii::PoolSet &pools,
                                        const pii::Record &rec) {
  return street(pools.forCountry(rec.country), rec.address);
}
[[nodiscard]] inline std::string city(const synthpii::PoolSet &pools,
                                      const pii::Record &rec) {
  return city(pools.forCountry(rec.country), rec.address);
}
[[nodiscard]] inline std::string state(const synthpii::PoolSet &pools,
                                       const pii::Record &rec) {
  return state(pools.forCountry(rec.country), rec.address);
}
[[nodiscard]] inline std::string postcode(const synthpii::PoolSet &pools,
                                          const pii::Record &rec) {
  return postcode(pools.forCountry(rec.country), rec.address);
}

} // namespace PhantomLedger::exporter::common::pii_render
