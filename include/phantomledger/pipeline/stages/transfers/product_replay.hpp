#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/channels/insurance/rates.hpp"
#include "phantomledger/transfers/legit/ledger/result.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::pipeline::stages::transfers {

using PrimaryAccounts = std::unordered_map<::PhantomLedger::entity::PersonId,
                                           ::PhantomLedger::entity::Key>;

class ProductTxnEmitter {
public:
  using Transaction = ::PhantomLedger::transactions::Transaction;

  ProductTxnEmitter(::PhantomLedger::time::Window window, std::uint64_t seed,
                    ::PhantomLedger::random::Rng &rng,
                    const ::PhantomLedger::transactions::Factory &txf) noexcept;

  [[nodiscard]] std::vector<Transaction>
  premiums(const ::PhantomLedger::pipeline::Entities &entities,
           const PrimaryAccounts &primaryAccounts);

  [[nodiscard]] std::vector<Transaction>
  claims(::PhantomLedger::transfers::insurance::ClaimRates rates,
         const ::PhantomLedger::pipeline::Entities &entities,
         const PrimaryAccounts &primaryAccounts);

  [[nodiscard]] std::vector<Transaction>
  obligations(const ::PhantomLedger::pipeline::Entities &entities,
              const PrimaryAccounts &primaryAccounts);

private:
  ::PhantomLedger::time::Window window_{};
  std::uint64_t seed_ = 0;
  ::PhantomLedger::random::Rng &rng_;
  const ::PhantomLedger::transactions::Factory &txf_;
};

class ProductReplay {
public:
  using PrimaryAccounts =
      ::PhantomLedger::pipeline::stages::transfers::PrimaryAccounts;
  using Transaction = ::PhantomLedger::transactions::Transaction;

  struct InsurancePrograms {
    ::PhantomLedger::transfers::insurance::ClaimRates claimRates{};
  };

  ProductReplay() = default;

  ProductReplay &insurancePrograms(InsurancePrograms value) noexcept;
  ProductReplay &insuranceClaims(
      ::PhantomLedger::transfers::insurance::ClaimRates value) noexcept;

  [[nodiscard]] std::vector<Transaction>
  merge(ProductTxnEmitter &emitter,
        const ::PhantomLedger::pipeline::Entities &entities,
        const PrimaryAccounts &primaryAccounts,
        ::PhantomLedger::transfers::legit::ledger::LegitTxnStreams &legitTxns)
      const;

private:
  InsurancePrograms insurance_{};
};

[[nodiscard]] PrimaryAccounts
primaryAccounts(const ::PhantomLedger::pipeline::Entities &entities);

} // namespace PhantomLedger::pipeline::stages::transfers
