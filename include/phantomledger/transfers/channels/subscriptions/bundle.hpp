#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transfers/channels/subscriptions/prices.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace PhantomLedger::transfers::subscriptions {

/// Knobs that drive one person's subscription bundle composition.
struct BundleRules {
  std::int32_t minPerPerson = 4;
  std::int32_t maxPerPerson = 8;
  double debitP = 0.55;

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
  }

  void validate() const {
    primitives::validate::Report r;
    validate(r);
    r.throwIfFailed();
  }
};

struct Sub {
  entity::Key deposit;
  entity::Key biller;
  double amount = 0.0;
  std::uint8_t day = 0;
};

class BundleEmitter {
public:
  BundleEmitter(const BundleRules &rules,
                std::span<const entity::Key> billerAccounts) noexcept
      : rules_(&rules), billerAccounts_(billerAccounts) {}

  void emit(random::Rng &subRng, const entity::Key &deposit,
            std::vector<Sub> &out) const {
    if (billerAccounts_.empty()) {
      return;
    }

    const auto nTotal = static_cast<std::int32_t>(
        subRng.uniformInt(rules_->minPerPerson,
                          static_cast<std::int64_t>(rules_->maxPerPerson) + 1));

    std::int32_t nDebit = 0;
    for (std::int32_t i = 0; i < nTotal; ++i) {
      if (subRng.coin(rules_->debitP)) {
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
      const auto billerIdx = subRng.choiceIndex(billerAccounts_.size());
      const auto day = static_cast<std::uint8_t>(subRng.uniformInt(1, 29));

      out.push_back(Sub{
          .deposit = deposit,
          .biller = billerAccounts_[billerIdx],
          .amount = kPricePool[priceIdx[i]],
          .day = day,
      });
    }
  }

private:
  const BundleRules *rules_;
  std::span<const entity::Key> billerAccounts_;
};

} // namespace PhantomLedger::transfers::subscriptions
