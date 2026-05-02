#include "phantomledger/entities/synth/products/tax.hpp"

#include "phantomledger/entities/synth/products/amount_sampling.hpp"
#include "phantomledger/entities/synth/products/dates.hpp"
#include "phantomledger/entities/synth/products/institutional.hpp"
#include "phantomledger/entities/synth/products/obligation_emission.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"

#include <array>
#include <cstddef>
#include <utility>

namespace PhantomLedger::entities::synth::products {

namespace {

namespace product = ::PhantomLedger::entity::product;
namespace channels = ::PhantomLedger::channels;

void emitTaxQuarterlies(product::ObligationStream &stream,
                        ::PhantomLedger::entity::PersonId person,
                        double quarterlyAmount,
                        ::PhantomLedger::time::Window window) {
  if (quarterlyAmount <= 0.0) {
    return;
  }

  static constexpr std::array<std::pair<unsigned, unsigned>, 4> kDueDates{{
      {4, 15},
      {6, 15},
      {9, 15},
      {1, 15},
  }};

  const auto startCal = ::PhantomLedger::time::toCalendarDate(window.start);
  const auto endCal = ::PhantomLedger::time::toCalendarDate(window.endExcl());

  for (int year = startCal.year; year <= endCal.year + 1; ++year) {
    for (std::size_t index = 0; index < kDueDates.size(); ++index) {
      const auto [month, day] = kDueDates[index];
      const int actualYear = index == 3 ? year + 1 : year;
      const auto due = midday(actualYear, month, day);

      if (!inWindow(due, window)) {
        continue;
      }

      appendObligation(stream, person, product::Direction::outflow,
                       institutional::irsTreasury(), quarterlyAmount, due,
                       channels::tag(channels::Product::taxEstimated),
                       product::ProductType::tax);
    }
  }
}

void emitTaxRefund(product::ObligationStream &stream,
                   ::PhantomLedger::entity::PersonId person, double amount,
                   std::int32_t month, ::PhantomLedger::time::Window window) {
  if (amount <= 0.0) {
    return;
  }

  const auto startCal = ::PhantomLedger::time::toCalendarDate(window.start);
  const auto endCal = ::PhantomLedger::time::toCalendarDate(window.endExcl());

  for (int year = startCal.year; year <= endCal.year; ++year) {
    const auto due = midday(year, static_cast<unsigned>(month), 15U);
    if (!inWindow(due, window)) {
      continue;
    }

    appendObligation(stream, person, product::Direction::inflow,
                     institutional::irsTreasury(), amount, due,
                     channels::tag(channels::Product::taxRefund),
                     product::ProductType::tax, 1);
  }
}

void emitTaxBalanceDue(product::ObligationStream &stream,
                       ::PhantomLedger::entity::PersonId person, double amount,
                       std::int32_t month,
                       ::PhantomLedger::time::Window window) {
  if (amount <= 0.0) {
    return;
  }

  const auto startCal = ::PhantomLedger::time::toCalendarDate(window.start);
  const auto endCal = ::PhantomLedger::time::toCalendarDate(window.endExcl());

  for (int year = startCal.year; year <= endCal.year; ++year) {
    const auto due = midday(year, static_cast<unsigned>(month), 15U);
    if (!inWindow(due, window)) {
      continue;
    }

    appendObligation(stream, person, product::Direction::outflow,
                     institutional::irsTreasury(), amount, due,
                     channels::tag(channels::Product::taxBalanceDue),
                     product::ProductType::tax, 2);
  }
}

} // namespace

TaxEmitter::TaxEmitter(
    ::PhantomLedger::random::Rng &rng,
    ::PhantomLedger::entity::product::PortfolioRegistry &portfolios,
    ::PhantomLedger::time::Window window, TaxTerms terms)
    : rng_{&rng}, portfolios_{&portfolios}, window_{window},
      terms_{std::move(terms)} {}

[[nodiscard]] bool TaxEmitter::emit(::PhantomLedger::entity::PersonId person,
                                    personaTax::Type persona) {
  double quarterly = 0.0;
  if (rng_->nextDouble() < terms_.adoption.probability(persona)) {
    quarterly = samplePaymentAmount(*rng_, terms_.quarterlyPayment.median,
                                    terms_.quarterlyPayment.sigma,
                                    terms_.quarterlyPayment.floor);
  }

  double refundAmount = 0.0;
  std::int32_t refundMonth = 3;
  double balanceDueAmount = 0.0;
  std::int32_t balanceDueMonth = 4;

  const double settlementRoll = rng_->nextDouble();
  if (settlementRoll < terms_.filingOutcome.refundP) {
    refundAmount = samplePaymentAmount(
        *rng_, terms_.refund.median, terms_.refund.sigma, terms_.refund.floor);
    refundMonth = static_cast<std::int32_t>(rng_->uniformInt(2, 6));
  } else if (settlementRoll <
             terms_.filingOutcome.refundP + terms_.filingOutcome.balanceDueP) {
    balanceDueAmount =
        samplePaymentAmount(*rng_, terms_.balanceDue.median,
                            terms_.balanceDue.sigma, terms_.balanceDue.floor);
    balanceDueMonth = 4;
  }

  if (quarterly <= 0.0 && refundAmount <= 0.0 && balanceDueAmount <= 0.0) {
    return false;
  }

  auto &obligations = portfolios_->obligations();
  emitTaxQuarterlies(obligations, person, quarterly, window_);
  emitTaxRefund(obligations, person, refundAmount, refundMonth, window_);
  emitTaxBalanceDue(obligations, person, balanceDueAmount, balanceDueMonth,
                    window_);

  return true;
}

} // namespace PhantomLedger::entities::synth::products
