#pragma once

#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/merchants.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/taxonomies/merchants/names.hpp"

#include <cmath>

namespace PhantomLedger::exporter::standard {

namespace detail {

[[nodiscard]] inline double round10(double v) noexcept {

  constexpr double kScale = 1.0e10;
  return std::round(v * kScale) / kScale;
}

} // namespace detail

inline void
writeMerchantRows(::PhantomLedger::exporter::csv::Writer &w,
                  const ::PhantomLedger::entity::merchant::Catalog &catalog) {
  using ::PhantomLedger::entity::Bank;
  namespace enc = ::PhantomLedger::encoding;

  for (const auto &rec : catalog.records) {

    const auto merchantKey = ::PhantomLedger::entity::makeKey(
        ::PhantomLedger::entity::Role::merchant, Bank::internal,
        rec.label.value);

    const bool inBank = (rec.counterpartyId.bank == Bank::internal);

    w.writeRow(enc::format(merchantKey).view(),
               enc::format(rec.counterpartyId).view(),
               ::PhantomLedger::merchants::name(rec.category),
               detail::round10(rec.weight), inBank);
  }
}

} // namespace PhantomLedger::exporter::standard
