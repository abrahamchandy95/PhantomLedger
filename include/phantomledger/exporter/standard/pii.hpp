#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/pii.hpp"
#include "phantomledger/exporter/common/minhash.hpp"
#include "phantomledger/exporter/common/pii_render.hpp"
#include "phantomledger/exporter/common/render.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/synth/pii/pools.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_set>

namespace PhantomLedger::exporter::standard {

namespace common = ::PhantomLedger::exporter::common;
namespace pii_render = ::PhantomLedger::exporter::common::pii_render;
namespace minhash = ::PhantomLedger::exporter::common::minhash;

inline void writePhoneRows(::PhantomLedger::exporter::csv::Writer &w,
                           const ::PhantomLedger::entity::pii::Roster &roster) {
  for (const auto &record : roster.records) {
    w.writeRow(record.phone.view());
  }
}

inline void writeEmailRows(::PhantomLedger::exporter::csv::Writer &w,
                           const ::PhantomLedger::entity::pii::Roster &roster) {
  for (const auto &record : roster.records) {
    w.writeRow(record.email.view());
  }
}

inline void
writeHasPhoneRows(::PhantomLedger::exporter::csv::Writer &w,
                  const ::PhantomLedger::entity::pii::Roster &roster) {
  for (std::size_t i = 0; i < roster.records.size(); ++i) {
    const auto person = static_cast<::PhantomLedger::entity::PersonId>(i + 1);
    w.writeRow(common::renderCustomerId(person).view(),
               roster.records[i].phone.view());
  }
}

inline void
writeHasEmailRows(::PhantomLedger::exporter::csv::Writer &w,
                  const ::PhantomLedger::entity::pii::Roster &roster) {
  for (std::size_t i = 0; i < roster.records.size(); ++i) {
    const auto person = static_cast<::PhantomLedger::entity::PersonId>(i + 1);
    w.writeRow(common::renderCustomerId(person).view(),
               roster.records[i].email.view());
  }
}

template <class Extract>
inline void
writeDistinctValueRows(::PhantomLedger::exporter::csv::Writer &w,
                       const ::PhantomLedger::entity::pii::Roster &roster,
                       Extract extract) {
  std::unordered_set<std::string> seen;
  seen.reserve(roster.records.size());
  for (const auto &rec : roster.records) {
    std::string value = extract(rec);
    if (value.empty()) {
      continue;
    }
    if (seen.insert(value).second) {
      w.writeRow(std::string_view{value});
    }
  }
}

template <class Extract>
inline void
writeHasValueRows(::PhantomLedger::exporter::csv::Writer &w,
                  const ::PhantomLedger::entity::pii::Roster &roster,
                  Extract extract) {
  for (std::size_t i = 0; i < roster.records.size(); ++i) {
    const auto person = static_cast<::PhantomLedger::entity::PersonId>(i + 1);
    std::string value = extract(roster.records[i]);
    if (value.empty()) {
      continue;
    }
    w.writeRow(common::renderCustomerId(person).view(),
               std::string_view{value});
  }
}

// ---- deduplicated phone / email vertices (for ER) ----

inline void
writePhoneVertexRows(::PhantomLedger::exporter::csv::Writer &w,
                     const ::PhantomLedger::entity::pii::Roster &roster) {
  writeDistinctValueRows(
      w, roster, [](const auto &rec) { return std::string{rec.phone.view()}; });
}

inline void
writeEmailVertexRows(::PhantomLedger::exporter::csv::Writer &w,
                     const ::PhantomLedger::entity::pii::Roster &roster) {
  writeDistinctValueRows(
      w, roster, [](const auto &rec) { return std::string{rec.email.view()}; });
}

// ---- PII value vertices (rendering delegated to common::pii_render) ----

inline void
writeNameVertexRows(::PhantomLedger::exporter::csv::Writer &w,
                    const ::PhantomLedger::synth::pii::PoolSet &pools,
                    const ::PhantomLedger::entity::pii::Roster &roster) {
  writeDistinctValueRows(
      w, roster, [&](const auto &rec) { return pii_render::name(pools, rec); });
}
inline void
writeBirthdateVertexRows(::PhantomLedger::exporter::csv::Writer &w,
                         const ::PhantomLedger::entity::pii::Roster &roster) {
  writeDistinctValueRows(
      w, roster, [](const auto &rec) { return pii_render::birthdate(rec); });
}
inline void
writeStreetVertexRows(::PhantomLedger::exporter::csv::Writer &w,
                      const ::PhantomLedger::synth::pii::PoolSet &pools,
                      const ::PhantomLedger::entity::pii::Roster &roster) {
  writeDistinctValueRows(w, roster, [&](const auto &rec) {
    return pii_render::street(pools, rec);
  });
}
inline void
writeCityVertexRows(::PhantomLedger::exporter::csv::Writer &w,
                    const ::PhantomLedger::synth::pii::PoolSet &pools,
                    const ::PhantomLedger::entity::pii::Roster &roster) {
  writeDistinctValueRows(
      w, roster, [&](const auto &rec) { return pii_render::city(pools, rec); });
}
inline void
writeStateVertexRows(::PhantomLedger::exporter::csv::Writer &w,
                     const ::PhantomLedger::synth::pii::PoolSet &pools,
                     const ::PhantomLedger::entity::pii::Roster &roster) {
  writeDistinctValueRows(w, roster, [&](const auto &rec) {
    return pii_render::state(pools, rec);
  });
}
inline void
writePostcodeVertexRows(::PhantomLedger::exporter::csv::Writer &w,
                        const ::PhantomLedger::synth::pii::PoolSet &pools,
                        const ::PhantomLedger::entity::pii::Roster &roster) {
  writeDistinctValueRows(w, roster, [&](const auto &rec) {
    return pii_render::postcode(pools, rec);
  });
}

// ---- PII edges (customer -> value) ----

inline void
writeHasNameRows(::PhantomLedger::exporter::csv::Writer &w,
                 const ::PhantomLedger::synth::pii::PoolSet &pools,
                 const ::PhantomLedger::entity::pii::Roster &roster) {
  writeHasValueRows(
      w, roster, [&](const auto &rec) { return pii_render::name(pools, rec); });
}
inline void
writeHasBirthdateRows(::PhantomLedger::exporter::csv::Writer &w,
                      const ::PhantomLedger::entity::pii::Roster &roster) {
  writeHasValueRows(w, roster,
                    [](const auto &rec) { return pii_render::birthdate(rec); });
}
inline void
writeHasStreetRows(::PhantomLedger::exporter::csv::Writer &w,
                   const ::PhantomLedger::synth::pii::PoolSet &pools,
                   const ::PhantomLedger::entity::pii::Roster &roster) {
  writeHasValueRows(w, roster, [&](const auto &rec) {
    return pii_render::street(pools, rec);
  });
}
inline void
writeHasCityRows(::PhantomLedger::exporter::csv::Writer &w,
                 const ::PhantomLedger::synth::pii::PoolSet &pools,
                 const ::PhantomLedger::entity::pii::Roster &roster) {
  writeHasValueRows(
      w, roster, [&](const auto &rec) { return pii_render::city(pools, rec); });
}
inline void
writeHasStateRows(::PhantomLedger::exporter::csv::Writer &w,
                  const ::PhantomLedger::synth::pii::PoolSet &pools,
                  const ::PhantomLedger::entity::pii::Roster &roster) {
  writeHasValueRows(w, roster, [&](const auto &rec) {
    return pii_render::state(pools, rec);
  });
}
inline void
writeHasPostcodeRows(::PhantomLedger::exporter::csv::Writer &w,
                     const ::PhantomLedger::synth::pii::PoolSet &pools,
                     const ::PhantomLedger::entity::pii::Roster &roster) {
  writeHasValueRows(w, roster, [&](const auto &rec) {
    return pii_render::postcode(pools, rec);
  });
}

template <class IdsFor>
inline void
writeMinhashVertexRows(::PhantomLedger::exporter::csv::Writer &w,
                       const ::PhantomLedger::synth::pii::PoolSet &pools,
                       const ::PhantomLedger::entity::pii::Roster &roster,
                       IdsFor idsFor) {
  std::unordered_set<std::string> seen;
  for (std::size_t i = 0; i < roster.records.size(); ++i) {
    const auto person = static_cast<::PhantomLedger::entity::PersonId>(i + 1);
    for (const auto &id : idsFor(pools, roster.records[i], person)) {
      std::string v{id.view()};
      if (!v.empty() && seen.insert(v).second) {
        w.writeRow(std::string_view{v});
      }
    }
  }
}

template <class IdsFor>
inline void
writeMinhashEdgeRows(::PhantomLedger::exporter::csv::Writer &w,
                     const ::PhantomLedger::synth::pii::PoolSet &pools,
                     const ::PhantomLedger::entity::pii::Roster &roster,
                     IdsFor idsFor) {
  for (std::size_t i = 0; i < roster.records.size(); ++i) {
    const auto person = static_cast<::PhantomLedger::entity::PersonId>(i + 1);
    const auto cid = common::renderCustomerId(person);
    for (const auto &id : idsFor(pools, roster.records[i], person)) {
      w.writeRow(cid.view(), id.view());
    }
  }
}

namespace minhash_ids {

[[nodiscard]] inline std::vector<minhash::BucketId>
name(const ::PhantomLedger::synth::pii::PoolSet &pools,
     const ::PhantomLedger::entity::pii::Record &rec,
     ::PhantomLedger::entity::PersonId person) {
  (void)person;
  const auto full = pii_render::name(pools, rec);
  if (full.empty()) {
    return {};
  }
  const auto sp = full.find(' ');
  const std::string_view first = sp == std::string::npos
                                     ? std::string_view{full}
                                     : std::string_view{full}.substr(0, sp);
  const std::string_view rest = sp == std::string::npos
                                    ? std::string_view{}
                                    : std::string_view{full}.substr(sp + 1);
  return minhash::nameMinhashIds(first, rest);
}

[[nodiscard]] inline std::vector<minhash::BucketId>
address(const ::PhantomLedger::synth::pii::PoolSet &pools,
        const ::PhantomLedger::entity::pii::Record &rec,
        ::PhantomLedger::entity::PersonId person) {
  (void)person;
  // full address = "street, city, state postcode"
  std::string full = pii_render::street(pools, rec);
  const auto c = pii_render::city(pools, rec);
  const auto s = pii_render::state(pools, rec);
  const auto z = pii_render::postcode(pools, rec);
  if (!c.empty()) {
    full += ", ";
    full += c;
  }
  if (!s.empty()) {
    full += ", ";
    full += s;
  }
  if (!z.empty()) {
    full += " ";
    full += z;
  }
  if (full.empty()) {
    return {};
  }
  return minhash::addressMinhashIds(full);
}

[[nodiscard]] inline std::vector<minhash::BucketId>
street(const ::PhantomLedger::synth::pii::PoolSet &pools,
       const ::PhantomLedger::entity::pii::Record &rec,
       ::PhantomLedger::entity::PersonId person) {
  (void)person;
  const auto v = pii_render::street(pools, rec);
  if (v.empty()) {
    return {};
  }
  return minhash::streetMinhashIds(v);
}

} // namespace minhash_ids

inline void
writeNameMinhashVertexRows(::PhantomLedger::exporter::csv::Writer &w,
                           const ::PhantomLedger::synth::pii::PoolSet &pools,
                           const ::PhantomLedger::entity::pii::Roster &roster) {
  writeMinhashVertexRows(w, pools, roster, minhash_ids::name);
}
inline void
writeHasNameMinhashRows(::PhantomLedger::exporter::csv::Writer &w,
                        const ::PhantomLedger::synth::pii::PoolSet &pools,
                        const ::PhantomLedger::entity::pii::Roster &roster) {
  writeMinhashEdgeRows(w, pools, roster, minhash_ids::name);
}
inline void writeAddressMinhashVertexRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::synth::pii::PoolSet &pools,
    const ::PhantomLedger::entity::pii::Roster &roster) {
  writeMinhashVertexRows(w, pools, roster, minhash_ids::address);
}
inline void
writeHasAddressMinhashRows(::PhantomLedger::exporter::csv::Writer &w,
                           const ::PhantomLedger::synth::pii::PoolSet &pools,
                           const ::PhantomLedger::entity::pii::Roster &roster) {
  writeMinhashEdgeRows(w, pools, roster, minhash_ids::address);
}
inline void writeStreetMinhashVertexRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::synth::pii::PoolSet &pools,
    const ::PhantomLedger::entity::pii::Roster &roster) {
  writeMinhashVertexRows(w, pools, roster, minhash_ids::street);
}
inline void
writeHasStreetMinhashRows(::PhantomLedger::exporter::csv::Writer &w,
                          const ::PhantomLedger::synth::pii::PoolSet &pools,
                          const ::PhantomLedger::entity::pii::Roster &roster) {
  writeMinhashEdgeRows(w, pools, roster, minhash_ids::street);
}

} // namespace PhantomLedger::exporter::standard
