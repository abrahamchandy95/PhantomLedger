#pragma once

#include "phantomledger/entities/categories.hpp"
#include "phantomledger/entities/identity.hpp"

#include <cstdint>

namespace PhantomLedger::entities::accounts {

namespace detail {

[[nodiscard]] constexpr Identity make(Type type, BankAccount bank,
                                      std::uint64_t number) noexcept {
  return Identity{type, bank, number};
}

} // namespace detail

[[nodiscard]] constexpr Identity account(std::uint64_t number) noexcept {
  return detail::make(Type::account, BankAccount::internal, number);
}

[[nodiscard]] constexpr Identity
merchant(std::uint64_t number,
         BankAccount bank = BankAccount::internal) noexcept {
  return detail::make(Type::merchant, bank, number);
}

[[nodiscard]] constexpr Identity employer(std::uint64_t number) noexcept {
  return detail::make(Type::employer, BankAccount::external, number);
}

[[nodiscard]] constexpr Identity landlord(std::uint64_t number) noexcept {
  return detail::make(Type::landlord, BankAccount::external, number);
}

[[nodiscard]] constexpr Identity client(std::uint64_t number) noexcept {
  return detail::make(Type::client, BankAccount::external, number);
}

[[nodiscard]] constexpr Identity platform(std::uint64_t number) noexcept {
  return detail::make(Type::platform, BankAccount::external, number);
}

[[nodiscard]] constexpr Identity processor(std::uint64_t number) noexcept {
  return detail::make(Type::processor, BankAccount::external, number);
}

[[nodiscard]] constexpr Identity business(std::uint64_t number) noexcept {
  return detail::make(Type::business, BankAccount::external, number);
}

[[nodiscard]] constexpr Identity brokerage(std::uint64_t number) noexcept {
  return detail::make(Type::brokerage, BankAccount::external, number);
}

} // namespace PhantomLedger::entities::accounts
