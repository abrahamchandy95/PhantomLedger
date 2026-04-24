#include "phantomledger/transfers/government.hpp"

#include "phantomledger/entropy/crypto/blake2b.hpp"
#include "phantomledger/primitives/time/window_calendar.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/probability/distributions/lognormal.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>

namespace PhantomLedger::transfer::government {
namespace {

using Key = entity::Key;
using PersonId = entity::PersonId;
using TimePoint = time::TimePoint;

constexpr std::size_t kDigestBytes = 8;

[[nodiscard]] std::uint64_t stableU64(std::string_view a,
                                      std::string_view b) noexcept {
  std::array<std::uint8_t, 256> buf{};
  std::size_t n = 0;

  const auto push = [&](std::string_view s) {
    const auto take = std::min(s.size(), buf.size() - n);
    std::memcpy(buf.data() + n, s.data(), take);
    n += take;
  };

  push(a);
  if (n < buf.size())
    buf[n++] = '|';
  push(b);
  if (n < buf.size())
    buf[n++] = '|';

  std::uint8_t digest[kDigestBytes]{};
  const auto ok = crypto::blake2b::digest(buf.data(), n, digest, kDigestBytes);
  if (!ok) {
    return 0;
  }

  std::uint64_t v = 0;
  std::memcpy(&v, digest, sizeof(v));
  return v;
}

[[nodiscard]] int syntheticBirthDay(PersonId person) noexcept {
  std::array<char, 24> buf{};
  const int n = std::snprintf(buf.data(), buf.size(), "%u",
                              static_cast<std::uint32_t>(person));
  const std::string_view pid{buf.data(),
                             static_cast<std::size_t>(std::max(n, 0))};
  return 1 + static_cast<int>((stableU64(pid, "identity") >> 16U) % 28ULL);
}

[[nodiscard]] TimePoint jitter(random::Rng &rng, TimePoint baseDate) noexcept {
  const auto hour = rng.uniformInt(6, 10);
  const auto minute = rng.uniformInt(0, 60);
  return baseDate + time::Hours{hour} + time::Minutes{minute};
}

[[nodiscard]] double sampleAmount(random::Rng &rng, double median, double sigma,
                                  double floor) {
  const auto raw =
      probability::distributions::lognormalByMedian(rng, median, sigma);
  return primitives::utils::roundMoney(std::max(floor, raw));
}

[[nodiscard]] bool hasAccount(const Population &pop, PersonId pid) noexcept {
  const auto &own = *pop.ownership;
  return own.byPersonOffset[pid - 1] != own.byPersonOffset[pid];
}

[[nodiscard]] Key primaryAccount(const Population &pop, PersonId pid) noexcept {
  const auto idx = pop.ownership->primaryIndex(pid);
  return pop.accounts->records[idx].id;
}

struct Recipient {
  PersonId person{};
  Key account{};
  double amount = 0.0;
  int ssaBucket = 0;
};

// Build & sort recipients for one benefit subsystem. Persona
// filtering is a template so the call sites read declaratively,
// e.g. `build(..., [](auto t) { return t == Type::retiree; })`.
template <class PersonaFilter>
[[nodiscard]] std::vector<Recipient>
build(const Population &pop, random::Rng &rng, double eligibleP, double median,
      double sigma, double floor, PersonaFilter matches) {
  std::vector<Recipient> out;
  out.reserve(pop.count / 4);

  for (PersonId pid = 1; pid <= pop.count; ++pid) {
    if (!hasAccount(pop, pid))
      continue;
    const auto persona = pop.personas->byPerson[pid - 1];
    if (!matches(persona))
      continue;
    if (!rng.coin(eligibleP))
      continue;

    out.push_back(Recipient{
        pid,
        primaryAccount(pop, pid),
        sampleAmount(rng, median, sigma, floor),
        time::ssaBucketForBirthDay(syntheticBirthDay(pid)),
    });
  }

  std::sort(out.begin(), out.end(),
            [](const auto &a, const auto &b) { return a.person < b.person; });
  return out;
}

void emit(const std::vector<Recipient> &recipients, const Key &source,
          channels::Tag channel, time::WindowCalendar &calendar,
          TimePoint start, TimePoint endExcl, random::Rng &rng,
          const transactions::Factory &txf,
          std::vector<transactions::Transaction> &out) {
  if (recipients.empty())
    return;

  const std::span<const TimePoint> buckets[3] = {
      calendar.ssaPaymentDatesForBucket(0),
      calendar.ssaPaymentDatesForBucket(1),
      calendar.ssaPaymentDatesForBucket(2),
  };

  const auto months = buckets[0].size();
  for (std::size_t m = 0; m < months; ++m) {
    for (const auto &r : recipients) {
      const auto ts = jitter(rng, buckets[r.ssaBucket][m]);
      if (ts < start || ts >= endExcl)
        continue;

      out.push_back(txf.make(transactions::Draft{
          .source = source,
          .destination = r.account,
          .amount = r.amount,
          .timestamp = time::toEpochSeconds(ts),
          .channel = channel,
      }));
    }
  }
}

} // namespace

std::vector<transactions::Transaction>
generate(const Params &params, const Window &window, random::Rng &rng,
         const transactions::Factory &txf, const Population &population,
         const Counterparties &counterparties) {
  std::vector<transactions::Transaction> out;

  const auto endExcl = window.endExcl();
  time::WindowCalendar calendar{window.start, endExcl};
  if (calendar.monthAnchors().empty())
    return out;

  if (params.ssaEnabled) {
    // FIX (Bug 1): was personas::Type::retired (nonexistent enumerator).
    auto recipients =
        build(population, rng, params.ssaEligibleP, params.ssaMedian,
              params.ssaSigma, params.ssaFloor,
              [](personas::Type t) { return t == personas::Type::retiree; });

    emit(recipients, counterparties.ssa,
         channels::tag(channels::Government::socialSecurity), calendar,
         window.start, endExcl, rng, txf, out);
  }

  if (params.disabilityEnabled) {
    // FIX (Bug 1): was personas::Type::retired (nonexistent enumerator).
    auto recipients = build(
        population, rng, params.disabilityP, params.disabilityMedian,
        params.disabilitySigma, params.disabilityFloor, [](personas::Type t) {
          return t != personas::Type::retiree && t != personas::Type::student;
        });

    emit(recipients, counterparties.disability,
         channels::tag(channels::Government::disability), calendar,
         window.start, endExcl, rng, txf, out);
  }

  std::sort(
      out.begin(), out.end(),
      transactions::Comparator{transactions::Comparator::Scope::fundsTransfer});
  return out;
}

} // namespace PhantomLedger::transfer::government
