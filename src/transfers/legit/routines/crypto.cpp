#include "phantomledger/transfers/legit/routines/crypto.hpp"

#include "phantomledger/encoding/external.hpp"
#include "phantomledger/primitives/random/distributions/lognormal.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/constants.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/synth/econ/nominal.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PhantomLedger::transfers::legit::routines::crypto {

namespace {

enum class Direction : std::uint8_t {
  rampOut = 0,
  rampIn = 1,
};

struct User {
  entity::PersonId person = entity::invalidPerson;
  entity::Key deposit{};
  entity::Key venue{};
  int adoptionYear = 0;
  unsigned adoptionMonth = 1;
  std::int64_t joinEpoch = std::numeric_limits<std::int64_t>::min();
  std::int64_t deathEpoch = std::numeric_limits<std::int64_t>::max();
};

struct Candidate {
  std::int64_t timestamp = 0;
  std::size_t userIndex = 0;
  entity::Key deposit{};
  entity::Key venue{};
  Direction direction = Direction::rampOut;

  // rampOut stores calibration-year USD; rampIn stores a fraction of the
  // account's remaining, previously-ramped value at this timestamp.
  double value = 0.0;
};

[[nodiscard]] bool adoptedBy(const User &user,
                             const time::CalendarDate &month) noexcept {
  return month.year > user.adoptionYear ||
         (month.year == user.adoptionYear && month.month >= user.adoptionMonth);
}

[[nodiscard]] std::int64_t eventTimestamp(random::Rng &rng,
                                          time::TimePoint monthStart) {
  const auto month = time::toCalendarDate(monthStart);
  const auto dayOffset = static_cast<int>(rng.uniformInt(
      0,
      static_cast<std::int64_t>(time::daysInMonth(month.year, month.month))));
  const auto hour = static_cast<int>(rng.uniformInt(8, 23));
  const auto minute = static_cast<int>(rng.uniformInt(0, 60));
  return time::toEpochSeconds(monthStart) +
         static_cast<std::int64_t>(dayOffset) * time::kSecondsPerDay +
         time::secondsInDay(hour, minute);
}

[[nodiscard]] std::pair<std::int64_t, std::int64_t>
membershipBounds(const blueprints::LegitBlueprint &plan,
                 entity::PersonId person) {
  auto join = time::toEpochSeconds(plan.startDate());
  auto death = std::numeric_limits<std::int64_t>::max();

  const auto *pack = plan.personas().pack;
  if (pack == nullptr || person == entity::invalidPerson) {
    return {join, death};
  }

  const auto idx = static_cast<std::size_t>(person - 1U);
  if (idx < pack->joinDays.size()) {
    join +=
        static_cast<std::int64_t>(pack->joinDays[idx]) * time::kSecondsPerDay;
  }
  if (idx < pack->timelines.size()) {
    death = time::toEpochSeconds(pack->timelines[idx].death);
  }

  return {join, death};
}

[[nodiscard]] std::vector<User>
selectUsers(const random::RngFactory &factory,
            const blueprints::LegitBlueprint &plan,
            const entity::account::Registry &registry, const Config &cfg) {
  std::vector<User> out;
  out.reserve(plan.persons().size());

  const auto &venues = plan.counterparties().cryptoVenues;
  if (venues.empty()) {
    return out;
  }

  const int yearSpan = cfg.modernAnchorYear - cfg.firstActivityYear + 1;

  for (const auto person : plan.persons()) {
    const auto recordIt = plan.primaryAcctRecordIx().find(person);
    if (recordIt == plan.primaryAcctRecordIx().end() ||
        recordIt->second >= registry.records.size()) {
      continue;
    }

    const auto deposit = registry.records[recordIt->second].id;
    if (encoding::isExternal(deposit)) {
      continue;
    }

    const auto personKey = std::to_string(static_cast<std::uint32_t>(person));
    auto profileRng = factory.rng({"legit", "crypto", "profile", personKey});
    if (!profileRng.coin(cfg.modernAdopterP)) {
      continue;
    }

    /* A quadratic cumulative ramp concentrates adoption toward the modern
     * anchor without inventing pre-2013 activity. The 9% default is the modern
     * cohort ceiling; early years see only a small fraction of it. */
    const double adoptionPosition =
        std::sqrt(profileRng.nextDouble()) * static_cast<double>(yearSpan);
    const auto yearOffset =
        std::min(yearSpan - 1, static_cast<int>(std::floor(adoptionPosition)));
    const int adoptionYear = cfg.firstActivityYear + yearOffset;
    const auto adoptionMonth =
        static_cast<unsigned>(profileRng.uniformInt(1, 13));
    const auto venue = venues[profileRng.choiceIndex(venues.size())];
    const auto [joinEpoch, deathEpoch] = membershipBounds(plan, person);

    out.push_back(User{
        .person = person,
        .deposit = deposit,
        .venue = venue,
        .adoptionYear = adoptionYear,
        .adoptionMonth = adoptionMonth,
        .joinEpoch = joinEpoch,
        .deathEpoch = deathEpoch,
    });
  }

  return out;
}

[[nodiscard]] double sampleRampOut(random::Rng &rng, const Config &cfg) {
  const double raw = probability::distributions::lognormalByMedian(
      rng, cfg.rampOutMedian, cfg.rampOutSigma);
  return std::clamp(raw, cfg.rampOutMin, cfg.rampOutMax);
}

[[nodiscard]] std::vector<Candidate>
makeCandidates(const random::RngFactory &factory,
               const blueprints::LegitBlueprint &plan,
               const std::vector<User> &users, const Config &cfg) {
  std::vector<Candidate> out;
  out.reserve(plan.monthStarts().size() * users.size() / 5U);

  const auto startEpoch = time::toEpochSeconds(plan.startDate());
  const auto endEpoch = startEpoch + static_cast<std::int64_t>(plan.days()) *
                                         time::kSecondsPerDay;

  for (std::size_t userIndex = 0; userIndex < users.size(); ++userIndex) {
    const auto &user = users[userIndex];
    const auto personKey =
        std::to_string(static_cast<std::uint32_t>(user.person));
    auto rng = factory.rng({"legit", "crypto", "activity", personKey});

    for (const auto monthStart : plan.monthStarts()) {
      const auto month = time::toCalendarDate(monthStart);
      if (month.year < cfg.firstActivityYear || !adoptedBy(user, month)) {
        continue;
      }

      if (rng.coin(cfg.monthlyRampOutP)) {
        const auto timestamp = eventTimestamp(rng, monthStart);
        const auto value = sampleRampOut(rng, cfg);
        if (timestamp >= startEpoch && timestamp < endEpoch) {
          out.push_back(Candidate{
              .timestamp = timestamp,
              .userIndex = userIndex,
              .deposit = user.deposit,
              .venue = user.venue,
              .direction = Direction::rampOut,
              .value = value,
          });
        }
      }

      if (rng.coin(cfg.monthlyRampInP)) {
        const auto timestamp = eventTimestamp(rng, monthStart);
        const auto fraction =
            cfg.rampInFractionMin +
            rng.nextDouble() * (cfg.rampInFractionMax - cfg.rampInFractionMin);
        if (timestamp >= startEpoch && timestamp < endEpoch) {
          out.push_back(Candidate{
              .timestamp = timestamp,
              .userIndex = userIndex,
              .deposit = user.deposit,
              .venue = user.venue,
              .direction = Direction::rampIn,
              .value = fraction,
          });
        }
      }
    }
  }

  std::ranges::sort(out, [](const Candidate &lhs, const Candidate &rhs) {
    return std::tie(lhs.timestamp, lhs.deposit, lhs.direction) <
           std::tie(rhs.timestamp, rhs.deposit, rhs.direction);
  });
  return out;
}

[[nodiscard]] double nominalRampOut(const Candidate &candidate,
                                    const Config &cfg) {
  const auto year =
      time::toCalendarDate(time::fromEpochSeconds(candidate.timestamp)).year;
  const double scale = synth::econ::priceScale(year);
  return primitives::utils::roundMoney(std::clamp(
      candidate.value * scale, cfg.rampOutMin * scale, cfg.rampOutMax * scale));
}

[[nodiscard]] transactions::Draft draftFrom(const Candidate &candidate,
                                            double amount,
                                            channels::Tag channel) noexcept {
  const bool outbound = candidate.direction == Direction::rampOut;
  return transactions::Draft{
      .source = outbound ? candidate.deposit : candidate.venue,
      .destination = outbound ? candidate.venue : candidate.deposit,
      .amount = amount,
      .timestamp = candidate.timestamp,
      .isFraud = 0,
      .ringId = -1,
      .channel = channel,
  };
}

} // namespace

