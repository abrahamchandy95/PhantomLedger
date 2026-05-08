#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/channels/subscriptions/bundle.hpp"
#include "phantomledger/transfers/channels/subscriptions/schedule.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_set>
#include <vector>

namespace PhantomLedger::transfers::subscriptions {

struct SubscriberAccount {
  entity::PersonId person = entity::invalidPerson;
  entity::Key deposit{};
};

struct SubscriberAccounts {
  std::span<const SubscriberAccount> rows;

  [[nodiscard]] bool empty() const noexcept { return rows.empty(); }
};

struct BillerDirectory {
  std::span<const entity::Key> accounts;

  [[nodiscard]] bool empty() const noexcept { return accounts.empty(); }
};

struct AccountExclusions {
  const std::unordered_set<entity::Key, std::hash<entity::Key>> *hubAccounts =
      nullptr;

  [[nodiscard]] bool contains(const entity::Key &account) const {
    return hubAccounts != nullptr && hubAccounts->contains(account);
  }
};

/// (timestamp, sub-index) pairs accumulated for a single month.
/// Storing only the index keeps the sort key small and cache-friendly.
struct Candidate {
  std::int64_t ts = 0;
  std::uint32_t subIdx = 0;
};

struct Screen {
  clearing::Ledger *ledger = nullptr;
  std::span<const transactions::Transaction> baseTxns;
};

class BundleBuilder {
public:
  BundleBuilder(const BundleRules &rules, const random::RngFactory &factory);

  [[nodiscard]] std::vector<Sub> build(SubscriberAccounts subscribers,
                                       BillerDirectory billers,
                                       AccountExclusions exclusions = {}) const;

private:
  const BundleRules *rules_ = nullptr;
  const random::RngFactory *factory_ = nullptr;
};

class ScheduleSampler {
public:
  explicit ScheduleSampler(time::Window window,
                           ScheduleRules rules = ScheduleRules{});

  ScheduleSampler(std::span<const time::TimePoint> monthStarts,
                  time::Window window, ScheduleRules rules = ScheduleRules{});

  [[nodiscard]] std::span<const time::TimePoint> monthStarts() const noexcept;

  [[nodiscard]] std::vector<Candidate>
  candidates(random::Rng &rng, time::TimePoint monthStart,
             std::span<const Sub> subs) const;

private:
  time::Window window_{};
  ScheduleRules rules_{};
  std::vector<time::TimePoint> monthStarts_;
  std::int64_t windowStartTs_ = 0;
  std::int64_t windowEndExclTs_ = 0;
};

class DebitEmitter {
public:
  explicit DebitEmitter(const transactions::Factory &txf) noexcept;
  DebitEmitter(const transactions::Factory &txf, const Screen &screen) noexcept;

  [[nodiscard]] std::vector<transactions::Transaction>
  emit(const ScheduleSampler &schedule, random::Rng &rng,
       std::span<const Sub> subs) const;

private:
  friend std::vector<transactions::Transaction>
  debits(const ScheduleSampler &schedule, random::Rng &rng,
         const DebitEmitter &emitter, std::span<const Sub> subs);

  void emitMonth(std::span<const Candidate> sorted, std::span<const Sub> subs,
                 std::vector<transactions::Transaction> &out,
                 std::size_t &baseCursor) const;

  void emitWithoutScreen(std::span<const Candidate> sorted,
                         std::span<const Sub> subs,
                         std::vector<transactions::Transaction> &out) const;

  void emitWithScreen(std::span<const Candidate> sorted,
                      std::span<const Sub> subs,
                      std::vector<transactions::Transaction> &out,
                      std::size_t &baseCursor) const;

  [[nodiscard]] bool screens() const noexcept;

  const transactions::Factory *txf_ = nullptr;
  const Screen *screen_ = nullptr;
};

[[nodiscard]] std::vector<transactions::Transaction>
debits(const ScheduleSampler &schedule, random::Rng &rng,
       const DebitEmitter &emitter, std::span<const Sub> subs);

} // namespace PhantomLedger::transfers::subscriptions
