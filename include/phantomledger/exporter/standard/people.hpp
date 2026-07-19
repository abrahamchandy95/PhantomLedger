#pragma once

#include "phantomledger/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/parties/people.hpp"
#include "phantomledger/exporter/common/render.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/pii/membership.hpp"

#include <cstdint>
#include <string_view>

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

    const auto isMule = static_cast<std::uint8_t>(roster.has(p, Flag::mule));
    const auto isFraud = static_cast<std::uint8_t>(roster.has(p, Flag::fraud));
    const auto isVictim =
        static_cast<std::uint8_t>(roster.has(p, Flag::victim));
    const auto isSoloFraud =
        static_cast<std::uint8_t>(roster.has(p, Flag::soloFraud));

    w.writeRow(enc::format(customerKey).view(), isMule, isFraud, isVictim,
               isSoloFraud);
  }
}

inline void
writeCustomerRows(::PhantomLedger::exporter::csv::Writer &w,
                  const ::PhantomLedger::entity::person::Roster &roster,
                  const ::PhantomLedger::synth::pii::Membership &membership) {
  namespace ent = ::PhantomLedger::entity;
  namespace common = ::PhantomLedger::exporter::common;
  for (ent::PersonId p = 1; p <= roster.count; ++p) {
    const auto created = membership.joinTs(p);
    const auto createdStr = ::PhantomLedger::time::formatTimestamp(created);
    w.writeRow(common::renderCustomerId(p).view(),
               std::string_view{createdStr});
  }
}

} // namespace PhantomLedger::exporter::standard
