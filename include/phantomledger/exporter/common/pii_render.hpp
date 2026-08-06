#pragma once
/*
  Shared PII Rendering For Exporters
  Turns the indices held on a person's `pii::Record` back into the strings
  exporters write — name, birthdate, street, and the home (city, state,
  postcode) tuple. Rendering only: it draws nothing and invents nothing, so
  every exporter that calls it reports the same PII for the same person.
 */

#include "phantomledger/entities/parties/pii.hpp"
#include "phantomledger/synth/geo/catalog.hpp"
#include "phantomledger/synth/pii/pools.hpp"

#include <string>
#include <string_view>

namespace PhantomLedger::exporter::common::pii_render {

namespace pii = ::PhantomLedger::entity::pii;
namespace synthpii = ::PhantomLedger::synth::pii;
namespace geo = ::PhantomLedger::synth::geo;

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

/* A home's (city, state, postal) as ONE coherent tuple from the pinned
 * catalogue — the single point where every exporter resolves home geography,
 * so the person, their household, and the downstream vertices all report the
 * same rows. The catalogue is the authority; the legacy per-country zip pool
 * is consulted ONLY when the household has no modeled area (a country with no
 * residential row in the catalogue — unreachable for the shipped locale mix,
 * which the catalogue fully covers). Both branches return views into
 * process-lifetime storage (the immutable static catalogue or the run's locale
 * pools), so callers may hold them for the export. */
struct HomeGeo {
  std::string_view city;
  std::string_view state;
  std::string_view postcode;
};

[[nodiscard]] inline HomeGeo homeGeo(const synthpii::LocalePool &pool,
                                     pii::Address addr) {
  const auto &catalogue = geo::geography();
  if (catalogue.contains(addr.geoArea)) {
    const auto &a = catalogue.at(addr.geoArea);
    return {a.city, a.stateCode, a.postalAreaCode};
  }
  if (addr.zipTableIdx < pool.zipTable.size()) {
    const auto &zip = pool.zipTable[addr.zipTableIdx];
    return {zip.city, zip.adminCode, zip.postalCode};
  }
  return {};
}

[[nodiscard]] inline std::string city(const synthpii::LocalePool &pool,
                                      pii::Address addr) {
  return std::string{homeGeo(pool, addr).city};
}

[[nodiscard]] inline std::string state(const synthpii::LocalePool &pool,
                                       pii::Address addr) {
  return std::string{homeGeo(pool, addr).state};
}

[[nodiscard]] inline std::string postcode(const synthpii::LocalePool &pool,
                                          pii::Address addr) {
  return std::string{homeGeo(pool, addr).postcode};
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
