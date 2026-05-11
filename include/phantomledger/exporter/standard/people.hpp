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
  namespace enc = ::PhantomLedger::encoding;
  namespace ent = ::PhantomLedger::entity;

  for (ent::PersonId p = 1; p <= roster.count; ++p) {
    const auto customerKey =
        ent::makeKey(ent::Role::customer, ent::Bank::internal, p);

    w.writeRow(enc::format(customerKey).view(), roster.has(p, Flag::mule),
               roster.has(p, Flag::fraud), roster.has(p, Flag::victim),
               roster.has(p, Flag::soloFraud));
  }
}

} // namespace PhantomLedger::exporter::standard
