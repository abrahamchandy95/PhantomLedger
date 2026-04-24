#pragma once

#include "phantomledger/entities/identifier/key.hpp"

#include <string_view>

namespace PhantomLedger::encoding {

using identifiers::Bank;

[[nodiscard]] constexpr bool isExternal(const entity::Key &id) noexcept {
  return id.bank == Bank::external;
}

[[nodiscard]] constexpr bool isExternal(std::string_view id) noexcept {
  return !id.empty() && id.front() == 'X';
}

} // namespace PhantomLedger::encoding
