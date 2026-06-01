#pragma once

#include "phantomledger/encoding/render.hpp"
#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/exporter/common/render.hpp"
#include "phantomledger/exporter/csv.hpp"

#include <cstdint>

namespace PhantomLedger::exporter::standard {

namespace enc = ::PhantomLedger::encoding;
namespace ent = ::PhantomLedger::entity;

inline void writeAccountNumberRows(::PhantomLedger::exporter::csv::Writer &w,
                                   const ent::account::Registry &registry) {
  using ent::account::bit;
  using ent::account::Flag;

  for (const auto &record : registry.records) {
    const auto isMule =
        static_cast<std::uint8_t>((record.flags & bit(Flag::mule)) != 0);
    const auto isFraud =
        static_cast<std::uint8_t>((record.flags & bit(Flag::fraud)) != 0);
    const auto isVictim =
        static_cast<std::uint8_t>((record.flags & bit(Flag::victim)) != 0);
    const auto isExternal =
        static_cast<std::uint8_t>((record.flags & bit(Flag::external)) != 0);

    w.writeRow(enc::format(record.id).view(), isMule, isFraud, isVictim,
               isExternal);
  }
}

inline void writeHasAccountRows(::PhantomLedger::exporter::csv::Writer &w,
                                const ent::account::Registry &registry) {
  for (const auto &record : registry.records) {
    if (record.owner == ent::invalidPerson) {
      continue;
    }
    const auto customerKey =
        ent::makeKey(ent::Role::customer, ent::Bank::internal, record.owner);
    w.writeRow(enc::format(customerKey).view(), enc::format(record.id).view());
  }
}

// Entity-Resolution account vertex: (account_id, is_fraud). The fraud label
// lives on the account; ER resolves on the owning customer, after which the
// label propagates account -> owner -> resolved component.
inline void writeAccountRows(::PhantomLedger::exporter::csv::Writer &w,
                             const ent::account::Registry &registry) {
  using ent::account::bit;
  using ent::account::Flag;
  for (const auto &record : registry.records) {
    const auto isFraud =
        static_cast<std::uint8_t>((record.flags & bit(Flag::fraud)) != 0);
    w.writeRow(enc::format(record.id).view(), isFraud);
  }
}

// Entity-Resolution ownership edge (FROM customer, TO account). Same data as
// HAS_ACCOUNT but emitted under the OWNS_ACCOUNT name the ER kit expects, with
// the customer id as the FROM endpoint.
inline void writeOwnsAccountRows(::PhantomLedger::exporter::csv::Writer &w,
                                 const ent::account::Registry &registry) {
  namespace common = ::PhantomLedger::exporter::common;
  for (const auto &record : registry.records) {
    if (record.owner == ent::invalidPerson) {
      continue;
    }
    w.writeRow(common::renderCustomerId(record.owner).view(),
               enc::format(record.id).view());
  }
}

} // namespace PhantomLedger::exporter::standard
