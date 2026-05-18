#pragma once

#include "phantomledger/encoding/render.hpp"
#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
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

} // namespace PhantomLedger::exporter::standard
