#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transfers/subscriptions/prices.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>
#include <span>
#include <stdexcept>
#include <vector>

namespace PhantomLedger::transfers::subscriptions {

/// Knobs that drive bundle composition for one person.
struct BundleTerms {
  std::uint8_t minPerPerson = 4;
  std::uint8_t maxPerPerson = 8;
  double debitP = 0.55;
  std::uint8_t dayJitter = 1;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] {
      if (maxPerPerson < minPerPerson) {
        throw std::invalid_argument(
            "subscriptions: maxPerPerson must be >= minPerPerson");
      }
    });
    r.check([&] { v::unit("debitP", debitP); });
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

// Append one person's subscription bundle

inline void appendBundle(random::Rng &subRng, const BundleTerms &terms,
                         const entity::Key &deposit,
                         std::span<const entity::Key> billerAccounts,
                         std::vector<Sub> &out) {
  const auto nTotal = subRng.uniformInt(
      terms.minPerPerson, static_cast<std::int64_t>(terms.maxPerPerson) + 1);

  std::int64_t nDebit = 0;
  for (std::int64_t i = 0; i < nTotal; ++i) {
    if (subRng.coin(terms.debitP)) {
      ++nDebit;
    }
  }
  if (nDebit == 0) {
    return;
  }

  const auto nPick =
      static_cast<std::size_t>(std::min<std::int64_t>(nDebit, kPricePoolSize));

  std::array<std::uint8_t, kPricePoolSize> idx{};
  std::iota(idx.begin(), idx.end(), std::uint8_t{0});
  for (std::size_t i = 0; i < nPick; ++i) {
    const auto j = static_cast<std::size_t>(
        subRng.uniformInt(static_cast<std::int64_t>(i),
                          static_cast<std::int64_t>(kPricePoolSize)));
    std::swap(idx[i], idx[j]);
  }

  out.reserve(out.size() + nPick);
  for (std::size_t i = 0; i < nPick; ++i) {
    const auto billerIdx = static_cast<std::size_t>(
        subRng.uniformInt(0, static_cast<std::int64_t>(billerAccounts.size())));
    const auto day =
        static_cast<std::uint8_t>(subRng.uniformInt(1, 29)); // [1, 28]
    out.push_back(Sub{
        .deposit = deposit,
        .biller = billerAccounts[billerIdx],
        .amount = kPricePool[idx[i]],
        .day = day,
    });
  }
}

} // namespace PhantomLedger::transfers::subscriptions
