#include "phantomledger/entities/synth/products/build.hpp"

#include "phantomledger/entities/products/event.hpp"
#include "phantomledger/entities/products/installment_terms.hpp"
#include "phantomledger/entities/products/insurance.hpp"
#include "phantomledger/entities/synth/products/institutional.hpp"
#include "phantomledger/primitives/hashing/combine.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/probability/distributions/lognormal.hpp"
#include "phantomledger/probability/distributions/normal.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/products/types.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>

namespace PhantomLedger::entities::synth::products {

namespace {

namespace product = ::PhantomLedger::entity::product;
namespace personaTax = ::PhantomLedger::personas;
namespace channels = ::PhantomLedger::channels;
namespace enumTax = ::PhantomLedger::taxonomies::enums;

[[nodiscard]] constexpr double clamp01(double v) noexcept {
  return std::max(0.0, std::min(1.0, v));
}

[[nodiscard]] double samplePaymentAmount(::PhantomLedger::random::Rng &rng,
                                         double median, double sigma,
                                         double floor) {
  const double raw =
      ::PhantomLedger::probability::distributions::lognormalByMedian(
          rng, median, sigma);
  const double clamped = std::max(floor, raw);

  return std::round(clamped * 100.0) / 100.0;
}

[[nodiscard]] double triangular(::PhantomLedger::random::Rng &rng, double left,
                                double mode, double right) {
  const double u = rng.nextDouble();
  const double fc = (mode - left) / (right - left);

  if (u < fc) {
    return left + std::sqrt(u * (right - left) * (mode - left));
  }

  return right - std::sqrt((1.0 - u) * (right - left) * (right - mode));
}

[[nodiscard]] std::int32_t samplePaymentDay(::PhantomLedger::random::Rng &rng) {
  return static_cast<std::int32_t>(rng.uniformInt(1, 29));
}

[[nodiscard]] std::int32_t
sampleMortgagePaymentDay(::PhantomLedger::random::Rng &rng) {
  if (rng.coin(0.85)) {
    return 1;
  }

  return static_cast<std::int32_t>(rng.uniformInt(2, 6));
}

[[nodiscard]] std::int32_t
sampleMortgageAgeDays(::PhantomLedger::random::Rng &rng) {
  const double rawYears = triangular(rng, 0.5, 5.0, 10.0);
  const auto days = static_cast<std::int32_t>(std::round(rawYears * 365.0));

  return std::max<std::int32_t>(30, days);
}

[[nodiscard]] std::int32_t
sampleAutoTermMonths(::PhantomLedger::random::Rng &rng,
                     const AutoLoanTerm &term, bool isNew) {
  const double mean = isNew ? term.newMeanMonths : term.usedMeanMonths;
  const double sigma = isNew ? term.newSigmaMonths : term.usedSigmaMonths;

  const double raw =
      ::PhantomLedger::probability::distributions::normal(rng, mean, sigma);
  const auto months = static_cast<std::int32_t>(std::round(raw));

  return std::clamp(months, term.minMonths, term.maxMonths);
}

[[nodiscard]] std::int32_t
sampleStudentTermMonths(::PhantomLedger::random::Rng &rng,
                        const StudentLoanPlanMix &planMix,
                        const StudentLoanTerm &term) {
  const double total = planMix.standardP + planMix.extendedP + planMix.idrLikeP;

  if (total <= 0.0) {
    return term.standardMonths;
  }

  const double u = rng.nextDouble() * total;

  if (u < planMix.standardP) {
    return term.standardMonths;
  }

  if (u < planMix.standardP + planMix.extendedP) {
    return term.extendedMonths;
  }

  return rng.coin(term.idr20YearP) ? term.idr20YearMonths
                                   : term.idr25YearMonths;
}

[[nodiscard]] std::int32_t sampleStudentRepaymentAgeMonths(
    ::PhantomLedger::random::Rng &rng, personaTax::Type personaType,
    const StudentLoanTerm &term, const StudentLoanDeferment &deferment) {
  if (personaType == personaTax::Type::student) {
    if (rng.coin(deferment.studentP)) {
      const auto graceMonths =
          std::max<std::int32_t>(1, deferment.gracePeriodMonths);

      return -static_cast<std::int32_t>(
          rng.uniformInt(1, static_cast<std::int64_t>(graceMonths) + 1));
    }

    return static_cast<std::int32_t>(rng.uniformInt(1, 13));
  }

  return static_cast<std::int32_t>(
      rng.uniformInt(1, static_cast<std::int64_t>(term.standardMonths) + 1));
}

[[nodiscard]] ::PhantomLedger::random::Rng
perPersonRng(std::uint64_t baseSeed, ::PhantomLedger::entity::PersonId person) {
  constexpr std::uint64_t kPortfolioSalt = 0xD0E550F150F1C0DEULL;
  const auto seed = ::PhantomLedger::hashing::make(
      baseSeed, kPortfolioSalt, static_cast<std::uint64_t>(person));

  return ::PhantomLedger::random::Rng::fromSeed(seed);
}

[[nodiscard]] double residualPolicyProbability(double targetP,
                                               double anchoredPopulationP,
                                               double anchoredPolicyP) {
  const double target = clamp01(targetP);
  const double anchoredShare = clamp01(anchoredPopulationP);
  const double anchoredPolicy = clamp01(anchoredPolicyP);

  if (target <= 0.0) {
    return 0.0;
  }

  if (anchoredShare >= 1.0) {
    return anchoredPolicy;
  }

  const double residual =
      (target - anchoredShare * anchoredPolicy) / (1.0 - anchoredShare);

  return clamp01(residual);
}

struct DelinquencyKnobs {
  double lateP;
  double missP;
  double partialP;
  double cureP;
  double clusterMult;

