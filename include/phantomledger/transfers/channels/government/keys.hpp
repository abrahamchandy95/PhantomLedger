#pragma once

#include "phantomledger/entities/identifiers.hpp"

#include <cstdint>

namespace PhantomLedger::transfers::government {

inline constexpr std::uint64_t kSsaAccountNumber = 9'000'001ULL;
inline constexpr std::uint64_t kDisabilityAccountNumber = 9'000'002ULL;

[[nodiscard]] inline constexpr entity::Key ssaCounterpartyKey() noexcept {
  return entity::makeKey(entity::Role::employer, entity::Bank::external,
                         kSsaAccountNumber);
}

[[nodiscard]] inline constexpr entity::Key
disabilityCounterpartyKey() noexcept {
  return entity::makeKey(entity::Role::employer, entity::Bank::external,
                         kDisabilityAccountNumber);
}

} // namespace PhantomLedger::transfers::government
