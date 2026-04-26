#pragma once
/*
 * Obligation events emitted by financial products.
 */

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"

#include <cstdint>
#include <string_view>

namespace PhantomLedger::entity::product {

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

[[nodiscard]] constexpr bool isInstallmentLoan(ProductType t) noexcept {
  return t == ProductType::mortgage || t == ProductType::autoLoan ||
         t == ProductType::studentLoan;
}

[[nodiscard]] constexpr std::string_view
productTypeName(ProductType t) noexcept {
  switch (t) {
  case ProductType::mortgage:
    return "mortgage";
  case ProductType::autoLoan:
    return "auto_loan";
  case ProductType::studentLoan:
    return "student_loan";
  case ProductType::insurance:
    return "insurance";
  case ProductType::tax:
    return "tax";
  case ProductType::unknown:
    break;
  }
  return "unknown";
}

/// A single scheduled or stochastic financial event from a product.
///
/// Product IDs are represented as a compact u32 index into the
/// portfolio's per-person product table rather than a free-form
/// string; callers that need a human-readable identifier should use
/// the portfolio to resolve it.
struct ObligationEvent {
  entity::PersonId personId{};
  Direction direction = Direction::outflow;

  /// External counterparty (lender, carrier, IRS, etc.).
  entity::Key counterpartyAcct{};

  double amount = 0.0;
  time::TimePoint timestamp{};
  channels::Tag channel = channels::none;

  ProductType productType = ProductType::unknown;
  std::uint32_t productId = 0;
};

} // namespace PhantomLedger::entity::product
