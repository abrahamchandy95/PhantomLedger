#include "phantomledger/synth/products/terms/tax.hpp"
#include "phantomledger/entities/products/event.hpp"

#include "phantomledger/synth/products/obligations.hpp"
#include "phantomledger/synth/products/sampling/amounts.hpp"
#include "phantomledger/synth/products/sampling/dates.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/counterparties/accounts.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace PhantomLedger::synth::products {

namespace {

namespace product = ::PhantomLedger::entity::product;
namespace counterparties = ::PhantomLedger::counterparties;
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

      appendObligation(
          stream, product::ObligationEvent{
                      .personId = person,
                      .direction = product::Direction::outflow,
                      .counterpartyAcct =
                          counterparties::key(counterparties::Tax::irsTreasury),
                      .amount = quarterlyAmount,
                      .timestamp = due,
                      .channel = channels::tag(channels::Product::taxEstimated),
                      .productType = product::ProductType::tax,
                  });
    }
  }
}

struct TaxFiling {
  enum class Kind : std::uint8_t { none, refund, balanceDue };
  Kind kind = Kind::none;
  double amount = 0.0;
  std::int32_t month = 0;
};

[[nodiscard]] TaxFiling sampleTaxFiling(::PhantomLedger::random::Rng &rng,
                                        const TaxTerms &terms) {
  const double settlementRoll = rng.nextDouble();

  if (settlementRoll < terms.filingOutcome.refundP) {
    return TaxFiling{
        .kind = TaxFiling::Kind::refund,
        .amount = samplePaymentAmount(rng, terms.refund.median,
                                      terms.refund.sigma, terms.refund.floor),
        .month = static_cast<std::int32_t>(rng.uniformInt(2, 6)),
    };
  }

  if (settlementRoll <
      terms.filingOutcome.refundP + terms.filingOutcome.balanceDueP) {
    return TaxFiling{
        .kind = TaxFiling::Kind::balanceDue,
        .amount =
            samplePaymentAmount(rng, terms.balanceDue.median,
                                terms.balanceDue.sigma, terms.balanceDue.floor),
        .month = 4,
    };
  }

  return TaxFiling{};
}

void emitTaxFiling(product::ObligationStream &stream,
                   ::PhantomLedger::entity::PersonId person,
                   const TaxFiling &filing,
                   ::PhantomLedger::time::Window window) {
  if (filing.kind == TaxFiling::Kind::none || filing.amount <= 0.0) {
    return;
  }

  const bool isRefund = filing.kind == TaxFiling::Kind::refund;
  const auto direction =
      isRefund ? product::Direction::inflow : product::Direction::outflow;
  const auto channel =
      channels::tag(isRefund ? channels::Product::taxRefund
                             : channels::Product::taxBalanceDue);
  const std::uint32_t productId = isRefund ? 1u : 2u;

  const auto startCal = ::PhantomLedger::time::toCalendarDate(window.start);
  const auto endCal = ::PhantomLedger::time::toCalendarDate(window.endExcl());

  for (int year = startCal.year; year <= endCal.year; ++year) {
    const auto due = midday(year, static_cast<unsigned>(filing.month), 15U);
    if (!inWindow(due, window)) {
      continue;
    }

    appendObligation(stream, product::ObligationEvent{
                                 .personId = person,
                                 .direction = direction,
                                 .counterpartyAcct = counterparties::key(
                                     counterparties::Tax::irsTreasury),
                                 .amount = filing.amount,
                                 .timestamp = due,
                                 .channel = channel,
                                 .productType = product::ProductType::tax,
                                 .productId = productId,
                             });
  }
}

} // namespace

TaxEmitter::TaxEmitter(
    ::PhantomLedger::random::Rng &rng,
    ::PhantomLedger::entity::product::ObligationStream &obligations,
    ::PhantomLedger::time::Window window, TaxTerms terms)
    : rng_{&rng}, obligations_{&obligations}, window_{window},
      terms_{std::move(terms)} {}

[[nodiscard]] bool TaxEmitter::emit(::PhantomLedger::entity::PersonId person,
                                    personaTax::Type persona) {
  double quarterly = 0.0;
  if (rng_->nextDouble() < terms_.adoption.probability(persona)) {
    quarterly = samplePaymentAmount(*rng_, terms_.quarterlyPayment.median,
                                    terms_.quarterlyPayment.sigma,
                                    terms_.quarterlyPayment.floor);
  }

  const auto filing = sampleTaxFiling(*rng_, terms_);

  if (quarterly <= 0.0 && filing.kind == TaxFiling::Kind::none) {
    return false;
  }

  emitTaxQuarterlies(*obligations_, person, quarterly, window_);
  emitTaxFiling(*obligations_, person, filing, window_);

  return true;
}

} // namespace PhantomLedger::synth::products
