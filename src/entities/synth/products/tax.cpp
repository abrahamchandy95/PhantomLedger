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

void emitAnnualTaxSettlement(product::ObligationStream &stream,
                             ::PhantomLedger::entity::PersonId person,
                             double refundAmount, std::int32_t refundMonth,
                             double balanceDueAmount,
                             std::int32_t balanceDueMonth,
                             ::PhantomLedger::time::Window window) {
  const auto startCal = ::PhantomLedger::time::toCalendarDate(window.start);
  const auto endCal = ::PhantomLedger::time::toCalendarDate(window.endExcl());

  for (int year = startCal.year; year <= endCal.year; ++year) {
    if (refundAmount > 0.0) {
      const auto due = midday(year, static_cast<unsigned>(refundMonth), 15U);

      if (inWindow(due, window)) {
        appendObligation(stream, person, product::Direction::inflow,
                         institutional::irsTreasury(), refundAmount, due,
                         channels::tag(channels::Product::taxRefund),
                         product::ProductType::tax, 1);
      }
    }

    if (balanceDueAmount > 0.0) {
      const auto due =
          midday(year, static_cast<unsigned>(balanceDueMonth), 15U);

      if (inWindow(due, window)) {
        appendObligation(stream, person, product::Direction::outflow,
                         institutional::irsTreasury(), balanceDueAmount, due,
                         channels::tag(channels::Product::taxBalanceDue),
                         product::ProductType::tax, 2);
      }
    }
  }
}

} // namespace

[[nodiscard]] bool
emitTax(::PhantomLedger::random::Rng &rng,
        ::PhantomLedger::entity::product::PortfolioRegistry &portfolios,
        ::PhantomLedger::entity::PersonId person, personaTax::Type persona,
        ::PhantomLedger::time::Window window, const TaxTerms &terms) {
  double quarterly = 0.0;
  if (rng.nextDouble() < terms.adoption.probability(persona)) {
    quarterly = samplePaymentAmount(rng, terms.quarterlyPayment.median,
                                    terms.quarterlyPayment.sigma,
                                    terms.quarterlyPayment.floor);
  }

  double refundAmount = 0.0;
  std::int32_t refundMonth = 3;
  double balanceDueAmount = 0.0;
  std::int32_t balanceDueMonth = 4;

  const double settlementRoll = rng.nextDouble();
  if (settlementRoll < terms.filingOutcome.refundP) {
    refundAmount = samplePaymentAmount(rng, terms.refund.median,
                                       terms.refund.sigma, terms.refund.floor);
    refundMonth = static_cast<std::int32_t>(rng.uniformInt(2, 6));
  } else if (settlementRoll <
             terms.filingOutcome.refundP + terms.filingOutcome.balanceDueP) {
    balanceDueAmount =
        samplePaymentAmount(rng, terms.balanceDue.median,
                            terms.balanceDue.sigma, terms.balanceDue.floor);
    balanceDueMonth = 4;
  }

  if (quarterly <= 0.0 && refundAmount <= 0.0 && balanceDueAmount <= 0.0) {
    return false;
  }

  emitTaxQuarterlies(portfolios.obligations(), person, quarterly, window);
  emitAnnualTaxSettlement(portfolios.obligations(), person, refundAmount,
                          refundMonth, balanceDueAmount, balanceDueMonth,
                          window);

  return true;
}

} // namespace PhantomLedger::entities::synth::products
