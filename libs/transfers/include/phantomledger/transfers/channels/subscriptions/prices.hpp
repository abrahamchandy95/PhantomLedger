#pragma once

#include <array>
#include <cstddef>

namespace PhantomLedger::transfers::subscriptions {

inline constexpr std::array<double, 18> kPricePool{{
    6.99,
    7.99,
    9.99,
    10.99,
    11.99,
    12.99,
    14.99,
    15.49,
    15.99,
    17.99,
    22.99,
    24.99,
    29.99,
    34.99,
    39.99,
    49.99,
    59.99,
    99.99,
}};

inline constexpr std::size_t kPricePoolSize = kPricePool.size();

} // namespace PhantomLedger::transfers::subscriptions
