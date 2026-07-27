//
// tests/test_card_scam_rail.cpp
//
// victimization-v3: THE AUTHORIZED-SCAM RAIL — docs/card_fraud_
// victimization.md D2.
//
// TWO LAYERS, deliberately in one place: the pure layer pins the shape
// of the model, and the world layer pins that the fold actually
// exercises it. Either one alone can pass while the corpus is wrong —
// a beautiful hazard function nothing calls, or a rail that emits rows
// on a gradient nobody declared.
//
// THE PURE GATES (no world build). The two age gradients are OPPOSITE
// and both are load-bearing:
//
//   INCIDENCE falls with age   (FTC CSN: per-capita fraud reports peak
//                               in the 20s-30s and decline after 60)
//   SEVERITY rises with age    (same series: median reported loss climbs
//                               monotonically, oldest band ~3x youngest)
//
// A model that carried only one would be wrong in a way the corpus
// cannot recover from — tilt incidence toward the old and every
// per-capita figure inverts; drop severity and the amount distribution
// stops telling a student from a retiree. Both directions are asserted
// here as code invariants, because both are easy to "simplify" away
// later by someone who remembers only that scams target the elderly.
//
// The persona factors are pinned too, including the three that are
// exactly 1.00. Persona and age are strongly correlated in this model, so
// giving retirees a low persona factor AND a low age factor would
// double-count one gradient; the 1.00s record that decision as a fact
// about the code rather than a comment.
//
// THE WORLD GATES. The rail exists, is labelled, is never reimbursed,
// rides channels that carry heavy legitimate volume, and — the invariant
// that matters most — never names a victim who is DEAD or who has not
// JOINED the bank at the case date. A dead person cannot be talked into
// authorizing a payment; that is not a typology, it is an impossibility.
// (The unauthorized card and ATO rails are deliberately NOT held to the
// alive half: deceased-account fraud is real, and that exemption was
// declared before this round — authority U-8 addendum.)
//
// ANTI-SHORTCUT. The tilt that makes age predict WHO gets scammed must
// not make age predict WHICH ROWS are scams. Two bounds: the realized
// per-band hazard spread (structural — the clamp), and an age-only
// recall sweep at high precision (behavioural — the same construction
// tests/test_card_victim_baselines.cpp uses for persona and merchant
// identity). The second is a regression barrier rather than a
// discriminator at this base rate, and is reported as such.
//

#include "phantomledger/entities/holdings/cards.hpp"
#include "phantomledger/pipeline/simulate.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/personas/timeline.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/fraud/types.hpp"
#include "phantomledger/taxonomies/personas/names.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"
#include "phantomledger/transactions/clearing/balance_book.hpp"
#include "phantomledger/transfers/channels/credit_cards/lifecycle.hpp"
#include "phantomledger/transfers/fraud/susceptibility.hpp"
#include "phantomledger/transfers/fraud/typologies/amounts.hpp"

#include "gate_world.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace pl = ::PhantomLedger;
namespace channels = pl::channels;
namespace personas = pl::personas;
namespace timeline = pl::synth::personas::timeline;
namespace susceptibility = pl::transfers::fraud::susceptibility;
namespace amounts = pl::transfers::fraud::typologies::amounts;

namespace {

constexpr std::uint64_t kSeed = 4242;
constexpr std::int32_t kPopulation = 600;
constexpr int kDays = 1461; // four whole years, 1991-1994

constexpr int kAgeBandYears = 10;

// Same bound and floor as every other baseline in this arc.
constexpr double kMaxRecallAtHighPrecision = 0.25;
constexpr double kPrecisionFloor = 0.90;

// The realized per-band hazard spread. Bounded by the clamp at
// kMaxScamWeight / kMinScamWeight = 12x, but the DECLARED factors put it
// near 2.2x, so this gate binds long before the clamp does — it is what
// catches a future factor table that quietly turns the tilt into a
// lookup.
constexpr double kMaxBandHazardSpread = 3.0;

int g_failures = 0;

void check(bool condition, const std::string &what) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

struct Counts {
  std::size_t fraud = 0;
  std::size_t legit = 0;

