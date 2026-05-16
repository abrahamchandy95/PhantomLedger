#pragma once

#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/fraud/types.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace PhantomLedger::exporter::aml_txn_edges::chains {

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

[[nodiscard]] std::optional<fraud::Typology>
typologyForChannel(channels::Tag channel) noexcept;

[[nodiscard]] std::vector<ChainRow>
buildChains(std::span<const transactions::Transaction> postedTxns);

} // namespace PhantomLedger::exporter::aml_txn_edges::chains
