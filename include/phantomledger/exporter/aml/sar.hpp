#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace PhantomLedger::exporter::aml::sar {

using SarId = ::PhantomLedger::encoding::RenderedId<32>;

struct SarRecord {

  SarId sarId;
  ::PhantomLedger::time::TimePoint filingDate;
  double amountInvolved = 0.0;
  ::PhantomLedger::time::TimePoint activityStart;
  ::PhantomLedger::time::TimePoint activityEnd;

  std::string_view violationType;

  std::vector<::PhantomLedger::entity::PersonId> subjectPersonIds;

  std::vector<std::string_view> subjectRoles;

  std::vector<::PhantomLedger::entity::Key> coveredAccountIds;
  std::vector<double> coveredAmounts;
};

struct SarSubjectRole {
  ::PhantomLedger::entity::PersonId person =
      ::PhantomLedger::entity::invalidPerson;
  std::string_view role;
};

struct SarRingSubject {
  std::uint32_t ringId = 0;
  std::vector<SarSubjectRole> subjects;
};

struct SarSoloSubject {
  ::PhantomLedger::entity::PersonId person =
      ::PhantomLedger::entity::invalidPerson;
  std::vector<::PhantomLedger::entity::Key> accountIds;
};

class SarSubjectIndex {
public:
  [[nodiscard]] std::span<const ::PhantomLedger::entity::Key>
  internalAccounts() const noexcept {
    return internalAccounts_;
  }

  [[nodiscard]] std::span<const SarRingSubject> rings() const noexcept {
    return rings_;
  }

  [[nodiscard]] std::span<const SarSoloSubject>
  soloFraudsters() const noexcept {
    return soloFraudsters_;
  }

private:
  std::vector<::PhantomLedger::entity::Key> internalAccounts_;
  std::vector<SarRingSubject> rings_;
  std::vector<SarSoloSubject> soloFraudsters_;

  friend SarSubjectIndex buildSarSubjectIndex(
      const ::PhantomLedger::entity::person::Roster &peopleRoster,
      const ::PhantomLedger::entity::person::Topology &topology,
      const ::PhantomLedger::entity::account::Registry &accounts,
      const ::PhantomLedger::entity::account::Ownership &ownership);
};

[[nodiscard]] SarSubjectIndex buildSarSubjectIndex(
    const ::PhantomLedger::entity::person::Roster &peopleRoster,
    const ::PhantomLedger::entity::person::Topology &topology,
    const ::PhantomLedger::entity::account::Registry &accounts,
    const ::PhantomLedger::entity::account::Ownership &ownership);

/// Generate one SAR per ring + one per solo fraudster.
[[nodiscard]] std::vector<SarRecord> generateSars(
    const SarSubjectIndex &subjects,
    std::span<const ::PhantomLedger::transactions::Transaction> finalTxns);

} // namespace PhantomLedger::exporter::aml::sar
