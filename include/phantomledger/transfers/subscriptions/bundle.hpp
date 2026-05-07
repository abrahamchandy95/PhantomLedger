#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transfers/subscriptions/prices.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace PhantomLedger::transfers::subscriptions {

/// Knobs that drive bundle composition for one person.
struct BundleRules {
  std::int32_t minPerPerson = 4;
  std::int32_t maxPerPerson = 8;
  double debitP = 0.55;
  std::int32_t dayJitter = 1;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::ge("minPerPerson", minPerPerson, 0); });
    r.check([&] {
      if (maxPerPerson < minPerPerson) {
        throw std::invalid_argument(
            "subscriptions: maxPerPerson must be >= minPerPerson");
      }
    });
    r.check([&] { v::unit("debitP", debitP); });
    r.check([&] { v::ge("dayJitter", dayJitter, 0); });
  }

  void validate() const {
    primitives::validate::Report r;
    validate(r);
    r.throwIfFailed();
  }
};

/// One materialized subscription. 32-byte flat layout so the per-month
/// loop walks contiguous memory.
struct Sub {
  entity::Key deposit;
  entity::Key biller;
  double amount = 0.0;
  std::uint8_t day = 0;
};

/// Append one person's subscription bundle to `out`. Uses the
/// configured rng to:
///   * sample a total subscription count in [minPerPerson, maxPerPerson]
///   * count debits via `debitP` Bernoulli trials
///   * draw distinct prices without replacement via `choiceIndices`
///   * draw biller and billing day uniformly per sub
///
/// Behavior parity note: the without-replacement price draw uses
/// `choiceIndices` so this composes byte-identically with routines
/// that previously inlined the same logic.
inline void appendBundle(random::Rng &subRng, const BundleRules &rules,
                         const entity::Key &deposit,
                         std::span<const entity::Key> billerAccounts,
                         std::vector<Sub> &out) {
  if (billerAccounts.empty()) {
    return;
  }

  const auto nTotal = static_cast<std::int32_t>(subRng.uniformInt(
      rules.minPerPerson, static_cast<std::int64_t>(rules.maxPerPerson) + 1));

  std::int32_t nDebit = 0;
  for (std::int32_t i = 0; i < nTotal; ++i) {
    if (subRng.coin(rules.debitP)) {
      ++nDebit;
    }
  }
  if (nDebit == 0) {
    return;
  }

  const auto nPick = static_cast<std::size_t>(std::min<std::int32_t>(
      nDebit, static_cast<std::int32_t>(kPricePoolSize)));

  const auto priceIdx =
      subRng.choiceIndices(kPricePoolSize, nPick, /*replace=*/false);

  out.reserve(out.size() + nPick);
  for (std::size_t i = 0; i < nPick; ++i) {
    const auto billerIdx = subRng.choiceIndex(billerAccounts.size());
    const auto day = static_cast<std::uint8_t>(subRng.uniformInt(1, 29));

    out.push_back(Sub{
        .deposit = deposit,
        .biller = billerAccounts[billerIdx],
        .amount = kPricePool[priceIdx[i]],
        .day = day,
    });
  }
}

} // namespace PhantomLedger::transfers::subscriptions
