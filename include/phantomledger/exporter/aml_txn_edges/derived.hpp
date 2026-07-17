#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/common/render.hpp"
#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/record.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <map>
#include <span>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PhantomLedger::exporter::aml_txn_edges::derived {

template <std::size_t N> struct InlineId {
  std::array<char, N> bytes{};
  std::uint8_t length = 0;

  [[nodiscard]] constexpr std::string_view view() const noexcept {
    return {bytes.data(), length};
  }
  [[nodiscard]] constexpr bool empty() const noexcept { return length == 0; }
  constexpr operator std::string_view() const noexcept { return view(); }
};
using AlertId = InlineId<24>;
using DispositionId = InlineId<24>;
using CtrId = InlineId<24>;
using CaseId = InlineId<24>;
using EvidenceId = InlineId<24>;
using PromotedTxnId = InlineId<24>;
using BusinessId = InlineId<24>;
using HashHex = InlineId<24>;
using RunId = InlineId<24>;
using PhoneText = InlineId<16>;
using DateText = InlineId<11>;
using InvestigatorId = InlineId<8>;
using BranchCode = InlineId<6>;
using TellerId = InlineId<12>;
using EinText = InlineId<11>;
using CpId = InlineId<48>;
using PrefixedCustomerId = InlineId<32>;

template <std::size_t N>
[[nodiscard]] InlineId<N>
hashedId(std::string_view prefix,
         std::initializer_list<std::string_view> hashInputs) noexcept;
[[nodiscard]] HashHex
makeHashHex(std::initializer_list<std::string_view> parts) noexcept;

[[nodiscard]] CpId makeCpId(std::string_view rawKey) noexcept;

[[nodiscard]] PrefixedCustomerId
prefixedCustomerId(std::string_view prefix,
                   const common::CustomerId &cid) noexcept;

[[nodiscard]] bool isCreditChannel(channels::Tag tag) noexcept;

enum class Rule : std::uint8_t {
  fraudMlFlag = 1,
  highAmountBelowCtr = 2,
  cashCtrThreshold = 3,
  velocityBurst = 4,
  highRiskCounterparty = 5, // reserved, never fires
};

[[nodiscard]] std::string_view ruleName(Rule r) noexcept;
[[nodiscard]] std::int32_t rulePriority(Rule r) noexcept;
[[nodiscard]] std::string_view ruleSourceSystem(Rule r) noexcept;

enum class DispositionOutcome : std::uint8_t {
  escalated,
  closedFp,
  closedNoAction,
};

[[nodiscard]] std::string_view dispositionAction(DispositionOutcome o) noexcept;
[[nodiscard]] std::string_view
dispositionCloseCode(DispositionOutcome o) noexcept;
[[nodiscard]] std::string_view alertStatusAfter(DispositionOutcome o) noexcept;

[[nodiscard]] AlertId makeAlertId(std::string_view accountId,
                                  std::size_t txnIdx,
                                  std::uint8_t ruleByte) noexcept;
[[nodiscard]] CtrId makeCtrId(std::string_view accountId,
                              std::size_t txnIdx) noexcept;
[[nodiscard]] CaseId makeRingCaseId(std::uint32_t ringId) noexcept;
[[nodiscard]] CaseId makeSoloCaseId(entity::PersonId p) noexcept;
[[nodiscard]] PromotedTxnId makePromotedTxnId(std::string_view caseId,
                                              std::size_t txnIdx) noexcept;
[[nodiscard]] BusinessId makeBusinessId(std::string_view accountKey,
                                        entity::PersonId owner) noexcept;

struct AlertRecord {
  AlertId id{};
  entity::Key onAccount{};
  Rule rule = Rule::fraudMlFlag;
  time::TimePoint createdDate{};
  std::string_view status = "open";
};

struct DispositionRecord {
  DispositionId id{};
  std::size_t alertIndex = 0;
  DispositionOutcome outcome = DispositionOutcome::closedFp;
  time::TimePoint date{};
  std::uint32_t investigatorNum = 1; // 1..12  → "INV_NNN"
  HashHex notesHash{};
  double confidence = 0.7;
};

