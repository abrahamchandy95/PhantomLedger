#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace PhantomLedger::products {

/// Which way money flows relative to the account holder.
enum class Direction : std::uint8_t {
  outflow = 0,
  inflow = 1,
};

enum class ProductType : std::uint8_t {
  unknown = 0,
  mortgage = 1,
  autoLoan = 2,
  studentLoan = 3,
  insurance = 4,
  tax = 5,
};

enum class PolicyType : std::uint8_t {
  auto_ = 0,
  home = 1,
  life = 2,
};

inline constexpr auto kDirections = std::to_array<Direction>({
    Direction::outflow,
    Direction::inflow,
});

inline constexpr auto kProductTypes = std::to_array<ProductType>({
    ProductType::unknown,
    ProductType::mortgage,
    ProductType::autoLoan,
    ProductType::studentLoan,
    ProductType::insurance,
    ProductType::tax,
});

inline constexpr auto kPolicyTypes = std::to_array<PolicyType>({
    PolicyType::auto_,
    PolicyType::home,
    PolicyType::life,
});

inline constexpr std::size_t kDirectionCount = kDirections.size();
inline constexpr std::size_t kProductTypeCount = kProductTypes.size();
inline constexpr std::size_t kPolicyTypeCount = kPolicyTypes.size();

} // namespace PhantomLedger::products
