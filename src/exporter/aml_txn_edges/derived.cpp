#include "phantomledger/exporter/aml_txn_edges/derived.hpp"

#include "phantomledger/encoding/render.hpp"
#include "phantomledger/exporter/common/hashing.hpp"
#include "phantomledger/primitives/time/constants.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/taxonomies/channels/predicates.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <map>
#include <unordered_map>
#include <unordered_set>

namespace PhantomLedger::exporter::aml_txn_edges::derived {

namespace ent = ::PhantomLedger::entity;
namespace tx_ns = ::PhantomLedger::transactions;
namespace t_ns = ::PhantomLedger::time;
namespace ch = ::PhantomLedger::channels;
namespace common = ::PhantomLedger::exporter::common;

namespace {

inline constexpr char kHexDigits[] = "0123456789abcdef";

template <std::size_t N>
[[nodiscard]] InlineId<N> renderHashedId(std::string_view prefix,
                                         std::uint64_t value) noexcept {
  InlineId<N> out;
  std::size_t pos = 0;
  std::memcpy(out.bytes.data() + pos, prefix.data(), prefix.size());
  pos += prefix.size();
  for (int shift = 60; shift >= 0; shift -= 4) {
    out.bytes[pos++] =
        kHexDigits[(value >> static_cast<unsigned>(shift)) & 0xFULL];
  }
  out.length = static_cast<std::uint8_t>(pos);
  return out;
}

struct DecimalBuffer {
  std::array<char, 20> bytes{};
  std::uint8_t length = 0;
  [[nodiscard]] std::string_view view() const noexcept {
    return {bytes.data(), length};
  }
};

[[nodiscard]] DecimalBuffer u64Decimal(std::uint64_t v) noexcept {
  DecimalBuffer out;
  if (v == 0) {
    out.bytes[out.length++] = '0';
    return out;
  }
  char tmp[20];
  std::size_t pos = sizeof(tmp);
  while (v != 0 && pos > 0) {
    tmp[--pos] = static_cast<char>('0' + (v % 10ULL));
    v /= 10ULL;
  }
  const auto n = sizeof(tmp) - pos;
  std::memcpy(out.bytes.data(), tmp + pos, n);
  out.length = static_cast<std::uint8_t>(n);
  return out;
}

[[nodiscard]] DecimalBuffer singleByteString(std::uint8_t b) noexcept {
  DecimalBuffer out;
  out.bytes[0] = static_cast<char>(b);
  out.length = 1;
  return out;
}

} // namespace

// ============================================================================
// Public id-maker API.
// ============================================================================

template <std::size_t N>
InlineId<N>
hashedId(std::string_view prefix,
         std::initializer_list<std::string_view> hashInputs) noexcept {
  return renderHashedId<N>(prefix, common::stableU64(hashInputs));
}

// Explicit instantiation for the only width we ship.
template InlineId<24>
    hashedId<24>(std::string_view,
                 std::initializer_list<std::string_view>) noexcept;

HashHex makeHashHex(std::initializer_list<std::string_view> parts) noexcept {
  return hashedId<24>("", parts);
}

AlertId makeAlertId(std::string_view accountId, std::size_t txnIdx,
                    std::uint8_t ruleByte) noexcept {
  const auto idxBytes = u64Decimal(static_cast<std::uint64_t>(txnIdx));
  const auto ruleBytes = singleByteString(ruleByte);
  return hashedId<24>("ALT_",
                      {"ALT", accountId, idxBytes.view(), ruleBytes.view()});
}

DispositionId makeDispositionId(std::string_view alertId) noexcept {
  return hashedId<24>("DSP_", {"DSP", alertId});
}

CtrId makeCtrId(std::string_view accountId, std::size_t txnIdx) noexcept {
  const auto idxBytes = u64Decimal(static_cast<std::uint64_t>(txnIdx));
  return hashedId<24>("CTR_", {"CTR", accountId, idxBytes.view()});
}

CaseId makeRingCaseId(std::uint32_t ringId) noexcept {
  const auto idBytes = u64Decimal(static_cast<std::uint64_t>(ringId));
  return hashedId<24>("CASE_", {"CASE", "ring", idBytes.view()});
}

CaseId makeSoloCaseId(ent::PersonId p) noexcept {
  const auto idBytes = u64Decimal(static_cast<std::uint64_t>(p));
  return hashedId<24>("CASE_", {"CASE", "solo", idBytes.view()});
}

EvidenceId makeEvidenceId(std::string_view caseId,
                          std::string_view kind) noexcept {
  return hashedId<24>("EVD_", {"EVD", caseId, kind});
}

PromotedTxnId makePromotedTxnId(std::string_view caseId,
                                std::size_t txnIdx) noexcept {
  const auto idxBytes = u64Decimal(static_cast<std::uint64_t>(txnIdx));
  return hashedId<24>("PTX_", {"PTX", caseId, idxBytes.view()});
}

BusinessId makeBusinessId(std::string_view accountKey,
                          ent::PersonId owner) noexcept {
  const auto pidBytes = u64Decimal(static_cast<std::uint64_t>(owner));
  return hashedId<24>("BUS_", {"BUS", accountKey, pidBytes.view()});
}

// ============================================================================
// CpId / PrefixedCustomerId — previously duplicated in vertices.cpp/edges.cpp.
// ============================================================================

CpId makeCpId(std::string_view rawKey) noexcept {
  CpId out;
  out.bytes[0] = 'C';
  out.bytes[1] = 'P';
  out.bytes[2] = '_';
  const auto take = rawKey.size() < (out.bytes.size() - 3)
                        ? rawKey.size()
                        : (out.bytes.size() - 3);
  std::memcpy(out.bytes.data() + 3, rawKey.data(), take);
  out.length = static_cast<std::uint8_t>(3 + take);
  return out;
}

PrefixedCustomerId prefixedCustomerId(
    std::string_view prefix,
    const ::PhantomLedger::exporter::common::CustomerId &cid) noexcept {
  PrefixedCustomerId out;
  std::size_t pos = 0;
  std::memcpy(out.bytes.data() + pos, prefix.data(), prefix.size());
  pos += prefix.size();
  out.bytes[pos++] = '_';
  const auto cidView = cid.view();
  std::memcpy(out.bytes.data() + pos, cidView.data(), cidView.size());
  pos += cidView.size();
  out.length = static_cast<std::uint8_t>(pos);
  return out;
}

// ============================================================================
// Channel classification.
// ============================================================================

bool isCreditChannel(ch::Tag tag) noexcept {
  return ch::isPaydayInbound(tag) || ch::is(tag, ch::Insurance::claim) ||
         ch::is(tag, ch::Product::taxRefund) ||
         ch::is(tag, ch::Credit::refund) || ch::is(tag, ch::Credit::chargeback);
}

// ============================================================================
// Rule / disposition taxonomy.
// ============================================================================

std::string_view ruleName(Rule r) noexcept {
  switch (r) {
  case Rule::fraudMlFlag:
    return "FRAUD_ML_FLAG";
  case Rule::highAmountBelowCtr:
    return "HIGH_AMOUNT_BELOW_CTR";
  case Rule::cashCtrThreshold:
    return "CASH_CTR_THRESHOLD";
  case Rule::velocityBurst:
    return "VELOCITY_BURST";
  case Rule::highRiskCounterparty:
    return "HIGH_RISK_COUNTERPARTY";
  }
  return "UNKNOWN";
}

std::int32_t rulePriority(Rule r) noexcept {
  switch (r) {
  case Rule::fraudMlFlag:
  case Rule::cashCtrThreshold:
    return 3;
  case Rule::highAmountBelowCtr:
  case Rule::velocityBurst:
    return 2;
  case Rule::highRiskCounterparty:
    return 1;
  }
  return 0;
}

std::string_view ruleSourceSystem(Rule r) noexcept {
  return (r == Rule::fraudMlFlag) ? "ml_pipeline" : "rules_engine";
}

std::string_view dispositionAction(DispositionOutcome o) noexcept {
  return (o == DispositionOutcome::escalated) ? "escalate" : "close";
}

std::string_view dispositionCloseCode(DispositionOutcome o) noexcept {
  switch (o) {
  case DispositionOutcome::escalated:
    return "ESCALATED_TO_SAR";
  case DispositionOutcome::closedFp:
    return "FALSE_POSITIVE";
  case DispositionOutcome::closedNoAction:
    return "NO_ACTION";
  }
  return "";
}

std::string_view alertStatusAfter(DispositionOutcome o) noexcept {
  return (o == DispositionOutcome::escalated) ? "escalated" : "closed";
}

// ============================================================================
// Fixed-shape display strings.
// ============================================================================

PhoneText phoneFor(ent::PersonId p) noexcept {
  const auto idBytes = u64Decimal(static_cast<std::uint64_t>(p));
  const auto h = common::stableU64({"PHONE", idBytes.view()});

  PhoneText out;
  auto *cursor = out.bytes.data();
  *cursor++ = '+';
  *cursor++ = '1';
  *cursor++ = '-';

  const auto npa = static_cast<unsigned>((h % 800ULL) + 200ULL);
  const auto exch = static_cast<unsigned>(((h >> 16) % 800ULL) + 200ULL);
  const auto subs = static_cast<unsigned>((h >> 32) % 10000ULL);

  const auto emitN = [&](unsigned v, int width) {
    char tmp[5];
    for (int i = width - 1; i >= 0; --i) {
      tmp[i] = static_cast<char>('0' + (v % 10U));
      v /= 10U;
    }
    for (int i = 0; i < width; ++i) {
      *cursor++ = tmp[i];
    }
  };

  emitN(npa, 3);
  *cursor++ = '-';
  emitN(exch, 3);
  *cursor++ = '-';
  emitN(subs, 4);
  out.length = static_cast<std::uint8_t>(cursor - out.bytes.data());
  return out;
}

DateText formatDob(std::int16_t year, std::uint8_t month,
                   std::uint8_t day) noexcept {
  DateText out;
  auto *cursor = out.bytes.data();
  const auto y = static_cast<unsigned>(year < 0 ? 0 : year);
  *cursor++ = static_cast<char>('0' + ((y / 1000U) % 10U));
  *cursor++ = static_cast<char>('0' + ((y / 100U) % 10U));
  *cursor++ = static_cast<char>('0' + ((y / 10U) % 10U));
  *cursor++ = static_cast<char>('0' + (y % 10U));
  *cursor++ = '-';
  *cursor++ = static_cast<char>('0' + ((month / 10U) % 10U));
  *cursor++ = static_cast<char>('0' + (month % 10U));
  *cursor++ = '-';
  *cursor++ = static_cast<char>('0' + ((day / 10U) % 10U));
  *cursor++ = static_cast<char>('0' + (day % 10U));
  out.length = 10;
  return out;
}

InvestigatorId investigatorIdFor(std::uint32_t n) noexcept {
  InvestigatorId out;
  auto *cursor = out.bytes.data();
  *cursor++ = 'I';
  *cursor++ = 'N';
  *cursor++ = 'V';
  *cursor++ = '_';
  *cursor++ = static_cast<char>('0' + ((n / 100U) % 10U));
  *cursor++ = static_cast<char>('0' + ((n / 10U) % 10U));
  *cursor++ = static_cast<char>('0' + (n % 10U));
  out.length = 7;
  return out;
}

BranchCode branchCodeForBucket(std::uint32_t bucket) noexcept {
  BranchCode out;
  auto *cursor = out.bytes.data();
  *cursor++ = 'B';
  *cursor++ = 'R';
  *cursor++ = static_cast<char>('0' + ((bucket / 100U) % 10U));
  *cursor++ = static_cast<char>('0' + ((bucket / 10U) % 10U));
  *cursor++ = static_cast<char>('0' + (bucket % 10U));
  out.length = 5;
  return out;
}

TellerId tellerIdFor(std::uint32_t n) noexcept {
  TellerId out;
  auto *cursor = out.bytes.data();
  *cursor++ = 'T';
  *cursor++ = 'L';
  *cursor++ = 'R';
  *cursor++ = '_';
  *cursor++ = static_cast<char>('0' + ((n / 1000U) % 10U));
  *cursor++ = static_cast<char>('0' + ((n / 100U) % 10U));
  *cursor++ = static_cast<char>('0' + ((n / 10U) % 10U));
  *cursor++ = static_cast<char>('0' + (n % 10U));
  out.length = 8;
  return out;
}

EinText einFor(std::string_view businessId) noexcept {
  const auto h = common::stableU64({"EIN", businessId});
  EinText out;
  auto *cursor = out.bytes.data();
  const auto two = static_cast<unsigned>(h % 90ULL) + 10U;
  const auto seven = static_cast<unsigned>((h >> 16) % 10000000ULL);
  *cursor++ = static_cast<char>('0' + ((two / 10U) % 10U));
  *cursor++ = static_cast<char>('0' + (two % 10U));
  *cursor++ = '-';
  unsigned v = seven;
  char tmp[7];
  for (int i = 6; i >= 0; --i) {
    tmp[i] = static_cast<char>('0' + (v % 10U));
    v /= 10U;
  }
  for (int i = 0; i < 7; ++i) {
    *cursor++ = tmp[i];
  }
  out.length = 10;
  return out;
}

// ============================================================================
// Aggregate accumulator — shared between FLOW_AGG (directed) and LINK_COMM
// (undirected). The buildBundle sweep calls this once per txn per key shape;
// each row update was previously a 14-line block written twice with identical
// logic, now collapsed.
// ============================================================================

void accumulate(AggregateRow &row, double amount, std::int64_t ts,
                std::int64_t cut30Epoch, std::int64_t cut90Epoch) noexcept {
  row.totalAmount += amount;
  if (row.txnCount == 0) {
    row.firstTs = ts;
    row.lastTs = ts;
  } else {
    if (ts < row.firstTs) {
      row.firstTs = ts;
    }
    if (ts > row.lastTs) {
      row.lastTs = ts;
    }
  }
  ++row.txnCount;
  if (ts >= cut90Epoch) {
    row.amount90d += amount;
    ++row.count90d;
    if (ts >= cut30Epoch) {
      row.amount30d += amount;
      ++row.count30d;
    }
  }
}

namespace {

[[nodiscard]] bool isInternal(const ent::Key &k) noexcept {
  return k.bank == ent::Bank::internal;
}

[[nodiscard]] ent::Key alertAccount(const tx_ns::Transaction &tx) noexcept {
  if (isInternal(tx.source)) {
    return tx.source;
  }
  if (isInternal(tx.target)) {
    return tx.target;
  }
  return ent::Key{};
}

struct OwnerIndex {
  std::unordered_map<ent::Key, ent::PersonId> ownerOf;