struct CtrRecord {
  CtrId id{};
  entity::Key onAccount{};
  time::TimePoint filingDate{};
  double amount = 0.0;
  std::uint32_t branchBucket = 0; // (account.number % 50) + 1
  std::uint32_t tellerNum = 0;    // 1..200
};

enum class CaseKind : std::uint8_t { ring, solo };

struct CaseRecord {
  CaseId id{};
  CaseKind kind = CaseKind::ring;
  std::uint32_t ringOrPerson = 0;
  time::TimePoint openedDate{};
  std::string_view caseSystem{};
  std::vector<entity::PersonId> subjectPersons;
  std::vector<std::size_t> alertIndices;
  std::vector<std::size_t> sarIndices;
  std::vector<std::size_t> evidenceIndices;
  std::vector<std::size_t> promotedTxnIndices;
};

struct EvidenceRecord {
  EvidenceId id{};
  std::size_t caseIndex = 0;
  std::string_view artifactType{};
  std::string_view sourceSystem{};
  HashHex contentHash{};
  time::TimePoint createdAt{};
};

struct PromotedTxnRecord {
  PromotedTxnId id{};
  std::size_t caseIndex = 0;
  std::size_t txnIndex = 0; // 1-based postedTxns index
  time::TimePoint promotedAt{};
  time::TimePoint ttlDate{};
};

struct BusinessRecord {
  BusinessId id{};
  entity::Key accountKey{};
  entity::PersonId owner = entity::invalidPerson;
  std::uint32_t stemIdx = 0;
  std::uint32_t numberSuffix = 0;
  std::string_view entityType{};
  time::TimePoint effectiveDate{};
};

using AcctPair = std::pair<entity::Key, entity::Key>;

struct AggregateRow {
  double totalAmount = 0.0;
  std::uint32_t txnCount = 0;
  std::int64_t firstTs = 0;
  std::int64_t lastTs = 0;
  double amount30d = 0.0;
  double amount90d = 0.0;
  std::uint32_t count30d = 0;
  std::uint32_t count90d = 0;
};

struct AggregateBucket {
  AcctPair pair{};
  AggregateRow row{};
};

void accumulate(AggregateRow &row, double amount, std::int64_t ts,
                std::int64_t cut30Epoch, std::int64_t cut90Epoch) noexcept;

// Empty-corpus sim window (2025-01-01 UTC), shared by the corpus and
// read-back builders.
inline constexpr std::int64_t kFallbackEpoch = 1735689600;

// -------------------------------------------------- shared per-row sweep
//
// The one-code-path seam for the PostgreSQL read-back: the corpus-based
// buildBundle and the read-back builder both feed rows through THE SAME
// TxnSweep::observe in corpus (row_seq) order and hand the result to THE
// SAME finishBundle, so the two corpus stores cannot drift. observe
// reads only source/target/amount/timestamp/fraud.flag — exactly the
// fields the read-back decode contract pins losslessly
// (test_pg_readback).

// Fraud-scale retention (documented acceptable): everything
// promoteFraudTxns needs from a fraud row.
struct FraudTxnRef {
  std::size_t txnIndex1 = 0; // 1-based corpus index == row_seq
  entity::Key source{};
  entity::Key target{};
};

// Fraud-scale retention of FULL fraud rows keyed by their 1-based
// corpus index (row_seq): the promoted-txn table writers need amount,
// timestamp and channel for the rows the bundle promoted — and every
// promoted index is a fraud index by construction (promoteFraudTxns
// draws exclusively from FraudTxnRef).
using FraudTxnByIndex =
    std::unordered_map<std::size_t, transactions::Transaction>;

struct BurstKeyInfo {
  entity::Key account{};
  std::int64_t day = 0;
};

