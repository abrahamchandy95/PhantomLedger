#pragma once

#include "phantomledger/encoding/render.hpp"
#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstdint>
#include <span>
#include <string_view>
#include <unordered_map>
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

// Fraud rows grouped for SAR generation: per-ring buckets plus the solo
// stream, retained as COPIES in corpus order. This is fraud-scale
// retention (the fraud budget fraction of the corpus), never
// transaction-scale, which is what makes SAR generation windowed-safe.
struct FraudTxnGroups {
  std::unordered_map<std::uint32_t,
                     std::vector<::PhantomLedger::transactions::Transaction>>
      byRing;
  std::vector<::PhantomLedger::transactions::Transaction> solo;
};

// Per-row accumulation, shared by the one-shot corpus path and the
// windowed streaming exporter. Rows must arrive in corpus order so the
// group contents — and therefore every SAR amount, activity period and
// dominant-channel violation type derived from them — are identical
// between the two paths.
void accumulateFraudTxn(FraudTxnGroups &groups,
                        const ::PhantomLedger::transactions::Transaction &tx);

/// Generate SARs from groups accumulated via accumulateFraudTxn (the
/// windowed path). fraud-audit-2026-07 F3: SAR presence is an
/// incomplete institutional-response label, not ground truth — a ring
/// or solo group files iff its content-keyed 70% draw passes AND its
/// activity total meets the 31 CFR §1020.320 $5,000 floor, so
/// downstream SAR consumers must (and do) handle any subset of groups.
[[nodiscard]] std::vector<SarRecord>
generateSars(const SarSubjectIndex &subjects, const FraudTxnGroups &groups);

/// One-shot corpus form: groups the fraud rows, then delegates to the
/// overload above — one code path, two engines.
[[nodiscard]] std::vector<SarRecord> generateSars(
    const SarSubjectIndex &subjects,
    std::span<const ::PhantomLedger::transactions::Transaction> finalTxns);

/// World form — THE entry point for exporters and the app: builds the
/// subject index from the world's people + holdings and delegates.
/// Exists so the index+generate pair lives in exactly one place
/// instead of being repeated at every call site.
[[nodiscard]] inline std::vector<SarRecord>
generateSars(const ::PhantomLedger::pipeline::People &people,
             const ::PhantomLedger::pipeline::Holdings &holdings,
             const FraudTxnGroups &groups) {
  const auto subjects = buildSarSubjectIndex(
      people.roster.roster, people.roster.topology, holdings.accounts.registry,
      holdings.accounts.ownership);
  return generateSars(subjects, groups);
}

} // namespace PhantomLedger::exporter::aml::sar