  std::int32_t lateDaysMin;
  std::int32_t lateDaysMax;

  double partialMinFrac;
  double partialMaxFrac;
};

template <class Delinquency>
[[nodiscard]] constexpr DelinquencyKnobs
delinquencyKnobs(const Delinquency &d) noexcept {
  return {
      .lateP = d.lateP,
      .missP = d.missP,
      .partialP = d.partialP,
      .cureP = d.cureP,
      .clusterMult = d.clusterMult,
      .lateDaysMin = d.lateDaysMin,
      .lateDaysMax = d.lateDaysMax,
      .partialMinFrac = d.partialMinFrac,
      .partialMaxFrac = d.partialMaxFrac,
  };
}

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

[[nodiscard]] ::PhantomLedger::time::TimePoint midday(int year, unsigned month,
                                                      unsigned day) {
  return ::PhantomLedger::time::makeTime(
      ::PhantomLedger::time::CalendarDate{year, month, day},
      ::PhantomLedger::time::TimeOfDay{12, 0, 0});
}

[[nodiscard]] bool inWindow(::PhantomLedger::time::TimePoint value,
                            ::PhantomLedger::time::Window window) {
  return value >= window.start && value < window.endExcl();
}

void appendObligation(product::ObligationStream &stream,
                      ::PhantomLedger::entity::PersonId person,
                      product::Direction direction,
                      ::PhantomLedger::entity::Key counterparty, double amount,
                      ::PhantomLedger::time::TimePoint timestamp,
                      channels::Tag channel, product::ProductType productType,
                      std::uint32_t productId = 0) {
  product::ObligationEvent ev{};
  ev.personId = person;
  ev.direction = direction;
  ev.counterpartyAcct = counterparty;
  ev.amount = amount;
  ev.timestamp = timestamp;
  ev.channel = channel;
  ev.productType = productType;
  ev.productId = productId;

  stream.append(ev);
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

void addInstallmentProduct(
    product::PortfolioRegistry &out, ::PhantomLedger::entity::PersonId person,
    product::ProductType productType, ::PhantomLedger::entity::Key counterparty,
    ::PhantomLedger::time::TimePoint start, std::int32_t termMonths,
    std::int32_t paymentDay, double monthlyPayment,
    ::PhantomLedger::time::Window window, DelinquencyKnobs knobs) {
  out.loans().set(person, productType, makeInstallmentTerms(knobs));

  emitInstallmentSchedule(out.obligations(), person, counterparty, productType,
                          start, termMonths, paymentDay, monthlyPayment,
                          window);
}

bool tryMortgage(::PhantomLedger::random::Rng &rng, const MortgageTerms &terms,
                 personaTax::Type personaType,
                 ::PhantomLedger::time::Window window,
                 ::PhantomLedger::entity::PersonId person,
                 product::PortfolioRegistry &out, bool &outIssued) {
  outIssued = false;

  if (rng.nextDouble() >= terms.adoption.probability(personaType)) {
    return false;
  }

  outIssued = true;

  const double payment = samplePaymentAmount(rng, terms.payment.median,
                                             terms.payment.sigma, 200.0);

  const std::int32_t paymentDay = sampleMortgagePaymentDay(rng);
  const std::int32_t ageDays = sampleMortgageAgeDays(rng);
  const auto loanStart = window.start - ::PhantomLedger::time::Days{ageDays};

  constexpr std::int32_t kMortgageTermMonths = 360;

  addInstallmentProduct(out, person, product::ProductType::mortgage,
                        institutional::mortgageLender(), loanStart,
                        kMortgageTermMonths, paymentDay, payment, window,
                        delinquencyKnobs(terms.delinquency));

  return true;
}

bool tryAutoLoan(::PhantomLedger::random::Rng &rng, const AutoLoanTerms &terms,
                 personaTax::Type personaType,
                 ::PhantomLedger::time::Window window,
                 ::PhantomLedger::entity::PersonId person,
                 product::PortfolioRegistry &out, bool &outIssued) {
  outIssued = false;

  if (rng.nextDouble() >= terms.adoption.probability(personaType)) {
    return false;
  }

  outIssued = true;

  const bool isNew = rng.nextDouble() < terms.vehicleMix.newVehicleShare;

  const double median =
      isNew ? terms.payment.newMedian : terms.payment.usedMedian;
  const double sigma = isNew ? terms.payment.newSigma : terms.payment.usedSigma;
  const double payment =
      samplePaymentAmount(rng, median, sigma, terms.payment.floor);

  const std::int32_t termMonths = sampleAutoTermMonths(rng, terms.term, isNew);

  const std::int32_t maxAgeDays = std::max<std::int32_t>(30, termMonths * 30);
  const std::int32_t ageDays = static_cast<std::int32_t>(
      rng.uniformInt(30, static_cast<std::int64_t>(maxAgeDays) + 1));

  const auto loanStart = window.start - ::PhantomLedger::time::Days{ageDays};

  addInstallmentProduct(out, person, product::ProductType::autoLoan,
                        institutional::autoLender(), loanStart, termMonths,
                        samplePaymentDay(rng), payment, window,
                        delinquencyKnobs(terms.delinquency));

  return true;
}

bool tryStudentLoan(::PhantomLedger::random::Rng &rng,
                    const StudentLoanTerms &terms, personaTax::Type personaType,
                    ::PhantomLedger::time::Window window,
                    ::PhantomLedger::entity::PersonId person,
                    product::PortfolioRegistry &out) {
  if (rng.nextDouble() >= terms.adoption.probability(personaType)) {
    return false;
  }

  const double payment = samplePaymentAmount(
      rng, terms.payment.median, terms.payment.sigma, terms.payment.floor);

  const std::int32_t termMonths =
      sampleStudentTermMonths(rng, terms.planMix, terms.term);

  const std::int32_t repaymentAgeMonths = sampleStudentRepaymentAgeMonths(
      rng, personaType, terms.term, terms.deferment);

  const auto repaymentStart =
      ::PhantomLedger::time::addMonths(window.start, -repaymentAgeMonths);

  addInstallmentProduct(out, person, product::ProductType::studentLoan,
                        institutional::studentServicer(), repaymentStart,
                        termMonths, samplePaymentDay(rng), payment, window,
                        delinquencyKnobs(terms.delinquency));

  return true;
}

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

bool tryTax(::PhantomLedger::random::Rng &rng, const TaxTerms &terms,
            personaTax::Type personaType, ::PhantomLedger::time::Window window,
            ::PhantomLedger::entity::PersonId person,
            product::PortfolioRegistry &out) {
  double quarterly = 0.0;
  if (rng.nextDouble() < terms.adoption.probability(personaType)) {
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

  emitTaxQuarterlies(out.obligations(), person, quarterly, window);
  emitAnnualTaxSettlement(out.obligations(), person, refundAmount, refundMonth,
                          balanceDueAmount, balanceDueMonth, window);

  return true;
}

bool tryInsurance(::PhantomLedger::random::Rng &rng,
                  const InsuranceTerms &terms, personaTax::Type personaType,
                  bool hasMortgage, bool hasAutoLoan, double mortgageAnchorP,
                  double autoLoanAnchorP,
                  ::PhantomLedger::entity::PersonId person,
                  product::PortfolioRegistry &out) {
  const double autoAnchorPolicyP = terms.loanRequirements.autoLoanRequiresAutoP;
  const double autoIssueP =
      hasAutoLoan ? autoAnchorPolicyP
                  : residualPolicyProbability(
                        terms.adoption.autoProbability(personaType),
                        autoLoanAnchorP, autoAnchorPolicyP);

  std::optional<product::InsurancePolicy> autoPol;
  if (rng.nextDouble() < autoIssueP) {
    const double premium = samplePaymentAmount(
        rng, terms.premiums.autoPolicy.median, terms.premiums.autoPolicy.sigma,
        terms.premiums.autoPolicy.floor);
    autoPol =
        product::autoPolicy(institutional::autoCarrier(), premium,
                            samplePaymentDay(rng), terms.claims.autoAnnualP);
  }

  const double homeAnchorPolicyP = terms.loanRequirements.mortgageRequiresHomeP;
  const double homeIssueP =
      hasMortgage ? homeAnchorPolicyP
                  : residualPolicyProbability(
                        terms.adoption.homeProbability(personaType),
                        mortgageAnchorP, homeAnchorPolicyP);

  std::optional<product::InsurancePolicy> homePol;
  if (rng.nextDouble() < homeIssueP) {
    const double premium = samplePaymentAmount(
        rng, terms.premiums.homePolicy.median, terms.premiums.homePolicy.sigma,
        terms.premiums.homePolicy.floor);
    homePol =
        product::homePolicy(institutional::homeCarrier(), premium,
                            samplePaymentDay(rng), terms.claims.homeAnnualP);
  }

  std::optional<product::InsurancePolicy> lifePol;
  if (rng.nextDouble() < terms.adoption.lifeProbability(personaType)) {
    const double premium = samplePaymentAmount(
        rng, terms.premiums.lifePolicy.median, terms.premiums.lifePolicy.sigma,
        terms.premiums.lifePolicy.floor);
    lifePol = product::lifePolicy(institutional::lifeCarrier(), premium,
                                  samplePaymentDay(rng));
  }

  if (!autoPol.has_value() && !homePol.has_value() && !lifePol.has_value()) {
    return false;
  }

  out.insurance().set(person, product::InsuranceHoldings{
                                  std::move(autoPol),
                                  std::move(homePol),
                                  std::move(lifePol),
                              });

  return true;
}

} // namespace

::PhantomLedger::entity::product::PortfolioRegistry
build(::PhantomLedger::random::Rng &rng, ::PhantomLedger::time::Window window,
      const ::PhantomLedger::entities::synth::personas::Pack &personas,
      const ::PhantomLedger::entity::card::Registry &creditCards,
      std::uint64_t baseSeed, const MortgageTerms &mortgage,
      const AutoLoanTerms &autoLoan, const StudentLoanTerms &studentLoan,
      const TaxTerms &tax, const InsuranceTerms &insurance) {
  (void)rng;
  (void)creditCards;

  ::PhantomLedger::entity::product::PortfolioRegistry out;

  const auto &assignment = personas.assignment;
  const auto population = static_cast<::PhantomLedger::entity::PersonId>(
      assignment.byPerson.size());

  for (::PhantomLedger::entity::PersonId person = 1; person <= population;
       ++person) {
    auto local = perPersonRng(baseSeed, person);
    const auto personaType = assignment.byPerson[person - 1];

    const double mortgageAnchorP = mortgage.adoption.probability(personaType);
    const double autoLoanAnchorP = autoLoan.adoption.probability(personaType);

    bool hasMortgage = false;
    bool hasAutoLoan = false;

    (void)tryMortgage(local, mortgage, personaType, window, person, out,
                      hasMortgage);
    (void)tryAutoLoan(local, autoLoan, personaType, window, person, out,
                      hasAutoLoan);
    (void)tryStudentLoan(local, studentLoan, personaType, window, person, out);
    (void)tryTax(local, tax, personaType, window, person, out);
    (void)tryInsurance(local, insurance, personaType, hasMortgage, hasAutoLoan,
                       mortgageAnchorP, autoLoanAnchorP, person, out);
  }

  out.obligations().sort();

  return out;
}

} // namespace PhantomLedger::entities::synth::products
