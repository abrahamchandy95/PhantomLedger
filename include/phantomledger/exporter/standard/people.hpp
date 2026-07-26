#pragma once

#include "phantomledger/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/parties/people.hpp"
#include "phantomledger/exporter/common/render.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/pii/membership.hpp"

#include <cstdint>
#include <string>
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
  namespace time_ns = ::PhantomLedger::time;
  for (ent::PersonId p = 1; p <= roster.count; ++p) {
    const auto created = membership.joinTs(p);
    const auto createdStr = time_ns::formatTimestamp(created);

    // H3 part 3c-ii: ACCOUNT CLOSURE (death + settlement) when it
    // lands inside the window; empty while still open at export.
    const auto closed = membership.closedAt(p);
    std::string closedStr;
    if (closed != time_ns::TimePoint{}) {
      closedStr = time_ns::formatTimestamp(closed);
    }

    w.writeRow(common::renderCustomerId(p).view(),
               std::string_view{createdStr}, std::string_view{closedStr});
  }
}

} // namespace PhantomLedger::exporter::standard
