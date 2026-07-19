#pragma once

#include "phantomledger/entities/counterparties.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/taxonomies/identifiers/types.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace PhantomLedger::synth::counterparties {

using identifiers::Bank;
using identifiers::Role;

/// Population-scaled count with a lower bound.
struct ScaledCount {
  double perTenK = 0.0;
  int minCount = 0;

  [[nodiscard]] int forPopulation(int population) const {
    const int scaled = static_cast<int>(
        std::round(perTenK * (static_cast<double>(population) / 10'000.0)));

    return std::max(minCount, scaled);
  }
  void validate(::PhantomLedger::primitives::validate::Report &r) const {
    namespace v = ::PhantomLedger::primitives::validate;
    r.check([&] { v::nonNegative("perTenK", perTenK); });
    r.check([&] { v::nonNegative("minCount", minCount); });
  }
};

struct BankedPoolTargets {
  ScaledCount count{};
  double internalBankP = 0.0;
  void validate(::PhantomLedger::primitives::validate::Report &r) const {
    count.validate(r);
    r.check([&] {
      ::PhantomLedger::primitives::validate::unit("internalBankP",
                                                  internalBankP);
    });
  }
};

struct ExternalPoolTargets {
  ScaledCount platforms{.perTenK = 2.0, .minCount = 2};
  ScaledCount processors{.perTenK = 1.0, .minCount = 2};
  ScaledCount ownerBusinesses{.perTenK = 200.0, .minCount = 25};
  ScaledCount brokerages{.perTenK = 40.0, .minCount = 5};

  void validate(::PhantomLedger::primitives::validate::Report &r) const {
    platforms.validate(r);
    processors.validate(r);
    ownerBusinesses.validate(r);
    brokerages.validate(r);
  }
};

struct CounterpartyTargets {
  BankedPoolTargets employers{
      .count = {.perTenK = 25.0, .minCount = 5},
      .internalBankP = 0.04,
  };

  BankedPoolTargets clients{
      .count = {.perTenK = 250.0, .minCount = 25},
      .internalBankP = 0.02,
  };

  ExternalPoolTargets external{};
  void validate(::PhantomLedger::primitives::validate::Report &r) const {
    employers.validate(r);
    clients.validate(r);
    external.validate(r);
  }
};

namespace detail {

inline void appendExternal(std::vector<entity::Key> &out, Role role,
                           int count) {
  out.reserve(out.size() + static_cast<std::size_t>(count));

  for (int i = 1; i <= count; ++i) {
    out.push_back(
        entity::makeKey(role, Bank::external, static_cast<std::uint64_t>(i)));
  }
}

inline void fillBankSplit(random::Rng &rng, Role role, int total,
                          double internalBankP,
                          entity::counterparty::BankSplit &out) {
  out.internal.reserve(out.internal.size() + static_cast<std::size_t>(total));
  out.external.reserve(out.external.size() + static_cast<std::size_t>(total));
  out.all.reserve(out.all.size() + static_cast<std::size_t>(total));

  std::uint64_t internalCounter = 0;
  std::uint64_t externalCounter = 0;

  for (int i = 0; i < total; ++i) {
    entity::Key id;

    if (rng.coin(internalBankP)) {
      ++internalCounter;
      id = entity::makeKey(role, Bank::internal, internalCounter);
      out.internal.push_back(id);
    } else {
      ++externalCounter;
      id = entity::makeKey(role, Bank::external, externalCounter);
      out.external.push_back(id);
    }

    out.all.push_back(id);
  }
}

} // namespace detail

/// Build all counterparty directories scaled to population size.
[[nodiscard]] inline entity::counterparty::Directory
make(random::Rng &rng, int population,
     const CounterpartyTargets &targets = {}) {
  entity::counterparty::Directory out;

  const int employerCount = targets.employers.count.forPopulation(population);
  detail::fillBankSplit(rng, Role::employer, employerCount,
                        targets.employers.internalBankP,
                        out.employers.accounts);

  const int clientCount = targets.clients.count.forPopulation(population);
  detail::fillBankSplit(rng, Role::client, clientCount,
                        targets.clients.internalBankP, out.clients.accounts);

  detail::appendExternal(out.external.platforms, Role::platform,
                         targets.external.platforms.forPopulation(population));

  detail::appendExternal(out.external.processors, Role::processor,
                         targets.external.processors.forPopulation(population));

  detail::appendExternal(
      out.external.ownerBusinesses, Role::business,
      targets.external.ownerBusinesses.forPopulation(population));

  detail::appendExternal(out.external.brokerages, Role::brokerage,
                         targets.external.brokerages.forPopulation(population));

  return out;
}

} // namespace PhantomLedger::synth::counterparties
