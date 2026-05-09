#include "phantomledger/transfers/channels/subscriptions/debits.hpp"

#include "phantomledger/primitives/time/almanac.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/clearing/screening.hpp"
#include "phantomledger/transactions/draft.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace PhantomLedger::transfers::subscriptions {
namespace {

inline constexpr channels::Tag kChannel =
    channels::tag(channels::Legit::subscription);

/// Stack-buffered uint -> decimal text, no allocation.
struct PersonText {
  std::array<char, 16> buf{};
  std::size_t len = 0;

  explicit PersonText(std::uint32_t v) noexcept {
    auto [p, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), v);
    (void)ec;
    len = static_cast<std::size_t>(p - buf.data());
  }

  [[nodiscard]] std::string_view view() const noexcept {
    return {buf.data(), len};
  }
};

[[nodiscard]] transactions::Draft draftFrom(const Sub &sub,
                                            std::int64_t timestamp) noexcept {
  return transactions::Draft{
      .source = sub.deposit,
      .destination = sub.biller,
      .amount = sub.amount,
      .timestamp = timestamp,
      .channel = kChannel,
  };
}

void sortCandidates(std::vector<Candidate> &month) {
  std::stable_sort(month.begin(), month.end(),
                   [](const Candidate &a, const Candidate &b) noexcept {
                     return a.ts < b.ts;
                   });
}

void sortTransfers(std::vector<transactions::Transaction> &out) {
  std::sort(
      out.begin(), out.end(),
      transactions::Comparator{transactions::Comparator::Scope::fundsTransfer});
}

} // namespace

BundleBuilder::BundleBuilder(const BundleRules &rules,
                             const random::RngFactory &factory)
    : rules_(&rules), factory_(&factory) {
  rules.validate();
}

std::vector<Sub> BundleBuilder::build(SubscriberAccounts subscribers,
                                      BillerDirectory billers,
                                      AccountExclusions exclusions) const {
  std::vector<Sub> out;

  if (subscribers.empty() || billers.empty()) {
    return out;
  }

  for (const auto &subscriber : subscribers.rows) {
    if (!entity::valid(subscriber.deposit)) {
      continue;
    }
    if (exclusions.contains(subscriber.deposit)) {
      continue;
    }

    const PersonText pid(subscriber.person);
    auto subRng = factory_->rng({"subscriptions", pid.view()});
    appendBundle(subRng, *rules_, subscriber.deposit, billers.accounts, out);
  }

  return out;
}

ScheduleSampler::ScheduleSampler(time::Window window, ScheduleRules rules)
    : window_(window), rules_(rules),
      windowStartTs_(time::toEpochSeconds(window_.start)),
      windowEndExclTs_(time::toEpochSeconds(window_.endExcl())) {
  rules_.validate();

  time::Almanac almanac{window_};
  const auto anchors = almanac.monthAnchors();
  monthStarts_.assign(anchors.begin(), anchors.end());
}

ScheduleSampler::ScheduleSampler(std::span<const time::TimePoint> monthStarts,
                                 time::Window window, ScheduleRules rules)
    : window_(window), rules_(rules),
      monthStarts_(monthStarts.begin(), monthStarts.end()),
      windowStartTs_(time::toEpochSeconds(window_.start)),
      windowEndExclTs_(time::toEpochSeconds(window_.endExcl())) {
  rules_.validate();
}

std::span<const time::TimePoint> ScheduleSampler::monthStarts() const noexcept {
  return {monthStarts_.data(), monthStarts_.size()};
}

std::vector<Candidate>
ScheduleSampler::candidates(random::Rng &rng, time::TimePoint monthStart,
                            std::span<const Sub> subs) const {
  std::vector<Candidate> month;
  month.reserve(subs.size());

  for (std::size_t i = 0; i < subs.size(); ++i) {
    const auto ts = cycleTimestamp(rng, monthStart, subs[i].day, rules_);
    if (ts < windowStartTs_ || ts >= windowEndExclTs_) {
      continue;
    }
    month.push_back(Candidate{ts, static_cast<std::uint32_t>(i)});
  }

  sortCandidates(month);
  return month;
}

DebitEmitter::DebitEmitter(const transactions::Factory &txf) noexcept
    : txf_(&txf) {}

DebitEmitter::DebitEmitter(const transactions::Factory &txf,
                           const Screen &screen) noexcept
    : txf_(&txf), screen_(&screen) {}

std::vector<transactions::Transaction>
DebitEmitter::emit(const ScheduleSampler &schedule, random::Rng &rng,
                   std::span<const Sub> subs) const {
  return debits(schedule, rng, *this, subs);
}

bool DebitEmitter::screens() const noexcept {
  return screen_ != nullptr && screen_->ledger != nullptr;
}

void DebitEmitter::emitMonth(std::span<const Candidate> sorted,
                             std::span<const Sub> subs,
                             std::vector<transactions::Transaction> &out,
                             std::size_t &baseCursor) const {
  if (screens()) {
    emitWithScreen(sorted, subs, out, baseCursor);
  } else {
    emitWithoutScreen(sorted, subs, out);
  }
}

void DebitEmitter::emitWithoutScreen(
    std::span<const Candidate> sorted, std::span<const Sub> subs,
    std::vector<transactions::Transaction> &out) const {
  for (const auto &candidate : sorted) {
    const auto &sub = subs[candidate.subIdx];
    out.push_back(txf_->make(draftFrom(sub, candidate.ts)));
  }
}

void DebitEmitter::emitWithScreen(std::span<const Candidate> sorted,
                                  std::span<const Sub> subs,
                                  std::vector<transactions::Transaction> &out,
                                  std::size_t &baseCursor) const {
  for (const auto &candidate : sorted) {
    baseCursor = clearing::advanceBookThrough(
        screen_->ledger, screen_->baseTxns, baseCursor,
        clearing::TimeBound{.until = candidate.ts, .inclusive = true});

    const auto &sub = subs[candidate.subIdx];
    const auto decision = screen_->ledger->transfer(sub.deposit, sub.biller,
                                                    sub.amount, kChannel);
    if (decision.rejected()) {
      continue;
    }

    out.push_back(txf_->make(draftFrom(sub, candidate.ts)));
  }
}

std::vector<transactions::Transaction> debits(const ScheduleSampler &schedule,
                                              random::Rng &rng,
                                              const DebitEmitter &emitter,
                                              std::span<const Sub> subs) {
  std::vector<transactions::Transaction> out;

  if (subs.empty() || schedule.monthStarts().empty()) {
    return out;
  }

  out.reserve(schedule.monthStarts().size() * subs.size() / 2U);

  std::size_t baseCursor = 0;
  for (const auto &monthStart : schedule.monthStarts()) {
    auto month = schedule.candidates(rng, monthStart, subs);
    if (month.empty()) {
      continue;
    }

    emitter.emitMonth(month, subs, out, baseCursor);
  }

  sortTransfers(out);
  return out;
}

} // namespace PhantomLedger::transfers::subscriptions
