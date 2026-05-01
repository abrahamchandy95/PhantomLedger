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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace PhantomLedger::entities::synth::products {

namespace {

namespace product = ::PhantomLedger::entity::product;
namespace personas = ::PhantomLedger::personas;

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
  // Round to cents.
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
                     const AutoLoanConfig &cfg, bool isNew) {
  const double mean = isNew ? cfg.newTermMeanMonths : cfg.usedTermMeanMonths;
  const double sigma = isNew ? cfg.newTermSigmaMonths : cfg.usedTermSigmaMonths;
  const double raw =
      ::PhantomLedger::probability::distributions::normal(rng, mean, sigma);
  const auto term = static_cast<std::int32_t>(std::round(raw));
  return std::clamp(term, cfg.termMinMonths, cfg.termMaxMonths);
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
  double lateP, missP, partialP, cureP, clusterMult;
  std::int32_t lateDaysMin, lateDaysMax;
  double partialMinFrac, partialMaxFrac;
};

[[nodiscard]] product::InstallmentTerms
makeInstallmentTerms(const DelinquencyKnobs &k) {
  product::InstallmentTerms t{};
  t.lateP = k.lateP;
  t.lateDaysMin = k.lateDaysMin;
  t.lateDaysMax = k.lateDaysMax;
  t.missP = k.missP;
  t.partialP = k.partialP;
  t.cureP = k.cureP;
  t.partialMinFrac = k.partialMinFrac;
  t.partialMaxFrac = k.partialMaxFrac;
  t.clusterMult = k.clusterMult;
  return t;
}

[[nodiscard]] constexpr ::PhantomLedger::channels::Tag
channelForInstallment(product::ProductType productType) {
  using ProductChannel = ::PhantomLedger::channels::Product;

  switch (productType) {
  case product::ProductType::mortgage:
    return ::PhantomLedger::channels::tag(ProductChannel::mortgage);
  case product::ProductType::autoLoan:
    return ::PhantomLedger::channels::tag(ProductChannel::autoLoan);
  case product::ProductType::studentLoan:
    return ::PhantomLedger::channels::tag(ProductChannel::studentLoan);
  case product::ProductType::unknown:
  case product::ProductType::insurance:
  case product::ProductType::tax:
    break;
  }

  throw std::invalid_argument(
      "emitInstallmentSchedule requires an installment product type");
}

void emitInstallmentSchedule(product::ObligationStream &stream,
                             ::PhantomLedger::entity::PersonId person,
                             ::PhantomLedger::entity::Key counterparty,
                             product::ProductType productType,
                             ::PhantomLedger::time::TimePoint loanStart,
                             std::int32_t termMonths, std::int32_t paymentDay,
                             double monthlyPayment,
                             ::PhantomLedger::time::Window window) {
  const auto windowStart = window.start;
  const auto windowEndExcl = window.endExcl();

  for (std::int32_t cycle = 0; cycle < termMonths; ++cycle) {
    const auto cycleAnchor = ::PhantomLedger::time::addMonths(loanStart, cycle);
    // Project to the configured payment day of that month.
    const auto dueDate = ::PhantomLedger::time::makeTime(
        ::PhantomLedger::time::CalendarDate{
            ::PhantomLedger::time::toCalendarDate(cycleAnchor).year,
            ::PhantomLedger::time::toCalendarDate(cycleAnchor).month,
            static_cast<unsigned>(paymentDay)},
        ::PhantomLedger::time::TimeOfDay{12, 0, 0});

    if (dueDate < windowStart || dueDate >= windowEndExcl) {
      continue;
    }

    product::ObligationEvent ev{};
    ev.personId = person;
    ev.direction = product::Direction::outflow;
    ev.counterpartyAcct = counterparty;
    ev.amount = monthlyPayment;
    ev.timestamp = dueDate;
    ev.channel = channelForInstallment(productType);
    ev.productType = productType;
    ev.productId = 0;

    stream.append(ev);
  }
}

bool tryMortgage(::PhantomLedger::random::Rng &rng, const MortgageConfig &cfg,
                 personas::Type personaType,
                 ::PhantomLedger::time::Window window,
                 ::PhantomLedger::entity::PersonId person,
                 product::PortfolioRegistry &out, bool &outIssued) {
  outIssued = false;

  if (rng.nextDouble() >= cfg.ownership.at(personaType)) {
    return false;
  }
  outIssued = true;

  const double payment =
      samplePaymentAmount(rng, cfg.paymentMedian, cfg.paymentSigma, 200.0);

  const std::int32_t paymentDay = sampleMortgagePaymentDay(rng);
  const std::int32_t ageDays = sampleMortgageAgeDays(rng);
  const auto loanStart = window.start - ::PhantomLedger::time::Days{ageDays};

  // Mortgages are 30-year products.
  constexpr std::int32_t kMortgageTermMonths = 360;

  out.loans().set(person, product::ProductType::mortgage,
                  makeInstallmentTerms({
                      .lateP = cfg.lateP,
                      .missP = cfg.missP,
                      .partialP = cfg.partialP,
                      .cureP = cfg.cureP,
                      .clusterMult = cfg.clusterMult,
                      .lateDaysMin = cfg.lateDaysMin,
                      .lateDaysMax = cfg.lateDaysMax,
                      .partialMinFrac = cfg.partialMinFrac,
                      .partialMaxFrac = cfg.partialMaxFrac,
                  }));

  emitInstallmentSchedule(out.obligations(), person,
                          institutional::mortgageLender(),
                          product::ProductType::mortgage, loanStart,
                          kMortgageTermMonths, paymentDay, payment, window);

  return true;
}

bool tryAutoLoan(::PhantomLedger::random::Rng &rng, const AutoLoanConfig &cfg,
                 personas::Type personaType,
                 ::PhantomLedger::time::Window window,
                 ::PhantomLedger::entity::PersonId person,
                 product::PortfolioRegistry &out, bool &outIssued) {
  outIssued = false;

  if (rng.nextDouble() >= cfg.ownership.at(personaType)) {
    return false;
  }
  outIssued = true;

  const bool isNew = rng.nextDouble() < cfg.newVehicleShare;

  const double median = isNew ? cfg.newPaymentMedian : cfg.usedPaymentMedian;
  const double sigma = isNew ? cfg.newPaymentSigma : cfg.usedPaymentSigma;
  const double payment = samplePaymentAmount(rng, median, sigma, 100.0);

  const std::int32_t termMonths = sampleAutoTermMonths(rng, cfg, isNew);

  const std::int32_t maxAgeDays = std::max<std::int32_t>(30, termMonths * 30);
  const std::int32_t ageDays = static_cast<std::int32_t>(
      rng.uniformInt(30, static_cast<std::int64_t>(maxAgeDays) + 1));

  const auto loanStart = window.start - ::PhantomLedger::time::Days{ageDays};

  out.loans().set(person, product::ProductType::autoLoan,
                  makeInstallmentTerms({
                      .lateP = cfg.lateP,
                      .missP = cfg.missP,
                      .partialP = cfg.partialP,
                      .cureP = cfg.cureP,
                      .clusterMult = cfg.clusterMult,
                      .lateDaysMin = cfg.lateDaysMin,
                      .lateDaysMax = cfg.lateDaysMax,
                      .partialMinFrac = cfg.partialMinFrac,
                      .partialMaxFrac = cfg.partialMaxFrac,
                  }));

  emitInstallmentSchedule(out.obligations(), person,
                          institutional::autoLender(),
                          product::ProductType::autoLoan, loanStart, termMonths,
                          samplePaymentDay(rng), payment, window);

  return true;
}

[[nodiscard]] std::int32_t sampleStudentTerm(::PhantomLedger::random::Rng &rng,
                                             const StudentLoanConfig &cfg) {
  const double total = cfg.standardPlanP + cfg.extendedPlanP + cfg.idrLikePlanP;
  const double u = rng.nextDouble() * total;

  if (u < cfg.standardPlanP) {
    return cfg.standardTermMonths;
  }
  if (u < cfg.standardPlanP + cfg.extendedPlanP) {
    return cfg.extendedTermMonths;
  }
  return rng.coin(cfg.idr20YearP) ? cfg.idr20YearTermMonths
                                  : cfg.idr25YearTermMonths;
}

bool tryStudentLoan(::PhantomLedger::random::Rng &rng,
                    const StudentLoanConfig &cfg, personas::Type personaType,
                    ::PhantomLedger::time::Window window,
                    ::PhantomLedger::entity::PersonId person,
                    product::PortfolioRegistry &out) {
  if (rng.nextDouble() >= cfg.ownership.at(personaType)) {
    return false;
  }

  const double payment =
      samplePaymentAmount(rng, cfg.paymentMedian, cfg.paymentSigma, 50.0);

  const std::int32_t termMonths = sampleStudentTerm(rng, cfg);

  std::int32_t repaymentAgeMonths;
  if (personaType == personas::Type::student) {
    if (rng.coin(cfg.studentDefermentP)) {
      repaymentAgeMonths = -static_cast<std::int32_t>(rng.uniformInt(1, 13));
    } else {
      repaymentAgeMonths = static_cast<std::int32_t>(rng.uniformInt(1, 13));
    }
  } else {
    repaymentAgeMonths = static_cast<std::int32_t>(
        rng.uniformInt(1, static_cast<std::int64_t>(termMonths) + 1));
  }

  const auto repaymentStart =
      ::PhantomLedger::time::addMonths(window.start, -repaymentAgeMonths);

  out.loans().set(person, product::ProductType::studentLoan,
                  makeInstallmentTerms({
                      .lateP = cfg.lateP,
                      .missP = cfg.missP,
                      .partialP = cfg.partialP,
                      .cureP = cfg.cureP,
                      .clusterMult = cfg.clusterMult,
                      .lateDaysMin = cfg.lateDaysMin,
                      .lateDaysMax = cfg.lateDaysMax,
                      .partialMinFrac = cfg.partialMinFrac,
                      .partialMaxFrac = cfg.partialMaxFrac,
                  }));

  emitInstallmentSchedule(out.obligations(), person,
                          institutional::studentServicer(),
                          product::ProductType::studentLoan, repaymentStart,
                          termMonths, samplePaymentDay(rng), payment, window);

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
      {1, 15}, // Jan of the following year
  }};

  const auto windowStart = window.start;
  const auto windowEndExcl = window.endExcl();
  const auto startCal = ::PhantomLedger::time::toCalendarDate(windowStart);
  const auto endCal = ::PhantomLedger::time::toCalendarDate(windowEndExcl);

  for (int year = startCal.year; year <= endCal.year + 1; ++year) {
    for (std::size_t i = 0; i < kDueDates.size(); ++i) {
      const auto [month, day] = kDueDates[i];
      // Slot 3 (Jan 15) belongs to the following tax year, so emit it
      // against year+1 of the calendar.
      const int actualYear = (i == 3) ? (year + 1) : year;

      const auto due = ::PhantomLedger::time::makeTime(
          ::PhantomLedger::time::CalendarDate{actualYear, month, day},
          ::PhantomLedger::time::TimeOfDay{12, 0, 0});

      if (due < windowStart || due >= windowEndExcl) {
        continue;
      }

      product::ObligationEvent ev{};
      ev.personId = person;
      ev.direction = product::Direction::outflow;
      ev.counterpartyAcct = institutional::irsTreasury();
      ev.amount = quarterlyAmount;
      ev.timestamp = due;
      ev.channel = ::PhantomLedger::channels::tag(
          ::PhantomLedger::channels::Product::taxEstimated);
      ev.productType = product::ProductType::tax;
      ev.productId = 0;
      stream.append(ev);
    }
  }
}

