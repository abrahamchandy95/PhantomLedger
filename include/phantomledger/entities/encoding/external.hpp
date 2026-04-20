#pragma once

#include "phantomledger/entities/identifier/bank.hpp"
#include "phantomledger/entities/identifier/key.hpp"

#include <string_view>

namespace PhantomLedger::encoding {

[[nodiscard]] constexpr bool
isExternal(const entities::identifier::Key &id) noexcept {
  return id.bank == entities::identifier::Bank::external;
}

[[nodiscard]] constexpr bool isExternal(std::string_view id) noexcept {
  return !id.empty() && id.front() == 'X';
}

} // namespace PhantomLedger::encoding
