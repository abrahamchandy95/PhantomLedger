#pragma once
//
// phantomledger/entities/infra/enumeration.hpp
//
// CARD TESTING (BIN enumeration) as declined authorization traffic.
//
// WHAT IT MODELS
// An operator who holds a list of card numbers runs one small authorization
// against each to learn which are live. The overwhelming majority are refused;
// the operator keeps the few that are not and sells or uses them later. The
// observable trace at an issuer is a burst of DECLINED authorizations sharing
// one device and one address across many unrelated cards.
//
// WHY IT IS WORTH MODELLING
// It is the one source of very high device-to-card fan-out in this corpus that
// does not carry a fraud label. Without it, every high-degree endpoint is an
// attacker running compromises, so degree alone separates the label and a
// model learns the shortcut instead of the graph. `attacker-infra-2026-07`
// closed the opposite defect — fan-out that did not exist — and this closes
// the one that closing it opened.
//
// "LEGITIMATE" WAS THE WRONG WORD AND IS CORRECTED HERE, 2026-08-11. Card
// testing is a crime, not benign traffic; what makes it usable as a diluent is
// that its rows are DECLINED AUTHORIZATIONS whose label this corpus WITHHOLDS
// (see the closing paragraph), not that the activity is innocent. Anyone
// reading this file to justify "high fan-out is not a fraud signal in the real
// world" is reading it wrong — see the citation block at
// `kProbedBasisPoints`, which now records the opposite.
//
// THE SELECTION IS LABEL-BLIND BY CONSTRUCTION, AND THAT IS THE POINT.
// Whether a card is probed is a hash of the CARD KEY and nothing else. It does
// not read `seen.fraud`, the compromise plans, victim exposure, or any other
// quantity downstream of the label. A real operator's list comes from a breach
// they had no part in and mostly holds numbers that never see fraud, so the
// label-blindness is the realistic choice as well as the safe one — and it is
// what keeps a probed card from being a fraud predictor. `test_card_enumeration`
// measures the resulting lift and requires it to straddle 1.0.
//
// DRAW-FREE AND STATELESS, like every other resolver in this directory: pure
// arithmetic over world state, so it moves no golden, and the batch and
// windowed engines agree without coordination.
//
// EVERY ROW IT PRODUCES CARRIES A WITHHELD LABEL. A probe is an authorization
// the issuer refused, not a settled purchase, and the transaction-level truth
// stays in `cf_Ground_Truth_Label`. That is what makes a 500-card device
// UNINFORMATIVE rather than diagnostic.

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/attackers.hpp"
#include "phantomledger/entities/infra/derived_endpoints.hpp"
#include "phantomledger/entities/infra/devices.hpp"
#include "phantomledger/entities/infra/ipv4.hpp"

#include <cstdint>
#include <optional>

