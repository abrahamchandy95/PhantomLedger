//
// tests/test_card_endpoint_graph.cpp
//
// THE ENDPOINT LAYER MUST CARRY A MESSAGE (attacker-infra-2026-07).
//
// ===================================================================
// THE DEFECT THIS GATE EXISTS FOR, AND WHY NOTHING CAUGHT IT
//
// `buildCompromisePlans` minted a brand-new device and a brand-new IP
// for every unauthorized compromise:
//
//     .device = Identity{ring, 0xACE00000 + seq, 0}
//     .ip     = network::randomIpv4(rng)
//
// `seq` advanced once per accepted plan and the address came out of a
// ~3.5e9 space, so NO ATTACKER ENDPOINT WAS EVER SEEN TWICE. Cross-
// victim endpoint sharing was ZERO BY CONSTRUCTION. "One device touching
// many cards" is the single most valuable signal a card-fraud graph
// carries — it is the reason to model this as a graph rather than a
// table — and it was absent while four separate gates stayed green,
// because every one of them checked that endpoints were PRESENT and
// none checked that they were SHARED.
//
// That is the standing lesson this file encodes: A COUNT OF ENDPOINTS IS
// NOT A MEASUREMENT OF THE GRAPH. Degree distribution is the thing a
// message-passing model consumes, so degree distribution is what has to
// be gated.
//
// ===================================================================
// WHAT IS GATED
//
// A. REUSE EXISTS AND IS HEAVY-TAILED. Attacker endpoints must be seen
//    by more than one victim, the mean fan-out must clear a floor, and
//    the maximum must clear a much higher one — a flat degree
//    distribution would satisfy a mean-only gate while carrying none of
//    the structure an alert actually fires on.
//
// B. THE OPERATING POSITION SPLIT IS IN BAND. Unauthorized cases are
//    operated either from exogenous infrastructure or from the victim's
//    own endpoint (remote-access / household compromise). The realized
//    victim-endpoint share also ABSORBS every case the planner could not
//    attribute — no campaign live at the case date, or no single endpoint
//    covering the case span — so this sub-gate is simultaneously the
//    coverage check on the operator pool's own sizing.
//
//    THE TWO COMPONENTS ARE NOT SEPARABLE FROM THE CORPUS, and saying so
//    is the honest form of this gate: a row that kept the victim's
//    session carries no record of WHICH branch put it there. The band is
//    therefore on the TOTAL, anchored on the nominal share the planner
//    declares, with the headroom above it as the attribution budget. The
//    nominal is printed beside the realized value so the residual is
//    always visible even though it cannot be attributed.
//
//    Sub-gate B' gates the pool sizing DIRECTLY instead of inferring it:
//    mean concurrent campaigns, measured from the campaign spans. The
//    sizing rule claims that quantity is window-independent, and the two
//    legs — 4 years and 2 years, with the coverage floor binding in
//    both — are what test the claim rather than assume it.
//
// C. THE OWNERSHIP SHORTCUT IS DEAD, AND SIZED. `Has_Device`/`Has_IP`
//    shipped header-only for four rounds because "endpoint has no Party
//    edge" was an exact synonym for "attacker endpoint". This sub-gate
//    measures the rule's PRECISION on the card view and fails if it
//    climbs back toward determinism — while also requiring the feature
//    to retain real LIFT, because a shortcut replaced by pure noise
//    would be the opposite mistake. SIZE a shortcut before fixing it,
//    then keep sizing it.
//
// D. ATTACKER SESSIONS ARE POINT-IN-TIME HONEST. HARD ZERO: every row
//    carrying an attacker endpoint falls inside that endpoint's own
//    tenure. Not a band — the planner requires whole-case-span coverage
//    when it resolves, so an outside-tenure row is a logic error, and a
//    band would license the `device-ip-lifecycle` defect to return in
//    miniature.
//
// E. THE RESIDENTIAL-PROXY MECHANISM IS PRESENT. Attacker device beside
//    a CUSTOMER address on the same row. It is the structure that lets a
//    fraud transaction be reached from an unrelated Party through a
//    shared address node, and it is half of what kills C.
//
// F. THE SOCIAL-ENGINEERING RAILS HAVE NO ATTACKER ENDPOINT. HARD ZERO. A
//    scammer on the telephone produces no session: on the gift-card and
//    impostor-push rails the VICTIM operates the instrument, so the row
//    carries the victim's own routed endpoint and there is no attacker
//    device or address to attribute. Two independent guards enforce it
//    (`!scamRail` in the planner, `authorizedRail` in the typology), and
//    this sub-gate exists because both were previously covered only by a
//    hand-built unit fixture — which cannot catch a planner that starts
//    handing these rails an operator at production draw counts.
//
// ===================================================================
// WORLD SHAPE THIS GATE PINS (an equivalence gate must pin its world)
//
// The structural quantity in A is CASES PER OPERATOR, and it is a ratio
// of fraud volume to operator count. Operator count scales with
// population x campaign length; fraud volume scales with the corpus. A
// 900-person gate world at the production `targetTxnFraudP` would carry
// roughly one case per operator and part A would be measuring noise.
//
// So the legs RAISE THE FRAUD BUDGET to put cases-per-operator in the
// same range production runs at, and PRINT the realized ratio. That is a
// deliberate choice of which invariant to preserve: this gate is about
// STRUCTURE, not prevalence, and prevalence has its own suite
// (test_card_prevalence). Reproducing production's fraud RATE here would
// have meant reproducing none of its graph.
//
// TWO LEGS AT DIFFERENT SHAPES — a gate that only runs at one
// population is not coverage. Leg 1 is long and narrow (4 years, 900
// people), leg 2 short and wide (2 years, 1800). The sizing rule claims
// mean concurrent operators is population-driven and window-independent;
// two legs is the cheapest test of that claim.

