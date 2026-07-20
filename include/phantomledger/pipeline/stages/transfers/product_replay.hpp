#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/pipeline/stages/products.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/channels/insurance/rates.hpp"
#include "phantomledger/transfers/legit/ledger/result.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::pipeline::stages::transfers {

using PrimaryAccounts = std::unordered_map<entity::PersonId, entity::Key>;

class ProductTxnEmitter {
public:
  using Transaction = transactions::Transaction;

  // RAM R2.2.1c: the emitter derives the whole-window obligation stream
  // transiently through `obligationSynthesis.generateWindow(people, ...)`
  // — the world no longer retains it (synthesize keeps only the burden
  // slice). The synthesis MUST be the same configuration that built the
  // world's portfolio terms, or the replay is not a replay.
  ProductTxnEmitter(
      time::Window window, std::uint64_t seed, random::Rng &rng,
      const transactions::Factory &txf, const pipeline::People &people,
      const stages::products::ObligationSynthesis &obligationSynthesis) noexcept;

  [[nodiscard]] std::vector<Transaction>
  premiums(const pipeline::Holdings &holdings,
           const PrimaryAccounts &primaryAccounts);

  [[nodiscard]] std::vector<Transaction>
  claims(::PhantomLedger::transfers::insurance::ClaimRates rates,
         const pipeline::Holdings &holdings,
         const PrimaryAccounts &primaryAccounts);

  [[nodiscard]] std::vector<Transaction>
  obligations(const pipeline::Holdings &holdings,
              const PrimaryAccounts &primaryAccounts);

private:
  time::Window window_{};
  std::uint64_t seed_ = 0;
  random::Rng &rng_;
  const transactions::Factory &txf_;
  const pipeline::People *people_ = nullptr;
  const stages::products::ObligationSynthesis *obligationSynthesis_ = nullptr;
};

class ProductReplay {
public:
  using PrimaryAccounts = stages::transfers::PrimaryAccounts;
  using Transaction = transactions::Transaction;

  struct InsurancePrograms {
    ::PhantomLedger::transfers::insurance::ClaimRates claimRates{};
  };

  ProductReplay() = default;

  ProductReplay &insurancePrograms(InsurancePrograms value) noexcept;
  ProductReplay &insuranceClaims(
      ::PhantomLedger::transfers::insurance::ClaimRates value) noexcept;

  // Read side for the windowed composition: the product cursor source must
  // generate with the same configured claim rates as merge().
  [[nodiscard]] const InsurancePrograms &insurancePrograms() const noexcept {
    return insurance_;
  }

  [[nodiscard]] std::vector<Transaction>
  merge(ProductTxnEmitter &emitter, const pipeline::Holdings &holdings,
        const PrimaryAccounts &primaryAccounts,
        ::PhantomLedger::transfers::legit::ledger::LegitTxnStreams &legitTxns)
      const;

private:
  InsurancePrograms insurance_{};
};

[[nodiscard]] PrimaryAccounts
primaryAccounts(const pipeline::Holdings &holdings);

} // namespace PhantomLedger::pipeline::stages::transfers
