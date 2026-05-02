#include "phantomledger/entities/synth/products/installment_emission.hpp"

#include "phantomledger/entities/synth/products/dates.hpp"
#include "phantomledger/entities/synth/products/obligation_emission.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/products/predicates.hpp"

#include <array>
#include <stdexcept>

namespace PhantomLedger::entities::synth::products {

namespace {

namespace product = ::PhantomLedger::entity::product;
namespace channels = ::PhantomLedger::channels;
namespace enumTax = ::PhantomLedger::taxonomies::enums;

[[nodiscard]] product::InstallmentTerms
makeInstallmentTerms(const DelinquencyKnobs &knobs) {
  product::InstallmentTerms terms{};
  terms.lateP = knobs.lateP;
  terms.lateDaysMin = knobs.lateDaysMin;
  terms.lateDaysMax = knobs.lateDaysMax;
  terms.missP = knobs.missP;
  terms.partialP = knobs.partialP;
  terms.cureP = knobs.cureP;
  terms.partialMinFrac = knobs.partialMinFrac;
  terms.partialMaxFrac = knobs.partialMaxFrac;
  terms.clusterMult = knobs.clusterMult;

  return terms;
}

[[nodiscard]] constexpr auto makeInstallmentChannelTable() noexcept {
  using ProductChannel = channels::Product;

  std::array<channels::Tag, ::PhantomLedger::products::kProductTypeCount> out{};

  out[enumTax::toIndex(product::ProductType::mortgage)] =
      channels::tag(ProductChannel::mortgage);
  out[enumTax::toIndex(product::ProductType::autoLoan)] =
      channels::tag(ProductChannel::autoLoan);
  out[enumTax::toIndex(product::ProductType::studentLoan)] =
      channels::tag(ProductChannel::studentLoan);

  return out;
}

inline constexpr auto kInstallmentChannels = makeInstallmentChannelTable();

[[nodiscard]] channels::Tag
channelForInstallment(product::ProductType productType) {
  const auto index = enumTax::toIndex(productType);

  if (index >= kInstallmentChannels.size() ||
      !::PhantomLedger::products::isInstallmentLoan(productType)) {
    throw std::invalid_argument(
        "emitInstallmentSchedule requires an installment product type");
  }

  return kInstallmentChannels[index];
}

void emitInstallmentSchedule(product::ObligationStream &stream,
                             ::PhantomLedger::entity::PersonId person,
                             ::PhantomLedger::entity::Key counterparty,
                             product::ProductType productType,
                             ::PhantomLedger::time::TimePoint loanStart,
                             std::int32_t termMonths, std::int32_t paymentDay,
                             double monthlyPayment,
                             ::PhantomLedger::time::Window window) {
  const auto channel = channelForInstallment(productType);

  for (std::int32_t cycle = 0; cycle < termMonths; ++cycle) {
    const auto cycleAnchor = ::PhantomLedger::time::addMonths(loanStart, cycle);
    const auto cycleDate = ::PhantomLedger::time::toCalendarDate(cycleAnchor);

    const auto dueDate = midday(cycleDate.year, cycleDate.month,
                                static_cast<unsigned>(paymentDay));

    if (!inWindow(dueDate, window)) {
      continue;
    }

    appendObligation(stream, person, product::Direction::outflow, counterparty,
                     monthlyPayment, dueDate, channel, productType);
  }
}

} // namespace

void addInstallmentProduct(
    ::PhantomLedger::entity::product::PortfolioRegistry &out,
    ::PhantomLedger::entity::PersonId person,
    ::PhantomLedger::entity::product::ProductType productType,
    ::PhantomLedger::entity::Key counterparty,
    ::PhantomLedger::time::TimePoint start, std::int32_t termMonths,
    std::int32_t paymentDay, double monthlyPayment,
    ::PhantomLedger::time::Window window, DelinquencyKnobs knobs) {
  out.loans().set(person, productType, makeInstallmentTerms(knobs));

  emitInstallmentSchedule(out.obligations(), person, counterparty, productType,
                          start, termMonths, paymentDay, monthlyPayment,
                          window);
}

} // namespace PhantomLedger::entities::synth::products
