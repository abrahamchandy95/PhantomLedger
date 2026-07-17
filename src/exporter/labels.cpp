#include "phantomledger/exporter/labels.hpp"

#include "phantomledger/exporter/common/render.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/taxonomies/fraud/names.hpp"

#include <algorithm>
#include <span>
#include <unordered_map>

namespace PhantomLedger::exporter::labels {

namespace {

namespace ch = ::PhantomLedger::channels;
namespace fr = ::PhantomLedger::fraud;
namespace ent = ::PhantomLedger::entity;
namespace tx_ns = ::PhantomLedger::transactions;
namespace time_ns = ::PhantomLedger::time;

[[nodiscard]] std::optional<fr::Typology>
mapFraudChannel(ch::Fraud channel) noexcept {
  switch (channel) {
  case ch::Fraud::classic:
    return fr::Typology::classic;
  case ch::Fraud::cycle:
    return fr::Typology::cycle;
  case ch::Fraud::layeringIn:
  case ch::Fraud::layeringHop:
  case ch::Fraud::layeringOut:
    return fr::Typology::layering;
  case ch::Fraud::funnelIn:
  case ch::Fraud::funnelOut:
    return fr::Typology::funnel;
  case ch::Fraud::structuring:
    return fr::Typology::structuring;
  case ch::Fraud::invoice:
    return fr::Typology::invoice;
  case ch::Fraud::muleIn:
  case ch::Fraud::muleForward:
    return fr::Typology::mule;
  case ch::Fraud::scatterGatherSplit:
  case ch::Fraud::scatterGatherMerge:
    return fr::Typology::scatterGather;
  case ch::Fraud::bipartite:
    return fr::Typology::bipartite;
  }
  return std::nullopt;
}

[[nodiscard]] bool isFraudChannelValue(std::uint8_t value) noexcept {
  return value >= static_cast<std::uint8_t>(ch::Fraud::classic) &&
         value <= static_cast<std::uint8_t>(ch::Fraud::bipartite);
}

void sortByTimestamp(std::vector<tx_ns::Transaction> &txns) {
  std::sort(txns.begin(), txns.end(),
            [](const tx_ns::Transaction &a, const tx_ns::Transaction &b) {
              return a.timestamp < b.timestamp;
            });
}

[[nodiscard]] fr::Typology
dominantTypology(const std::vector<tx_ns::Transaction> &txns) noexcept {
  std::array<std::uint32_t, fr::kTypologyCount> counts{};
  for (const auto &tx : txns) {
    const auto t = typologyForChannel(tx.session.channel);
    if (!t.has_value()) {
      continue;
    }
    ++counts[static_cast<std::size_t>(*t)];
  }
  std::size_t bestIdx = 0;
  std::uint32_t bestCount = 0;
  for (std::size_t i = 0; i < counts.size(); ++i) {
    if (counts[i] > bestCount) {
      bestCount = counts[i];
      bestIdx = i;
    }
  }
  return static_cast<fr::Typology>(bestIdx);
}

[[nodiscard]] ChainRow summarizeChain(std::uint32_t chainId,
                                      std::vector<tx_ns::Transaction> &txns) {
  sortByTimestamp(txns);
  const auto &first = txns.front();
  const auto &last = txns.back();
  const double principal = first.amount;
  const double finalAmount = last.amount;
  const double haircut =
      (principal > 0.0) ? (1.0 - (finalAmount / principal)) : 0.0;
  return ChainRow{
      .id = "chain_" + std::to_string(chainId),
      .chainId = chainId,
      .ringId = first.fraud.ringId,
      .typology = dominantTypology(txns),
      .numHops = static_cast<std::uint32_t>(txns.size()),
      .principal = principal,
      .finalAmount = finalAmount,
      .totalHaircut = haircut,
      .startTs = first.timestamp,
      .endTs = last.timestamp,
      .durationSeconds = last.timestamp - first.timestamp,
  };
}

[[nodiscard]] std::span<const ent::PersonId>
sliceView(const std::vector<ent::PersonId> &store,
          ent::person::Slice slice) noexcept {
  return std::span<const ent::PersonId>(store.data() + slice.offset,
                                        slice.size);
}

void collectShellsForRing(ShellInputs in, const ent::person::Ring &ring,
                          std::vector<ShellAccountRow> &out) {
  const auto persons = sliceView(in.topology.fraudStore, ring.frauds);
  for (const auto person : persons) {
    if (person == ent::invalidPerson) {
      continue;
    }
    const auto recIx = in.ownership.primaryIndex(person);
    const auto &rec = in.registry.records[recIx];
    if (!ent::account::hasFlag(rec.flags, ent::account::Flag::shell)) {
      continue;
    }
    out.push_back(ShellAccountRow{
        .accountId = rec.id,
        .ringId = ring.id,
    });
  }
}

// fraud-audit-2026-07 F2: the score is a derived pass-through statistic,
// not the constant 1.0. Every input is an order-insensitive aggregate.
[[nodiscard]] double shellScoreOf(const ShellAggregates &agg) noexcept {
  const double high = std::max(agg.inflow, agg.outflow);
  const double passThrough =
      std::min(agg.inflow, agg.outflow) / std::max(high, 1e-9);
  const double total = static_cast<double>(agg.totalCount);
  const double organicShare =
      (total - static_cast<double>(agg.fraudCount)) / std::max(total, 1.0);
  return passThrough * (1.0 - organicShare);
}

} // namespace

std::optional<fr::Typology> typologyForChannel(ch::Tag channel) noexcept {
  if (!isFraudChannelValue(channel.value)) {
    return std::nullopt;
  }
  return mapFraudChannel(static_cast<ch::Fraud>(channel.value));
}

void accumulateChainTxn(ChainGroups &groups, const tx_ns::Transaction &tx) {
  if (!tx.fraud.chainId.has_value()) {
    return;
  }
  groups[*tx.fraud.chainId].push_back(tx);
}

std::vector<ChainRow> finalizeChains(ChainGroups &groups) {
  std::vector<ChainRow> rows;
  rows.reserve(groups.size());
  for (auto &[chainId, txns] : groups) {
    if (txns.empty()) {
      continue;
    }
    rows.push_back(summarizeChain(chainId, txns));
  }
  std::sort(rows.begin(), rows.end(), [](const ChainRow &a, const ChainRow &b) {
    return a.chainId < b.chainId;
  });
  return rows;
}

std::vector<ChainRow>
buildChains(std::span<const tx_ns::Transaction> postedTxns) {
  ChainGroups groups;
  for (const auto &tx : postedTxns) {
    accumulateChainTxn(groups, tx);
  }
  return finalizeChains(groups);
}

ShellStats initShellStats(ShellInputs in) {
  ShellStats stats;
  stats.candidates.reserve(in.topology.rings.size() * 2);
  for (const auto &ring : in.topology.rings) {
    collectShellsForRing(in, ring, stats.candidates);
  }
  std::sort(stats.candidates.begin(), stats.candidates.end(),
            [](const ShellAccountRow &a, const ShellAccountRow &b) {
              return a.accountId.number < b.accountId.number;
            });
  stats.byAccount.reserve(stats.candidates.size());
  for (const auto &row : stats.candidates) {
    stats.byAccount.try_emplace(row.accountId);
  }
  return stats;
}

void accumulateShellTxn(ShellStats &stats, const tx_ns::Transaction &tx) {
  if (stats.byAccount.empty()) {
    return;
  }
  const bool isFraud = tx.fraud.flag != 0;
  const auto touch = [&](const ent::Key &account, bool inbound) {
    const auto it = stats.byAccount.find(account);
    if (it == stats.byAccount.end()) {
      return;
    }
    auto &agg = it->second;
    (inbound ? agg.inflow : agg.outflow) += tx.amount;
    if (agg.totalCount == 0) {
      agg.firstTs = tx.timestamp;
      agg.lastTs = tx.timestamp;
    } else {
      agg.firstTs = std::min(agg.firstTs, tx.timestamp);
      agg.lastTs = std::max(agg.lastTs, tx.timestamp);
    }
    ++agg.totalCount;
    if (isFraud) {
      ++agg.fraudCount;
    }
  };
  touch(tx.source, /*inbound=*/false);
  touch(tx.target, /*inbound=*/true);
}

std::vector<ShellAccountRow> finalizeShells(const ShellStats &stats) {
  std::vector<ShellAccountRow> rows = stats.candidates;
  for (auto &row : rows) {
    // Every candidate's slot is seeded by initShellStats; an absent
    // entry means zero activity and keeps the dormant 0.0 default.
    const auto it = stats.byAccount.find(row.accountId);
    if (it == stats.byAccount.end()) {
      continue;
    }
    row.shellScore = shellScoreOf(it->second);
  }
  return rows;
}

std::vector<ShellAccountRow>
buildShells(ShellInputs in, std::span<const tx_ns::Transaction> postedTxns) {
  auto stats = initShellStats(in);
  for (const auto &tx : postedTxns) {
    accumulateShellTxn(stats, tx);
  }
  return finalizeShells(stats);
}

void writeChainRows(csv::Writer &w, std::span<const ChainRow> rows) {
  for (const auto &row : rows) {
    w.cell(row.id).cell(row.chainId);
    if (row.ringId.has_value()) {
      w.cell(*row.ringId);
    } else {
      w.cellEmpty();
    }
    w.cell(fr::name(row.typology))
        .cell(row.numHops)
        .cell(primitives::utils::roundMoney(row.principal))
        .cell(primitives::utils::roundMoney(row.finalAmount))
        .cell(primitives::utils::roundMoney(row.totalHaircut))
        .cell(time_ns::formatTimestamp(time_ns::fromEpochSeconds(row.startTs)))
        .cell(time_ns::formatTimestamp(time_ns::fromEpochSeconds(row.endTs)))
        .cell(row.durationSeconds);
    w.endRow();
  }
}

void writeShellAccountRows(csv::Writer &w,
                           std::span<const ShellAccountRow> rows) {
  for (const auto &row : rows) {
    w.cell(common::renderKey(row.accountId));
    if (row.ringId.has_value()) {
      w.cell(*row.ringId);
    } else {
      w.cellEmpty();
    }
    w.cell(primitives::utils::roundMoney(row.shellScore));
    w.endRow();
  }
}

void writeTransactionChainLabelRows(
    csv::Writer &w, std::span<const tx_ns::Transaction> postedTxns,
    std::size_t &nextIndex1) {
  for (const auto &tx : postedTxns) {
    if (!tx.fraud.chainId.has_value()) {
      ++nextIndex1;
      continue;
    }
    w.cell(static_cast<std::uint32_t>(nextIndex1)).cell(*tx.fraud.chainId);
    if (tx.fraud.ringId.has_value()) {
      w.cell(*tx.fraud.ringId);
    } else {
      w.cellEmpty();
    }
    w.endRow();
    ++nextIndex1;
  }
}

void writeTransactionChainLabelRows(
    csv::Writer &w, std::span<const tx_ns::Transaction> postedTxns) {
  std::size_t idx = 1;
  writeTransactionChainLabelRows(w, postedTxns, idx);
}

} // namespace PhantomLedger::exporter::labels
