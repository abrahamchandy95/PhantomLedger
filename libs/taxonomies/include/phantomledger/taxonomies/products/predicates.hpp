#pragma once

#include "phantomledger/taxonomies/products/types.hpp"

namespace PhantomLedger::products {

[[nodiscard]] constexpr bool isInstallmentLoan(ProductType type) noexcept {
  return type == ProductType::mortgage || type == ProductType::autoLoan ||
         type == ProductType::studentLoan;
}

} // namespace PhantomLedger::products
