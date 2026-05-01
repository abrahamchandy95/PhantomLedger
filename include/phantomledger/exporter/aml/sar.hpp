#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/transactions/record.hpp"

#include <span>
#include <string>
#include <vector>

namespace PhantomLedger::exporter::aml::sar {

struct SarRecord {
  std::string sarId;
  ::PhantomLedger::time::TimePoint filingDate;
  double amountInvolved = 0.0;
  ::PhantomLedger::time::TimePoint activityStart;
  ::PhantomLedger::time::TimePoint activityEnd;
  std::string violationType;
  std::vector<::PhantomLedger::entity::PersonId> subjectPersonIds;
  std::vector<std::string> subjectRoles;
  std::vector<::PhantomLedger::entity::Key> coveredAccountIds;
  std::vector<double> coveredAmounts;
};

/// Generate one SAR per ring + one per solo fraudster.
[[nodiscard]] std::vector<SarRecord> generateSars(
    const ::PhantomLedger::entity::person::Roster &peopleRoster,
    const ::PhantomLedger::entity::person::Topology &topology,
    const ::PhantomLedger::entity::account::Registry &accounts,
    const ::PhantomLedger::entity::account::Ownership &ownership,
    std::span<const ::PhantomLedger::transactions::Transaction> finalTxns);

} // namespace PhantomLedger::exporter::aml::sar
