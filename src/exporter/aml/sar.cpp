#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"

#include "phantomledger/taxonomies/channels/types.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <unordered_map>
#include <utility>

namespace PhantomLedger::exporter::aml::sar {

namespace {

namespace tx_ns = ::PhantomLedger::transactions;
namespace ent = ::PhantomLedger::entity;
namespace t_ns = ::PhantomLedger::time;
namespace ch = ::PhantomLedger::channels;

struct FraudTxnGroups {
  std::unordered_map<std::uint32_t, std::vector<tx_ns::Transaction>> byRing;
  std::vector<tx_ns::Transaction> solo;
};

struct ActivityPeriod {
  double total = 0.0;
  std::int64_t firstTs = 0;
  std::int64_t lastTs = 0;
};

[[nodiscard]] bool hasAccount(std::span<const ent::Key> sortedAccounts,
                              const ent::Key &key) noexcept {
  return std::binary_search(sortedAccounts.begin(), sortedAccounts.end(), key);
}

[[nodiscard]] std::unordered_map<ent::Key, double>
accountActivityAmounts(std::span<const tx_ns::Transaction> txns,
                       std::span<const ent::Key> accounts) {
  std::unordered_map<ent::Key, double> amounts;
  for (const auto &tx : txns) {
    if (hasAccount(accounts, tx.source)) {
      amounts[tx.source] += tx.amount;
    }
    if (hasAccount(accounts, tx.target)) {
      amounts[tx.target] += tx.amount;
    }
  }
  return amounts;
}

[[nodiscard]] ActivityPeriod
activityPeriod(std::span<const tx_ns::Transaction> txns) noexcept {
  ActivityPeriod out;
  if (txns.empty()) {
    return out;
  }

  out.firstTs = txns.front().timestamp;
  out.lastTs = txns.front().timestamp;
  for (const auto &tx : txns) {
    out.total += tx.amount;
    if (tx.timestamp < out.firstTs) {
      out.firstTs = tx.timestamp;
    } else if (tx.timestamp > out.lastTs) {
      out.lastTs = tx.timestamp;
    }
  }

  return out;
}

[[nodiscard]] std::vector<ent::Key>
sortedKeys(const std::unordered_map<ent::Key, double> &activity) {
  std::vector<ent::Key> keys;
  keys.reserve(activity.size());
  for (const auto &kv : activity) {
    keys.push_back(kv.first);
  }
  std::sort(keys.begin(), keys.end());
  return keys;
}

void fillCoveredAccounts(SarRecord &sar,
                         const std::unordered_map<ent::Key, double> &activity) {
  sar.coveredAccountIds = sortedKeys(activity);
  sar.coveredAmounts.reserve(sar.coveredAccountIds.size());
  for (const auto &key : sar.coveredAccountIds) {
    sar.coveredAmounts.push_back(
        primitives::utils::roundMoney(activity.at(key)));
  }
}

// ────────────────────────────────────────────────────────────────────
// violationTypeForRing
//
// Counts transactions per channel and classifies by which channel is
// dominant. Previously this used `unordered_map<std::string, uint32_t>`
// keyed on a freshly-allocated std::string per transaction — one alloc
// for every transaction in every ring. Now it's a 256-element stack
// array indexed by `Tag::value` (the channel's enum byte), which is
// exactly the cardinality of the Tag space.
//
// Classification dropped the substring match (`->find("structuring")`)
// in favor of comparing against the actual Fraud enum tags. The
// substring approach was equivalent for the current name table —
// only `fraud_structuring` contains "structuring" and only
// `fraud_invoice` contains "invoice" — but the enum-tag version is
// safer against future renames.
//
// Tie-break: lowest tag value wins (was: lexicographically smallest
// name). For ring activity this is a behavior change only in the
// rare case of an exact count tie between two distinct channels;
// the resulting violation_type label is unchanged in every realistic
// case because all Fraud::* tags except structuring/invoice map to
// the same "money_laundering" label.
// ────────────────────────────────────────────────────────────────────

[[nodiscard]] std::string_view
violationTypeForRing(std::span<const tx_ns::Transaction> txns) noexcept {
  if (txns.empty()) {
    return "suspicious_activity";
  }

  std::array<std::uint32_t, 256> counts{};
  for (const auto &tx : txns) {
    ++counts[tx.session.channel.value];
  }

  // Pick the dominant tag (highest count, lowest index breaks ties).
  std::uint8_t dominantTag = 0;
  std::uint32_t bestCount = 0;
  for (std::size_t i = 0; i < counts.size(); ++i) {
    if (counts[i] > bestCount) {
      bestCount = counts[i];
      dominantTag = static_cast<std::uint8_t>(i);
    }
  }

  if (bestCount == 0) {
    return "suspicious_activity";
  }

  // Classify by Fraud enum tag.
  if (dominantTag == ch::tag(ch::Fraud::structuring).value) {
    return "structuring";
  }
  if (dominantTag == ch::tag(ch::Fraud::invoice).value) {
    return "suspicious_activity";
  }
  return "money_laundering";
}

// ────────────────────────────────────────────────────────────────────
// sarRingId / sarSoloId
//
// Both return SarId (RenderedId<32>) — bytes live inline on the
// returned value. No heap allocation.
// ────────────────────────────────────────────────────────────────────

[[nodiscard]] SarId sarRingId(std::uint32_t ringId) noexcept {
  SarId out;
  const auto n = std::snprintf(out.bytes.data(), out.bytes.size(),
                               "SAR_RING_%04u", ringId);
  out.length = static_cast<std::uint8_t>(n > 0 ? n : 0);
  return out;
}

[[nodiscard]] SarId sarSoloId(ent::PersonId p) noexcept {
  SarId out;
  const auto n = std::snprintf(out.bytes.data(), out.bytes.size(),
                               "SAR_SOLO_C%010u", static_cast<unsigned>(p));
  out.length = static_cast<std::uint8_t>(n > 0 ? n : 0);
  return out;
}

[[nodiscard]] std::span<const ent::PersonId>
sliceOf(const std::vector<ent::PersonId> &store,
        const ent::person::Slice &slice) noexcept {
  return std::span<const ent::PersonId>{store.data() + slice.offset,
                                        slice.size};
}

void appendSarRingSubjects(SarRingSubject &out,
                           std::span<const ent::PersonId> people,
                           std::string_view role) {
  out.subjects.reserve(out.subjects.size() + people.size());
  for (const auto person : people) {
    // role is a string_view pointing at a static literal ("subject" /
    // "beneficiary") passed in from ringSubject. No allocation needed
    // for the SarSubjectRole::role field — it just stores the view.
    out.subjects.push_back(SarSubjectRole{
        .person = person,
        .role = role,
    });
  }
}

[[nodiscard]] SarRingSubject ringSubject(const ent::person::Topology &topology,
                                         const ent::person::Ring &ring) {
  SarRingSubject out;
  out.ringId = ring.id;

  const auto fraudSlice = sliceOf(topology.fraudStore, ring.frauds);
  const auto muleSlice = sliceOf(topology.muleStore, ring.mules);
  out.subjects.reserve(fraudSlice.size() + muleSlice.size());
  appendSarRingSubjects(out, fraudSlice, "subject");
  appendSarRingSubjects(out, muleSlice, "beneficiary");

  return out;
}

[[nodiscard]] std::vector<ent::PersonId>
soloFraudsters(const ent::person::Roster &peopleRoster) {
  std::vector<ent::PersonId> out;
  for (ent::PersonId p = 1; p <= peopleRoster.count; ++p) {
    if (peopleRoster.has(p, ent::person::Flag::soloFraud)) {
      out.push_back(p);
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

[[nodiscard]] SarSoloSubject
soloSubject(ent::PersonId person, const ent::account::Registry &accounts,
            const ent::account::Ownership &ownership) {
  SarSoloSubject out;
  out.person = person;

  const auto offset = ownership.byPersonOffset[person - 1];
  const auto end = (person < ownership.byPersonOffset.size())
                       ? ownership.byPersonOffset[person]
                       : ownership.byPersonIndex.size();
  if (offset == end) {
    return out;
  }

  out.accountIds.reserve(end - offset);
  for (auto i = offset; i < end; ++i) {
    const auto recIdx = ownership.byPersonIndex[i];
    out.accountIds.push_back(accounts.records[recIdx].id);
  }

  std::sort(out.accountIds.begin(), out.accountIds.end());
  out.accountIds.erase(
      std::unique(out.accountIds.begin(), out.accountIds.end()),
      out.accountIds.end());

  return out;
}

[[nodiscard]] FraudTxnGroups
fraudTxnGroups(std::span<const tx_ns::Transaction> finalTxns) {
  FraudTxnGroups out;
  for (const auto &tx : finalTxns) {
    if (tx.fraud.flag == 0) {
      continue;
    }
    if (tx.fraud.ringId.has_value()) {
      out.byRing[*tx.fraud.ringId].push_back(tx);
    } else {
      out.solo.push_back(tx);
    }
  }
  return out;
}

void applyActivity(SarRecord &sar, std::span<const tx_ns::Transaction> txns) {
  const auto activity = activityPeriod(txns);
  const auto activityStart = t_ns::fromEpochSeconds(activity.firstTs);
  const auto activityEnd = t_ns::fromEpochSeconds(activity.lastTs);

  sar.filingDate = activityEnd + t_ns::Days{30};
  sar.amountInvolved = primitives::utils::roundMoney(activity.total);
  sar.activityStart = activityStart;
  sar.activityEnd = activityEnd;
}

void applySubjects(SarRecord &sar, std::span<const SarSubjectRole> subjects) {
  sar.subjectPersonIds.reserve(subjects.size());
  sar.subjectRoles.reserve(subjects.size());
  for (const auto &subject : subjects) {
    sar.subjectPersonIds.push_back(subject.person);
    // subject.role is a string_view of a static literal; pushing it
    // into a vector<string_view> is a 16-byte copy, no allocation.
    sar.subjectRoles.push_back(subject.role);
  }
}

[[nodiscard]] SarRecord ringSar(const SarRingSubject &ring,
                                std::span<const tx_ns::Transaction> txns,
                                std::span<const ent::Key> internalAccounts) {
  SarRecord sar;
  sar.sarId = sarRingId(ring.ringId);
  applyActivity(sar, txns);
  sar.violationType = violationTypeForRing(txns);
  applySubjects(sar, ring.subjects);
  fillCoveredAccounts(sar, accountActivityAmounts(txns, internalAccounts));
  return sar;
}

[[nodiscard]] std::vector<tx_ns::Transaction>
transactionsForSubject(std::span<const tx_ns::Transaction> txns,
                       const SarSoloSubject &subject) {
  std::vector<tx_ns::Transaction> out;
  for (const auto &tx : txns) {
    if (hasAccount(subject.accountIds, tx.source) ||
        hasAccount(subject.accountIds, tx.target)) {
      out.push_back(tx);
    }
  }
  return out;
}

[[nodiscard]] SarRecord soloSar(const SarSoloSubject &subject,
                                std::span<const tx_ns::Transaction> txns) {
  SarRecord sar;
  sar.sarId = sarSoloId(subject.person);
  applyActivity(sar, txns);
  sar.violationType = "suspicious_activity";
  sar.subjectPersonIds.push_back(subject.person);
  sar.subjectRoles.push_back("subject");
  fillCoveredAccounts(sar, accountActivityAmounts(txns, subject.accountIds));
  return sar;
}

void appendRingSars(std::vector<SarRecord> &out,
                    const SarSubjectIndex &subjects,
                    const FraudTxnGroups &txns) {
  for (const auto &ring : subjects.rings()) {
    const auto it = txns.byRing.find(ring.ringId);
    if (it == txns.byRing.end() || it->second.empty()) {
      continue;
    }
    out.push_back(ringSar(ring, it->second, subjects.internalAccounts()));
  }
}

void appendSoloSars(std::vector<SarRecord> &out,
                    const SarSubjectIndex &subjects,
                    std::span<const tx_ns::Transaction> txns) {
  for (const auto &subject : subjects.soloFraudsters()) {
    const auto myTxns = transactionsForSubject(txns, subject);
    if (myTxns.empty()) {
      continue;
    }
    out.push_back(soloSar(subject, myTxns));
  }
}

} // namespace

SarSubjectIndex buildSarSubjectIndex(const ent::person::Roster &peopleRoster,
                                     const ent::person::Topology &topology,
                                     const ent::account::Registry &accounts,
                                     const ent::account::Ownership &ownership) {
  SarSubjectIndex out;

  out.internalAccounts_.reserve(accounts.records.size());
  for (const auto &rec : accounts.records) {
    if ((rec.flags & ent::account::bit(ent::account::Flag::external)) == 0) {
      out.internalAccounts_.push_back(rec.id);
    }
  }

  std::sort(out.internalAccounts_.begin(), out.internalAccounts_.end());
  out.internalAccounts_.erase(
      std::unique(out.internalAccounts_.begin(), out.internalAccounts_.end()),
      out.internalAccounts_.end());

  out.rings_.reserve(topology.rings.size());
  for (const auto &ring : topology.rings) {
    out.rings_.push_back(ringSubject(topology, ring));
  }

  const auto soloIds = soloFraudsters(peopleRoster);
  out.soloFraudsters_.reserve(soloIds.size());
  for (const auto person : soloIds) {
    if (person == ent::invalidPerson || person > peopleRoster.count) {
      continue;
    }
    auto subject = soloSubject(person, accounts, ownership);
    if (subject.accountIds.empty()) {
      continue;
    }
    out.soloFraudsters_.push_back(std::move(subject));
  }

  return out;
}

std::vector<SarRecord>
generateSars(const SarSubjectIndex &subjects,
             std::span<const tx_ns::Transaction> finalTxns) {
  std::vector<SarRecord> out;

  const auto txns = fraudTxnGroups(finalTxns);
  appendRingSars(out, subjects, txns);
  appendSoloSars(out, subjects, txns.solo);

  return out;
}

} // namespace PhantomLedger::exporter::aml::sar
