#include "phantomledger/exporter/aml/sar.hpp"

#include "phantomledger/taxonomies/channels/names.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <utility>

namespace PhantomLedger::exporter::aml::sar {

namespace {

namespace tx_ns = ::PhantomLedger::transactions;
namespace ent = ::PhantomLedger::entity;
namespace t_ns = ::PhantomLedger::time;

struct FraudTxnGroups {
  std::unordered_map<std::uint32_t, std::vector<tx_ns::Transaction>> byRing;
  std::vector<tx_ns::Transaction> solo;
};

struct ActivityPeriod {
  double total = 0.0;
  std::int64_t firstTs = 0;
  std::int64_t lastTs = 0;
};

[[nodiscard]] double round2(double v) noexcept {
  return std::round(v * 100.0) / 100.0;
}

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
    sar.coveredAmounts.push_back(round2(activity.at(key)));
  }
}

[[nodiscard]] std::string
violationTypeForRing(std::span<const tx_ns::Transaction> txns) {
  if (txns.empty()) {
    return "suspicious_activity";
  }
  std::unordered_map<std::string, std::uint32_t> counts;
  for (const auto &tx : txns) {
    counts[std::string{::PhantomLedger::channels::name(tx.session.channel)}] +=
        1;
  }
  if (counts.empty()) {
    return "suspicious_activity";
  }

  const std::string *dominant = nullptr;
  std::uint32_t bestCount = 0;
  for (const auto &[name, count] : counts) {
    if (dominant == nullptr || count > bestCount ||
        (count == bestCount && name < *dominant)) {
      dominant = &name;
      bestCount = count;
    }
  }

  if (dominant == nullptr) {
    return "suspicious_activity";
  }
  if (dominant->find("structuring") != std::string::npos) {
    return "structuring";
  }
  if (dominant->find("invoice") != std::string::npos) {
    return "suspicious_activity";
  }
  return "money_laundering";
}

[[nodiscard]] std::string sarRingId(std::uint32_t ringId) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "SAR_RING_%04u", ringId);
  return std::string{buf};
}

[[nodiscard]] std::string sarSoloId(ent::PersonId p) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "SAR_SOLO_C%010u", static_cast<unsigned>(p));
  return std::string{buf};
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
    out.subjects.push_back(SarSubjectRole{
        .person = person,
        .role = std::string{role},
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
  sar.amountInvolved = round2(activity.total);
  sar.activityStart = activityStart;
  sar.activityEnd = activityEnd;
}

void applySubjects(SarRecord &sar, std::span<const SarSubjectRole> subjects) {
  sar.subjectPersonIds.reserve(subjects.size());
  sar.subjectRoles.reserve(subjects.size());
  for (const auto &subject : subjects) {
    sar.subjectPersonIds.push_back(subject.person);
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
  sar.subjectRoles.emplace_back("subject");
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
