#include "phantomledger/transfers/legit/routines/deposits.hpp"

#include "phantomledger/activity/income/revenue/clock.hpp"
#include "phantomledger/primitives/random/distributions/lognormal.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/synth/econ/nominal.hpp"
#include "phantomledger/synth/personas/timeline.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace PhantomLedger::transfers::legit::routines::deposits {

namespace {

enum class Kind : std::uint8_t {
  cash,
  check,
};

struct Depositor {
  entity::PersonId person = entity::invalidPerson;
  entity::Key account{};
  bool cash = false;
  bool check = false;
};

struct Candidate {
  std::int64_t timestamp = 0;
  entity::PersonId person = entity::invalidPerson;
  entity::Key account{};
  double amount = 0.0;
  Kind kind = Kind::cash;
};

inline constexpr std::uint64_t kCashPointDomain = 0x5044'4550'4341'5348ULL;
inline constexpr std::uint64_t kCheckPointDomain = 0x5044'4550'4348'454BULL;

[[nodiscard]] constexpr std::uint64_t splitmix(std::uint64_t value) noexcept {
  value += 0x9E3779B97F4A7C15ULL;
  value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
  return value ^ (value >> 31U);
}

[[nodiscard]] std::optional<entity::Key>
stableExternalPoint(std::span<const entity::Key> points,
                    const entity::Key &account, std::uint64_t domain) noexcept {
  if (points.empty()) {
    return std::nullopt;
  }

  const auto mixed =
      splitmix(account.number ^
               (static_cast<std::uint64_t>(account.role) << 56U) ^ domain);
  const auto start = static_cast<std::size_t>(mixed % points.size());

  // Production pools contain external keys exclusively. Scanning from the
  // stable start also makes the public API fail closed when a custom pool
  // accidentally contains an invalid or internal entry.
  for (std::size_t offset = 0; offset < points.size(); ++offset) {
    const auto &point = points[(start + offset) % points.size()];
    if (entity::valid(point) && point.bank == entity::Bank::external) {
      return point;
    }
  }
  return std::nullopt;
}

[[nodiscard]] bool activeAt(const blueprints::LegitBlueprint &plan,
                            entity::PersonId person,
                            std::int64_t timestamp) noexcept {
  if (person == entity::invalidPerson) {
    return false;
  }

  const auto *pack = plan.personas().pack;
  if (pack == nullptr) {
    return true;
  }

  const auto row = static_cast<std::size_t>(person - 1U);
  if (row < pack->joinDays.size()) {
    const auto join =
        plan.startDate() + time::Days{static_cast<int>(pack->joinDays[row])};
    if (time::fromEpochSeconds(timestamp) < join) {
      return false;
    }
  }
  if (row < pack->timelines.size() &&
      !synth::personas::timeline::aliveAt(pack->timelines[row],
                                          time::fromEpochSeconds(timestamp))) {
    return false;
  }
  return true;
}

[[nodiscard]] double sampleAmount(random::Rng &rng, const FlowConfig &cfg,
                                  int eventYear) {
  const double scale = synth::econ::priceScale(eventYear);
  const double floor = cfg.floor * scale;
  const double cap = cfg.cap * scale;
  double amount = probability::distributions::lognormalByMedian(rng, cfg.median,
                                                                cfg.sigma) *
                  scale;

  if (!std::isfinite(amount) || !std::isfinite(floor) || !std::isfinite(cap)) {
    return 0.0;
  }

  amount = std::clamp(amount, floor, cap);
  if (cfg.roundTo > 0.0) {
    // Denominations are nominal: historic deposits contain fewer current-unit
    // bills rather than fractional, CPI-scaled bills.
    const double latticeFloor = std::ceil(floor / cfg.roundTo) * cfg.roundTo;
    const double latticeCap = std::floor(cap / cfg.roundTo) * cfg.roundTo;
    if (!(latticeCap >= latticeFloor)) {
      return 0.0;
    }
    amount = std::round(amount / cfg.roundTo) * cfg.roundTo;
    amount = std::clamp(amount, latticeFloor, latticeCap);
  }
  amount = primitives::utils::roundMoney(amount);
  return std::isfinite(amount) && amount > 0.0 ? amount : 0.0;
}

[[nodiscard]] std::optional<Candidate>
candidateForMonth(random::Rng &rng, const FlowConfig &cfg,
                  time::TimePoint monthStart, entity::PersonId person,
                  entity::Key account, Kind kind) {
  if (!rng.coin(cfg.monthlyP)) {
    return std::nullopt;
  }

  const auto at = activity::income::revenue::businessDayTs(
      monthStart, rng,
      activity::income::revenue::BusinessDayWindow{
          .earliestHour = 9,
          .latestHour = 17,
          .startDay = 0,
          .endDayExclusive = 28,
      });
  const auto date = time::toCalendarDate(at);
  if (date.year <= 0) {
    return std::nullopt;
  }

  const double amount = sampleAmount(rng, cfg, date.year);
  if (!(amount > 0.0) || !std::isfinite(amount)) {
    return std::nullopt;
  }

  return Candidate{
      .timestamp = time::toEpochSeconds(at),
      .person = person,
      .account = account,
      .amount = amount,
      .kind = kind,
  };
}

[[nodiscard]] std::vector<Depositor>
selectDepositors(const blueprints::LegitBlueprint &plan,
                 const entity::account::Registry &registry, const Config &cfg,
                 const random::RngFactory &factory) {
  std::vector<Depositor> out;
  out.reserve(plan.persons().size() / 2U + 1U);

  for (const auto person : plan.persons()) {
    const auto it = plan.primaryAcctRecordIx().find(person);
    if (it == plan.primaryAcctRecordIx().end() ||
        it->second >= registry.records.size()) {
      continue;
    }

    const auto &record = registry.records[it->second];
    if (!entity::valid(record.id) || record.id.bank != entity::Bank::internal ||
        record.owner != person) {
      continue;
    }

    const auto personKey = std::to_string(static_cast<unsigned>(person));
    auto cashAdoption = factory.rng(
        {"legit", "personal_deposits", "cash", "adoption", personKey});
    auto checkAdoption = factory.rng(
        {"legit", "personal_deposits", "check", "adoption", personKey});
    const bool cash = cashAdoption.coin(cfg.cash.userP);
    const bool check = checkAdoption.coin(cfg.check.userP);
    if (cash || check) {
      out.push_back(Depositor{
          .person = person,
          .account = record.id,
          .cash = cash,
          .check = check,
      });
    }
  }

  return out;
}

[[nodiscard]] std::vector<Candidate>
makeCandidates(const blueprints::LegitBlueprint &plan,
               std::span<const Depositor> depositors, const Config &cfg,
               const random::RngFactory &factory) {
  std::vector<Candidate> out;
  out.reserve(depositors.size() * plan.monthStarts().size() / 4U + 1U);

  const auto startEpoch = time::toEpochSeconds(plan.startDate());
  const auto endEpoch = time::toEpochSeconds(
      plan.startDate() + time::Days{static_cast<int>(plan.days())});

  for (const auto &depositor : depositors) {
    const auto personKey =
        std::to_string(static_cast<unsigned>(depositor.person));

    for (const auto monthStart : plan.monthStarts()) {
      const auto month = time::toCalendarDate(monthStart);
      const auto yearKey = std::to_string(month.year);
      const auto monthKey = std::to_string(month.month);

      if (depositor.cash) {
        auto rng = factory.rng({"legit", "personal_deposits", "cash", personKey,
                                yearKey, monthKey});
        if (auto candidate =
                candidateForMonth(rng, cfg.cash, monthStart, depositor.person,
                                  depositor.account, Kind::cash);
            candidate.has_value() && candidate->timestamp >= startEpoch &&
            candidate->timestamp < endEpoch &&
            activeAt(plan, depositor.person, candidate->timestamp)) {
          out.push_back(*candidate);
        }
      }

      if (depositor.check) {
        auto rng = factory.rng({"legit", "personal_deposits", "check",
                                personKey, yearKey, monthKey});
        if (auto candidate =
                candidateForMonth(rng, cfg.check, monthStart, depositor.person,
                                  depositor.account, Kind::check);
            candidate.has_value() && candidate->timestamp >= startEpoch &&
            candidate->timestamp < endEpoch &&
            activeAt(plan, depositor.person, candidate->timestamp)) {
          out.push_back(*candidate);
        }
      }
    }
  }

  std::ranges::sort(out, [](const Candidate &lhs, const Candidate &rhs) {
    return std::tie(lhs.timestamp, lhs.person, lhs.kind, lhs.account,
                    lhs.amount) < std::tie(rhs.timestamp, rhs.person, rhs.kind,
                                           rhs.account, rhs.amount);
  });
  return out;
}

} // namespace