  [[nodiscard]] ent::PersonId at(const ent::Key &k) const noexcept {
    const auto it = ownerOf.find(k);
    return (it == ownerOf.end()) ? ent::invalidPerson : it->second;
  }
};

[[nodiscard]] OwnerIndex
buildOwnerIndex(const ::PhantomLedger::pipeline::Entities &entities) {
  OwnerIndex out;
  out.ownerOf.reserve(entities.accounts.registry.records.size());
  for (const auto &rec : entities.accounts.registry.records) {
    if (rec.owner != ent::invalidPerson) {
      out.ownerOf.emplace(rec.id, rec.owner);
    }
  }
  return out;
}

void emitAlert(Bundle &b, const ent::Key &onAcct, Rule rule,
               t_ns::TimePoint createdDate, std::size_t idx) {
  const auto acctRendered = ::PhantomLedger::encoding::format(onAcct);
  AlertRecord a;
  a.onAccount = onAcct;
  a.rule = rule;
  a.createdDate = createdDate;
  a.id = makeAlertId(acctRendered.view(), idx, static_cast<std::uint8_t>(rule));
  b.alerts.push_back(std::move(a));
}

} // namespace

Bundle buildBundle(
    const ::PhantomLedger::pipeline::Entities &entities,
    std::span<const tx_ns::Transaction> postedTxns,
    const ::PhantomLedger::exporter::aml::vertices::SharedContext &ctx,
    std::span<const ::PhantomLedger::exporter::aml::sar::SarRecord> sars) {
  (void)ctx;
  Bundle b;

  // ── 1. simStart / simEnd ──
  constexpr std::int64_t kFallbackEpoch = 1735689600; // 2025-01-01 UTC
  std::int64_t minTs = kFallbackEpoch;
  std::int64_t maxTs = kFallbackEpoch;
  if (!postedTxns.empty()) {
    minTs = postedTxns.front().timestamp;
    maxTs = postedTxns.front().timestamp;
    for (const auto &tx : postedTxns) {
      if (tx.timestamp < minTs) {
        minTs = tx.timestamp;
      }
      if (tx.timestamp > maxTs) {
        maxTs = tx.timestamp;
      }
    }
  }
  b.simStartEpoch = minTs;
  b.simEndEpoch = maxTs;
  b.simStart = t_ns::fromEpochSeconds(minTs);
  b.simEnd = t_ns::fromEpochSeconds(maxTs);

  {
    const auto epochBytes = u64Decimal(static_cast<std::uint64_t>(maxTs));
    b.derivationRunId = hashedId<24>("RUN_", {"RUN", epochBytes.view()});
  }

  const auto ownerIdx = buildOwnerIndex(entities);

  // ── 2. Single sweep: alerts + CTRs + per-day velocity + aggregates ──
  b.alerts.reserve(postedTxns.size() / 8);
  b.ctrs.reserve(postedTxns.size() / 100);

  struct BurstKeyInfo {
    ent::Key account{};
    std::int64_t day = 0;
  };
  std::unordered_map<std::uint64_t, std::uint32_t> burstCounts;
  std::unordered_map<std::uint64_t, BurstKeyInfo> burstInfo;
  burstCounts.reserve(postedTxns.size() / 4);
  burstInfo.reserve(postedTxns.size() / 4);

  std::map<AcctPair, AggregateRow> flowAccum;
  std::map<AcctPair, AggregateRow> linkAccum;

  const std::int64_t cut30 = maxTs - 30 * t_ns::kSecondsPerDay;
  const std::int64_t cut90 = maxTs - 90 * t_ns::kSecondsPerDay;

  std::size_t idx = 1;
  for (const auto &tx : postedTxns) {
    const double amount = tx.amount;
    const auto onAcct = alertAccount(tx);
    const bool onValid = (onAcct.number != 0);
    const auto createdDate = t_ns::fromEpochSeconds(tx.timestamp);

    // ── Alert rules ──
    if (onValid && tx.fraud.flag != 0) {
      emitAlert(b, onAcct, Rule::fraudMlFlag, createdDate, idx);
    }
    if (onValid && amount >= 9000.0 && amount < 10000.0) {
      emitAlert(b, onAcct, Rule::highAmountBelowCtr, createdDate, idx);
    }

    if (onValid && amount >= 10000.0) {
      emitAlert(b, onAcct, Rule::cashCtrThreshold, createdDate, idx);

      const auto acctRendered = ::PhantomLedger::encoding::format(onAcct);
      CtrRecord c;
      c.onAccount = onAcct;
      c.filingDate = createdDate;
      c.amount = primitives::utils::roundMoney(amount);
      c.branchBucket = static_cast<std::uint32_t>((onAcct.number % 50U) + 1U);
      const auto dayBytes = u64Decimal(
          static_cast<std::uint64_t>(tx.timestamp / t_ns::kSecondsPerDay));
      c.tellerNum = static_cast<std::uint32_t>(
          (common::stableU64({"TLR", acctRendered.view(), dayBytes.view()}) %
           200ULL) +
          1ULL);
      c.id = makeCtrId(acctRendered.view(), idx);
      b.ctrs.push_back(std::move(c));
    }

    // ── Velocity-burst accumulator (resolved after the loop) ──
    if (onValid) {
      const std::int64_t day = tx.timestamp / t_ns::kSecondsPerDay;
      const auto packed =
          (static_cast<std::uint64_t>(day) << 24) ^
          (static_cast<std::uint64_t>(onAcct.number) << 8) ^
          (static_cast<std::uint64_t>(static_cast<std::uint8_t>(onAcct.role))
           << 4) ^
          static_cast<std::uint64_t>(static_cast<std::uint8_t>(onAcct.bank));
      burstCounts[packed]++;
      burstInfo[packed] = BurstKeyInfo{onAcct, day};
    }

    // ── FLOW_AGG (directed) and LINK_COMM (undirected) ──
    accumulate(flowAccum[AcctPair{tx.source, tx.target}], amount, tx.timestamp,
               cut30, cut90);
    const AcctPair linkKey = (tx.source < tx.target)
                                 ? AcctPair{tx.source, tx.target}
                                 : AcctPair{tx.target, tx.source};
    accumulate(linkAccum[linkKey], amount, tx.timestamp, cut30, cut90);

    ++idx;
  }

  // ── 3. VELOCITY_BURST resolution ──
  for (const auto &[key, count] : burstCounts) {
    if (count < 5) {
      continue;
    }
    const auto &info = burstInfo[key];
    // Day-as-idx so distinct days for the same account get distinct ids.
    emitAlert(b, info.account, Rule::velocityBurst,
              t_ns::fromEpochSeconds(info.day * t_ns::kSecondsPerDay),
              static_cast<std::size_t>(info.day));
  }

  // ── 4. Dispositions (one per alert) ──
  b.dispositions.reserve(b.alerts.size());
  for (std::size_t i = 0; i < b.alerts.size(); ++i) {
    auto &alert = b.alerts[i];
    const auto alertIdView = alert.id.view();

    DispositionRecord d;
    d.alertIndex = i;
    d.id = makeDispositionId(alertIdView);
    d.notesHash = makeHashHex({"NOTES", alertIdView});
    d.investigatorNum = static_cast<std::uint32_t>(
        (common::stableU64({"INV", alertIdView}) % 12ULL) + 1ULL);

    const auto lag = static_cast<int>(
        (common::stableU64({"LAG", alertIdView}) % 7ULL) + 1ULL);
    d.date = alert.createdDate + t_ns::Days{lag};

    switch (alert.rule) {
    case Rule::fraudMlFlag:
    case Rule::cashCtrThreshold:
      d.outcome = DispositionOutcome::escalated;
      d.confidence = 0.85;
      break;
    case Rule::highAmountBelowCtr:
    case Rule::velocityBurst:
      d.outcome = ((common::stableU64({"DECIDE", alertIdView}) % 8ULL) == 0ULL)
                      ? DispositionOutcome::escalated
                      : DispositionOutcome::closedFp;
      d.confidence = (d.outcome == DispositionOutcome::escalated) ? 0.6 : 0.4;
      break;
    case Rule::highRiskCounterparty:
      d.outcome = DispositionOutcome::closedNoAction;
      d.confidence = 0.5;
      break;
    }

    alert.status = alertStatusAfter(d.outcome);
    b.dispositions.push_back(std::move(d));
  }

  // ── 5. Cases (one per ring + one per solo fraudster) ──
  const auto &topology = entities.people.topology;
  const auto &memberStore = topology.memberStore;
  b.cases.reserve(topology.rings.size() + 64);

  for (const auto &ring : topology.rings) {
    CaseRecord c;
    c.kind = CaseKind::ring;
    c.ringOrPerson = ring.id;
    c.id = makeRingCaseId(ring.id);
    c.openedDate = b.simStart + t_ns::Days{static_cast<int>(ring.id % 30U)};
    c.caseSystem = kCaseSystems[ring.id % kCaseSystems.size()];

    const auto off = ring.members.offset;
    const auto end = off + ring.members.size;
    c.subjectPersons.reserve(ring.members.size);
    for (std::uint32_t j = off; j < end; ++j) {
      c.subjectPersons.push_back(memberStore[j]);
    }
    b.cases.push_back(std::move(c));
  }

  const auto &roster = entities.people.roster;
  for (ent::PersonId p = 1; p <= roster.count; ++p) {
    if (!roster.has(p, ent::person::Flag::soloFraud)) {
      continue;
    }
    CaseRecord c;
    c.kind = CaseKind::solo;
    c.ringOrPerson = p;
    c.id = makeSoloCaseId(p);
    c.openedDate = b.simStart + t_ns::Days{static_cast<int>(p % 60U)};
    c.caseSystem = kCaseSystems[p % kCaseSystems.size()];
    c.subjectPersons = {p};
    b.cases.push_back(std::move(c));
  }

  // ── 6. Case → alertIndices via subject-person→case reverse index ──
  std::unordered_map<ent::PersonId, std::vector<std::size_t>> caseByPerson;
  caseByPerson.reserve(b.cases.size() * 4);
  for (std::size_t ci = 0; ci < b.cases.size(); ++ci) {
    for (const auto p : b.cases[ci].subjectPersons) {
      caseByPerson[p].push_back(ci);
    }
  }

  for (std::size_t ai = 0; ai < b.alerts.size(); ++ai) {
    const auto owner = ownerIdx.at(b.alerts[ai].onAccount);
    if (owner == ent::invalidPerson) {
      continue;
    }
    const auto it = caseByPerson.find(owner);
    if (it == caseByPerson.end()) {
      continue;
    }
    for (const auto ci : it->second) {
      b.cases[ci].alertIndices.push_back(ai);
    }
  }
  for (auto &c : b.cases) {
    std::sort(c.alertIndices.begin(), c.alertIndices.end());
    c.alertIndices.erase(
        std::unique(c.alertIndices.begin(), c.alertIndices.end()),
        c.alertIndices.end());
  }

  // ── 7. Case → sarIndices via subject-person overlap ──
  for (std::size_t si = 0; si < sars.size(); ++si) {
    std::unordered_set<ent::PersonId> sarSubjects(
        sars[si].subjectPersonIds.begin(), sars[si].subjectPersonIds.end());
    for (auto &c : b.cases) {
      for (const auto p : c.subjectPersons) {
        if (sarSubjects.contains(p)) {
          c.sarIndices.push_back(si);
          break;
        }
      }
    }
  }

  // ── 8. EvidenceArtifacts (3 per case) ──
  b.evidence.reserve(b.cases.size() * 3);
  for (std::size_t ci = 0; ci < b.cases.size(); ++ci) {
    auto &c = b.cases[ci];
    const auto caseIdView = c.id.view();
    for (const auto &kind : kEvidenceKinds) {
      EvidenceRecord e;
      e.caseIndex = ci;
      e.artifactType = kind.artifactType;
      e.sourceSystem = kind.sourceSystem;
      e.id = makeEvidenceId(caseIdView, kind.artifactType);
      e.contentHash = makeHashHex({"HASH", caseIdView, kind.artifactType});
      e.createdAt = c.openedDate + t_ns::Days{1};
      c.evidenceIndices.push_back(b.evidence.size());
      b.evidence.push_back(std::move(e));
    }
  }

  // ── 9. Promoted txns ──
  std::vector<std::size_t> fraudTxnIdx;
  fraudTxnIdx.reserve(postedTxns.size() / 200);
  for (std::size_t i = 0; i < postedTxns.size(); ++i) {
    if (postedTxns[i].fraud.flag != 0) {
      fraudTxnIdx.push_back(i + 1);
    }
  }

  for (std::size_t ci = 0; ci < b.cases.size(); ++ci) {
    auto &c = b.cases[ci];
    std::unordered_set<ent::PersonId> subjects(c.subjectPersons.begin(),
                                               c.subjectPersons.end());
    const auto caseIdView = c.id.view();
    const auto promotedAt = c.openedDate + t_ns::Days{2};
    const auto ttlDate = c.openedDate + t_ns::Days{365 * 7};
    std::size_t emitted = 0;
    for (const auto txnIdx1 : fraudTxnIdx) {
      if (emitted >= 100) {
        break;
      }
      const auto &tx = postedTxns[txnIdx1 - 1];
      const auto srcOwner = ownerIdx.at(tx.source);
      const auto dstOwner = ownerIdx.at(tx.target);
      const bool touches =
          (srcOwner != ent::invalidPerson && subjects.contains(srcOwner)) ||
          (dstOwner != ent::invalidPerson && subjects.contains(dstOwner));
      if (!touches) {
        continue;
      }
      PromotedTxnRecord r;
      r.caseIndex = ci;
      r.txnIndex = txnIdx1;
      r.promotedAt = promotedAt;
      r.ttlDate = ttlDate;
      r.id = makePromotedTxnId(caseIdView, txnIdx1);
      c.promotedTxnIndices.push_back(b.promotedTxns.size());
      b.promotedTxns.push_back(std::move(r));
      ++emitted;
    }
  }

  // ── 10. Businesses (one per internal business account with owner) ──
  for (const auto &rec : entities.accounts.registry.records) {
    if ((rec.flags & ent::account::bit(ent::account::Flag::external)) != 0) {
      continue;
    }
    if (rec.id.role != ent::Role::business) {
      continue;
    }
    if (rec.owner == ent::invalidPerson) {
      continue;
    }

    const auto acctRendered = ::PhantomLedger::encoding::format(rec.id);

    BusinessRecord biz;
    biz.accountKey = rec.id;
    biz.owner = rec.owner;
    biz.id = makeBusinessId(acctRendered.view(), rec.owner);
    biz.stemIdx = static_cast<std::uint32_t>(
        common::stableU64({"STEM", biz.id.view()}) % kLegalStems.size());
    biz.numberSuffix =
        static_cast<std::uint32_t>((rec.id.number % 900U) + 100U);
    biz.entityType = kEntityTypes[common::stableU64({"TYPE", biz.id.view()}) %
                                  kEntityTypes.size()];
    biz.effectiveDate =
        b.simStart - t_ns::Days{static_cast<int>(rec.owner % 365U)};
    b.businesses.push_back(std::move(biz));
  }

  // ── 11. Flatten aggregate maps into sorted vectors ──
  b.flowAgg.reserve(flowAccum.size());
  for (auto &kv : flowAccum) {
    b.flowAgg.push_back({kv.first, kv.second});
  }
  std::sort(b.flowAgg.begin(), b.flowAgg.end(),
            [](const AggregateBucket &a, const AggregateBucket &c) {
              return a.pair < c.pair;
            });

  b.linkComm.reserve(linkAccum.size());
  for (auto &kv : linkAccum) {
    b.linkComm.push_back({kv.first, kv.second});
  }
  std::sort(b.linkComm.begin(), b.linkComm.end(),
            [](const AggregateBucket &a, const AggregateBucket &c) {
              return a.pair < c.pair;
            });

  return b;
}

} // namespace PhantomLedger::exporter::aml_txn_edges::derived
