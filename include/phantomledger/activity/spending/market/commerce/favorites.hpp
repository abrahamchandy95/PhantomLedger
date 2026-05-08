#pragma once

#include "phantomledger/primitives/utils/groups.hpp"

#include <cstdint>

namespace PhantomLedger::spending::market::commerce {

using CsrIndex = primitives::utils::Csr<std::uint32_t, std::uint32_t>;
using Favorites = CsrIndex;
using Billers = CsrIndex;

} // namespace PhantomLedger::spending::market::commerce