#include "phantomledger/entities/holdings/card_reissue.hpp"
#include "phantomledger/entities/infra/attackers.hpp"
#include "phantomledger/entities/infra/enrollment.hpp"
#include "phantomledger/pipeline/simulate.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/pii/pools.hpp"
#include "phantomledger/synth/pii/samplers.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/fraud/types.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"
#include "phantomledger/transactions/clearing/balance_book.hpp"
#include "phantomledger/transfers/channels/credit_cards/lifecycle.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

namespace pl = ::PhantomLedger;
namespace channels = ::PhantomLedger::channels;

int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::printf("FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

// ------------------------------------------------------------- bands
//
// Every edge below is a DECLARED CHOICE. The ones that can fail for a
// real reason are called out; the rest are tripwires against a mechanism
// disappearing.

// A. reuse
constexpr double kMinSharedDeviceShare = 0.35; // devices seen by >1 victim
constexpr double kMinMeanVictimsPerDevice = 2.0;
constexpr std::size_t kMinMaxVictimsPerDevice = 12; // the heavy tail exists
constexpr double kMinSharedIpShare = 0.25;
constexpr double kMinMeanVictimsPerIp = 1.5;

// B. operating position. The nominal victim-endpoint share is 0.18; the
// band's headroom above it is the attribution-failure budget (no live
// campaign, or no endpoint covering the case span). If the realized
// value ever leaves this band the operator pool is mis-sized, which is
// exactly what this sub-gate is for.
constexpr double kVictimEndpointNominal = 0.18; // injector.cpp's declared share
constexpr double kVictimEndpointShareLo = 0.15;
constexpr double kVictimEndpointShareHi = 0.28;

// B'. mean CONCURRENT campaigns, the quantity the sizing rule's coverage
// floor is written in. The floor is 7.0; the band is what "the rule is
// working and window-independent" looks like from the outside, with room
// for the sampling spread of a few dozen lognormal campaign lengths.
//
// THIS SUB-GATE HAS ALREADY EARNED ITS KEEP. Its first run read 4.92 and
// 4.32 against a nominal 6.0 and turned red, and the cause was a real
// construction defect rather than a mis-set band: campaign starts were
// drawn over [0, W) instead of [-L, W), so the first ~L days of every run
// were covered only by campaigns beginning inside them. The disposition
// was to fix the generator, never to widen the band.
constexpr double kMeanConcurrentLo = 6.0;
constexpr double kMeanConcurrentHi = 14.0;

// C. the shortcut ceiling. Precision of "this endpoint has no Party
// association on file, therefore fraud", measured on the card view.
// Deterministic would be 1.0; this is the number that used to be 1.0 for
// the attacker half of the population.
constexpr double kMaxNotOnFilePrecision = 0.10;

// E. residential proxy
constexpr double kMinProxyShare = 0.15;
constexpr double kMaxProxyShare = 0.45;

struct Leg {
  const char *name;
  std::uint64_t seed;
  int startYear;
  std::int32_t days;
  std::int32_t population;
  double targetTxnFraudP;
};

[[nodiscard]] pl::synth::pii::PoolSet buildPoolSet(std::uint64_t seed) {
  pl::synth::pii::PoolSet poolSet;
  const pl::synth::pii::PoolSizes sizes;
  poolSet.byCountry[pl::taxonomies::enums::toIndex(pl::locale::Country::us)] =
      pl::synth::pii::buildLocalePool(pl::locale::Country::us, sizes,
                                      static_cast<std::uint32_t>(seed));
  return poolSet;
}

[[nodiscard]] pl::pipeline::SimulationResult
runLeg(const pl::synth::pii::PoolSet &poolSet, const Leg &leg) {
  const pl::time::Window window{
      .start = pl::time::makeTime({leg.startYear, 1, 1}),
      .days = leg.days,
  };

  // Ring rate raised so the world carries AML rings at this population
  // (the ring count rounds to zero below ~833 people), and the fraud
  // budget raised for the cases-per-operator reason in the file comment.
  // Neither touches the mechanism under test.
  pl::synth::people::Fraud fraudProfile{};
  fraudProfile.rings.perTenKMean = 20.0;
  fraudProfile.rings.perTenKSigma = 0.0;
  fraudProfile.limits.targetTxnFraudP = leg.targetTxnFraudP;

  const pl::pipeline::stages::entities::EntitySynthesis entities{
      .population = leg.population,
      .identity =
          pl::synth::pii::IdentityContext{
              .pools = &poolSet,
              .simStart = window.start,
              .localeMix = pl::synth::pii::LocaleMix::usOnly(),
          },
      .fraud = fraudProfile,
  };

  pl::clearing::BalanceRules balanceRules{};
  pl::transfers::credit_cards::LifecycleRules lifecycleRules{};

  auto rng = pl::random::Rng::fromSeed(leg.seed);
  pl::pipeline::SimulationPipeline pipeline{rng, window, entities, leg.seed};
  pipeline.transferStage()
      .legit()
      .window(window)
      .seed(leg.seed)
      .openingBalanceRules(&balanceRules)
      .creditLifecycle(&lifecycleRules);
  pipeline.transferStage().fraud().profile(&fraudProfile);

  return pipeline.run();
}

struct Fanout {
  std::size_t endpoints = 0;
  std::size_t shared = 0;
  std::size_t maxVictims = 0;
  double meanVictims = 0.0;
};

template <typename Key>
[[nodiscard]] Fanout summarize(const std::map<Key, std::set<pl::entity::Key>> &m) {
  Fanout out;
  out.endpoints = m.size();
  std::size_t total = 0;
  for (const auto &[key, victims] : m) {
    total += victims.size();
    out.maxVictims = std::max(out.maxVictims, victims.size());
    if (victims.size() > 1) {
      ++out.shared;
    }
  }
  out.meanVictims = out.endpoints == 0
                        ? 0.0
                        : static_cast<double>(total) /
                              static_cast<double>(out.endpoints);
  return out;
}

void measure(const Leg &leg, const pl::pipeline::SimulationResult &result) {
  const auto &attackers = result.infra.attackers;
  const auto &txns = result.transfers.ledger.posted.txns;

  std::printf("\n=== %s: pop %d, %d days, seed %llu ===\n", leg.name,
              leg.population, leg.days,
              static_cast<unsigned long long>(leg.seed));
  std::printf("  operators %zu, attacker devices %zu, attacker IPs %zu\n",
              attackers.operatorCount(), attackers.deviceCount(),
              attackers.ipCount());

  check(attackers.operatorCount() > 0,
        std::string(leg.name) + ": the world must carry attacker operators; "
                                "with none, every sub-gate below is vacuous");

  // ------------------------------------------- endpoint -> tenure index
  std::map<pl::devices::Identity, std::vector<pl::infra::Tenure>> deviceSpans;
  for (std::size_t i = 0; i < attackers.devices.size(); ++i) {
    deviceSpans[attackers.devices[i]].push_back(attackers.deviceTenures[i]);
  }
  std::map<pl::network::Ipv4, std::vector<pl::infra::Tenure>> ipSpans;
  for (std::size_t i = 0; i < attackers.ips.size(); ++i) {
    ipSpans[attackers.ips[i]].push_back(attackers.ipTenures[i]);
  }

  // ------------------------------- the institution's endpoint registry
  // A device has a `Has_Device` row iff SOME enrolled usage names it,
  // which is exactly the condition the exporter writes on.
  std::unordered_set<pl::devices::Identity> deviceOnFile;
  for (const auto &u : result.infra.devices.usages) {
    if (u.enrolled) {
      deviceOnFile.insert(u.deviceId);
    }
  }
  std::unordered_set<pl::network::Ipv4> ipOnFile;
  for (const auto &u : result.infra.ips.usages) {
    if (u.enrolled) {
      ipOnFile.insert(u.ipAddress);
    }
  }
  // Customer-held endpoints regardless of registry coverage — needed to
  // separate "a customer's address" from "attacker infrastructure" in
  // the residential-proxy count.
  std::unordered_set<pl::network::Ipv4> customerIp;
  for (const auto &u : result.infra.ips.usages) {
    customerIp.insert(u.ipAddress);
  }

  static constexpr auto kCardTag =
      channels::tag(channels::Legit::cardPurchase);
  static constexpr auto kMerchantTag = channels::tag(channels::Legit::merchant);

  std::map<pl::devices::Identity, std::set<pl::entity::Key>> victimsPerDevice;
  std::map<pl::network::Ipv4, std::set<pl::entity::Key>> victimsPerIp;

  std::size_t unauthorizedRows = 0;
  std::size_t unauthorizedOnAttackerDevice = 0;
  std::size_t unauthorizedOnVictimDevice = 0;
  std::size_t attackerRowsOutsideDeviceTenure = 0;
  std::size_t attackerRowsOutsideIpTenure = 0;
  std::size_t proxyRows = 0;
  std::size_t operatorInfraRows = 0;
  std::size_t scamRailRows = 0;
  std::size_t scamRailWithAttackerEndpoint = 0;
  std::size_t scamRailSessionless = 0;
  std::size_t giftCardRows = 0;
  std::size_t giftCardOnlineRows = 0;

  std::size_t viewOwnedMerchant = 0;
  std::size_t viewOwnedMerchantFraud = 0;

  // Sub-gate H. Keyed on the card, memoised, and computed over the SAME
  // window bounds the exporter's `generationFor` uses — a mismatch here would
  // measure a different schedule than the one that reached the tables.
  std::size_t viewMultiGenCard = 0;
  std::size_t viewMultiGenCardFraud = 0;
  std::map<pl::entity::Key, bool> multiGenerationCard;
  // DISARM SWITCH for sub-gate H, left in place because the check's whole
  // claim is a NEGATIVE, and a negative that has never been made to fail is
  // indistinguishable from a check that cannot fail. Flipping this to true
  // simulates fraud-driven reissue — the real-world mechanism deliberately
  // omitted from `card_reissue.hpp` — by marking every compromised card as
  // reissued. MEASURED with it on: lift 1.153x (leg-long) and 1.478x
  // (leg-wide); both clear the ceiling below, so both legs red.
  //
  // THE ASYMMETRY BETWEEN THE LEGS IS THE INTERESTING PART. Leg-long is a
  // 4-year window where 66% of view cards already reissue, so forcing the
  // compromised ones true adds little contrast — the flag has SATURATED, and
  // a saturated flag is weak evidence of anything. Leg-wide's 2-year window
  // sits at 43% and the same leak shows three times as strongly. Sensitivity
  // here therefore DEGRADES with window length, which is worth knowing before
  // trusting this sub-gate on a 20-year corpus.
  constexpr bool kDisarmFraudDrivenReissue = false;
  std::set<pl::entity::Key> fraudCards;
  if (kDisarmFraudDrivenReissue) {
    for (const auto &tx : txns) {
      if (tx.fraud.flag != 0) {
        fraudCards.insert(tx.source);
      }
    }
  }
  const auto legStart = pl::time::makeTime({leg.startYear, 1, 1});
  const auto legStartEpoch = pl::time::toEpochSeconds(legStart);
  const auto legEndEpoch =
      pl::time::toEpochSeconds(pl::time::addDays(legStart, leg.days));

  // Destination -> acceptance footprint, from the world's OWN catalogue.
  std::map<pl::entity::Key, pl::entity::merchant::Footprint> footprintByKey;
  std::map<pl::entity::Key, pl::entity::PersonId> ownerByMerchant;
  for (const auto &rec : result.counterparties.merchants.records) {
    footprintByKey.emplace(rec.counterpartyId, rec.footprint);
    if (rec.owner != pl::entity::invalidPerson) {
      ownerByMerchant.emplace(rec.counterpartyId, rec.owner);
    }
  }

  // Card-view confusion counts for sub-gate C.
  std::size_t viewRows = 0;
  std::size_t viewFraud = 0;
  std::size_t viewDeviceNotOnFile = 0;
  std::size_t viewDeviceNotOnFileFraud = 0;
  std::size_t viewIpNotOnFile = 0;
  std::size_t viewIpNotOnFileFraud = 0;
  std::set<pl::devices::Identity> cardViewAttackerDevices;
  std::set<pl::network::Ipv4> cardViewAttackerIps;

  for (const auto &tx : txns) {
    const bool fraud = tx.fraud.flag != 0;
    const bool attackerDevice = attackers.ownsDevice(tx.session.deviceId);
    const bool attackerIp = attackers.ownsIp(tx.session.ipAddress);

    // The UNAUTHORIZED family only. The two authorized scam rails are
    // victim-operated by declaration (victim-session-2026-07) and would
    // dilute the operating-position split with rows that were never
    // eligible for an attacker endpoint.
    if (fraud && tx.fraud.type == pl::fraud::FraudType::txnFraudSolo) {
      ++unauthorizedRows;
      if (attackerDevice) {
        ++unauthorizedOnAttackerDevice;
      } else {
        ++unauthorizedOnVictimDevice;
      }
    }

    // ------- F: THE SOCIAL-ENGINEERING RAILS HAVE NO ATTACKER ENDPOINT
    //
    // A scammer on the telephone does not produce a session. On the two
    // AUTHORIZED rails the person operating the payment instrument IS
    // THE VICTIM — walking into a store to buy gift cards, or logging
    // into their own banking app to push a transfer — so the row must
    // carry the victim's own routed device and address, and there is no
    // attacker endpoint to attribute in the first place.
    //
    // Asserting it HERE, on the corpus, is the point. The property was
    // gated only in `test_unauthorized_keyed`'s hand-built Run C
    // fixture, which cannot catch a PLANNER that starts handing these
    // rails an operator: `injector.cpp` guards on `!scamRail` and
    // `unauthorized.cpp` guards again on `authorizedRail`, and a
    // corpus-level zero is what proves BOTH guards are still doing their
    // job at production draw counts.
    if (fraud && (tx.fraud.type == pl::fraud::FraudType::scamGiftCard ||
                  tx.fraud.type == pl::fraud::FraudType::scamImpostor)) {
      ++scamRailRows;
      if (attackerDevice || attackerIp) {
        ++scamRailWithAttackerEndpoint;
      }
      if (!tx.session.deviceId.assigned()) {
        ++scamRailSessionless;
      }
    }

    // BOTH GIFT-CARD CHANNELS MUST EXIST (giftcard-channel-2026-07). The
    // rail was hardcoded card-PRESENT, so 100% of coached gift-card
    // purchases were in-store swipes and the ONLINE branch — the one where
    // an issuer has a live session to score and something to interrupt
    // before the codes are read out — did not exist in the corpus at all.
    //
    // Resolved against the leg's OWN acceptance catalogue, the same way
    // the exporter resolves `use_chip`, so this measures what the row will
    // actually render as rather than re-deriving the decision.
    if (fraud && tx.fraud.type == pl::fraud::FraudType::scamGiftCard) {
      ++giftCardRows;
      const auto it = footprintByKey.find(tx.target);
      if (it != footprintByKey.end() &&
          it->second == pl::entity::merchant::Footprint::online) {
        ++giftCardOnlineRows;
      }
    }

    if (attackerDevice) {
      victimsPerDevice[tx.session.deviceId].insert(tx.source);
      const auto it = deviceSpans.find(tx.session.deviceId);
      const bool inside =
          it != deviceSpans.end() &&
          std::ranges::any_of(it->second, [&](const pl::infra::Tenure &t) {
            return t.contains(tx.timestamp);
          });
      if (!inside) {
        ++attackerRowsOutsideDeviceTenure;
      }

      if (attackerIp) {
        ++operatorInfraRows;
      } else if (customerIp.contains(tx.session.ipAddress)) {
        ++proxyRows;
      }
    }

    if (attackerIp) {
      victimsPerIp[tx.session.ipAddress].insert(tx.source);
      const auto it = ipSpans.find(tx.session.ipAddress);
      const bool inside =
          it != ipSpans.end() &&
          std::ranges::any_of(it->second, [&](const pl::infra::Tenure &t) {
            return t.contains(tx.timestamp);
          });
      if (!inside) {
        ++attackerRowsOutsideIpTenure;
      }
    }

    const auto channel = tx.session.channel;
    if (channel != kCardTag && channel != kMerchantTag) {
      continue;
    }
    ++viewRows;
    if (fraud) {
      ++viewFraud;
    }
    // THE LABELLING GAP (attacker-infra-2026-07). Attacker endpoints used
    // to reach the exporter with a hard-coded `flagged = false`, so the
    // only `device/flagged` positives in the whole overlay were the 5 AML
    // ring-shared devices — which enter the card view ONLY through a ring
    // member's LEGITIMATE purchase. Entity-level device ground truth was
    // both vanishing AND anti-correlated with the transaction label.
    // These counters are what make "the overlay is now non-degenerate" a
    // measurement rather than a claim.
    if (attackerDevice) {
      cardViewAttackerDevices.insert(tx.session.deviceId);
    }
    if (attackerIp) {
      cardViewAttackerIps.insert(tx.session.ipAddress);
    }
    if (!deviceOnFile.contains(tx.session.deviceId)) {
      ++viewDeviceNotOnFile;
      if (fraud) {
        ++viewDeviceNotOnFileFraud;
      }
    }
    if (!ipOnFile.contains(tx.session.ipAddress)) {
      ++viewIpNotOnFile;
      if (fraud) {
        ++viewIpNotOnFileFraud;
      }
    }

    // ------- G: THE MERCHANT OWNERSHIP REGISTER CARRIES NO FRAUD SIGNAL
    //
    // `cf_Is_Merchant` links a merchant to its proprietor Party, and
    // merchants are where card fraud LANDS — so if register membership
    // correlated with the label at all, the ownership edge would be a
    // shortcut into the destination side of every fraud row.
    //
    // It cannot, by construction: `ownership::onFile` hashes the merchant
    // KEY alone and reads no other attribute. This measures that the
    // construction actually holds end-to-end, because the tempting
    // version of this feature — small local outlets have proprietors,
    // national services and online merchants do not — WOULD have leaked:
    // the card rail is ~70% card-not-present and draws only from
    // `Footprint::online`, so any eligibility rule reading `footprint` or
    // `weight` inherits the modality split and with it the label.
    if (const auto it = ownerByMerchant.find(tx.target);
        it != ownerByMerchant.end()) {
      ++viewOwnedMerchant;
      if (fraud) {
        ++viewOwnedMerchantFraud;
      }
    }

    // ------- H: THE REISSUE SCHEDULE CARRIES NO FRAUD SIGNAL
    //
    // card-churn-2026-07 gives one `cf_Card` vertex per OBSERVED card
    // generation, so "this party holds more than one card number" became an
    // exported, model-visible feature. In the real world that feature
    // predicts fraud strongly and for the wrong reason: an issuer reissues
    // BECAUSE a card was compromised. Modelling that would make card
    // cardinality a label, so fraud-driven reissue is deliberately absent
    // (see card_reissue.hpp) — and this is the check that the absence holds
    // end-to-end rather than only in the header comment.
    //
    // WHY THE FLAG IS RULE-LEVEL, NOT OBSERVED. Counting generations that
    // actually appear in the view would confound the measurement with
    // EXPOSURE: a card with more rows is more likely to straddle a boundary
    // AND — via `exposure.hpp`, which tilts unauthorized victim selection by
    // card activity — more likely to be victimized. That correlation is real
    // and intended, so a row-weighted observed flag would sit above 1.0 for a
    // legitimate reason and the gate would be measuring the wrong thing.
    // `generationsFor` is a hash of the CARD KEY and the window bounds alone,
    // so a lift on it can only come from a construction correlation, which is
    // precisely what must be zero. Same reasoning as sub-gate G taking
    // ownership from the world catalogue rather than from the view.
    const auto [genIt, genInserted] =
        multiGenerationCard.try_emplace(tx.source, false);
    if (genInserted) {
      genIt->second = pl::entity::card::reissue::generationsFor(
                          tx.source, legStartEpoch, legEndEpoch)
                          .size() > 1;
    }
    if (kDisarmFraudDrivenReissue && fraudCards.contains(tx.source)) {
      genIt->second = true;
    }
    if (genIt->second) {
      ++viewMultiGenCard;
      if (fraud) {
        ++viewMultiGenCardFraud;
      }
    }
  }

  // ------------------------------------------------------ PRECONDITIONS
  // Each of these exists because the corresponding sub-gate would
  // otherwise pass on an empty set. A gate must assert its own
  // precondition.
  check(unauthorizedRows > 0,
        std::string(leg.name) +
            ": the leg must produce unauthorized (card/ATO) fraud rows");
  check(unauthorizedOnAttackerDevice > 0,
        std::string(leg.name) +
            ": unauthorized rows must reach exogenous attacker "
            "infrastructure; zero means the pool is unreachable");
  check(viewRows > 0, std::string(leg.name) + ": the card view must be "
                                              "non-empty");

  const auto deviceFan = summarize(victimsPerDevice);
  const auto ipFan = summarize(victimsPerIp);

  const double sharedDeviceShare =
      deviceFan.endpoints == 0
          ? 0.0
          : static_cast<double>(deviceFan.shared) /
                static_cast<double>(deviceFan.endpoints);
  const double sharedIpShare =
      ipFan.endpoints == 0 ? 0.0
                           : static_cast<double>(ipFan.shared) /
                                 static_cast<double>(ipFan.endpoints);

  const double casesPerOperator =
      attackers.operatorCount() == 0
          ? 0.0
          : static_cast<double>(unauthorizedOnAttackerDevice) /
                static_cast<double>(attackers.operatorCount());

  std::printf("  A device fan-out: %zu used, shared %.4f, mean %.3f victims, "
              "max %zu\n",
              deviceFan.endpoints, sharedDeviceShare, deviceFan.meanVictims,
              deviceFan.maxVictims);
  std::printf("  A ip     fan-out: %zu used, shared %.4f, mean %.3f victims, "
              "max %zu\n",
              ipFan.endpoints, sharedIpShare, ipFan.meanVictims,
              ipFan.maxVictims);
  std::printf("    (rows per operator %.2f — the world-shape ratio this leg "
              "pins)\n",
              casesPerOperator);

  check(sharedDeviceShare >= kMinSharedDeviceShare,
        std::string(leg.name) + ": attacker devices seen by more than one "
                                "victim must be >= " +
            std::to_string(kMinSharedDeviceShare) + ", got " +
            std::to_string(sharedDeviceShare) +
            " — cross-victim reuse is THE card-fraud graph signal");
  check(deviceFan.meanVictims >= kMinMeanVictimsPerDevice,
        std::string(leg.name) + ": mean victims per attacker device must be "
                                ">= " +
            std::to_string(kMinMeanVictimsPerDevice) + ", got " +
            std::to_string(deviceFan.meanVictims));
  check(deviceFan.maxVictims >= kMinMaxVictimsPerDevice,
        std::string(leg.name) +
            ": the fan-out distribution must have a HEAVY TAIL — max victims "
            "per device >= " +
            std::to_string(kMinMaxVictimsPerDevice) + ", got " +
            std::to_string(deviceFan.maxVictims) +
            ". A flat distribution passes a mean gate and carries none of "
            "the structure an alert fires on");
  check(sharedIpShare >= kMinSharedIpShare,
        std::string(leg.name) + ": shared attacker-IP share must be >= " +
            std::to_string(kMinSharedIpShare) + ", got " +
            std::to_string(sharedIpShare));
  check(ipFan.meanVictims >= kMinMeanVictimsPerIp,
        std::string(leg.name) + ": mean victims per attacker IP must be >= " +
            std::to_string(kMinMeanVictimsPerIp) + ", got " +
            std::to_string(ipFan.meanVictims));

  // ------------------------------------------- B operating position
  const double victimEndpointShare =
      static_cast<double>(unauthorizedOnVictimDevice) /
      static_cast<double>(unauthorizedRows);
  std::printf("  B operating position: %zu rows, attacker-infra %zu, "
              "victim-endpoint %zu (share %.4f vs nominal %.4f; the excess is "
              "the unattributable residual)\n",
              unauthorizedRows, unauthorizedOnAttackerDevice,
              unauthorizedOnVictimDevice, victimEndpointShare,
              kVictimEndpointNominal);
  check(victimEndpointShare >= kVictimEndpointShareLo &&
            victimEndpointShare <= kVictimEndpointShareHi,
        std::string(leg.name) + ": victim-endpoint share must land in [" +
            std::to_string(kVictimEndpointShareLo) + ", " +
            std::to_string(kVictimEndpointShareHi) + "], got " +
            std::to_string(victimEndpointShare) +
            ". Above the band means the operator pool is under-sized and "
            "cases are failing to attribute; below means the "
            "remote-access/household branch has stopped firing");

  // ------------------------------------- B' pool sizing, measured
  // The coverage floor is written in mean CONCURRENT campaigns, so that
  // is what gets measured — not the operator COUNT, which the rule
  // deliberately scales with window length to hold concurrency fixed. A
  // precondition that exists to catch a construction failure must
  // measure the construction, never re-derive it.
  double campaignDayTotal = 0.0;
  for (const auto &op : attackers.operators) {
    campaignDayTotal +=
        static_cast<double>(op.campaignLastEpochExcl - op.campaignFirstEpoch) /
        86'400.0;
  }
  const double meanConcurrent =
      campaignDayTotal / static_cast<double>(leg.days);
  // The rule's own INPUT, printed beside its output. The count is derived
  // from an expected campaign length, so when concurrency misses its
  // floor this is the number that says whether the sizing arithmetic or
  // the length distribution is at fault.
  const double meanClippedCampaign =
      attackers.operatorCount() == 0
          ? 0.0
          : campaignDayTotal / static_cast<double>(attackers.operatorCount());
  std::printf("  B. mean concurrent campaigns %.2f (floor 7.00, "
              "window-independent by construction); mean clipped campaign "
              "%.1f days\n",
              meanConcurrent, meanClippedCampaign);
  check(meanConcurrent >= kMeanConcurrentLo &&
            meanConcurrent <= kMeanConcurrentHi,
        std::string(leg.name) + ": mean concurrent campaigns must land in [" +
            std::to_string(kMeanConcurrentLo) + ", " +
            std::to_string(kMeanConcurrentHi) + "], got " +
            std::to_string(meanConcurrent) +
            ". Below it, most case dates have no campaign running and "
            "attribution silently collapses into the victim-endpoint "
            "branch; above it, the case load is spread too thin for the "
            "reuse signal sub-gate A measures");

  // --------------------------------------- C the shortcut, sized
  const double baseRate =
      static_cast<double>(viewFraud) / static_cast<double>(viewRows);
  const double devicePrecision =
      viewDeviceNotOnFile == 0
          ? 0.0
          : static_cast<double>(viewDeviceNotOnFileFraud) /
                static_cast<double>(viewDeviceNotOnFile);
  const double ipPrecision =
      viewIpNotOnFile == 0 ? 0.0
                           : static_cast<double>(viewIpNotOnFileFraud) /
                                 static_cast<double>(viewIpNotOnFile);
  const double deviceLift = baseRate > 0.0 ? devicePrecision / baseRate : 0.0;
  const double ipLift = baseRate > 0.0 ? ipPrecision / baseRate : 0.0;

  std::printf("  C card view %zu rows, base fraud rate %.6f\n", viewRows,
              baseRate);
  std::printf("    device not-on-file: %zu rows, precision %.6f, lift %.2fx\n",
              viewDeviceNotOnFile, devicePrecision, deviceLift);
  std::printf("    ip     not-on-file: %zu rows, precision %.6f, lift %.2fx\n",
              viewIpNotOnFile, ipPrecision, ipLift);

  check(viewDeviceNotOnFile > 0 && viewIpNotOnFile > 0,
        std::string(leg.name) +
            ": some card-view rows must sit on endpoints the institution "
            "has NOT recorded, or sub-gate C is measuring nothing");
  check(viewDeviceNotOnFile - viewDeviceNotOnFileFraud > 0,
        std::string(leg.name) +
            ": LEGITIMATE rows must appear on un-recorded devices. Without "
            "them 'no Party edge' is again a perfect fraud rule and "
            "Has_Device cannot ship");
  check(devicePrecision <= kMaxNotOnFilePrecision,
        std::string(leg.name) + ": 'device not on file => fraud' precision "
                                "must stay <= " +
            std::to_string(kMaxNotOnFilePrecision) + ", got " +
            std::to_string(devicePrecision) +
            ". This number was 1.0 for the attacker half of the endpoint "
            "population before this round, which is why the ownership "
            "tables were withheld");
  check(ipPrecision <= kMaxNotOnFilePrecision,
        std::string(leg.name) +
            ": 'ip not on file => fraud' precision must stay <= " +
            std::to_string(kMaxNotOnFilePrecision) + ", got " +
            std::to_string(ipPrecision));
  // The opposite failure mode. A shortcut replaced by pure noise would
  // be a loss of realism, not a win: an un-enrolled endpoint IS riskier
  // in production, and the graph should say so.
  // ------------------------------- C' entity ground truth is non-degenerate
  std::printf("  C. card-view attacker endpoints (overlay positives): "
              "devices %zu, ips %zu\n",
              cardViewAttackerDevices.size(), cardViewAttackerIps.size());
  check(!cardViewAttackerDevices.empty(),
        std::string(leg.name) +
            ": attacker DEVICES must be reachable in the card view, or "
            "cf_Ground_Truth_Label's device positives are back to being the "
            "5 AML ring devices that only enter through a LEGITIMATE "
            "purchase — vanishing and anti-correlated with the label");
  check(!cardViewAttackerIps.empty(),
        std::string(leg.name) +
            ": attacker IPs must be reachable in the card view, for the same "
            "reason");

  check(deviceLift > 1.0,
        std::string(leg.name) +
            ": endpoint-not-on-file must retain real LIFT over base rate "
            "(got " +
            std::to_string(deviceLift) +
            "x). Killing the shortcut by making the feature pure noise "
            "would be the opposite error");

  // ------------------------------------------------- D point-in-time
  std::printf("  D outside-tenure rows: device %zu, ip %zu (both must be 0)\n",
              attackerRowsOutsideDeviceTenure, attackerRowsOutsideIpTenure);
  check(attackerRowsOutsideDeviceTenure == 0,
        std::string(leg.name) +
            ": every row carrying an attacker device must fall inside that "
            "device's own tenure; got " +
            std::to_string(attackerRowsOutsideDeviceTenure) + " outside");
  check(attackerRowsOutsideIpTenure == 0,
        std::string(leg.name) +
            ": every row carrying an attacker IP must fall inside that "
            "address's own tenure; got " +
            std::to_string(attackerRowsOutsideIpTenure) + " outside");

  // ------------------------------------------- E residential proxy
  const auto attackerDeviceRows = proxyRows + operatorInfraRows;
  const double proxyShare =
      attackerDeviceRows == 0 ? 0.0
                              : static_cast<double>(proxyRows) /
                                    static_cast<double>(attackerDeviceRows);
  std::printf("  E attacker-device rows %zu: own infra %zu, residential proxy "
              "%zu (share %.4f)\n",
              attackerDeviceRows, operatorInfraRows, proxyRows, proxyShare);
  // ------- F social-engineering rails: no attacker endpoint, ever
  std::printf("  F scam-rail (gift-card + impostor push) rows %zu: with an "
              "attacker endpoint %zu (must be 0), sessionless %zu (must be "
              "0)\n",
              scamRailRows, scamRailWithAttackerEndpoint, scamRailSessionless);
  check(scamRailRows > 0,
        std::string(leg.name) +
            ": the leg must produce social-engineering scam rows, or sub-gate "
            "F is vacuous");
  check(scamRailWithAttackerEndpoint == 0,
        std::string(leg.name) + ": a telephone scammer produces NO session — "
                                "no gift-card or impostor-push row may carry "
                                "attacker infrastructure, got " +
            std::to_string(scamRailWithAttackerEndpoint) +
            ". The victim operates their own instrument on these rails");
  check(scamRailSessionless == 0,
        std::string(leg.name) +
            ": every scam row must still carry the VICTIM'S own routed "
            "endpoint — absent attacker infrastructure is not the same as "
            "absent session, and a blank endpoint reaches the card-fraud "
            "sink as a throw");

  const double giftCardOnlineShare =
      giftCardRows == 0 ? 0.0
                        : static_cast<double>(giftCardOnlineRows) /
                              static_cast<double>(giftCardRows);
  std::printf("  F. gift-card channel: %zu rows, online %zu (share %.4f) — "
              "BOTH branches must exist\n",
              giftCardRows, giftCardOnlineRows, giftCardOnlineShare);
  check(giftCardRows > 0,
        std::string(leg.name) + ": the gift-card rail must be exercised");
  check(giftCardOnlineRows > 0,
        std::string(leg.name) +
            ": coached gift-card purchases must include DIGITAL e-gift "
            "codes bought online. This rail was hardcoded card-present, so "
            "the online branch — the one where an issuer has a live session "
            "to interrupt before the codes are read out — did not exist");
  check(giftCardOnlineShare < 0.5,
        std::string(leg.name) +
            ": the in-store physical rack must stay the MAJORITY channel "
            "across the corpus era; got online share " +
            std::to_string(giftCardOnlineShare));

  // ------- G merchant ownership register: structure, not signal
  const double ownedPrecision =
      viewOwnedMerchant == 0 ? 0.0
                             : static_cast<double>(viewOwnedMerchantFraud) /
                                   static_cast<double>(viewOwnedMerchant);
  const double ownedLift = baseRate > 0.0 ? ownedPrecision / baseRate : 0.0;
  std::printf("  G merchant register: %zu of %zu catalogue merchants owned; "
              "%zu card-view rows on an owned merchant, fraud rate %.6f, "
              "lift %.3fx (must sit on 1.0)\n",
              ownerByMerchant.size(),
              result.counterparties.merchants.records.size(),
              viewOwnedMerchant, ownedPrecision, ownedLift);
  // IS THE REGISTER DEGENERATE? Coverage alone does not answer that. A
  // register where one Party owns every merchant would satisfy a
  // non-emptiness check, satisfy the loader, and be a single hub vertex
  // that tells a model nothing — the same shape as the endpoint defect
  // this file exists for. So measure the OWNER-SIDE distribution too:
  // most proprietors hold one outlet, a few hold several.
  std::map<pl::entity::PersonId, std::size_t> merchantsPerOwner;
  for (const auto &[merchantKey, owner] : ownerByMerchant) {
    ++merchantsPerOwner[owner];
  }
  std::size_t maxPerOwner = 0;
  for (const auto &[owner, count] : merchantsPerOwner) {
    maxPerOwner = std::max(maxPerOwner, count);
  }
  const double meanPerOwner =
      merchantsPerOwner.empty()
          ? 0.0
          : static_cast<double>(ownerByMerchant.size()) /
                static_cast<double>(merchantsPerOwner.size());
  std::printf("  G. register shape: %zu distinct proprietors, mean %.2f "
              "merchants each, max %zu\n",
              merchantsPerOwner.size(), meanPerOwner, maxPerOwner);
  check(!ownerByMerchant.empty(),
        std::string(leg.name) +
            ": the merchant ownership register must be non-empty, or "
            "cf_Is_Merchant is empty and the downstream loader aborts");
  check(merchantsPerOwner.size() >= ownerByMerchant.size() / 3,
        std::string(leg.name) +
            ": the register must spread over MANY proprietors — got " +
            std::to_string(merchantsPerOwner.size()) + " owners for " +
            std::to_string(ownerByMerchant.size()) +
            " merchants. A register concentrated on a few Parties is one hub "
            "vertex wearing an ownership table");
  check(maxPerOwner <= 6,
        std::string(leg.name) +
            ": no proprietor should hold an implausible number of outlets; "
            "max " +
            std::to_string(maxPerOwner));
  check(viewOwnedMerchant > 0,
        std::string(leg.name) +
            ": card-view rows must reach owned merchants, or sub-gate G "
            "measures nothing");
  // THE BAND IS NOT ZERO-WIDTH, AND THE REASON IS WORTH RECORDING.
  // Observed 1.122x (leg-long) and 0.950x (leg-wide) — it STRADDLES 1.0,
  // and that sign flip across two independent seeds is the evidence that
  // there is no construction correlation. What remains is a finite-
  // catalogue realization effect: with only a few hundred merchants, a
  // 45%-coverage hash lands on a subset whose online/physical composition
  // differs from the complement's by a few points, and the card rail's
  // ~70% card-not-present share turns that into a small lift of either
  // sign. A systematic leak would keep the same sign in both legs.
  check(ownedLift > 0.80 && ownedLift < 1.25,
        std::string(leg.name) +
            ": 'destination merchant has an owner edge' must carry NO "
            "SYSTEMATIC fraud signal — lift must straddle 1.0, got " +
            std::to_string(ownedLift) +
            ". Register membership is a hash of the merchant key alone "
            "precisely so this holds; a rule reading footprint or weight "
            "would inherit the card rail's card-not-present modality split "
            "and turn an ownership table into a label shortcut");

  // ------- H reissue schedule: cardinality, not signal
  const double multiGenPrecision =
      viewMultiGenCard == 0 ? 0.0
                            : static_cast<double>(viewMultiGenCardFraud) /
                                  static_cast<double>(viewMultiGenCard);
  const double multiGenLift =
      baseRate > 0.0 ? multiGenPrecision / baseRate : 0.0;
  std::size_t multiGenCards = 0;
  for (const auto &[key, multi] : multiGenerationCard) {
    if (multi) {
      ++multiGenCards;
    }
  }
  std::printf("  H reissue schedule: %zu of %zu view cards reissue in-window; "
              "%zu rows on a reissued card, fraud rate %.6f, lift %.3fx (must "
              "sit on 1.0)\n",
              multiGenCards, multiGenerationCard.size(), viewMultiGenCard,
              multiGenPrecision, multiGenLift);
  check(multiGenCards > 0,
        std::string(leg.name) +
            ": some card must be reissued inside the leg window, or sub-gate "
            "H measures nothing. The pre-round state was exactly ONE card "
            "number per account for the whole run, which scores 0 here");
  check(multiGenCards < multiGenerationCard.size(),
        std::string(leg.name) +
            ": not every card may reissue in-window either — with no "
            "single-generation cards the lift below has no complement to "
            "compare against and passes vacuously");
  check(viewMultiGenCard > 0,
        std::string(leg.name) +
            ": reissued cards must carry card-view rows, or sub-gate H "
            "measures nothing");
  // THE BAND IS MEASURED, NOT INHERITED FROM SUB-GATE G. Eight readings over
  // four seed pairs — 1.012, 0.990, 0.965, 0.953, 1.007, 1.063, 0.987,
  // 0.954 — give mean 0.991 and SD 0.0366. That SD is 2.4x the naive
  // binomial estimate (0.015) because fraud rows CLUSTER: a compromise
  // produces several charges on one card, so the effective sample is cases
  // and not rows. Sizing this band off the binomial would have produced a
  // gate that flakes, which is the same "derived instead of measured" mistake
  // the concurrency floor made twice.
  //
  // The ceiling sits at 3.5 SD above the observed mean, which is above the
  // weaker of the two disarm readings (1.153) and well below the stronger
  // (1.478). Fraud-driven reissue is the leak this forbids: in production a
  // replacement card is very often a CONSEQUENCE of compromise, so wiring
  // `seen.fraud` into the schedule would put a full-window verdict into an
  // exported vertex count. `generationsFor` reads the card key and the window
  // and nothing else, which is what holds this at 1.0.
  check(multiGenLift > 0.87 && multiGenLift < 1.13,
        std::string(leg.name) +
            ": 'the cardholder was reissued a card' must carry NO SYSTEMATIC "
            "fraud signal — lift must straddle 1.0, got " +
            std::to_string(multiGenLift) +
            ". Card cardinality is now an exported feature (one cf_Card "
            "vertex per observed generation), so a reissue rule that read the "
            "fraud flag would turn a row count into the label");

  check(proxyShare >= kMinProxyShare && proxyShare <= kMaxProxyShare,
        std::string(leg.name) + ": residential-proxy share must land in [" +
            std::to_string(kMinProxyShare) + ", " +
            std::to_string(kMaxProxyShare) + "], got " +
            std::to_string(proxyShare) +
            ". This is the mechanism that puts a fraud transaction one hop "
            "from an unrelated Party through a shared address");
}

} // namespace

int main() {
  const Leg legs[] = {
      // Long and narrow: campaigns succeed each other many times over,
      // so the replacement chains and the coverage floor are exercised.
      {"leg-long", 20260728ULL, 2012, 1461, 900, 0.008},
      // Short and wide: the sizing rule claims mean concurrent operators
      // is population-driven and window-independent. Two shapes is the
      // cheapest test of that claim.
      {"leg-wide", 20260729ULL, 2016, 731, 1800, 0.008},
  };

  try {
    for (const auto &leg : legs) {
      const auto poolSet = buildPoolSet(leg.seed);
      const auto result = runLeg(poolSet, leg);
      measure(leg, result);
    }
  } catch (const std::exception &e) {
    std::fprintf(stderr, "FAIL: exception: %s\n", e.what());
    return 2;
  }

  if (g_failures > 0) {
    std::fprintf(stderr, "\n%d check(s) failed.\n", g_failures);
    return 1;
  }
  std::printf("\nAll endpoint-graph checks passed.\n");
  return 0;
}
