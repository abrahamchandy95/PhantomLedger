#include "phantomledger/transfers/channels/credit_cards/payment.hpp"

#include "phantomledger/primitives/random/distributions/beta.hpp"
#include "phantomledger/transfers/channels/credit_cards/cycle.hpp"

#include <algorithm>

namespace PhantomLedger::transfers::credit_cards {
namespace {

inline constexpr int kPayHourLow = 9;
inline constexpr int kPayHourHigh = 21;

inline constexpr int kEarlyDayWindow = 4;

inline constexpr time::TimeOfDay kAutopayTime{12, 0, 0};

inline constexpr double kPartialResidualFloor = 1e-9;

[[nodiscard]] double samplePartialPayment(const PaymentMixture &mixture,
                                          random::Rng &rng, double statementAbs,
                                          double minimumDue) {
  const double remaining = std::max(0.0, statementAbs - minimumDue);
  if (remaining <= kPartialResidualFloor) {
    return minimumDue;
  }
  const double fraction = probability::distributions::beta(
      rng, mixture.partialAlpha, mixture.partialBeta);
  return minimumDue + fraction * remaining;
}

} // namespace

double samplePaymentAmount(const PaymentMixture &mixture, random::Rng &rng,
                           double statementAbs, double minimumDueAmt) {
  double miss = std::max(0.0, mixture.miss);
  double payMin = std::max(0.0, mixture.payMin);
  double payPart = std::max(0.0, mixture.payPartial);
  const double payFull = std::max(0.0, mixture.payFull);

  const double total = miss + payMin + payPart + payFull;
  if (total <= 0.0) {
    return 0.0;
  }
  miss /= total;
  payMin /= total;
  payPart /= total;
  // payFull is the residual.

  double u = rng.nextDouble();

  if (u < miss) {
    return 0.0;
  }
  u -= miss;

  if (u < payMin) {
    return minimumDueAmt;
  }
  u -= payMin;

  if (u < payPart) {
    return samplePartialPayment(mixture, rng, statementAbs, minimumDueAmt);
  }

  return statementAbs;
}

time::TimePoint samplePaymentTime(const PaymentTiming &timing, random::Rng &rng,
                                  time::TimePoint due, bool autopay) {
  const time::TimePoint dueCutoff = resolveDueDate(due);
  const time::TimePoint dueDay =
      time::makeTime(time::toCalendarDate(dueCutoff));

  if (autopay) {
    return time::makeTime(time::toCalendarDate(dueCutoff), kAutopayTime);
  }

  const int hour = static_cast<int>(rng.uniformInt(kPayHourLow, kPayHourHigh));
  const int minute = static_cast<int>(rng.uniformInt(0, 60));
  const auto paymentTime = time::Hours{hour} + time::Minutes{minute};

  if (rng.coin(timing.lateProbability)) {
    const int delay = static_cast<int>(
        rng.uniformInt(static_cast<std::int64_t>(timing.lateDaysMin),
                       static_cast<std::int64_t>(timing.lateDaysMax) + 1));
    time::TimePoint sampled = dueDay + time::Days{delay} + paymentTime;
    if (sampled <= dueCutoff) {
      sampled += time::Days{1};
    }
    return sampled;
  }

  const int earlyDays = static_cast<int>(rng.uniformInt(0, kEarlyDayWindow));
  time::TimePoint sampled = dueDay - time::Days{earlyDays} + paymentTime;
  if (sampled > dueCutoff) {
    sampled -= time::Days{1};
  }
  return sampled;
}

} // namespace PhantomLedger::transfers::credit_cards
