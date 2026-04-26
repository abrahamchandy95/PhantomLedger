#include "phantomledger/transfers/subscriptions/debits.hpp"

#include "phantomledger/primitives/time/almanac.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/clearing/screening.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/subscriptions/schedule.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <string_view>

namespace PhantomLedger::transfers::subscriptions {
/*
 * Two-stages:
    1. Per-person bundle assignment

    2. Per-month cycle
 */
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

/// (timestamp, sub-index) pairs accumulated for a single month.
/// Storing only the index keeps the sort key small and cache-friendly.
struct Candidate {
  std::int64_t ts = 0;
  std::uint32_t subIdx = 0;
};

/// No-screen fast path. The conditional on screening is hoisted to
/// the call site so this loop runs without a per-iteration branch.
void emitWithoutScreen(std::span<const Candidate> sorted,
                       std::span<const Sub> subs,
                       const transactions::Factory &txf,
                       std::vector<transactions::Transaction> &out) {
  for (const auto &c : sorted) {
    const auto &s = subs[c.subIdx];
    out.push_back(txf.make(transactions::Draft{
        .source = s.deposit,
        .destination = s.biller,
        .amount = s.amount,
        .timestamp = c.ts,
        .channel = kChannel,
    }));
  }
}

/// Screened path. Walks the base-txn cursor monotonically forward
/// across the pre-sorted candidate slice.
void emitWithScreen(std::span<const Candidate> sorted,
                    std::span<const Sub> subs, const transactions::Factory &txf,
                    const Screen &screen, std::size_t &baseCursor,
                    std::vector<transactions::Transaction> &out) {
  for (const auto &c : sorted) {
    baseCursor = clearing::advanceBookThrough(screen.ledger, screen.baseTxns,
                                              baseCursor, c.ts,
                                              /*inclusive=*/true);
    const auto &s = subs[c.subIdx];
    const auto decision =
        screen.ledger->transfer(s.deposit, s.biller, s.amount, kChannel);
    if (decision.rejected()) {
      continue;
    }
    out.push_back(txf.make(transactions::Draft{
        .source = s.deposit,
        .destination = s.biller,
        .amount = s.amount,
        .timestamp = c.ts,
        .channel = kChannel,
    }));
  }
}

} // namespace

std::vector<transactions::Transaction>
debits(const BundleTerms &terms, const time::Window &window, random::Rng &rng,
       const transactions::Factory &txf, const random::RngFactory &factory,
       const Population &population, const Counterparties &counterparties,
       const Screen &screen) {
  std::vector<transactions::Transaction> out;

  if (counterparties.billerAccounts.empty() ||
      population.primaryAccountByPerson.empty()) {
    return out;
  }

  time::Almanac almanac{window};
  const auto monthStarts = almanac.monthAnchors();
  if (monthStarts.empty()) {
    return out;
  }

  // Stage 1: build the flat bundle of all materialized subs once.
  std::vector<Sub> subs;
  const auto personCount = population.primaryAccountByPerson.size();
  for (std::size_t i = 0; i < personCount; ++i) {
    const entity::Key &deposit = population.primaryAccountByPerson[i];
    if (!entity::valid(deposit)) {
      continue;
    }
    if (population.hubSet != nullptr && population.hubSet->contains(deposit)) {
      continue;
    }
    const PersonText pid(static_cast<std::uint32_t>(i + 1));
    auto subRng = factory.rng({"subscriptions", pid.view()});
    appendBundle(subRng, terms, deposit, counterparties.billerAccounts, subs);
  }

  if (subs.empty()) {
    return out;
  }
  out.reserve(monthStarts.size() * subs.size() / 2U);

  // Stage 2: per-month, build candidates, sort, screen, emit.
  std::vector<Candidate> month;
  month.reserve(subs.size());

  const auto windowStartTs = time::toEpochSeconds(window.start);
  const auto endExclTs = time::toEpochSeconds(window.endExcl());
  std::size_t baseCursor = 0;
  const bool screening = (screen.ledger != nullptr);

  for (const auto &monthStart : monthStarts) {
    month.clear();
    for (std::size_t i = 0; i < subs.size(); ++i) {
      const auto ts =
          cycleTimestamp(rng, monthStart, subs[i].day, terms.dayJitter);
      if (ts < windowStartTs || ts >= endExclTs) {
        continue;
      }
      month.push_back(Candidate{ts, static_cast<std::uint32_t>(i)});
    }
    if (month.empty()) {
      continue;
    }

    // Stable sort keeps the screen replay deterministic for ties.
    std::stable_sort(month.begin(), month.end(),
                     [](const Candidate &a, const Candidate &b) noexcept {
                       return a.ts < b.ts;
                     });

    if (screening) {
      emitWithScreen(month, subs, txf, screen, baseCursor, out);
    } else {
      emitWithoutScreen(month, subs, txf, out);
    }
  }

  std::sort(
      out.begin(), out.end(),
      transactions::Comparator{transactions::Comparator::Scope::fundsTransfer});
  return out;
}

} // namespace PhantomLedger::transfers::subscriptions
