#pragma once

#include "phantomledger/random/rng.hpp"
#include "phantomledger/random/seed.hpp"

#include <concepts>
#include <cstdint>
#include <initializer_list>
#include <ranges>
#include <stdexcept>
#include <string_view>

namespace PhantomLedger::random {

class RngFactory {
public:
  explicit constexpr RngFactory(std::uint64_t base) noexcept : base_(base) {}

  [[nodiscard]] constexpr std::uint64_t base() const noexcept { return base_; }

  [[nodiscard]] Rng rng(std::initializer_list<std::string_view> parts) const {
    return Rng::fromSeed(seed(parts));
  }

  template <std::ranges::input_range Parts>
    requires std::convertible_to<std::ranges::range_reference_t<Parts>,
                                 std::string_view>
  [[nodiscard]] Rng rng(const Parts &parts) const {
    return Rng::fromSeed(seed(parts));
  }

private:
  template <typename Parts>
  [[nodiscard]] std::uint64_t seed(const Parts &parts) const {
    if (const auto derived = deriveSeed(base_, parts)) {
      return *derived;
    }
    throw std::runtime_error("RngFactory: failed to derive seed");
  }

  std::uint64_t base_;
};

} // namespace PhantomLedger::random
