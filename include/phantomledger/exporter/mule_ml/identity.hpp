#pragma once

#include "phantomledger/entities/parties/pii.hpp"
#include "phantomledger/exporter/common/pii_render.hpp"
#include "phantomledger/synth/pii/pools.hpp"
#include "phantomledger/taxonomies/locale/names.hpp"

#include <string>

namespace PhantomLedger::exporter::mule_ml {

struct PartyIdentity {
  std::string name;
  std::string ssn;
  std::string dob;
  std::string address;
  std::string state;
  std::string city;
  std::string zipcode;
  std::string country;
};

[[nodiscard]] inline PartyIdentity blankIdentity() noexcept { return {}; }

namespace detail {

[[nodiscard]] inline std::string format4(int v) {
  std::string out(4, '0');
  for (int i = 3; i >= 0 && v > 0; --i) {
    out[static_cast<std::size_t>(i)] = static_cast<char>('0' + (v % 10));
    v /= 10;
  }
  return out;
}

[[nodiscard]] inline std::string format2(unsigned v) {
  return std::string{static_cast<char>('0' + ((v / 10U) % 10U)),
                     static_cast<char>('0' + (v % 10U))};
}

[[nodiscard]] inline std::string
formatDob(::PhantomLedger::entity::pii::Dob dob) {
  if (dob.year == 0) {
    return {};
  }
  std::string out;
  out.reserve(10);
  out.append(format4(dob.year));
  out.push_back('-');
  out.append(format2(dob.month));
  out.push_back('-');
  out.append(format2(dob.day));
  return out;
}

[[nodiscard]] inline std::string
formatName(const ::PhantomLedger::synth::pii::LocalePool &pool,
           ::PhantomLedger::entity::pii::Name name) {
  std::string out;
  if (name.firstIdx < pool.firstNames.size()) {
    out += pool.firstNames[name.firstIdx];
  }
  if (name.middleIdx != ::PhantomLedger::entity::pii::kNoMiddleIdx &&
      name.middleIdx < pool.middleNames.size()) {
    if (!out.empty()) {
      out.push_back(' ');
    }
    out += pool.middleNames[name.middleIdx];
  }
  if (name.lastIdx < pool.lastNames.size()) {
    if (!out.empty()) {
      out.push_back(' ');
    }
    out += pool.lastNames[name.lastIdx];
  }
  return out;
}

} // namespace detail

[[nodiscard]] inline PartyIdentity
renderIdentity(const ::PhantomLedger::entity::pii::Record &record,
               const ::PhantomLedger::synth::pii::PoolSet &pools) {
  PartyIdentity out;
  const auto &pool = pools.forCountry(record.country);

  out.name = detail::formatName(pool, record.name);
  out.ssn = std::string{record.ssn.view()};
  out.dob = detail::formatDob(record.dob);
  out.country = std::string{::PhantomLedger::locale::code(record.country)};

  if (record.address.streetIdx < pool.streets.size()) {
    out.address = pool.streets[record.address.streetIdx];
  }

  // geo-causal-v1: city/state/zip are the household's modeled home area
  // (population-weighted, coherent with the person's home), resolved
  // through the shared exporter resolver — not an independent zip draw.
  const auto home = common::pii_render::homeGeo(pool, record.address);
  out.city = std::string{home.city};
  out.state = std::string{home.state};
  out.zipcode = std::string{home.postcode};

  return out;
}

} // namespace PhantomLedger::exporter::mule_ml
