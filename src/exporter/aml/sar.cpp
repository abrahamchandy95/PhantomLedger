#include "phantomledger/exporter/aml/sar.hpp"

#include "phantomledger/taxonomies/channels/names.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace PhantomLedger::exporter::aml::sar {

namespace {

namespace tx_ns = ::PhantomLedger::transactions;
namespace ent = ::PhantomLedger::entity;
namespace t_ns = ::PhantomLedger::time;

[[nodiscard]] double round2(double v) noexcept {
  return std::round(v * 100.0) / 100.0;
}

[[nodiscard]] std::unordered_map<ent::Key, double>
accountActivityAmounts(std::span<const tx_ns::Transaction> txns,
                       const std::unordered_set<ent::Key> &internalAccounts) {
  std::unordered_map<ent::Key, double> amounts;
  for (const auto &tx : txns) {
    if (internalAccounts.count(tx.source) != 0) {
      amounts[tx.source] += tx.amount;
    }
    if (internalAccounts.count(tx.target) != 0) {
      amounts[tx.target] += tx.amount;
    }
  }
  return amounts;
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

} // namespace

std::vector<SarRecord>
generateSars(const ent::person::Roster &peopleRoster,
             const ent::person::Topology &topology,
             const ent::account::Registry &accounts,
             const ent::account::Ownership &ownership,
             std::span<const tx_ns::Transaction> finalTxns) {

  std::vector<SarRecord> out;

  std::unordered_set<ent::Key> internalAccounts;
  internalAccounts.reserve(accounts.records.size());
  for (const auto &rec : accounts.records) {
    if ((rec.flags & ent::account::bit(ent::account::Flag::external)) == 0) {
      internalAccounts.insert(rec.id);
    }
  }

  std::unordered_map<std::uint32_t, std::vector<tx_ns::Transaction>>
      fraudByRing;
  std::vector<tx_ns::Transaction> fraudSolo;
  for (const auto &tx : finalTxns) {
    if (tx.fraud.flag == 0) {
      continue;
    }
    if (tx.fraud.ringId.has_value()) {
      fraudByRing[*tx.fraud.ringId].push_back(tx);
    } else {
      fraudSolo.push_back(tx);
    }
  }

  for (const auto &ring : topology.rings) {
    const auto it = fraudByRing.find(ring.id);
    if (it == fraudByRing.end() || it->second.empty()) {
      continue;
    }
    const auto &ringTxns = it->second;

    double total = 0.0;
    std::int64_t firstTs = ringTxns.front().timestamp;
    std::int64_t lastTs = ringTxns.front().timestamp;
    for (const auto &tx : ringTxns) {
      total += tx.amount;
      if (tx.timestamp < firstTs) {
        firstTs = tx.timestamp;
      } else if (tx.timestamp > lastTs) {
        lastTs = tx.timestamp;
      }
    }
    const auto activityStart = t_ns::fromEpochSeconds(firstTs);
    const auto activityEnd = t_ns::fromEpochSeconds(lastTs);
    const auto filingDate = activityEnd + t_ns::Days{30};

    const auto violation = violationTypeForRing(ringTxns);

    SarRecord sar;
    sar.sarId = sarRingId(ring.id);
    sar.filingDate = filingDate;
    sar.amountInvolved = round2(total);
    sar.activityStart = activityStart;
    sar.activityEnd = activityEnd;
    sar.violationType = violation;

    // Subjects: ring fraud personas first, then mules.
    const auto fraudSlice = sliceOf(topology.fraudStore, ring.frauds);
    const auto muleSlice = sliceOf(topology.muleStore, ring.mules);
    sar.subjectPersonIds.reserve(fraudSlice.size() + muleSlice.size());
    sar.subjectRoles.reserve(fraudSlice.size() + muleSlice.size());
    for (const auto pid : fraudSlice) {
      sar.subjectPersonIds.push_back(pid);
      sar.subjectRoles.emplace_back("subject");
    }
    for (const auto pid : muleSlice) {
      sar.subjectPersonIds.push_back(pid);
      sar.subjectRoles.emplace_back("beneficiary");
    }

    // Covered accounts — sorted Keys with rounded activity totals.
    auto activity = accountActivityAmounts(ringTxns, internalAccounts);
    std::vector<ent::Key> coveredKeys;
    coveredKeys.reserve(activity.size());
    for (const auto &kv : activity) {
      coveredKeys.push_back(kv.first);
    }
    std::sort(coveredKeys.begin(), coveredKeys.end());
    sar.coveredAccountIds = std::move(coveredKeys);
    sar.coveredAmounts.reserve(sar.coveredAccountIds.size());
    for (const auto &k : sar.coveredAccountIds) {
      sar.coveredAmounts.push_back(round2(activity[k]));
    }

    out.push_back(std::move(sar));
  }

  // ────────── Solo fraudster SARs ──────────

  // Collect solo-fraud personIds in deterministic order.
  std::vector<ent::PersonId> soloIds;
  for (ent::PersonId p = 1; p <= peopleRoster.count; ++p) {
    if (peopleRoster.has(p, ent::person::Flag::soloFraud)) {
      soloIds.push_back(p);
    }
  }
  std::sort(soloIds.begin(), soloIds.end());

  for (const auto pid : soloIds) {

    if (pid == 0 || pid > peopleRoster.count) {
      continue;
    }
    const auto offset = ownership.byPersonOffset[pid - 1];
    const auto end = (pid < ownership.byPersonOffset.size())
                         ? ownership.byPersonOffset[pid]
                         : ownership.byPersonIndex.size();
    if (offset == end) {
      continue;
    }

    std::unordered_set<ent::Key> personAccts;
    personAccts.reserve(end - offset);
    for (auto i = offset; i < end; ++i) {
      const auto regIdx = ownership.byPersonIndex[i];
      personAccts.insert(accounts.records[regIdx].id);
    }

    std::vector<tx_ns::Transaction> myTxns;
    for (const auto &tx : fraudSolo) {
      if (personAccts.count(tx.source) != 0 ||
          personAccts.count(tx.target) != 0) {
        myTxns.push_back(tx);
      }
    }
    if (myTxns.empty()) {
      continue;
    }

    double total = 0.0;
    std::int64_t firstTs = myTxns.front().timestamp;
    std::int64_t lastTs = myTxns.front().timestamp;
    for (const auto &tx : myTxns) {
      total += tx.amount;
      if (tx.timestamp < firstTs) {
        firstTs = tx.timestamp;
      } else if (tx.timestamp > lastTs) {
        lastTs = tx.timestamp;
      }
    }
    const auto activityStart = t_ns::fromEpochSeconds(firstTs);
    const auto activityEnd = t_ns::fromEpochSeconds(lastTs);
    const auto filingDate = activityEnd + t_ns::Days{30};

    SarRecord sar;
    sar.sarId = sarSoloId(pid);
    sar.filingDate = filingDate;
    sar.amountInvolved = round2(total);
    sar.activityStart = activityStart;
    sar.activityEnd = activityEnd;
    sar.violationType = "suspicious_activity";
    sar.subjectPersonIds.push_back(pid);
    sar.subjectRoles.emplace_back("subject");

    auto activity = accountActivityAmounts(myTxns, personAccts);
    std::vector<ent::Key> coveredKeys;
    coveredKeys.reserve(activity.size());
    for (const auto &kv : activity) {
      coveredKeys.push_back(kv.first);
    }
    std::sort(coveredKeys.begin(), coveredKeys.end());
    sar.coveredAccountIds = std::move(coveredKeys);
    sar.coveredAmounts.reserve(sar.coveredAccountIds.size());
    for (const auto &k : sar.coveredAccountIds) {
      sar.coveredAmounts.push_back(round2(activity[k]));
    }

    out.push_back(std::move(sar));
  }

  (void)peopleRoster;
  return out;
}

} // namespace PhantomLedger::exporter::aml::sar
