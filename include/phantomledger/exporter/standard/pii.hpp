#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/pii.hpp"
#include "phantomledger/exporter/common/render.hpp"
#include "phantomledger/exporter/csv.hpp"

namespace PhantomLedger::exporter::standard {

namespace common = ::PhantomLedger::exporter::common;

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

} // namespace PhantomLedger::exporter::standard
