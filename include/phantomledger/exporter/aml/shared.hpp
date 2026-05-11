#pragma once

#include "phantomledger/entities/encoding/render.hpp"

#include <cstdint>
#include <cstdio>
#include <set>
#include <string>
#include <string_view>

namespace PhantomLedger::exporter::aml {

using TransactionId = ::PhantomLedger::encoding::RenderedId<20>;
using BankId = ::PhantomLedger::encoding::RenderedId<16>;

inline constexpr std::string_view kOurBankId = "BNK_0000";

[[nodiscard]] inline std::uint64_t
stableU64(std::initializer_list<std::string_view> parts) noexcept {
  constexpr std::uint64_t kFnvOffsetBasis = 0xcbf29ce484222325ULL;
  constexpr std::uint64_t kFnvPrime = 0x100000001b3ULL;
  std::uint64_t h = kFnvOffsetBasis;
  for (const auto part : parts) {
    for (const auto c : part) {
      h ^= static_cast<std::uint8_t>(c);
      h *= kFnvPrime;
    }
    h ^= static_cast<std::uint8_t>('|');
    h *= kFnvPrime;
  }
  return h;
}

[[nodiscard]] inline TransactionId transactionId(std::size_t idx1) noexcept {
  TransactionId out;

  const auto n =
      std::snprintf(out.bytes.data(), out.bytes.size(), "TXN_%012zu", idx1);
  out.length = static_cast<std::uint8_t>(n > 0 ? n : 0);
  return out;
}

[[nodiscard]] inline BankId
counterpartyBankId(std::string_view counterpartyId) noexcept {
  const auto x = stableU64({counterpartyId, "bank"});
  const auto bankNumber = static_cast<unsigned>((x % 20U) + 1U);
  BankId out;
  const auto n =
      std::snprintf(out.bytes.data(), out.bytes.size(), "BNK_%04u", bankNumber);
  out.length = static_cast<std::uint8_t>(n > 0 ? n : 0);
  return out;
}

// ────────────────────────────────────────────────────────────────────
// FUTURE: cache the result on SharedContext so we build it once at
// export start instead of rebuilding for every bank-edge writer.
// ────────────────────────────────────────────────────────────────────

[[nodiscard]] inline std::set<std::string>
allBankIds(const std::set<std::string> &counterpartyIds) {
  std::set<std::string> seen;
  seen.emplace(kOurBankId);
  for (const auto &cp : counterpartyIds) {
    seen.emplace(counterpartyBankId(cp).view());
  }
  return seen;
}

} // namespace PhantomLedger::exporter::aml