void emitAnnualTaxSettlement(product::ObligationStream &stream,
                             ::PhantomLedger::entity::PersonId person,
                             double refundAmount, std::int32_t refundMonth,
                             double balanceDueAmount,
                             std::int32_t balanceDueMonth,
                             ::PhantomLedger::time::Window window) {
  const auto windowStart = window.start;
  const auto windowEndExcl = window.endExcl();
  const auto startCal = ::PhantomLedger::time::toCalendarDate(windowStart);
  const auto endCal = ::PhantomLedger::time::toCalendarDate(windowEndExcl);

  for (int year = startCal.year; year <= endCal.year; ++year) {
    if (refundAmount > 0.0) {
      const auto due = ::PhantomLedger::time::makeTime(
          ::PhantomLedger::time::CalendarDate{
              year, static_cast<unsigned>(refundMonth), 15U},
          ::PhantomLedger::time::TimeOfDay{12, 0, 0});
      if (due >= windowStart && due < windowEndExcl) {
        product::ObligationEvent ev{};
        ev.personId = person;
        ev.direction = product::Direction::inflow;
        ev.counterpartyAcct = institutional::irsTreasury();
        ev.amount = refundAmount;
        ev.timestamp = due;
        ev.channel = ::PhantomLedger::channels::tag(
            ::PhantomLedger::channels::Product::taxRefund);
        ev.productType = product::ProductType::tax;
        ev.productId = 1; // disambiguate from quarterlies
        stream.append(ev);
      }
    }
    if (balanceDueAmount > 0.0) {
      const auto due = ::PhantomLedger::time::makeTime(
          ::PhantomLedger::time::CalendarDate{
              year, static_cast<unsigned>(balanceDueMonth), 15U},
          ::PhantomLedger::time::TimeOfDay{12, 0, 0});
      if (due >= windowStart && due < windowEndExcl) {
        product::ObligationEvent ev{};
        ev.personId = person;
        ev.direction = product::Direction::outflow;
        ev.counterpartyAcct = institutional::irsTreasury();
        ev.amount = balanceDueAmount;
        ev.timestamp = due;
        ev.channel = ::PhantomLedger::channels::tag(
            ::PhantomLedger::channels::Product::taxBalanceDue);
        ev.productType = product::ProductType::tax;
        ev.productId = 2;
        stream.append(ev);
      }
    }
  }
}

