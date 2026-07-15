#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transfers/fraud/engine.hpp"
#include "phantomledger/transfers/fraud/typologies/unauthorized.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <vector>

namespace pl = ::PhantomLedger;
namespace fraud = pl::transfers::fraud;
namespace unauth = fraud::typologies::unauthorized;

namespace {

int failures = 0;

void expect(bool cond, const char *what) {
  if (!cond) {
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", what);
  }
}

[[nodiscard]] std::vector<unauth::CompromisePlan> makePlans() {
  std::vector<unauth::CompromisePlan> plans;

  const auto victimA = pl::entity::makeKey(pl::entity::Role::account,
                                           pl::entity::Bank::internal, 101);
  const auto victimB = pl::entity::makeKey(pl::entity::Role::account,
                                           pl::entity::Bank::internal, 202);
  const auto victimC = pl::entity::makeKey(pl::entity::Role::account,
                                           pl::entity::Bank::internal, 303);
  const auto victimD = pl::entity::makeKey(pl::entity::Role::account,
                                           pl::entity::Bank::internal, 404);
  const auto drop = pl::entity::makeKey(pl::entity::Role::account,
                                        pl::entity::Bank::external, 999);

  // Card-rail plan, multi-event (tests + spends).
  plans.push_back(unauth::CompromisePlan{
      .victimAccount = victimA,
      .dropAccount = {},
      .device = pl::devices::Identity{.ownerType = pl::devices::OwnerType::ring,
                                      .ownerId = 0xACE00000ULL,
                                      .slot = 0},
      .ip = pl::network::Ipv4::pack(198, 51, 100, 1),
      .cardRail = true,
      .startTs = 1'000'000,
      .spanSeconds = 3600 * 24,
      .targetEvents = 6,
      .seq = 0,
  });

  // ATO drain plan.
  plans.push_back(unauth::CompromisePlan{
      .victimAccount = victimB,
      .dropAccount = drop,
      .device = pl::devices::Identity{.ownerType = pl::devices::OwnerType::ring,
                                      .ownerId = 0xACE00001ULL,
                                      .slot = 0},
      .ip = pl::network::Ipv4::pack(198, 51, 100, 2),
      .cardRail = false,
      .startTs = 2'000'000,
      .spanSeconds = 3600 * 8,
      .targetEvents = 3,
      .seq = 1,
  });

  // Second card-rail plan with different shape.
  plans.push_back(unauth::CompromisePlan{
      .victimAccount = victimC,
      .dropAccount = {},
      .device = pl::devices::Identity{.ownerType = pl::devices::OwnerType::ring,
                                      .ownerId = 0xACE00002ULL,
                                      .slot = 0},
      .ip = pl::network::Ipv4::pack(198, 51, 100, 3),
      .cardRail = true,
      .startTs = 3'000'000,
      .spanSeconds = 3600 * 72,
      .targetEvents = 9,
      .seq = 2,
  });

  // Second ATO plan.
  plans.push_back(unauth::CompromisePlan{
      .victimAccount = victimD,
      .dropAccount = drop,
      .device = pl::devices::Identity{.ownerType = pl::devices::OwnerType::ring,
                                      .ownerId = 0xACE00003ULL,
                                      .slot = 0},
      .ip = pl::network::Ipv4::pack(198, 51, 100, 4),
      .cardRail = false,
      .startTs = 4'000'000,
      .spanSeconds = 3600 * 30,
      .targetEvents = 5,
      .seq = 3,
  });

  return plans;
}

[[nodiscard]] std::vector<pl::entity::Key> makeBillers() {
  std::vector<pl::entity::Key> billers;
  for (std::uint64_t n = 1; n <= 5; ++n) {
    billers.push_back(pl::entity::makeKey(
        pl::entity::Role::business, pl::entity::Bank::external, 5'000 + n));
  }
  return billers;
}

[[nodiscard]] bool sameTxn(const pl::transactions::Transaction &a,
                           const pl::transactions::Transaction &b) {
  return a.source == b.source && a.target == b.target && a.amount == b.amount &&
         a.timestamp == b.timestamp && a.session.channel == b.session.channel &&
         a.fraud.flag == b.fraud.flag &&
         a.session.deviceId == b.session.deviceId &&
         a.session.ipAddress == b.session.ipAddress;
}

} // namespace

int main() {
  constexpr std::uint64_t kFactorySeed = 0xF00DF00DULL;
  constexpr std::int32_t kBudget = 1000; // never binding

  const auto plans = makePlans();
  const auto billers = makeBillers();
  const std::span<const pl::entity::Key> billerSpan(billers.data(),
                                                    billers.size());

  // ---- Run A: all plans in one call, fresh context ----
  pl::random::RngFactory factoryA{kFactorySeed};
  auto seqRngA = pl::random::Rng::fromSeed(1);
  fraud::IllicitContext ctxA{
      .execution =
          fraud::Execution{
              .txf = pl::transactions::Factory(seqRngA),
              .rng = &seqRngA,
              .factory = &factoryA,
          },
      .window = {},
      .billerAccounts = billerSpan,
  };
  const auto outA = unauth::generate(
      ctxA, std::span<const unauth::CompromisePlan>(plans.data(), plans.size()),
      kBudget);
  expect(!outA.empty(), "run A produced transactions");

  // ---- Run B: burned history, per-plan calls, reverse order ----
  pl::random::RngFactory factoryB{kFactorySeed};
  auto seqRngB = pl::random::Rng::fromSeed(999);
  for (int burn = 0; burn < 1000; ++burn) {
    (void)seqRngB.nextDouble(); // simulate a different process history
  }
  fraud::IllicitContext ctxB{
      .execution =
          fraud::Execution{
              .txf = pl::transactions::Factory(seqRngB),
              .rng = &seqRngB,
              .factory = &factoryB,
          },
      .window = {},
      .billerAccounts = billerSpan,
  };

  std::vector<std::vector<pl::transactions::Transaction>> perPlanB(
      plans.size());
  for (std::size_t i = plans.size(); i-- > 0;) {
    perPlanB[i] = unauth::generate(
        ctxB, std::span<const unauth::CompromisePlan>(&plans[i], 1), kBudget);
  }

  // ---- Compare: A sliced in plan order must equal B's per-plan runs ----
  std::size_t cursor = 0;
  for (std::size_t i = 0; i < plans.size(); ++i) {
    const auto &group = perPlanB[i];
    expect(!group.empty(), "per-plan run produced transactions");
    expect(cursor + group.size() <= outA.size(),
           "run A has enough transactions for this plan");
    if (cursor + group.size() > outA.size()) {
      break;
    }
    for (std::size_t k = 0; k < group.size(); ++k) {
      if (!sameTxn(outA[cursor + k], group[k])) {
        ++failures;
        std::fprintf(stderr,
                     "FAIL: plan %zu txn %zu differs between batched and "
                     "isolated generation\n",
                     i, k);
      }
    }
    cursor += group.size();
  }
  expect(cursor == outA.size(),
         "batched output fully accounted for by per-plan output");

  if (failures != 0) {
    std::fprintf(stderr, "unauthorized keyed-stream: %d failure(s)\n",
                 failures);
    return EXIT_FAILURE;
  }

  std::printf("unauthorized keyed-stream independence holds: %zu plans, %zu "
              "transactions, history- and batch-invariant\n",
              plans.size(), outA.size());
  return EXIT_SUCCESS;
}
