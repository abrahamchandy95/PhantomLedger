#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/fraud/types.hpp"
#include "phantomledger/transactions/record.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
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
  double shellScore = 1.0;
};

struct ShellInputs {
  const entity::account::Registry &registry;
  const entity::account::Ownership &ownership;
  const entity::person::Topology &topology;
};

[[nodiscard]] std::optional<fraud::Typology>
typologyForChannel(channels::Tag channel) noexcept;

[[nodiscard]] std::vector<ChainRow>
buildChains(std::span<const transactions::Transaction> postedTxns);

[[nodiscard]] std::vector<ShellAccountRow> buildShells(ShellInputs in);

void writeChainRows(csv::Writer &w, std::span<const ChainRow> rows);

void writeShellAccountRows(csv::Writer &w,
                           std::span<const ShellAccountRow> rows);

void writeTransactionChainLabelRows(
    csv::Writer &w, std::span<const transactions::Transaction> postedTxns);

} // namespace PhantomLedger::exporter::labels
