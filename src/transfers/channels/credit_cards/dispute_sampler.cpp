#include "phantomledger/transfers/channels/credit_cards/dispute_sampler.hpp"

#include "phantomledger/transactions/draft.hpp"

namespace PhantomLedger::transfers::credit_cards {
namespace {

inline constexpr int kPostHourLow = 9;
inline constexpr int kPostHourHigh = 21;

struct DelayBounds {
  int min = 0;
  int max = 0;
};

[[nodiscard]] DelayBounds delayBoundsFor(const DisputeWindow &window,
                                         channels::Credit kind) noexcept {
  if (kind == channels::Credit::chargeback) {
    return {window.chargebackMin, window.chargebackMax};
  }
  return {window.refundMin, window.refundMax};
}

} // namespace

std::optional<transactions::Transaction> DisputeSampler::buildCredit(
    random::Rng &rng, const transactions::Transaction &purchase,
    channels::Credit kind, time::TimePoint windowEndExcl) const {
  const auto bounds = delayBoundsFor(behavior_.window, kind);

  const int delay = static_cast<int>(
      rng.uniformInt(static_cast<std::int64_t>(bounds.min),
                     static_cast<std::int64_t>(bounds.max) + 1));
  const int hour =
      static_cast<int>(rng.uniformInt(kPostHourLow, kPostHourHigh));

  const time::TimePoint ts = time::fromEpochSeconds(purchase.timestamp) +
                             time::Days{delay} + time::Hours{hour};
  if (ts >= windowEndExcl) {
    return std::nullopt;
  }

  return factory_.make(transactions::Draft{
      .source = purchase.target,
      .destination = purchase.source,
      .amount = purchase.amount,
      .timestamp = time::toEpochSeconds(ts),
      .channel = channels::tag(kind),
  });
}

std::optional<transactions::Transaction>
DisputeSampler::sample(random::Rng &rng,
                       const transactions::Transaction &purchase,
                       time::TimePoint windowEndExcl) const {
  if (rng.nextDouble() < behavior_.rates.refundProbability) {
    return buildCredit(rng, purchase, channels::Credit::refund, windowEndExcl);
  }

  if (rng.nextDouble() < behavior_.rates.chargebackProbability) {
    return buildCredit(rng, purchase, channels::Credit::chargeback,
                       windowEndExcl);
  }

  return std::nullopt;
}

} // namespace PhantomLedger::transfers::credit_cards
