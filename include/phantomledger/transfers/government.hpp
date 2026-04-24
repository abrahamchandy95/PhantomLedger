#pragma once
/*
 * Government benefit transfer generator.
 *
 * Emits monthly Social Security retirement deposits and monthly
 * disability deposits into eligible persons' primary accounts.
 *
 * Payment dates follow the post-1997 SSA cohorting rule: retirees
 * and disability recipients are sorted into three buckets by a
 * synthetic birth-day derived from the person ID (to match the fake
 * DOB used by downstream ML exporters), and each bucket is paid on
 * its designated Wednesday of the month with holiday back-off.
 *
 * Amounts are lognormal-by-median with a hard floor. Eligibility is
 * a Bernoulli draw per person, per subsystem.
 *
 * External counterparties (SSA / disability) are resolved by key;
 * callers must pass the two Key values the product environment uses
 * to avoid hard-coding externals here.
 */
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

#include "phantomledger/entities/accounts/account.hpp"
#include "phantomledger/entities/behavior/behavior.hpp"
#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::transfer::government {

/// Parameters for benefit sampling. Defaults trace back to published
/// SSA figures for FY 2026; see source docs in sibling `.cpp`.
struct Params {
  // --- Social Security (retirees) ---
  bool ssaEnabled = true;
  double ssaEligibleP = 0.87;
  double ssaMedian = 2071.0;
  double ssaSigma = 0.30;
  double ssaFloor = 900.0;

  // --- Disability (non-retired, non-student) ---
  bool disabilityEnabled = true;
  double disabilityP = 0.04;
  double disabilityMedian = 1630.0;
  double disabilitySigma = 0.25;
  double disabilityFloor = 500.0;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::unit("ssaEligibleP", ssaEligibleP); });
    r.check([&] { v::positive("ssaMedian", ssaMedian); });
    r.check([&] { v::nonNegative("ssaSigma", ssaSigma); });
    r.check([&] { v::nonNegative("ssaFloor", ssaFloor); });
    r.check([&] { v::unit("disabilityP", disabilityP); });
    r.check([&] { v::positive("disabilityMedian", disabilityMedian); });
    r.check([&] { v::nonNegative("disabilitySigma", disabilitySigma); });
    r.check([&] { v::nonNegative("disabilityFloor", disabilityFloor); });
  }
};

/// External counterparty keys for the two benefit subsystems.
struct Counterparties {
  entity::Key ssa{};
  entity::Key disability{};
};

/// Read-only view of the population needed to resolve recipients.
struct Population {
  std::uint32_t count = 0;
  const entity::behavior::Assignment *personas = nullptr;
  const entity::account::Registry *accounts = nullptr;
  const entity::account::Ownership *ownership = nullptr;
};

struct Window {
  time::TimePoint start{};
  int days = 0;

  [[nodiscard]] time::TimePoint endExcl() const noexcept {
    return start + time::Days{days};
  }
};

/// Generate all government-benefit deposits inside the window.
/// The returned vector is sorted by timestamp.
[[nodiscard]] std::vector<transactions::Transaction>
generate(const Params &params, const Window &window, random::Rng &rng,
         const transactions::Factory &txf, const Population &population,
         const Counterparties &counterparties);

} // namespace PhantomLedger::transfer::government
