#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/pipeline/data.hpp"
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

  ProductTxnEmitter(time::Window window, std::uint64_t seed, random::Rng &rng,
                    const transactions::Factory &txf) noexcept;

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
