#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/devices.hpp"
#include "phantomledger/entities/infra/ipv4.hpp"
#include "phantomledger/entities/infra/router.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transfers/fraud/engine.hpp"
#include "phantomledger/transfers/fraud/typologies/unauthorized.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <unordered_map>
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

// The victim roster used by the session gate below. One person per plan,
// each with EXACTLY ONE device and ONE IP — with a single-entry pool
// `Router::routeFromPool` returns items[0] and never reaches its
// switch coin, so routing is draw-free and the expected session is
// exact rather than stream-dependent.
[[nodiscard]] pl::entity::PersonId personForVictim(std::uint64_t acct) {
  return static_cast<pl::entity::PersonId>(acct / 101);
}

[[nodiscard]] pl::devices::Identity ownDevice(pl::entity::PersonId p) {
  return pl::devices::Identity::person(static_cast<std::uint64_t>(p), 1);
}

[[nodiscard]] pl::network::Ipv4 ownIp(pl::entity::PersonId p) {
  return pl::network::Ipv4::pack(10, 0, 0, static_cast<std::uint8_t>(p));
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
  const auto victimE = pl::entity::makeKey(pl::entity::Role::account,
                                           pl::entity::Bank::internal, 505);
  const auto victimF = pl::entity::makeKey(pl::entity::Role::account,
                                           pl::entity::Bank::internal, 606);
  const auto drop = pl::entity::makeKey(pl::entity::Role::account,
                                        pl::entity::Bank::external, 999);

  // Card-rail plan, multi-event (tests + spends + reimbursements).
  plans.push_back(unauth::CompromisePlan{
      .victimAccount = victimA,
      .dropAccount = {},
      .device = pl::devices::Identity{.ownerType = pl::devices::OwnerType::ring,
                                      .ownerId = 0xACE00000ULL,
                                      .slot = 0},
      .ip = pl::network::Ipv4::pack(198, 51, 100, 1),
      .rail = unauth::Rail::card,
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
      .rail = unauth::Rail::ato,
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
      .rail = unauth::Rail::card,
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
      .rail = unauth::Rail::ato,
      .startTs = 4'000'000,
      .spanSeconds = 3600 * 30,
      .targetEvents = 5,
      .seq = 3,
  });

  // Gift-card scam plan (victim-authorized burst, scam-fraud-2026-07).
  plans.push_back(unauth::CompromisePlan{
      .victimAccount = victimE,
      .dropAccount = {},
      .device = pl::devices::Identity{.ownerType = pl::devices::OwnerType::ring,
                                      .ownerId = 0xACE00004ULL,
                                      .slot = 0},
      .ip = pl::network::Ipv4::pack(198, 51, 100, 5),
      .rail = unauth::Rail::giftCardScam,
      .startTs = 5'000'000,
      .spanSeconds = 3600 * 3,
      .targetEvents = 4,
      .seq = 4,
  });

  // Impostor-push plan (the OTHER victim-authorized rail,
  // victimization-v3). Added with the session gate below: the
  // authorized-rail session fix has two rails and a gate that exercised
  // only one of them would leave half the change unmeasured.
  plans.push_back(unauth::CompromisePlan{
      .victimAccount = victimF,
      .dropAccount = drop,
      .device = pl::devices::Identity{.ownerType = pl::devices::OwnerType::ring,
                                      .ownerId = 0xACE00005ULL,
                                      .slot = 0},
      .ip = pl::network::Ipv4::pack(198, 51, 100, 6),
      .rail = unauth::Rail::scamImpostor,
      .startTs = 6'000'000,
      .spanSeconds = 3600 * 4,
      .targetEvents = 3,
      .seq = 5,
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

// A Router over exactly the plan victims, mirroring the production
// wiring (`Router::build(rules, ownerOf, devicesByPerson, ipsByPerson)`
// in src/pipeline/stages/infra.cpp).
[[nodiscard]] pl::infra::Router
makeRouter(std::span<const unauth::CompromisePlan> plans) {
  std::unordered_map<pl::entity::Key, pl::entity::PersonId> ownerOf;
  std::unordered_map<pl::entity::PersonId, std::vector<pl::devices::Identity>>
      devicesByPerson;
  std::unordered_map<pl::entity::PersonId, std::vector<pl::network::Ipv4>>
      ipsByPerson;

  for (const auto &plan : plans) {
    const auto person = personForVictim(plan.victimAccount.number);
    ownerOf.emplace(plan.victimAccount, person);
    devicesByPerson[person] = {ownDevice(person)};
    ipsByPerson[person] = {ownIp(person)};
  }

  return pl::infra::Router::build(pl::infra::RoutingRules{}, std::move(ownerOf),
                                  std::move(devicesByPerson),
                                  std::move(ipsByPerson));
}

[[nodiscard]] bool sameTxn(const pl::transactions::Transaction &a,
                           const pl::transactions::Transaction &b) {
  return a.source == b.source && a.target == b.target && a.amount == b.amount &&
         a.timestamp == b.timestamp && a.session.channel == b.session.channel &&
         a.fraud.flag == b.fraud.flag && a.fraud.type == b.fraud.type &&
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
  const std::span<const unauth::CompromisePlan> planSpan(plans.data(),
                                                         plans.size());

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
  const auto outA = unauth::generate(ctxA, planSpan, kBudget);
  expect(!outA.empty(), "run A produced transactions");

  // The gift-card scam rail must produce its own label class, and its
  // rows must never be reimbursed (no flag-0 rows follow a scam plan).
  {
    std::size_t scamRows = 0;
    std::size_t reimbursements = 0;
    for (const auto &tx : outA) {
      if (tx.fraud.type == pl::fraud::FraudType::scamGiftCard) {
        ++scamRows;
        expect(tx.fraud.flag == 1, "scam rows carry the fraud flag");
        expect(tx.amount >= 50.0 && tx.amount <= 500.0,
               "scam amounts inside the gift-card band");
      }
      if (tx.fraud.flag == 0) {
        ++reimbursements;
        expect(tx.session.channel ==
                   pl::channels::tag(pl::channels::Credit::chargeback),
               "flag-0 rows are chargeback reimbursements");
      }
    }
    expect(scamRows == 4, "the scam plan emitted its 4 gift-card rows");
    expect(reimbursements > 0,
           "reported card compromises produced reimbursements");
  }

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

  // ---- Run C: WHO OPERATED THE ROW (victim-session-2026-07, step d) ----
  //
  // Runs A and B carry no Router, so every session there is empty; that
  // is deliberate and is what keeps the keyed-independence property above
  // measured on the generator alone. This leg supplies the Router and
  // gates the rail-conditional session.
  //
  //   authorized (giftCardScam, scamImpostor)  the VICTIM operated, on
  //       their own device — the row must carry the session the Router
  //       resolved for the victim, NOT the attacker's.
  //   unauthorized (card, ato)  a third party operated with stolen
  //       credentials — the exogenous attacker session is the modeled
  //       truth and must SURVIVE. This half is a TRIPWIRE: it fails if a
  //       later round over-applies the authorized-rail fix.
  {
    pl::random::RngFactory factoryC{kFactorySeed};
    auto seqRngC = pl::random::Rng::fromSeed(1);
    const auto router = makeRouter(planSpan);
    fraud::IllicitContext ctxC{
        .execution =
            fraud::Execution{
                .txf = pl::transactions::Factory(seqRngC, &router),
                .rng = &seqRngC,
                .factory = &factoryC,
            },
        .window = {},
        .billerAccounts = billerSpan,
    };
    const auto outC = unauth::generate(ctxC, planSpan, kBudget);

    std::unordered_map<std::uint64_t, const unauth::CompromisePlan *> byVictim;
    for (const auto &plan : plans) {
      byVictim.emplace(plan.victimAccount.number, &plan);
    }

    std::size_t authorizedRows = 0;
    std::size_t unauthorizedRows = 0;
    std::size_t giftCardRows = 0;
    std::size_t impostorRows = 0;
    std::size_t fraudDeviceRendered = 0;

    for (const auto &tx : outC) {
      if (tx.fraud.flag != 1) {
        // Chargeback credits are externally initiated and their source is
        // the merchant, so they carry no session on either engine.
        expect(!tx.session.deviceId.assigned(),
               "flag-0 chargeback rows carry no device");
        continue;
      }

      const auto it = byVictim.find(tx.source.number);
      expect(it != byVictim.end(), "every flag-1 row maps back to a plan");
      if (it == byVictim.end()) {
        continue;
      }
      const auto &plan = *it->second;
      const auto person = personForVictim(plan.victimAccount.number);

      if (unauth::authorizedRail(plan.rail)) {
        ++authorizedRows;
        if (plan.rail == unauth::Rail::giftCardScam) {
          ++giftCardRows;
        } else {
          ++impostorRows;
        }
        expect(tx.session.deviceId == ownDevice(person),
               "authorized-rail row carries the VICTIM'S own device");
        expect(tx.session.ipAddress == ownIp(person),
               "authorized-rail row carries the VICTIM'S own IP");
        expect(tx.session.deviceId != plan.device,
               "authorized-rail row does NOT carry the attacker device");
        expect(tx.session.ipAddress != plan.ip,
               "authorized-rail row does NOT carry the attacker IP");
        // Equivalent to "renders without the kFraudDevice 'FD' prefix":
        // exporter::common::renderDeviceId switches on ownerType, and
        // OwnerType::ring is the only branch that emits it. Asserted on
        // the identity rather than the rendering so this test stays
        // inside the transfers layer.
        expect(tx.session.deviceId.ownerType == pl::devices::OwnerType::person,
               "authorized-rail device renders as a person device, not FD");
      } else {
        ++unauthorizedRows;
        expect(tx.session.deviceId == plan.device,
               "unauthorized-rail row keeps the attacker device");
        expect(tx.session.ipAddress == plan.ip,
               "unauthorized-rail row keeps the attacker IP");
        if (tx.session.deviceId.ownerType == pl::devices::OwnerType::ring) {
          ++fraudDeviceRendered;
        }
      }
    }

    // PRECONDITIONS: neither half may pass vacuously, and BOTH authorized
    // rails must be present — the fix is rail-conditional, so a leg that
    // saw only one of them would gate half of it.
    expect(giftCardRows == 4, "the gift-card rail was exercised (4 rows)");
    expect(impostorRows == 3, "the impostor rail was exercised (3 rows)");
    expect(authorizedRows == 7, "both authorized rails were exercised");
    expect(unauthorizedRows > 0, "the card/ato tripwire was exercised");

    // DECLARED AND SIZED, not gated (docs/fraud_model_audit.md OPEN
    // ITEMS): every card/ato row still renders its device through
    // encoding::kFraudDevice ("FD"), a deterministic label in
    // public.transactions.device_id. The attacker device is CORRECT on
    // these rails; the defect is exporter-side rendering, registered as
    // its own item rather than folded into this fix.
    std::printf("session by rail: authorized %zu (giftCard %zu, impostor %zu) "
                "-> victim-own device; unauthorized %zu -> attacker device, "
                "of which %zu render with the 'FD' fraud-device prefix "
                "(DECLARED, registered separately)\n",
                authorizedRows, giftCardRows, impostorRows, unauthorizedRows,
                fraudDeviceRendered);
  }

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
