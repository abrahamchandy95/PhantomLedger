#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/exporter/csv.hpp"

namespace PhantomLedger::exporter::standard {

inline void writeAccountNumberRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::entity::account::Registry &registry) {
  using ::PhantomLedger::entity::account::bit;
  using ::PhantomLedger::entity::account::Flag;

  for (const auto &record : registry.records) {
    const bool isMule = (record.flags & bit(Flag::mule)) != 0;
    const bool isFraud = (record.flags & bit(Flag::fraud)) != 0;
    const bool isVictim = (record.flags & bit(Flag::victim)) != 0;
    const bool isExternal = (record.flags & bit(Flag::external)) != 0;

    w.writeRow(::PhantomLedger::encoding::format(record.id), isMule, isFraud,
               isVictim, isExternal);
  }
}

inline void writeHasAccountRows(
    ::PhantomLedger::exporter::csv::Writer &w,
    const ::PhantomLedger::entity::account::Registry &registry) {
  for (const auto &record : registry.records) {
    if (record.owner == ::PhantomLedger::entity::invalidPerson) {
      continue;
    }
    const auto customerKey = ::PhantomLedger::entity::makeKey(
        ::PhantomLedger::entity::Role::customer,
        ::PhantomLedger::entity::Bank::internal, record.owner);
    w.writeRow(::PhantomLedger::encoding::format(customerKey),
               ::PhantomLedger::encoding::format(record.id));
  }
}

} // namespace PhantomLedger::exporter::standard
