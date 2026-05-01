#pragma once

#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/exporter/csv.hpp"

namespace PhantomLedger::exporter::standard {

inline void
writePersonRows(::PhantomLedger::exporter::csv::Writer &w,
                const ::PhantomLedger::entity::person::Roster &roster) {
  using ::PhantomLedger::entity::person::Flag;

  for (::PhantomLedger::entity::PersonId p = 1; p <= roster.count; ++p) {
    const auto customerKey = ::PhantomLedger::entity::makeKey(
        ::PhantomLedger::entity::Role::customer,
        ::PhantomLedger::entity::Bank::internal, p);

    w.writeRow(::PhantomLedger::encoding::format(customerKey),
               roster.has(p, Flag::mule), roster.has(p, Flag::fraud),
               roster.has(p, Flag::victim), roster.has(p, Flag::soloFraud));
  }
}

} // namespace PhantomLedger::exporter::standard
