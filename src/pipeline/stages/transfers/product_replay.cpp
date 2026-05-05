#include "phantomledger/pipeline/stages/transfers/product_replay.hpp"

#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transfers/insurance/claims.hpp"
#include "phantomledger/transfers/insurance/premiums.hpp"
#include "phantomledger/transfers/legit/ledger/streams.hpp"
#include "phantomledger/transfers/obligations/schedule.hpp"

#include <utility>

namespace PhantomLedger::pipeline::stages::transfers {

namespace {

namespace insurance = ::PhantomLedger::transfers::insurance;
namespace legit_ledger = ::PhantomLedger::transfers::legit::ledger;
namespace obligations = ::PhantomLedger::transfers::obligations;

} // namespace

ProductReplay &
ProductReplay::insurancePrograms(InsurancePrograms value) noexcept {
  insurance_ = value;
  return *this;
}

ProductReplay &ProductReplay::insuranceClaims(
    ::PhantomLedger::transfers::insurance::ClaimRates value) noexcept {
  insurance_.claimRates = value;
  return *this;
}

std::vector<ProductReplay::Transaction>
ProductReplay::merge(::PhantomLedger::time::Window window, std::uint64_t seed,
                     ::PhantomLedger::random::Rng &rng,
                     const ::PhantomLedger::pipeline::Entities &entities,
                     const ::PhantomLedger::pipeline::Infra &infra,
                     const PrimaryAccounts &primaryAccounts,
                     ::PhantomLedger::transfers::legit::ledger::TransfersPayload
                         &legitPayload) const {
  std::vector<Transaction> stream = std::move(legitPayload.replaySortedTxns);

  ::PhantomLedger::transactions::Factory txf{rng, &infra.router,
                                             &infra.ringInfra};

  insurance::Population insPop{.primaryAccounts = &primaryAccounts};

  auto premiumTxns =
      insurance::premiums(window, rng, txf, entities.portfolios, insPop);
  stream = legit_ledger::mergeReplaySorted(std::move(stream), premiumTxns);

  ::PhantomLedger::random::RngFactory claimsFactory{seed};
  insurance::ClaimScheduler claimScheduler{insurance_.claimRates, rng, txf,
                                           claimsFactory};
  auto claimTxns = claimScheduler.generate(window, entities.portfolios, insPop);
  stream = legit_ledger::mergeReplaySorted(std::move(stream), claimTxns);

  obligations::Population oblPop{.primaryAccounts = &primaryAccounts};
  obligations::Scheduler obligationsScheduler{rng, txf};
  auto obligationTxns =
      obligationsScheduler.generate(entities.portfolios,
                                    ::PhantomLedger::time::HalfOpenInterval{
                                        .start = window.start,
                                        .endExcl = window.endExcl(),
                                    },
                                    oblPop);
  stream = legit_ledger::mergeReplaySorted(std::move(stream), obligationTxns);

  return stream;
}

ProductReplay::PrimaryAccounts
primaryAccounts(const ::PhantomLedger::pipeline::Entities &entities) {
  ProductReplay::PrimaryAccounts out;
  out.reserve(entities.accounts.registry.records.size());

  for (const auto &record : entities.accounts.registry.records) {
    if (record.owner == ::PhantomLedger::entity::invalidPerson) {
      continue;
    }

    out.try_emplace(record.owner, record.id);
  }

  return out;
}

} // namespace PhantomLedger::pipeline::stages::transfers