  [[nodiscard]] std::size_t total() const { return fraud + legit; }
  [[nodiscard]] double rate() const {
    return total() == 0 ? 0.0
                        : static_cast<double>(fraud) /
                              static_cast<double>(total());
  }
};

struct Amounts {
  std::size_t rows = 0;
  double sum = 0.0;

  void add(double amount) {
    ++rows;
    sum += amount;
  }
  [[nodiscard]] double mean() const {
    return rows == 0 ? 0.0 : sum / static_cast<double>(rows);
  }
};

[[nodiscard]] pl::time::Window window() {
  pl::time::Window w;
  w.start = pl::time::makeTime({1991, 1, 1});
  w.days = kDays;
  return w;
}

[[nodiscard]] pl::pipeline::SimulationResult
runWorld(const pl::synth::pii::PoolSet &poolSet,
         const pl::synth::people::Fraud &fraudProfile) {
  const auto w = window();

  const pl::pipeline::stages::entities::EntitySynthesis entities{
      .population = kPopulation,
      .identity =
          pl::synth::pii::IdentityContext{
              .pools = &poolSet,
              .simStart = w.start,
              .localeMix = pl::synth::pii::LocaleMix::usOnly(),
          },
      .fraud = fraudProfile,
  };

  pl::clearing::BalanceRules balanceRules{};
  pl::transfers::credit_cards::LifecycleRules lifecycleRules{};

  auto rng = pl::random::Rng::fromSeed(kSeed);
  pl::pipeline::SimulationPipeline pipeline{rng, w, entities, kSeed};
  pipeline.transferStage()
      .legit()
      .window(w)
      .seed(kSeed)
      .openingBalanceRules(&balanceRules)
      .creditLifecycle(&lifecycleRules);
  pipeline.transferStage().fraud().profile(&fraudProfile);

  return pipeline.run();
}

/// Rank groups by empirical rate, admit them one at a time, and report
/// the best recall reached while precision still clears the floor.
[[nodiscard]] double sweep(const std::vector<Counts> &groupsIn,
                           std::size_t positives, double *bestPrecisionOut) {
  auto groups = groupsIn;
  std::sort(groups.begin(), groups.end(),
            [](const Counts &a, const Counts &b) {
              if (a.rate() != b.rate()) {
                return a.rate() > b.rate();
              }
              return a.total() > b.total();
            });

  std::size_t admittedPositive = 0;
  std::size_t admittedTotal = 0;
  double bestRecall = 0.0;
  double bestPrecision = 0.0;

  for (const auto &g : groups) {
    admittedPositive += g.fraud;
    admittedTotal += g.total();
    if (admittedTotal == 0) {
      continue;
    }
    const double precision = static_cast<double>(admittedPositive) /
                             static_cast<double>(admittedTotal);
    const double recall =
        static_cast<double>(admittedPositive) / static_cast<double>(positives);
    bestPrecision = std::max(bestPrecision, precision);
    if (precision >= kPrecisionFloor) {
      bestRecall = std::max(bestRecall, recall);
    }
  }

  if (bestPrecisionOut != nullptr) {
    *bestPrecisionOut = bestPrecision;
  }
  return bestRecall;
}

// ------------------------------------------------------ THE PURE GATES

void pureGates() {
  std::printf("pure gates: the two opposite age gradients\n");

  // Band midpoints, youngest to oldest.
  static constexpr double kAges[] = {25.0, 35.0, 45.0, 55.0, 65.0, 75.0, 85.0};

  double prevIncidence = 1e9;
  double prevSeverity = -1.0;
  for (const double age : kAges) {
    const double incidence = susceptibility::scamAgeIncidence(age);
    const double severity = susceptibility::scamAgeSeverity(age);
    std::printf("  age %5.1f  incidence %.2f  severity %.2f\n", age, incidence,
                severity);
    check(incidence <= prevIncidence,
          "per-capita scam INCIDENCE must not rise with age (FTC CSN: "
          "reports peak in the 20s-30s and decline after 60) — it rose at "
          "age " + std::to_string(age));
    check(severity >= prevSeverity,
          "per-case scam SEVERITY must not fall with age (FTC CSN: median "
          "reported loss climbs monotonically) — it fell at age " +
              std::to_string(age));
    prevIncidence = incidence;
    prevSeverity = severity;
  }

  const double youngest = susceptibility::scamAgeSeverity(kAges[0]);
  const double oldest =
      susceptibility::scamAgeSeverity(kAges[std::size(kAges) - 1]);
  const double ratio = oldest / youngest;
  std::printf("  severity ratio oldest/youngest %.2f  (target ~3x)\n", ratio);
  check(ratio > 2.0 && ratio < 4.5,
        "the severity span must reproduce the ~3x FTC median-loss ratio "
        "across age bands; measured " + std::to_string(ratio));

  const double incidenceSpread =
      susceptibility::scamAgeIncidence(kAges[0]) /
      susceptibility::scamAgeIncidence(kAges[std::size(kAges) - 1]);
  std::printf("  incidence spread youngest/oldest %.2f\n", incidenceSpread);
  check(incidenceSpread > 1.5 && incidenceSpread < 4.0,
        "the incidence gradient must be present but bounded (a strong tilt "
        "would make age predict the label); measured " +
            std::to_string(incidenceSpread));

  // THE NO-DOUBLE-COUNT DECISION, as an invariant. Persona factors carry
  // NON-AGE structure only: a retiree's low hazard is age, and asserting
  // it twice would manufacture a gradient the research does not support.
  check(susceptibility::scamPersonaFactor(personas::Type::retiree) == 1.00,
        "the retiree persona factor must stay 1.00 — its gradient is AGE, "
        "and stating it twice double-counts one effect");
  check(susceptibility::scamPersonaFactor(personas::Type::highNetWorth) ==
            1.00,
        "the highNetWorth persona factor must stay 1.00 — wealth belongs to "
        "severity, not to how often the phone rings");
  check(susceptibility::scamPersonaFactor(personas::Type::salaried) == 1.00,
        "salaried is the reference persona");
  for (const auto type : personas::kTypes) {
    const double f = susceptibility::scamPersonaFactor(type);
    check(f >= 1.00 && f <= 1.40,
          std::string("persona factor for ") + std::string(personas::name(type)) +
              " must stay in the declared band [1.00, 1.40]");
  }

  // THE LEVEL FACT: an authorized push is an order of magnitude above a
  // fraudulent card spend. Fewer cases, far larger losses — this is the
  // single most important quantitative difference between the rails.
  auto rng = pl::random::Rng::fromSeed(90210);
  Amounts push;
  Amounts card;
  Amounts pushOld;
  constexpr int kDraws = 20000;
  for (int i = 0; i < kDraws; ++i) {
    push.add(amounts::scamWireAmount(rng, 1.0, 1.0));
    pushOld.add(amounts::scamWireAmount(rng, 1.0, 2.20));
    card.add(amounts::cardFraudSpend(rng, 1.0));
  }
  std::printf("  mean push $%.0f  mean card $%.0f  mean push@2.20 $%.0f\n",
              push.mean(), card.mean(), pushOld.mean());
  check(push.mean() > 5.0 * card.mean(),
        "an authorized push must be an order of magnitude above a card-rail "
        "spend; measured push " + std::to_string(push.mean()) + " vs card " +
            std::to_string(card.mean()));

  const double levelRatio = pushOld.mean() / push.mean();
  std::printf("  severity level ratio %.2f  (applied 2.20)\n", levelRatio);
  check(levelRatio > 1.9 && levelRatio < 2.5,
        "the severity multiplier must scale the whole distribution — median "
        "and both clamps — so only the LEVEL moves; measured " +
            std::to_string(levelRatio));
}

} // namespace

