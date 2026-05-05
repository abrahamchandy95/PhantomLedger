#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/insurance/rates.hpp"
#include "phantomledger/transfers/legit/ledger/result.hpp"

#include <unordered_map>
#include <vector>

namespace PhantomLedger::pipeline::stages::transfers {

class ProductReplay {
public:
  using PrimaryAccounts = std::unordered_map<::PhantomLedger::entity::PersonId,
                                             ::PhantomLedger::entity::Key>;
  using Transaction = ::PhantomLedger::transactions::Transaction;

  struct InsurancePrograms {
    ::PhantomLedger::transfers::insurance::ClaimRates claimRates{};
  };

  ProductReplay() = default;

  ProductReplay &insurancePrograms(InsurancePrograms value) noexcept;
  ProductReplay &insuranceClaims(
      ::PhantomLedger::transfers::insurance::ClaimRates value) noexcept;

  [[nodiscard]] std::vector<Transaction>
  merge(::PhantomLedger::time::Window window, std::uint64_t seed,
        ::PhantomLedger::random::Rng &rng,
        const ::PhantomLedger::pipeline::Entities &entities,
        const ::PhantomLedger::pipeline::Infra &infra,
        const PrimaryAccounts &primaryAccounts,
        ::PhantomLedger::transfers::legit::ledger::TransfersPayload
            &legitPayload) const;

private:
  InsurancePrograms insurance_{};
};

[[nodiscard]] ProductReplay::PrimaryAccounts
primaryAccounts(const ::PhantomLedger::pipeline::Entities &entities);

} // namespace PhantomLedger::pipeline::stages::transfers