struct TxnSweep {
  // expectedRows sizes the reserves identically on both paths (corpus
  // size vs queryStreamBounds().rows), which also keeps the
  // unordered_map bucket trajectories — and hence velocity-alert
  // iteration order — identical between them.
  TxnSweep(std::int64_t simEndEpoch, std::size_t expectedRows);

  void observe(const transactions::Transaction &tx, std::size_t idx1);

  std::vector<AlertRecord> alerts;
  std::vector<CtrRecord> ctrs;
  std::unordered_map<std::uint64_t, std::uint32_t> burstCounts;
  std::unordered_map<std::uint64_t, BurstKeyInfo> burstInfo;
  std::map<AcctPair, AggregateRow> flowAccum;
  std::map<AcctPair, AggregateRow> linkAccum;
  std::vector<FraudTxnRef> fraudRefs;

  std::int64_t cut30 = 0;
  std::int64_t cut90 = 0;
};

struct Bundle {
  time::TimePoint simStart{};
  time::TimePoint simEnd{};
  std::int64_t simStartEpoch = 0;
  std::int64_t simEndEpoch = 0;

  RunId derivationRunId{};

  std::vector<AlertRecord> alerts;
  std::vector<DispositionRecord> dispositions;
  std::vector<CtrRecord> ctrs;
  std::vector<CaseRecord> cases;
  std::vector<EvidenceRecord> evidence;
  std::vector<PromotedTxnRecord> promotedTxns;
  std::vector<BusinessRecord> businesses;

  std::vector<AggregateBucket> flowAgg;
  std::vector<AggregateBucket> linkComm;
};

// Sim window + derivation run id from the corpus timestamp extent (the
// read-back path supplies these from queryStreamBounds).
void applySimWindow(Bundle &b, std::int64_t minTs, std::int64_t maxTs);

// Everything after the sweep: velocity alerts, dispositions, cases,
// SAR/alert attachment, evidence, fraud-txn promotion, businesses, and
// the sorted aggregates — from the world plus the sweep only. Shared
// verbatim by both builders; applySimWindow must already have run on b.
[[nodiscard]] Bundle finishBundle(Bundle b, TxnSweep sweep,
                                  const pipeline::People &people,
                                  const pipeline::Holdings &holdings,
                                  std::span<const aml::sar::SarRecord> sars);

[[nodiscard]] Bundle
buildBundle(const pipeline::People &people, const pipeline::Holdings &holdings,
            std::span<const transactions::Transaction> postedTxns,
            std::span<const aml::sar::SarRecord> sars);

[[nodiscard]] PhoneText phoneFor(entity::PersonId personId) noexcept;

[[nodiscard]] DateText formatDob(std::int16_t year, std::uint8_t month,
                                 std::uint8_t day) noexcept;

[[nodiscard]] InvestigatorId investigatorIdFor(std::uint32_t n) noexcept;
[[nodiscard]] BranchCode branchCodeForBucket(std::uint32_t bucket) noexcept;
[[nodiscard]] TellerId tellerIdFor(std::uint32_t n) noexcept;
[[nodiscard]] EinText einFor(std::string_view businessId) noexcept;

inline constexpr std::array<std::string_view, 12> kLegalStems{
    "Arbor", "Beacon", "Cypress",  "Dune",    "Echo",     "Foundry",
    "Grove", "Harbor", "Ironwood", "Juniper", "Keystone", "Lantern",
};

inline constexpr std::array<std::string_view, 5> kEntityTypes{
    "LLC", "S-Corp", "C-Corp", "Sole Proprietorship", "Partnership",
};

inline constexpr std::array<std::string_view, 3> kCaseSystems{
    "actimize",
    "verafin",
    "in_house",
};

struct EvidenceKind {
  std::string_view artifactType;
  std::string_view sourceSystem;
};

inline constexpr std::array<EvidenceKind, 3> kEvidenceKinds{{
    {"txn_pattern_export", "case_mgmt"},
    {"device_session_log", "siem"},
    {"kyc_snapshot", "case_mgmt"},
}};

} // namespace PhantomLedger::exporter::aml_txn_edges::derived
