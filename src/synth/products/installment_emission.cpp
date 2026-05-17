#include "phantomledger/synth/products/installment_emission.hpp"
#include "phantomledger/entities/products/event.hpp"

#include "phantomledger/synth/products/dates.hpp"
#include "phantomledger/synth/products/obligation_emission.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/products/predicates.hpp"

#include <array>
#include <stdexcept>

namespace PhantomLedger::synth::products {

namespace {

namespace product = ::PhantomLedger::entity::product;
namespace channels = ::PhantomLedger::channels;
namespace enumTax = ::PhantomLedger::taxonomies::enums;

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
                             const InstallmentIssue &issue,
                             ::PhantomLedger::time::Window window) {
  const auto channel = channelForInstallment(issue.productType);

  for (std::int32_t cycle = 0; cycle < issue.termMonths; ++cycle) {
    const auto cycleAnchor =
        ::PhantomLedger::time::addMonths(issue.start, cycle);
    const auto cycleDate = ::PhantomLedger::time::toCalendarDate(cycleAnchor);

    const auto dueDate = midday(cycleDate.year, cycleDate.month,
                                static_cast<unsigned>(issue.paymentDay));

    if (!inWindow(dueDate, window)) {
      continue;
    }

    appendObligation(stream, product::ObligationEvent{
                                 .personId = issue.person,
                                 .direction = product::Direction::outflow,
                                 .counterpartyAcct = issue.counterparty,
                                 .amount = issue.monthlyPayment,
                                 .timestamp = dueDate,
                                 .channel = channel,
                                 .productType = issue.productType,
                             });
  }
}

} // namespace

void addInstallmentProduct(
    ::PhantomLedger::entity::product::LoanTermsLedger &loans,
    ::PhantomLedger::entity::product::ObligationStream &obligations,
    ::PhantomLedger::time::Window window, const InstallmentIssue &issue) {
  loans.set(issue.person, issue.productType, issue.terms);
  emitInstallmentSchedule(obligations, issue, window);
}

} // namespace PhantomLedger::synth::products