void FlowConfig::validate(const char *prefix) const {
  namespace v = primitives::validate;
  const std::string root = prefix == nullptr ? "deposits" : prefix;
  v::finite(root + ".userP", userP);
  v::unit(root + ".userP", userP);
  v::finite(root + ".monthlyP", monthlyP);
  v::unit(root + ".monthlyP", monthlyP);
  v::finite(root + ".median", median);
  v::positive(root + ".median", median);
  v::finite(root + ".sigma", sigma);
  v::nonNegative(root + ".sigma", sigma);
  v::finite(root + ".floor", floor);
  v::positive(root + ".floor", floor);
  v::finite(root + ".cap", cap);
  v::ge(root + ".cap", cap, floor);
  v::finite(root + ".roundTo", roundTo);
  v::nonNegative(root + ".roundTo", roundTo);
}

void Config::validate() const {
  cash.validate("deposits.cash");
  check.validate("deposits.check");
}

Generator::Generator(
    const transactions::Factory &txf,
    ::PhantomLedger::transfers::legit::ledger::SeededScreen &screen, Config cfg)
    : txf_{txf}, screen_{screen}, cfg_{std::move(cfg)} {
  cfg_.validate();
}

std::vector<transactions::Transaction>
Generator::generate(const blueprints::LegitBlueprint &plan,
                    const entity::account::Registry &registry) {
  if (plan.monthStarts().empty() || plan.days() <= 0) {
    return {};
  }

  const random::RngFactory factory{plan.seed()};
  const auto depositors = selectDepositors(plan, registry, cfg_, factory);
  auto candidates = makeCandidates(plan, depositors, cfg_, factory);

  std::vector<transactions::Transaction> out;
  out.reserve(candidates.size());

  for (const auto &candidate : candidates) {
    screen_.advanceThrough(candidate.timestamp, /*inclusive=*/true);

    const bool cash = candidate.kind == Kind::cash;
    const auto points = cash ? plan.counterparties().depositPointsFor(
                                   candidate.person, candidate.timestamp)
                             : plan.counterparties().checkDepositPointsFor(
                                   candidate.person, candidate.timestamp);
    const auto source = stableExternalPoint(
        points, candidate.account, cash ? kCashPointDomain : kCheckPointDomain);
    if (!source.has_value()) {
      continue;
    }

    const auto channel = cash ? channels::tag(channels::Legit::cashDeposit)
                              : channels::tag(channels::Deposit::checkDeposit);
    if (!screen_.acceptTransfer(ledger::KeyedTransfer{
            .source = *source,
            .destination = candidate.account,
            .amount = candidate.amount,
            .channel = channel,
            .timestamp = candidate.timestamp,
        })) {
      continue;
    }

    out.push_back(txf_.make(transactions::Draft{
        .source = *source,
        .destination = candidate.account,
        .amount = candidate.amount,
        .timestamp = candidate.timestamp,
        .isFraud = 0,
        .ringId = -1,
        .channel = channel,
    }));
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::deposits
