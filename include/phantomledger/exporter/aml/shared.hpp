#pragma once

#include "phantomledger/encoding/render.hpp"
#include "phantomledger/exporter/common/hashing.hpp"

#include <cstdint>
#include <cstdio>
#include <set>
#include <string_view>

namespace PhantomLedger::exporter::aml {

using ::PhantomLedger::exporter::common::stableU64;

using TransactionId = ::PhantomLedger::encoding::RenderedId<20>;
using BankId = ::PhantomLedger::encoding::RenderedId<16>;

inline constexpr std::string_view kOurBankId = "BNK_0000";

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

[[nodiscard]] inline BankId
makeBankIdFromLiteral(std::string_view literal) noexcept {
  BankId out;
  const auto n =
      literal.size() < out.bytes.size() ? literal.size() : out.bytes.size();
  for (std::size_t i = 0; i < n; ++i) {
    out.bytes[i] = literal[i];
  }
  out.length = static_cast<std::uint8_t>(n);
  return out;
}

[[nodiscard]] inline std::set<BankId> allBankIds(
    const std::set<::PhantomLedger::encoding::RenderedKey> &counterpartyIds) {
  std::set<BankId> seen;
  seen.emplace(makeBankIdFromLiteral(kOurBankId));
  for (const auto &cp : counterpartyIds) {
    seen.emplace(counterpartyBankId(cp));
  }
  return seen;
}

} // namespace PhantomLedger::exporter::aml
