#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/fraud/types.hpp"
#include "phantomledger/transactions/record.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::exporter::labels {

namespace headers {

inline constexpr std::array<std::string_view, 11> kChain{
    "id",       "chain_id",  "ring_id",          "typology",
    "num_hops", "principal", "final_amount",     "total_haircut",
    "start_ts", "end_ts",    "duration_seconds",
};

inline constexpr std::array<std::string_view, 3> kShellAccount{
    "account_id",
    "ring_id",
    "shell_score",
};

inline constexpr std::array<std::string_view, 3> kTransactionChainLabel{
    "transaction_id",
    "chain_id",
    "ring_id",
};

} // namespace headers

struct ChainRow {
  std::string id;
  std::uint32_t chainId = 0;
  std::optional<std::uint32_t> ringId;
  fraud::Typology typology = fraud::Typology::classic;
  std::uint32_t numHops = 0;
  double principal = 0.0;
  double finalAmount = 0.0;
  double totalHaircut = 0.0;
  std::int64_t startTs = 0;
  std::int64_t endTs = 0;
  std::int64_t durationSeconds = 0;
};

struct ShellAccountRow {
  entity::Key accountId;
  std::optional<std::uint32_t> ringId;
  double shellScore = 0.0;
};

struct ShellInputs {
  const entity::account::Registry &registry;
  const entity::account::Ownership &ownership;
  const entity::person::Topology &topology;
};

[[nodiscard]] std::optional<fraud::Typology>
typologyForChannel(channels::Tag channel) noexcept;

// Chain rows grouped by chain id, retained as COPIES in corpus order.
// Chain rows are a subset of the fraud rows — fraud-scale retention,
// never transaction-scale — which is what makes chain labeling
// windowed-safe. Both engines share the same per-row accumulation and
// the same finalization, so the ChainRow output is byte-identical.
using ChainGroups =
    std::unordered_map<std::uint32_t, std::vector<transactions::Transaction>>;

// Per-row accumulation (windowed streaming exporter and the one-shot
// corpus path both call this). Rows must arrive in corpus order.
void accumulateChainTxn(ChainGroups &groups,
                        const transactions::Transaction &tx);

// Summarize the accumulated groups into chain rows, sorted by chain id.
[[nodiscard]] std::vector<ChainRow> finalizeChains(ChainGroups &groups);

// One-shot corpus form: accumulate then finalize — one code path.
[[nodiscard]] std::vector<ChainRow>
buildChains(std::span<const transactions::Transaction> postedTxns);

// Per-candidate-account flow aggregates behind the derived shell score
// (fraud-audit-2026-07 F2). The timestamps are retained for calibration
// measurements; the score consumes only the flow/count aggregates, all
// of which are order-insensitive — window/thread invariant by
// construction.
struct ShellAggregates {
  double inflow = 0.0;
  double outflow = 0.0;
  std::int64_t firstTs = 0;
  std::int64_t lastTs = 0;
  std::uint64_t fraudCount = 0;
  std::uint64_t totalCount = 0;
};

// Shell-score accumulator. The candidate set (ring members' primary
// accounts carrying the shell flag) is fixed from static topology
// BEFORE the fold, so per-row accumulation touches only fraud-scale
// state — never transaction-scale. Both aml sinks and the one-shot
// corpus path use the same per-row function, so the engines cannot
// drift.
struct ShellStats {
  std::vector<ShellAccountRow> candidates;
  std::unordered_map<entity::Key, ShellAggregates> byAccount;
};

// Seed the candidate rows and their aggregate slots from topology.
[[nodiscard]] ShellStats initShellStats(ShellInputs in);

// Per-row accumulation (windowed streaming exporter and the one-shot
// corpus path both call this). Non-candidate accounts are ignored.
void accumulateShellTxn(ShellStats &stats,
                        const transactions::Transaction &tx);

// Score the candidates: passThrough × (1 − organicShare). Dormant
// pass-through ring accounts score near 1; camouflaged or organically
// active accounts score lower. The two-decimal rendering comes from the
// shared money rounding in writeShellAccountRows — no new float
// formatting.
[[nodiscard]] std::vector<ShellAccountRow>
finalizeShells(const ShellStats &stats);

// One-shot corpus form: init, accumulate, finalize — one code path.
[[nodiscard]] std::vector<ShellAccountRow>
buildShells(ShellInputs in,
            std::span<const transactions::Transaction> postedTxns);

void writeChainRows(csv::Writer &w, std::span<const ChainRow> rows);

void writeShellAccountRows(csv::Writer &w,
                           std::span<const ShellAccountRow> rows);

// Streaming form: `nextIndex1` carries the 1-based transaction id across
// batches so the windowed sink emits identical ids to the one-shot path.
void writeTransactionChainLabelRows(
    csv::Writer &w, std::span<const transactions::Transaction> postedTxns,
    std::size_t &nextIndex1);

void writeTransactionChainLabelRows(
    csv::Writer &w, std::span<const transactions::Transaction> postedTxns);

} // namespace PhantomLedger::exporter::labels
