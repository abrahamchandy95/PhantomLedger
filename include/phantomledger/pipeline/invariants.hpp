#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PhantomLedger::pipeline {

namespace detail {

inline constexpr std::size_t kMissingSampleLimit = 10;

[[nodiscard]] inline std::string
formatMissing(const std::unordered_map<entity::Key, std::uint32_t> &missing) {
  using Item = std::pair<entity::Key, std::uint32_t>;

  std::vector<Item> items;
  items.reserve(missing.size());
  for (const auto &kv : missing) {
    items.emplace_back(kv);
  }

  std::ranges::sort(items, [](const Item &a, const Item &b) noexcept {
    if (a.second != b.second) {
      return a.second > b.second;
    }
    return a.first < b.first;
  });

  const auto take = std::min(items.size(), kMissingSampleLimit);

  std::string out;
  for (std::size_t i = 0; i < take; ++i) {
    if (i != 0) {
      out.append(", ");
    }
    out.append(encoding::format(items[i].first));
    out.push_back(' ');
    out.push_back('(');
    out.append(std::to_string(items[i].second));
    out.push_back(')');
  }
  return out;
}

} // namespace detail

inline void
validateTransactionAccounts(const entity::account::Lookup &lookup,
                            std::span<const transactions::Transaction> txns) {
  std::unordered_map<entity::Key, std::uint32_t> missingSources;
  std::unordered_map<entity::Key, std::uint32_t> missingTargets;

  for (const auto &tx : txns) {
    if (!lookup.byId.contains(tx.source)) {
      ++missingSources[tx.source];
    }
    if (!lookup.byId.contains(tx.target)) {
      ++missingTargets[tx.target];
    }
  }

  if (missingSources.empty() && missingTargets.empty()) {
    return;
  }

  std::string message =
      "Transactions reference accounts that are absent from the account "
      "registry; ";

  bool first = true;
  if (!missingSources.empty()) {
    message.append("missing source accounts: ");
    message.append(detail::formatMissing(missingSources));
    first = false;
  }
  if (!missingTargets.empty()) {
    if (!first) {
      message.append("; ");
    }
    message.append("missing target accounts: ");
    message.append(detail::formatMissing(missingTargets));
  }

  throw std::runtime_error(std::move(message));
}

} // namespace PhantomLedger::pipeline
