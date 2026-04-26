#include "phantomledger/transfers/credit_cards/dispute.hpp"

#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"

namespace PhantomLedger::transfers::credit_cards {
namespace {

inline constexpr int kPostHourLow = 9;
inline constexpr int kPostHourHigh = 21;

[[nodiscard]] std::optional<transactions::Transaction>
buildMerchantCredit(random::Rng &rng, const transactions::Transaction &purchase,
                    int delayMin, int delayMax, channels::Credit channel,
                    time::TimePoint windowEndExcl,
                    const transactions::Factory &factory) {
  const int delay =
      static_cast<int>(rng.uniformInt(static_cast<std::int64_t>(delayMin),
                                      static_cast<std::int64_t>(delayMax) + 1));
  const int hour =
      static_cast<int>(rng.uniformInt(kPostHourLow, kPostHourHigh));

  const time::TimePoint ts = time::fromEpochSeconds(purchase.timestamp) +
                             time::Days{delay} + time::Hours{hour};
  if (ts >= windowEndExcl) {
    return std::nullopt;
  }

  return factory.make(transactions::Draft{
      .source = purchase.target,      // merchant pays back
      .destination = purchase.source, // to the card account
      .amount = purchase.amount,
      .timestamp = time::toEpochSeconds(ts),
      .channel = channels::tag(channel),
  });
}

} // namespace

std::optional<transactions::Transaction> sampleMerchantCredit(
    const DisputeWindow &window, const DisputeRates &rates, random::Rng &rng,
    const transactions::Transaction &purchase, time::TimePoint windowEndExcl,
    const transactions::Factory &factory) {
  if (rng.nextDouble() < rates.refundProbability) {
    return buildMerchantCredit(rng, purchase, window.refundMin,
                               window.refundMax, channels::Credit::refund,
                               windowEndExcl, factory);
  }

  if (rng.nextDouble() < rates.chargebackProbability) {
    return buildMerchantCredit(
        rng, purchase, window.chargebackMin, window.chargebackMax,
        channels::Credit::chargeback, windowEndExcl, factory);
  }

  return std::nullopt;
}

} // namespace PhantomLedger::transfers::credit_cards
