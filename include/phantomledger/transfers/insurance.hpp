#pragma once
/*
 * Insurance transfer generator.
 *
 * Reads insurance policy terms from the portfolio (single source of
 * truth for ownership and premium amounts) and emits:
 *
 *   1. Monthly premium payments on each policy's billing day.
 *   2. Stochastic claim payouts sampled per year.
 *
 * The ownership decision and premium amount sampling happen in the
 * product builder, not here. This module only schedules the resulting
 * events into the transaction stream.
 *
 * Home insurance for mortgaged households is escrowed into the
 * mortgage payment; this module checks the portfolio's mortgage flag
 * and skips standalone home-premium emission in that case (claims
 * still fire normally).
 */
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/entities/products/portfolio.hpp"
#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <unordered_map>
#include <vector>

namespace PhantomLedger::transfer::insurance {

struct Rates {
  double auto_ = 0.0;
  double home = 0.0;
  double life = 0.0;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::unit("auto", auto_); });
    r.check([&] { v::unit("home", home); });
    r.check([&] { v::unit("life", life); });
  }
};

struct Params {
  // --- Premium distributions (monthly, USD; lognormal by median) ---
  double autoMedian = 225.0;
  double autoSigma = 0.35;
  double homeMedian = 163.0;
  double homeSigma = 0.40;
  double lifeMedian = 40.0;
  double lifeSigma = 0.50;

  // --- Claim probabilities (annual) ---
  double autoClaimAnnualP = 0.042;
  double homeClaimAnnualP = 0.055;

  // --- Claim payout distributions (lognormal by median) ---
  double autoClaimMedian = 4700.0;
  double autoClaimSigma = 0.80;
  double homeClaimMedian = 15750.0;
  double homeClaimSigma = 0.90;

  // --- Financed-collateral coverage anchors ---
  double mortgageHomeRequiredP = 0.998;
  double autoLoanAutoRequiredP = 0.997;

  // Named switch rather than a positional array: the value-to-rates
  // mapping is enum-label-driven, so any future reordering of
  // personas::Type cannot silently shift the table.
  [[nodiscard]] static constexpr Rates forPersona(personas::Type t) noexcept {
    using T = personas::Type;
    switch (t) {
    case T::student:
      return {0.55, 0.00, 0.05};
    case T::retiree:
      return {0.80, 0.80, 0.40};
    case T::freelancer:
      return {0.85, 0.35, 0.30};
    case T::smallBusiness:
      return {0.92, 0.60, 0.60};
    case T::highNetWorth:
      return {0.95, 0.90, 0.80};
    case T::salaried:
      return {0.90, 0.55, 0.55};
    }
    return {0.90, 0.55, 0.55}; // salaried fallback
  }

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::positive("autoMedian", autoMedian); });
    r.check([&] { v::nonNegative("autoSigma", autoSigma); });
    r.check([&] { v::positive("homeMedian", homeMedian); });
    r.check([&] { v::nonNegative("homeSigma", homeSigma); });
    r.check([&] { v::positive("lifeMedian", lifeMedian); });
    r.check([&] { v::nonNegative("lifeSigma", lifeSigma); });
    r.check([&] { v::unit("autoClaimAnnualP", autoClaimAnnualP); });
    r.check([&] { v::unit("homeClaimAnnualP", homeClaimAnnualP); });
    r.check([&] { v::positive("autoClaimMedian", autoClaimMedian); });
    r.check([&] { v::positive("homeClaimMedian", homeClaimMedian); });
    r.check([&] { v::unit("mortgageHomeRequiredP", mortgageHomeRequiredP); });
    r.check([&] { v::unit("autoLoanAutoRequiredP", autoLoanAutoRequiredP); });
  }
};

struct Window {
  time::TimePoint start{};
  int days = 0;

  [[nodiscard]] time::TimePoint endExcl() const noexcept {
    return start + time::Days{days};
  }
};

struct Population {
  const std::unordered_map<entity::PersonId, entity::Key> *primaryAccounts =
      nullptr;
};

/// Generate insurance premium and claim transactions from portfolio
/// data. The returned vector is sorted by timestamp.
///
/// The factory is used to derive a per-person RNG for claim
/// occurrence — this isolates the claim stream from the main `rng`
/// so downstream callers can reorder the outer person loop without
/// perturbing claim events.
[[nodiscard]] std::vector<transactions::Transaction>
generate(const Params &params, const Window &window, random::Rng &rng,
         const transactions::Factory &txf, const random::RngFactory &factory,
         const entity::product::PortfolioRegistry &portfolios,
         const Population &population);

} // namespace PhantomLedger::transfer::insurance