void Config::validate() const {
  namespace v = primitives::validate;
  v::unit("modernAdopterP", modernAdopterP);
  v::ge("firstActivityYear", firstActivityYear, 2013);
  v::ge("modernAnchorYear", modernAnchorYear, firstActivityYear);
  v::unit("monthlyRampOutP", monthlyRampOutP);
  v::unit("monthlyRampInP", monthlyRampInP);
  v::positive("rampOutMedian", rampOutMedian);
  v::nonNegative("rampOutSigma", rampOutSigma);
  v::positive("rampOutMin", rampOutMin);
  v::ge("rampOutMax", rampOutMax, rampOutMin);
  v::between("rampInFractionMin", rampInFractionMin, 0.0, 1.0);
  v::between("rampInFractionMax", rampInFractionMax, rampInFractionMin, 1.0);
  v::positive("rampInMin", rampInMin);
  v::ge("rampInMax", rampInMax, rampInMin);
}

Generator::Generator(const transactions::Factory &txf,
                     ledger::SeededScreen &screen, Config cfg)
    : txf_{txf}, screen_{screen}, cfg_{cfg} {
  cfg_.validate();
}

std::vector<transactions::Transaction>
Generator::generate(const blueprints::LegitBlueprint &plan,
                    const entity::account::Registry &registry) {
  std::vector<transactions::Transaction> out;
  if (plan.monthStarts().empty() ||
      plan.counterparties().cryptoVenues.empty()) {
    return out;
  }

  /* Both planning and session routing live below this derived factory. No draw
   * is taken from the routine assembler's shared RNG. Separate sub-lanes keep a
   * screening change from re-rolling adoption, dates, amounts, or venues. */
  const random::RngFactory factory{plan.seed()};
  const auto users = selectUsers(factory, plan, registry, cfg_);
  if (users.empty()) {
    return out;
  }
  const auto candidates = makeCandidates(factory, plan, users, cfg_);
  if (candidates.empty()) {
    return out;
  }

  auto sessionRng = factory.rng({"legit", "crypto", "sessions"});
  const auto cryptoTxf = txf_.rebound(sessionRng);
  std::unordered_map<entity::Key, double> inventory;
  inventory.reserve(users.size());

  const auto outChannel = channels::tag(channels::Crypto::rampOut);
  const auto inChannel = channels::tag(channels::Crypto::rampIn);
  out.reserve(candidates.size());

  for (const auto &candidate : candidates) {
    if (candidate.userIndex >= users.size()) {
      continue;
    }
    const auto &user = users[candidate.userIndex];
    if (candidate.timestamp < user.joinEpoch ||
        candidate.timestamp >= user.deathEpoch) {
      continue;
    }

    screen_.advanceThrough(candidate.timestamp, /*inclusive=*/true);
    auto &available = inventory[candidate.deposit];

    if (candidate.direction == Direction::rampOut) {
      const double amount = nominalRampOut(candidate, cfg_);
      const auto transfer = ledger::KeyedTransfer{
          .source = candidate.deposit,
          .destination = candidate.venue,
          .amount = amount,
          .channel = outChannel,
          .timestamp = candidate.timestamp,
      };
      if (!screen_.acceptTransfer(transfer)) {
        continue;
      }

      available = primitives::utils::roundMoney(available + amount);
      out.push_back(cryptoTxf.make(draftFrom(candidate, amount, outChannel)));
      continue;
    }

    /* The bank ledger has no crypto asset or market-price process. Returning
     * more USD than this routine previously ramped out would therefore create
     * unexplained off-ledger value. Track remaining acquisition value and cap
     * every ramp-in against it; an accepted return consumes that inventory. */
    if (available <= 0.0) {
      continue;
    }
    const auto year =
        time::toCalendarDate(time::fromEpochSeconds(candidate.timestamp)).year;
    const double scale = synth::econ::priceScale(year);
    const double proposed = primitives::utils::roundMoney(
        std::min(available, available * candidate.value));
    if (proposed < cfg_.rampInMin * scale) {
      continue;
    }
    const double amount =
        std::min(available, primitives::utils::roundMoney(
                                std::min(proposed, cfg_.rampInMax * scale)));
    const auto transfer = ledger::KeyedTransfer{
        .source = candidate.venue,
        .destination = candidate.deposit,
        .amount = amount,
        .channel = inChannel,
        .timestamp = candidate.timestamp,
    };
    if (!screen_.acceptTransfer(transfer)) {
      continue;
    }

    available =
        std::max(0.0, primitives::utils::roundMoney(available - amount));
    out.push_back(cryptoTxf.make(draftFrom(candidate, amount, inChannel)));
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::crypto