bool tryTax(::PhantomLedger::random::Rng &rng, const TaxConfig &cfg,
            personas::Type personaType, ::PhantomLedger::time::Window window,
            ::PhantomLedger::entity::PersonId person,
            product::PortfolioRegistry &out) {
  // Quarterlies — gated on persona ownership probability.
  double quarterly = 0.0;
  if (rng.nextDouble() < cfg.ownership.at(personaType)) {
    const double raw =
        ::PhantomLedger::probability::distributions::lognormalByMedian(
            rng, cfg.quarterlyAmountMedian, cfg.quarterlyAmountSigma);
    quarterly = std::round(std::max(100.0, raw) * 100.0) / 100.0;
  }

  // Annual settlement — refund vs. balance-due is mutually exclusive.
  double refundAmount = 0.0;
  std::int32_t refundMonth = 3;
  double balanceDueAmount = 0.0;
  std::int32_t balanceDueMonth = 4;

  const double settlementRoll = rng.nextDouble();
  if (settlementRoll < cfg.refundP) {
    const double raw =
        ::PhantomLedger::probability::distributions::lognormalByMedian(
            rng, cfg.refundAmountMedian, cfg.refundAmountSigma);
    refundAmount = std::round(std::max(100.0, raw) * 100.0) / 100.0;
    refundMonth = static_cast<std::int32_t>(rng.uniformInt(2, 6));
  } else if (settlementRoll < cfg.refundP + cfg.balanceDueP) {
    const double raw =
        ::PhantomLedger::probability::distributions::lognormalByMedian(
            rng, cfg.balanceDueAmountMedian, cfg.balanceDueAmountSigma);
    balanceDueAmount = std::round(std::max(100.0, raw) * 100.0) / 100.0;
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

bool tryInsurance(::PhantomLedger::random::Rng &rng, const InsuranceConfig &cfg,
                  personas::Type personaType, bool hasMortgage,
                  bool hasAutoLoan, double mortgageAnchorP,
                  double autoLoanAnchorP,
                  ::PhantomLedger::entity::PersonId person,
                  product::PortfolioRegistry &out) {
  // ---- AUTO ----
  const double autoAnchorPolicyP = cfg.autoLoanAutoRequiredP;
  const double autoIssueP =
      hasAutoLoan
          ? autoAnchorPolicyP
          : residualPolicyProbability(cfg.targets.autoP.at(personaType),
                                      autoLoanAnchorP, autoAnchorPolicyP);

  std::optional<product::InsurancePolicy> autoPol;
  if (rng.nextDouble() < autoIssueP) {
    const double premium =
        samplePaymentAmount(rng, cfg.autoMedian, cfg.autoSigma, 30.0);
    autoPol = product::autoPolicy(institutional::autoCarrier(), premium,
                                  samplePaymentDay(rng), cfg.autoClaimAnnualP);
  }

  // ---- HOME ----
  const double homeAnchorPolicyP = cfg.mortgageHomeRequiredP;
  const double homeIssueP =
      hasMortgage
          ? homeAnchorPolicyP
          : residualPolicyProbability(cfg.targets.homeP.at(personaType),
                                      mortgageAnchorP, homeAnchorPolicyP);

  std::optional<product::InsurancePolicy> homePol;
  if (rng.nextDouble() < homeIssueP) {
    const double premium =
        samplePaymentAmount(rng, cfg.homeMedian, cfg.homeSigma, 20.0);
    homePol = product::homePolicy(institutional::homeCarrier(), premium,
                                  samplePaymentDay(rng), cfg.homeClaimAnnualP);
  }

  // ---- LIFE ----
  std::optional<product::InsurancePolicy> lifePol;
  if (rng.nextDouble() < cfg.targets.lifeP.at(personaType)) {
    const double premium =
        samplePaymentAmount(rng, cfg.lifeMedian, cfg.lifeSigma, 10.0);
    lifePol = product::lifePolicy(institutional::lifeCarrier(), premium,
                                  samplePaymentDay(rng));
  }

  if (!autoPol.has_value() && !homePol.has_value() && !lifePol.has_value()) {
    return false;
  }

  product::InsuranceHoldings holdings{};
  holdings.auto_ = autoPol;
  holdings.home = homePol;
  holdings.life = lifePol;
  out.insurance().set(person, std::move(holdings));
  return true;
}

} // namespace

::PhantomLedger::entity::product::PortfolioRegistry
build(::PhantomLedger::random::Rng & /*rng*/, const Inputs &in) {
  if (in.personas == nullptr) {
    throw std::invalid_argument("products::build: personas is required");
  }
  if (in.creditCards == nullptr) {
    throw std::invalid_argument("products::build: creditCards is required");
  }

  ::PhantomLedger::entity::product::PortfolioRegistry out;

  const auto &assignment = in.personas->assignment;
  const auto population = static_cast<::PhantomLedger::entity::PersonId>(
      assignment.byPerson.size());

  for (::PhantomLedger::entity::PersonId person = 1; person <= population;
       ++person) {
    auto local = perPersonRng(in.baseSeed, person);
    const auto personaType = assignment.byPerson[person - 1];

    const double mortgageAnchorP = in.mortgageCfg.ownership.at(personaType);
    const double autoLoanAnchorP = in.autoLoanCfg.ownership.at(personaType);

    bool hasMortgage = false;
    bool hasAutoLoan = false;

    (void)tryMortgage(local, in.mortgageCfg, personaType, in.window, person,
                      out, hasMortgage);
    (void)tryAutoLoan(local, in.autoLoanCfg, personaType, in.window, person,
                      out, hasAutoLoan);
    (void)tryStudentLoan(local, in.studentLoanCfg, personaType, in.window,
                         person, out);
    (void)tryTax(local, in.taxCfg, personaType, in.window, person, out);
    (void)tryInsurance(local, in.insuranceCfg, personaType, hasMortgage,
                       hasAutoLoan, mortgageAnchorP, autoLoanAnchorP, person,
                       out);

    (void)in.creditCards;
  }

  out.obligations().sort();

  return out;
}

} // namespace PhantomLedger::entities::synth::products
