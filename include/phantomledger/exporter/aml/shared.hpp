#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <string_view>

namespace PhantomLedger::exporter::aml {

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

[[nodiscard]] inline std::string
counterpartyBankId(std::string_view counterpartyId) {
  const auto x = stableU64({counterpartyId, "bank"});
  const auto bankNumber = static_cast<unsigned>((x % 20U) + 1U);
  char buf[16];
  std::snprintf(buf, sizeof(buf), "BNK_%04u", bankNumber);
  return std::string{buf};
}

[[nodiscard]] inline std::set<std::string>
allBankIds(const std::set<std::string> &counterpartyIds) {
  std::set<std::string> seen{std::string{kOurBankId}};
  for (const auto &cp : counterpartyIds) {
    seen.insert(counterpartyBankId(cp));
  }
  return seen;
}

} // namespace PhantomLedger::exporter::aml