int main() {
  try {
    pureGates();

    const auto poolSet = pltest::buildPoolSet(kSeed);
    const auto fraudProfile = pltest::scaledFraudProfile();

    std::printf("scam rail: %d people, %d days from 1991\n", kPopulation,
                kDays);
    std::fflush(stdout);
    const auto result = runWorld(poolSet, fraudProfile);

    std::unordered_map<pl::entity::Key, pl::entity::PersonId> accountOwner;
    accountOwner.reserve(result.holdings.accounts.registry.records.size());
    for (const auto &record : result.holdings.accounts.registry.records) {
      accountOwner.emplace(record.id, record.owner);
    }
    const auto ownerOf =
        [&](const pl::entity::Key &key) -> pl::entity::PersonId {
      if (const auto *terms = result.holdings.creditCards.forKey(key)) {
        return terms->owner;
      }
      if (const auto it = accountOwner.find(key); it != accountOwner.end()) {
        return it->second;
      }
      return pl::entity::invalidPerson;
    };

    const auto &pack = result.people.personas;
    const auto population = result.people.roster.roster.count;
    check(!pack.timelines.empty() && !pack.birthDates.empty(),
          "the persona timeline and birth-date carriers must be filled — "
          "without them the whole v3 model degrades silently");
    if (g_failures != 0) {
      return 1;
    }

    const auto w = window();

    // ------------------------------------------------ THE RAIL ITSELF
    std::size_t impostorRows = 0;
    std::size_t giftCardRows = 0;
    std::size_t impostorWire = 0;
    std::size_t impostorPush = 0;
    std::size_t legitExternal = 0;
    std::size_t legitP2p = 0;
    std::size_t deadVictims = 0;
    std::size_t preJoinVictims = 0;
    std::size_t unresolvedScam = 0;

    Amounts impostorAmounts;
    Amounts cardFraudAmounts;
    std::map<int, Amounts> impostorByBand;   // severity axis
    std::map<int, Counts> scamByBand;        // anti-shortcut axis
    std::map<personas::Type, Counts> scamByPersona;

    // Every (victim, cents) pair a scam row produced, so a reimbursement
    // credit back to the victim would be visible.
    std::map<std::pair<pl::entity::PersonId, std::int64_t>, std::size_t>
        scamDebits;

    for (const auto &t : result.transfers.ledger.posted.txns) {
      const auto chan = t.session.channel.value;
      if (chan == channels::tag(channels::Legit::externalUnknown).value &&
          t.fraud.flag == 0) {
        ++legitExternal;
      }
      if (chan == channels::tag(channels::Legit::p2p).value &&
          t.fraud.flag == 0) {
        ++legitP2p;
      }

      const auto type = t.fraud.type;
      const bool impostor = type == pl::fraud::FraudType::scamImpostor;
      const bool giftCard = type == pl::fraud::FraudType::scamGiftCard;

      if (type == pl::fraud::FraudType::txnFraudSolo &&
          chan == channels::tag(channels::Legit::cardPurchase).value) {
        cardFraudAmounts.add(t.amount);
      }

      // The anti-shortcut axes cover the whole card + push view, with the
      // SCAM label as the positive class.
      const bool inScamView =
          chan == channels::tag(channels::Legit::cardPurchase).value ||
          chan == channels::tag(channels::Legit::merchant).value ||
          chan == channels::tag(channels::Legit::p2p).value ||
          chan == channels::tag(channels::Legit::externalUnknown).value;

      const auto person = ownerOf(t.source);
      const bool resolved = pl::entity::valid(person) &&
                            static_cast<std::uint32_t>(person) <= population &&
                            static_cast<std::size_t>(person) <=
                                pack.timelines.size();

      int band = -1;
      personas::Type persona = personas::Type::salaried;
      if (resolved) {
        const auto idx = static_cast<std::size_t>(person) - 1;
        const auto at = pl::time::fromEpochSeconds(t.timestamp);
        persona = timeline::personaAt(pack.timelines[idx], at);
        const auto born = pack.birthDates[idx];
        const auto now = pl::time::toCalendarDate(at);
        int age = now.year - born.year;
        if (now.month < born.month ||
            (now.month == born.month && now.day < born.day)) {
          --age;
        }
        band = (age / kAgeBandYears) * kAgeBandYears;
      }

      if (inScamView && resolved) {
        auto &bandCell = scamByBand[band];
        auto &personaCell = scamByPersona[persona];
        if (impostor || giftCard) {
          ++bandCell.fraud;
          ++personaCell.fraud;
        } else {
          ++bandCell.legit;
          ++personaCell.legit;
        }
      }

      if (!impostor && !giftCard) {
        continue;
      }

      if (impostor) {
        ++impostorRows;
        impostorAmounts.add(t.amount);
        if (chan == channels::tag(channels::Legit::externalUnknown).value) {
          ++impostorWire;
        } else if (chan == channels::tag(channels::Legit::p2p).value) {
          ++impostorPush;
        }
      } else {
        ++giftCardRows;
      }

      if (!resolved) {
        ++unresolvedScam;
        continue;
      }

      const auto idx = static_cast<std::size_t>(person) - 1;
      const auto at = pl::time::fromEpochSeconds(t.timestamp);

      // THE MEMBERSHIP PIN. An authorized scam requires a living
      // customer: a dead person cannot authorize a payment, and someone
      // who has not joined has no account to push from.
      if (!timeline::aliveAt(pack.timelines[idx], at)) {
        ++deadVictims;
      }
      if (idx < pack.joinDays.size()) {
        const auto joined =
            pl::time::addDays(w.start, static_cast<int>(pack.joinDays[idx]));
        if (at < joined) {
          ++preJoinVictims;
        }
      }

      if (impostor && band >= 0) {
        impostorByBand[band].add(t.amount);
        scamDebits[{person,
                    static_cast<std::int64_t>(t.amount * 100.0 + 0.5)}] += 1;
      }
    }

    std::printf("  rows: impostor %zu (wire %zu / push %zu), gift-card %zu\n",
                impostorRows, impostorWire, impostorPush, giftCardRows);
    check(impostorRows > 0,
          "the impostor-push rail must produce rows — a rail nobody reaches "
          "is a rail that does not exist");
    check(giftCardRows > 0,
          "the gift-card rail must survive v3 (it kept its .12 share)");
    check(impostorWire > 0 && impostorPush > 0,
          "the impostor rail must use BOTH payment methods; a single channel "
          "would hand a model the rail for free");
    check(legitExternal > 0 && legitP2p > 0,
          "both impostor channels must also carry legitimate volume "
          "(external " + std::to_string(legitExternal) + ", p2p " +
              std::to_string(legitP2p) + ") — that is what stops the channel "
              "from labelling the row");
    check(unresolvedScam == 0,
          "every scam row's source must resolve to a person; " +
              std::to_string(unresolvedScam) + " did not, so the gates below "
              "would be measuring a subset");

    check(deadVictims == 0,
          std::to_string(deadVictims) +
              " authorized-scam row(s) name a victim who was already DEAD at "
              "the case date — a dead person cannot authorize a payment");
    check(preJoinVictims == 0,
          std::to_string(preJoinVictims) +
              " authorized-scam row(s) name a victim who had not JOINED the "
              "bank at the case date — the account did not exist yet");

    // ------------------------------------------------- THE LEVEL FACT
    std::printf("  mean amount: impostor $%.0f over %zu rows, card fraud "
                "$%.0f over %zu rows\n",
                impostorAmounts.mean(), impostorAmounts.rows,
                cardFraudAmounts.mean(), cardFraudAmounts.rows);
    if (cardFraudAmounts.rows > 0) {
      check(impostorAmounts.mean() > 3.0 * cardFraudAmounts.mean(),
            "an authorized push must dwarf a card-rail spend in the corpus, "
            "not just in the sampler");
    }

    // ------------------------------- THE HAZARD GRADIENT (exact, no noise)
    // Measured on the ACTUAL population through the same function the
    // picker calls, at window start and at window end. No sampling is
    // involved, so this is a statement about the model rather than about
    // this seed.
    const susceptibility::VictimPopulation victims{&pack, w.start};
    for (const int offsetDays : {0, kDays - 1}) {
      const auto at = pl::time::addDays(w.start, offsetDays);
      const auto weights =
          victims.scamWeights(at, static_cast<std::size_t>(population));
      check(!weights.empty(),
            "the scam hazard must be computable over the real population");
      if (weights.empty()) {
        continue;
      }

      std::map<int, Amounts> byBand;
      for (std::size_t idx = 0; idx < weights.size(); ++idx) {
        if (!(weights[idx] > 0.0)) {
          continue; // ineligible at this date
        }
        const auto born = pack.birthDates[idx];
        const auto now = pl::time::toCalendarDate(at);
        int age = now.year - born.year;
        if (now.month < born.month ||
            (now.month == born.month && now.day < born.day)) {
          --age;
        }
        byBand[(age / kAgeBandYears) * kAgeBandYears].add(weights[idx]);
      }

      std::printf("  hazard at day %d:\n", offsetDays);
      double lowest = 1e9;
      double highest = 0.0;
      double youngMean = 0.0;
      double oldMean = 0.0;
      std::size_t youngRows = 0;
      std::size_t oldRows = 0;
      for (const auto &[band, cell] : byBand) {
        std::printf("    age %3d-%3d  n=%4zu  mean weight %.3f\n", band,
                    band + kAgeBandYears - 1, cell.rows, cell.mean());
        if (cell.rows < 5) {
          continue; // too few to speak for a band
        }
        lowest = std::min(lowest, cell.mean());
        highest = std::max(highest, cell.mean());
        if (band < 40) {
          youngMean += cell.sum;
          youngRows += cell.rows;
        } else if (band >= 60) {
          oldMean += cell.sum;
          oldRows += cell.rows;
        }
      }

      if (youngRows >= 5 && oldRows >= 5) {
        const double young = youngMean / static_cast<double>(youngRows);
        const double old = oldMean / static_cast<double>(oldRows);
        std::printf("    under-40 mean %.3f  vs  60+ mean %.3f\n", young, old);
        check(young > old,
              "scam INCIDENCE hazard must be higher for under-40 than for "
              "60+ — the severity gradient runs the other way and the two "
              "must not be conflated");
      }

      if (highest > 0.0 && lowest < 1e9) {
        const double spread = highest / lowest;
        std::printf("    band spread %.2f  <- gate: < %.2f\n", spread,
                    kMaxBandHazardSpread);
        check(spread < kMaxBandHazardSpread,
              "the per-band hazard spread must stay bounded; " +
                  std::to_string(spread) +
                  " means age is on its way to predicting the label");
      }
    }

    // ------------------------------------------ SEVERITY, IN THE CORPUS
    // Printed always, gated only where the cells are large enough to
    // speak: at this population the oldest bands hold few victims, and a
    // gate on three rows would fail for the wrong reason.
    std::printf("  impostor amount by victim age band:\n");
    double youngSum = 0.0;
    double oldSum = 0.0;
    std::size_t youngRows = 0;
    std::size_t oldRows = 0;
    for (const auto &[band, cell] : impostorByBand) {
      std::printf("    age %3d-%3d  n=%4zu  mean $%.0f\n", band,
                  band + kAgeBandYears - 1, cell.rows, cell.mean());
      if (band < 50) {
        youngSum += cell.sum;
        youngRows += cell.rows;
      } else if (band >= 60) {
        oldSum += cell.sum;
        oldRows += cell.rows;
      }
    }
    if (youngRows >= 10 && oldRows >= 10) {
      const double young = youngSum / static_cast<double>(youngRows);
      const double old = oldSum / static_cast<double>(oldRows);
      std::printf("    under-50 mean $%.0f  vs  60+ mean $%.0f\n", young, old);
      check(old > young,
            "per-case scam loss must be LARGER for older victims (FTC CSN "
            "median loss rises with age) — measured under-50 " +
                std::to_string(young) + " vs 60+ " + std::to_string(old));
    } else {
      std::printf("    (severity gate stood down: %zu under-50 / %zu 60+ "
                  "rows is too few to speak)\n",
                  youngRows, oldRows);
    }

    // -------------------------------------- NO REIMBURSEMENT, EVER
    std::size_t reimbursed = 0;
    for (const auto &t : result.transfers.ledger.posted.txns) {
      if (t.session.channel.value !=
          channels::tag(channels::Credit::chargeback).value) {
        continue;
      }
      const auto person = ownerOf(t.target);
      if (!pl::entity::valid(person)) {
        continue;
      }
      const auto cents = static_cast<std::int64_t>(t.amount * 100.0 + 0.5);
      if (scamDebits.contains({person, cents})) {
        ++reimbursed;
      }
    }
    std::printf("  chargeback credits matching a scam debit: %zu\n",
                reimbursed);
    check(reimbursed == 0,
          "an AUTHORIZED push must never be reimbursed — Reg E covers "
          "unauthorized transfers and the UK code postdates the window; " +
              std::to_string(reimbursed) + " credit(s) matched a scam debit");

    // ------------------------------------------------- ANTI-SHORTCUT
    // The scam label as the positive class, an age-band-only and a
    // persona-only classifier as the attacker. A regression barrier at
    // this base rate rather than a discriminator — reported as such.
    std::size_t scamPositives = 0;
    std::vector<Counts> bandGroups;
    for (const auto &[band, cell] : scamByBand) {
      (void)band;
      scamPositives += cell.fraud;
      bandGroups.push_back(cell);
    }
    std::vector<Counts> personaGroups;
    for (const auto &[persona, cell] : scamByPersona) {
      (void)persona;
      personaGroups.push_back(cell);
    }

    if (scamPositives > 0) {
      double bandPrecision = 0.0;
      double personaPrecision = 0.0;
      const double bandRecall = sweep(bandGroups, scamPositives, &bandPrecision);
      const double personaRecall =
          sweep(personaGroups, scamPositives, &personaPrecision);
      std::printf("  age-only:     best precision %.4f, recall@P>=%.2f "
                  "%.4f  <- gate: < %.2f\n",
                  bandPrecision, kPrecisionFloor, bandRecall,
                  kMaxRecallAtHighPrecision);
      std::printf("  persona-only: best precision %.4f, recall@P>=%.2f "
                  "%.4f  <- gate: < %.2f\n",
                  personaPrecision, kPrecisionFloor, personaRecall,
                  kMaxRecallAtHighPrecision);
      check(bandRecall < kMaxRecallAtHighPrecision,
            "an AGE-ONLY classifier must not recover the scam label at high "
            "precision");
      check(personaRecall < kMaxRecallAtHighPrecision,
            "a PERSONA-ONLY classifier must not recover the scam label at "
            "high precision");
    }

    if (g_failures != 0) {
      std::fprintf(stderr, "%d gate(s) failed\n", g_failures);
      return 1;
    }
    std::printf("test_card_scam_rail: OK\n");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "EXCEPTION: %s\n", e.what());
    return 1;
  }
}
