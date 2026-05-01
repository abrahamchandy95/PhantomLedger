#pragma once

#include "phantomledger/entities/pii.hpp"
#include "phantomledger/entities/synth/pii/pools.hpp"
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
formatName(const ::PhantomLedger::entities::synth::pii::LocalePool &pool,
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
               const ::PhantomLedger::entities::synth::pii::PoolSet &pools) {
  PartyIdentity out;
  const auto &pool = pools.forCountry(record.country);

  out.name = detail::formatName(pool, record.name);
  out.ssn = std::string{record.ssn.view()};
  out.dob = detail::formatDob(record.dob);
  out.country = std::string{::PhantomLedger::locale::code(record.country)};

  if (record.address.streetIdx < pool.streets.size()) {
    out.address = pool.streets[record.address.streetIdx];
  }
  if (record.address.zipTableIdx < pool.zipTable.size()) {
    const auto &zip = pool.zipTable[record.address.zipTableIdx];
    out.city = zip.city;
    out.state = zip.adminCode;
    out.zipcode = zip.postalCode;
  }
  return out;
}

} // namespace PhantomLedger::exporter::mule_ml
