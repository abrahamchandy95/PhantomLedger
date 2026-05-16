#include "phantomledger/pipeline/stages/transfers/product_replay.hpp"

#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/transfers/channels/insurance/claims.hpp"
#include "phantomledger/transfers/channels/insurance/premiums.hpp"
#include "phantomledger/transfers/channels/obligations/schedule.hpp"
#include "phantomledger/transfers/legit/ledger/streams.hpp"

#include <utility>

namespace PhantomLedger::pipeline::stages::transfers {

namespace {

namespace insurance = ::PhantomLedger::transfers::insurance;
namespace legit_ledger = ::PhantomLedger::transfers::legit::ledger;
namespace obligations = ::PhantomLedger::transfers::obligations;

} // namespace

ProductTxnEmitter::ProductTxnEmitter(
    ::PhantomLedger::time::Window window, std::uint64_t seed,
    ::PhantomLedger::random::Rng &rng,
    const ::PhantomLedger::transactions::Factory &txf) noexcept
    : window_(window), seed_(seed), rng_(rng), txf_(txf) {}

std::vector<ProductTxnEmitter::Transaction>
ProductTxnEmitter::premiums(const ::PhantomLedger::pipeline::Holdings &holdings,
                            const PrimaryAccounts &primaryAccounts) {
  insurance::Population population{.primaryAccounts = &primaryAccounts};
  insurance::PremiumGenerator generator{rng_, txf_};
  return generator.generate(window_, holdings.portfolios.insurance(),
                            holdings.portfolios.loans(), population);
}

std::vector<ProductTxnEmitter::Transaction> ProductTxnEmitter::claims(
    ::PhantomLedger::transfers::insurance::ClaimRates rates,
    const ::PhantomLedger::pipeline::Holdings &holdings,
    const PrimaryAccounts &primaryAccounts) {
  insurance::Population population{.primaryAccounts = &primaryAccounts};
  ::PhantomLedger::random::RngFactory claimsFactory{seed_};
  insurance::ClaimScheduler scheduler{rates, rng_, txf_, claimsFactory};
  return scheduler.generate(window_, holdings.portfolios.insurance(),
                            population);
}

std::vector<ProductTxnEmitter::Transaction> ProductTxnEmitter::obligations(
    const ::PhantomLedger::pipeline::Holdings &holdings,
    const PrimaryAccounts &primaryAccounts) {
  obligations::Population population{.primaryAccounts = &primaryAccounts};
  obligations::Scheduler scheduler{rng_, txf_};
  return scheduler.generate(holdings.portfolios.loans(),
                            holdings.portfolios.obligations(),
                            ::PhantomLedger::time::HalfOpenInterval{
                                .start = window_.start,
                                .endExcl = window_.endExcl(),
                            },
                            population);
}

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
ProductReplay::merge(ProductTxnEmitter &emitter,
                     const ::PhantomLedger::pipeline::Holdings &holdings,
                     const PrimaryAccounts &primaryAccounts,
                     ::PhantomLedger::transfers::legit::ledger::LegitTxnStreams
                         &legitTxns) const {
  std::vector<Transaction> stream = std::move(legitTxns.replaySortedTxns);

  stream = legit_ledger::mergeReplaySorted(
      std::move(stream), emitter.premiums(holdings, primaryAccounts));

  stream = legit_ledger::mergeReplaySorted(
      std::move(stream),
      emitter.claims(insurance_.claimRates, holdings, primaryAccounts));

  stream = legit_ledger::mergeReplaySorted(
      std::move(stream), emitter.obligations(holdings, primaryAccounts));

  return stream;
}

PrimaryAccounts
primaryAccounts(const ::PhantomLedger::pipeline::Holdings &holdings) {
  PrimaryAccounts out;
  out.reserve(holdings.accounts.registry.records.size());

  for (const auto &record : holdings.accounts.registry.records) {
    if (record.owner == ::PhantomLedger::entity::invalidPerson) {
      continue;
    }

    out.try_emplace(record.owner, record.id);
  }

  return out;
}

} // namespace PhantomLedger::pipeline::stages::transfers
