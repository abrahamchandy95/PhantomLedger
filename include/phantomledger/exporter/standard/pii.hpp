#pragma once

#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/pii.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/exporter/standard/internal/customer_id.hpp"

namespace PhantomLedger::exporter::standard {

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
  // Dense index — record at `i` belongs to PersonId `i + 1`.
  for (std::size_t i = 0; i < roster.records.size(); ++i) {
    const auto person = static_cast<::PhantomLedger::entity::PersonId>(i + 1);
    w.writeRow(detail::customerIdFor(person), roster.records[i].phone.view());
  }
}

inline void
writeHasEmailRows(::PhantomLedger::exporter::csv::Writer &w,
                  const ::PhantomLedger::entity::pii::Roster &roster) {
  for (std::size_t i = 0; i < roster.records.size(); ++i) {
    const auto person = static_cast<::PhantomLedger::entity::PersonId>(i + 1);
    w.writeRow(detail::customerIdFor(person), roster.records[i].email.view());
  }
}

} // namespace PhantomLedger::exporter::standard