namespace PhantomLedger::infra::enumeration {

namespace derived = ::PhantomLedger::infra::derived;

// ------------------------------------------------------------- domains

/* Distinct lanes so the operator pick and the probe decision cannot be
 * collinear: a card that lands on a heavy operator must not be more likely to
 * be probed because of it. */
inline constexpr std::uint64_t kProbeDomain = 0x454E'554D'0000'0001ULL;
inline constexpr std::uint64_t kOperatorDomain = 0x454E'554D'0000'0002ULL;
inline constexpr std::uint64_t kEndpointSaltDomain = 0x454E'554D'0000'0003ULL;

// ----------------------------------------------------------- constants

/* Share of view-observed cards that are probed at all, in basis points.
 *
 * CLASS S UNCITED. No issuer publishes a card-testing incidence rate, and the
 * public numbers that exist ("X% of merchants saw an enumeration attack") are
 * merchant-side prevalence, not the probability that a given card number
 * appears on some operator's list. This is a declared level.
 *
 * IT IS SIZED FOR FAN-OUT, NOT FOR PREVALENCE, and that is the honest
 * description. The quantity this exists to produce is a device that touches
 * many cards; the level is what makes the top enumeration device's degree land
 * in the same order as a busy attacker's, so degree stops separating them.
 *
 * ⚠️ DO NOT RAISE THIS TO IMPROVE THE DILUTION. IT WAS MEASURED AND IT MAKES
 * THE LEAK WORSE, 2026-08-11 (`device-sharing-evidence-2026-08`, sub-gates K.6
 * and K.7). Swept over 400 / 1000 / 2000 / 3000 / 4000 / 6000 bp on all four
 * `test_card_endpoint_graph` legs, measured on the CONSUMER-VISIBLE view:
 *
 *   leg-wide  AP ratio from degree alone   1.598 -> 1.754 -> 1.839 -> 1.860
 *                                          -> 1.881 -> 1.828
 *   leg-long  best 'degree >= k' precision 0.043 -> 0.053 -> 0.064 -> 0.167
 *
 * Both RISE with the share. At 2000bp and above the AP ratio exceeds
 * `kMaxDegreeApRatio = 1.75`, i.e. raising this constant REDS the gate that
 * bounds the very quantity it was supposed to protect.
 *
 * THE CAUSE, MEASURED AND LOGICALLY FORCED: probes DO NOT ADD AN INNOCENT
 * HIGH-DEGREE POPULATION, because `probeFor` resolves its endpoint from the
 * SAME `AttackerInfra` inventory the compromise planner draws from — a
 * different salt over the same operator lines. Of the probe rows that land on
 * a device already present in the settled view, **100% land on a device that
 * already carries settled FRAUD** — 71/71, 229/229, 138/138, 191/191 over the
 * four legs, no exceptions. It cannot be otherwise: attacker inventory carries
 * no legitimate traffic, so such a device is visible in the settled view only
 * because a compromise case used it. Every probe stapled to one of those RAISES
 * a fraud-carrying device's degree and pushes it UP the ranking with its fraud
 * intact. **That is amplification wearing the name dilution.**
 *
 * WHAT WOULD ACTUALLY WORK is a probe endpoint pool DISJOINT from the
 * compromise device lines, so a probe device carries probe rows only and its
 * label really is absent. Registered in `docs/fraud_model_audit.md`; the
 * containment class is the cheap one (exporter-side, `golden_run.b2sum`
 * unmoved) but it is a resolver change and has not been made here.
 *
 * THE MECHANISM IS NOW CITED TO THE CARD NETWORKS, AND THE DIRECTION IS
 * AGAINST THIS FILE'S DILUENT ROLE. Both must be recorded together or the next
 * reader repeats the error this paragraph corrects.
 *
 *  * Visa, "Anti-Enumeration and Account Testing Best Practices for
 *    Merchants" V1.2, April 2023 (Visa Public), names device fan-out a fraud
 *    trigger in as many words: multiple transactions with different payment
 *    accounts sharing one email address and one device ID may be a trigger for
 *    fraud classification or review. DIRECTION ONLY — the document contains no
 *    rate, probability or denominator anywhere. Accessed 2026-08-11.
 *  * Mastercard, US patent 10552836 B2 (filed 2016-10-11, granted 2020),
 *    claims fraud risk scoring from the NUMBER OF TRANSACTION ACCOUNTS that
 *    have used a device. A patent is a method claim, not evidence of efficacy:
 *    no threshold, no worked example, no measured rate. Accessed 2026-08-11.
 *
 * AND THE SENTENCE THAT USED TO END THIS COMMENT IS WITHDRAWN. It read:
 * "Deliberately larger than the unauthorized-fraud victim rate, because a
 * probed card is overwhelmingly a card nothing ever happens to." The sizing
 * rationale stands; the justification does not. Visa's VAAI Score datasheet
 * (2025, VisaNet data — the authorization network itself, so a census
 * denominator and NOT a reports database) measures: "Globally, enumerated
 * accounts have 22x higher fraud rates than regular accounts", and of the
 * enumerated accounts that saw fraud, 33% saw their first fraud within 5 days
 * of the enumeration transaction being APPROVED. Accessed 2026-08-11.
 *
 * WHY THIS CORPUS IS STILL DEFENSIBLE, AND THE ARGUMENT IS NARROWER THAN THE
 * OLD ONE. Visa's 22x is anchored on probes that were APPROVED — an operator
 * confirming a live card, which is why the clock starts there. Every probe this
 * file emits is DECLINED (`Do Not Honor`, one attempt, `kProbesPerCard = 1`),
 * so the population Visa measures does not exist in this corpus at all. The
 * `test_card_enumeration` requirement that probe-to-fraud lift straddle 1.0 is
 * therefore a statement about the DECLINED tail only, and it is correct for
 * that tail. It would be wrong the moment an approved probe branch is added,
 * and anyone adding one owes that gate a new band before they do.
 *
 * REGISTERED, NOT CLOSED: this corpus has no approved-probe population, so it
 * cannot express the strongest documented card-testing signal there is. See
 * `docs/fraud_model_audit.md`, `device-sharing-evidence-2026-08`. */
inline constexpr std::uint32_t kProbedBasisPoints = 400;

/* A probe is one authorization, not a session: the operator learns what it
 * needs from the issuer's answer and moves on. Modelling repeats would add
 * rows without adding structure, and would make probe COUNT a second signal
 * on top of the fan-out this exists to create. */
inline constexpr int kProbesPerCard = 1;

/* Seconds before the settled row the probe is anchored to. Card testing runs
 * against a number the operator cannot know is active, so anchoring to a real
 * row is a modelling convenience, not a claim that the operator watched it;
 * the offset keeps the probe strictly earlier so the exported table stays
 * causally ordered. Larger than the non-funding decline's 60s so the two
 * synthetic declines on one row cannot collide on a timestamp. */
inline constexpr std::int64_t kProbeLeadSeconds = 900;

/* The probe's ticket. Operators authorize a small round amount — large enough
 * that the issuer treats it as a real authorization, small enough to be
 * unremarkable. DECLARED, CLASS S UNCITED.
 *
 * It is deliberately CONSTANT. Varying it would hand back a weak amount-based
 * tell on exactly the rows this exists to make uninformative, and a constant
 * is also what the real behaviour looks like: one script, one amount. */
inline constexpr double kProbeAmount = 1.0;

// -------------------------------------------------------------- result

struct Probe {
  devices::Identity device{};
  network::Ipv4 ip{};
  std::int64_t timestamp = 0;
  std::uint32_t operatorIndex = 0;
};

// ------------------------------------------------------------ resolver

/* Is this card on some operator's list, and if so which endpoints ran it?
 *
 * `ts` is the timestamp of the card's FIRST row in the card view, which is
 * what anchors the probe. Using the first row rather than an arbitrary one is
 * what makes this prefix-stable: a score-time export and a full-window export
 * of the same run agree on which row is first for every card they share, so
 * the probe lands identically in both.
 *
 * Returns nullopt when the card is not probed, when no campaign was live, or
 * when the chosen operator held no endpoint spanning the instant. The last of
 * those is a legitimate answer rather than a failure — `deviceAt` thins the
 * candidate set near a replacement boundary, the same way it does for the
 * compromise planner — and it simply means no probe row is written. */
/* `probedBasisPoints` DEFAULTS to the shipped level and exists so a gate can
 * SWEEP the level without recompiling the world or duplicating this resolver's
 * logic. `device-sharing-evidence-2026-08` needed exactly that: the level is
 * declared "sized for fan-out, not for prevalence", and until K.6 nothing had
 * ever measured the fan-out it actually produces in the view a consumer reads,
 * so choosing a value required measuring several. Duplicating the predicate in
 * the test instead would let the two drift, which is the failure
 * `onSomeList`'s "kept beside `probeFor` so the two cannot drift" comment
 * already guards against. Passing anything but the default in PRODUCTION code
 * is a bug. */
[[nodiscard]] inline std::optional<Probe>
probeFor(const AttackerInfra &attackers, entity::Key card, std::int64_t ts,
         std::uint32_t probedBasisPoints = kProbedBasisPoints) {
  if (attackers.empty()) {
    return std::nullopt;
  }

  /* Card key alone. Not the timestamp, not the owner, not the row: anything
   * carrying activity would make a probed card a busier card, and busier cards
   * are more likely to be victimised (`exposure.hpp` tilts victim selection on
   * activity), which would reintroduce the correlation this is built to
   * avoid. */
  const auto keyMix =
      derived::splitmix(card.number) ^
      derived::splitmix(static_cast<std::uint64_t>(card.role) << 8U ^
                        static_cast<std::uint64_t>(card.bank));

  if (derived::splitmix(keyMix ^ kProbeDomain) % 10'000U >= probedBasisPoints) {
    return std::nullopt;
  }

  /* Weighted by case load, so probes concentrate on the operators that already
   * run many cases. That is what produces ONE high-degree enumeration device
   * rather than the flat spread a uniform pick would give — and a flat spread
   * would defeat the purpose, since the fan-out has to be comparable to an
   * attacker's before it can stop degree separating them. */
  const auto op = attackers.operatorAt(
      derived::unitFrom(keyMix ^ kOperatorDomain), ts);
  if (!op.has_value()) {
    return std::nullopt;
  }

  const auto probeTs = ts - kProbeLeadSeconds;
  const auto salt = derived::splitmix(keyMix ^ kEndpointSaltDomain);

  /* A probe is an instant, so the endpoint need only span [probeTs, probeTs+1).
   * Asking for a wider span would thin the candidate set for no reason. */
  const auto device = attackers.deviceAt(*op, probeTs, probeTs + 1, salt);
  const auto ip = attackers.ipAt(*op, probeTs, probeTs + 1, salt);
  if (!device.has_value() || !ip.has_value()) {
    return std::nullopt;
  }

  return Probe{.device = *device,
               .ip = *ip,
               .timestamp = probeTs,
               .operatorIndex = *op};
}

/* The predicate on its own, for gates that ask "was this card probed?" without
 * needing a live campaign. Kept beside `probeFor` so the two cannot drift. */
[[nodiscard]] inline bool onSomeList(entity::Key card) noexcept {
  const auto keyMix =
      derived::splitmix(card.number) ^
      derived::splitmix(static_cast<std::uint64_t>(card.role) << 8U ^
                        static_cast<std::uint64_t>(card.bank));
  return derived::splitmix(keyMix ^ kProbeDomain) % 10'000U <
         kProbedBasisPoints;
}

} // namespace PhantomLedger::infra::enumeration
