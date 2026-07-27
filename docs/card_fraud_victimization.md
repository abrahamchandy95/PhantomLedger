# Victimization: who gets defrauded, and how PhantomLedger should decide

**Status: DELIVERED (V1, V2, V4, V3). Authority rows: docs/fraud_model_audit.md
U-11 (v1/v2) and U-12 (v3). Parts 1–5 below are the ORIGINAL research and
plan, kept verbatim as the record of what was decided and why; the
DELIVERED sections at the end record what actually shipped, including the
two places the shipped model departs from this plan.**

Origin: the owner's observation that a 500-person, 30-year corpus
producing zero fraud victims is not realistic — "someone in another
country can social-engineer them into paying $1,500 for a sick relative;
this does not depend on the number of fraudulent people in the
dataset." That is correct, and the audit below both confirms it and
narrows the defect to something much smaller than first feared.

---

## Part 1 — AUDIT (what the code does today)

### F1. The blocking defect is one line

`src/transfers/fraud/injector.cpp:577`

```cpp
if (rings_.topology->rings.empty()) {
  return {};        // the ENTIRE fraud stage
}
```

Ring count is `round(lognormal(6.0, 0.4) × population/10000)` with no
floor (`rings.hpp:38`), so it is **0 below roughly population 833**. At
population 500 the whole fraud stage — camouflage, ring laundering,
unauthorized card fraud, gift-card scams, ATO — returns empty.

This is a DEFECT, not a design. Ring absence at small population is
realistic; ring absence silencing *scams* is not.

### F2. The unauthorized/scam rail is ALREADY exogenous-attacker

This corrects an earlier claim in this arc that "victims are derived
from fraudsters." That is true only for the AML ring typologies.

`buildCompromisePlans` (injector.cpp:348) draws victims with
`rng.choiceIndex(personLimit)` over the **whole roster**, and explicitly
EXCLUDES ring participants and ring victims (line 374). The compromise's
source is the victim's own account, the destination is a merchant
(card-fraud-realism-v2 b-2), and the IP is a random address. **No roster
criminal participates in a card compromise or a scam.** The
architecture the owner asked for is the architecture that exists — it is
just gated behind F1.

### F3. Its budget is window-scaled, not window-invariant

`txnFraudBudget = targetTxnFraudP × (realizedBaseCount + camouflage +
illicit)`, and `realizedBaseCount` grows with the window. Compromise
count therefore scales with corpus size. (Ring VICTIM count is
window-invariant — sampled once in `make()` — but that population is
separate and feeds only the AML typologies.)

### F4. The rail's victimization level is already about right

At population 500 over 30 years, had F1 not fired:

```
txnFraudBudget      ≈ 0.0012 × 4.2M corpus rows      ≈ 5,000 fraud rows
÷ events per case   U{5..14}, mean ≈ 9               ≈   550 compromise cases
distinct victims    500 × (1 − e^−1.1)               ≈   333  = 67% of people
```

Against a real-world ~69% victimized at least once over 29 years at a
4%/year hazard. The LEVEL needs no repair. What needs repair is F1, and
then WHO gets selected.

### F5. Victim selection is UNIFORM — and that is the realism gap

`rng.choiceIndex(personLimit)` gives every customer identical hazard.
Real victimization is strongly differentiated, and for a GNN a uniform
draw is worse than unrealistic: it means there is **no learnable
victim-side signal at all**. Every victim-side feature is noise by
construction.

---

## Part 2 — RESEARCH (who actually gets defrauded)

### The central finding: the two families run in OPPOSITE directions

This is the result that shapes the whole design, and it is why the owner
is right that this deserves its own fraud type.

**A. Authorized scams** (victim is deceived into paying — imposter,
family-emergency, tech support, romance, job scams). The owner's
example is exactly this class.

- **Incidence skews YOUNGER.** FTC Consumer Sentinel Network reporting
  consistently shows younger adults (20s–30s) reporting a *loss* to
  fraud at a higher rate than adults 70+.
- **Severity skews sharply OLDER.** Median loss per victim rises with
  age and is highest in the 70–79 and 80+ bands.
- Elder-specific typologies concentrate: the family-emergency
  ("grandparent") scam and tech-support scam — the subject of FinCEN
  elder-financial-exploitation advisories and CFPB analyses of
  elder-exploitation SAR filings.
- **Payment method is age-graded**: gift cards and wire transfers skew
  older; peer-to-peer apps and cryptocurrency skew younger, with crypto
  carrying the largest dollar losses.
- **Not reimbursed** — the victim authorized the payment.

**B. Unauthorized card fraud** (third-party misuse of an existing
account).

- BJS National Crime Victimization Survey, Identity Theft Supplement:
  on the order of 1 in 10 persons aged 16+ per year experience identity
  theft, with **misuse of an existing credit-card account the largest
  single category**.
- Victimization is **EXPOSURE-driven**: it rises with number of cards
  held, transaction volume, e-commerce activity and income. Higher-income
  households report higher rates.
- Age gradient is mild and peaks in prime working years; **LOWER for
  65+** — fewer cards, less online commerce.
- **Reimbursed** (Reg E / Reg Z zero-liability), which PhantomLedger
  already models via the p=.85 report-and-reimburse layer
  (scam-fraud-2026-07).

**So retirees are LOW-exposure for unauthorized card fraud and
HIGH-severity for authorized scams.** One "vulnerability" knob would
model this backwards.

### ANCHORING DISCIPLINE — what is defensible and what is a choice

Per this repository's convention:

- **MEASUREMENT (defensible now):** the *directions* above, and the
  order of magnitude of overall prevalence (single-digit percent per
  cardholder-year; ~1 in 10 for identity theft broadly). These are
  robust across FTC/BJS/FinCEN/CFPB publications.
- **CHOICE (must be declared, not claimed):** every per-persona and
  per-age-band multiplier below. I can supply the functional FORM and
  the SIGN from published findings; the exact table values are not
  something to reconstruct from memory into a constant.
- **OWNER VERIFICATION REQUIRED** before any multiplier is treated as
  MEASUREMENT: the specific FTC Consumer Sentinel age-band loss table
  and the BJS ITS victimization-by-income table. Until then each row
  ships as a CHOICE with a named comparator, exactly as
  `kCardNotPresentShare = 0.70` did.

### Proposed persona mapping (DIRECTION defensible; magnitudes CHOICE)

| Persona | Unauthorized card exposure | Scam incidence | Scam severity |
|---|---|---|---|
| `student` | LOW — thin file, few cards | HIGH — job, fake-check, online-shopping scams | LOW |
| `salaried` | HIGH — most cards, most e-commerce | baseline | baseline |
| `freelancer` | MODERATE-HIGH | ELEVATED — invoice and job scams | MODERATE |
| `smallBusiness` | HIGH — business card plus volume | ELEVATED — BEC-adjacent | HIGH |
| `highNetWorth` | HIGHEST — card count, volume, ticket size | MODERATE | HIGHEST |
| `retiree` | **LOW** — fewer cards, less online | MODERATE | **HIGHEST** |

---

## Part 3 — THE DESIGN

### D1. Unauthorized rail: weight by EXPOSURE, not by a persona table

The unauthorized gradient is mechanical — it is transaction exposure —
and PhantomLedger already generates exposure per person (persona
spending profiles, H4 era volume, card ownership). So weight victim
selection by **realized card activity** rather than inventing a persona
multiplier table.

This follows the repository's DERIVE DON'T STORE law, needs no new
anchors, and the persona gradient in the table above emerges as a
consequence rather than being asserted. A retiree ends up
low-exposure because the model already gives them fewer card rows.

*Implementation cost:* the injector currently receives only the scalar
`realizedBaseCount`. Per-person exposure needs a carrier. Two options
for the owner (D-Q1 below): a per-person card-row count computed in the
legit stage and carried in, or a cheaper proxy — credit-card ownership
(already in `holdings.creditCards`) crossed with persona-at-date.

### D2. Authorized scams: a NEW fraud type with its own hazard

Today the only authorized-scam rail is `Rail::giftCardScam` /
`FraudType::scamGiftCard` — gift card is one *payment method*, not the
class. The class is authorized-push-payment fraud.

Proposal, APPEND-ONLY per the designated-initializer law:

- `FraudType::scamImpostor = 6` — victim-authorized transfer under
  deception via wire / P2P / bank transfer. `scamGiftCard = 5` stays as
  the gift-card payment method of the same family.
- Selection hazard: `persona-at-date × age-at-date` susceptibility,
  normalized so **total prevalence is unchanged** — the budget law
  `F = pL/(1−p)` still owns how much fraud exists. This is the same
  normalization technique the registered Bettencourt b′ tilt uses, and
  it keeps this round orthogonal to prevalence calibration.
- Severity: amount scales with age band (the steep part of the real
  data), on top of the existing CPI realization.
- Payment-method mix age-graded: gift card / wire older, P2P younger.
- No reimbursement (contrast with the card rail's p=.85), which is a
  real, learnable asymmetry the corpus already half-models.

### D3. THE ANTI-SHORTCUT CONDITION (non-negotiable)

Adding a persona and age tilt makes those attributes correlate with the
label. That is realistic — and it is exactly how this arc's original
defect happened. So, mirroring `test_card_baselines`:

**A persona-only classifier and an age-only classifier must not solve
the task.** Same metric: recall at precision ≥ 0.90 below a stated
bound. If a tilt ever clears it, the exponent is wrong — not the gate.

This gate ships in the SAME round as the tilt, never after.

---

## Part 4 — ROUND PLAN

| Round | Content | Golden impact |
|---|---|---|
| **V1** | The F1 guard: replace the blanket `rings.empty()` return with per-family guards, so the unauthorized/scam family runs independently of ring topology. Plus a low-population fraud-visibility gate (the coverage hole that hid this). | **Likely ZERO** — for any config with ≥1 ring, behaviour is bit-identical. If a golden moves, that config had no rings and the golden was pinning the bug. |
| **V2** | Exposure-weighted unauthorized victim selection (D1). Named lane. | MODEL-MOVING, four goldens |
| **V3** | `scamImpostor` type + persona/age scam hazard + age-graded severity + payment-method mix (D2). | MODEL-MOVING, four goldens |
| **V4** | Persona-only and age-only baselines (D3) + prevalence-suite extension covering victim-side distribution by persona. | ZERO |

V1 is independently valuable and should land first regardless of what
the owner decides about V2–V4.

Sequencing note: V4's baselines must exist before V3 lands, or the tilt
has no gate — the same ordering error the Bettencourt b′ plan already
records. Practically that means V4's harness ships WITH V3, measuring
the pre-tilt world in the same round.

---

## Part 5 — OWNER DECISIONS REQUESTED

- **D-Q1.** Exposure carrier for D1: per-person realized card-row count
  (truer, needs a new carrier through the legit stage) or the cheaper
  credit-card-ownership × persona-at-date proxy?
- **D-Q2.** Scope of V3's payment methods: keep it to wire/P2P added to
  the existing gift-card rail, or model the full FTC method mix
  including crypto (which would need a crypto counterparty class that
  does not exist)?
- **D-Q3.** Do the per-persona multipliers ship as declared CHOICEs now,
  or do you want the FTC/BJS tables verified first so they can ship as
  MEASUREMENT? The former is faster; the latter is stronger and is what
  the audit convention prefers for anything this load-bearing.
- **D-Q4.** Does `smallBusiness` get BEC/check-fraud typologies of its
  own, or stay on the consumer rails for now? (Registered either way.)

## V3 — DELIVERED (victimization-2026-07b)

The authorized-scam rail landed as the last round on this arc's critical
path. Authority: docs/fraud_model_audit.md U-12.

What shipped:

1. `FraudType::scamImpostor = 6` (`scam_impostor`), appended — no
   existing enum value moves.
2. `Rail::scamImpostor` on the unauthorized family: the victim
   AUTHORIZES a push to the attacker's payee account, 50/50 over a
   wire-shaped `externalUnknown` transfer and a `p2p` app push. Both
   channels carry heavy legitimate volume, so the rail cannot label the
   row. Crypto stays DECLINED — the era lock ends the window in 2020.
3. THE PICKER IS PER RAIL. card/ato keep the v2 exposure CDF
   (date-independent, built once); the two authorized rails draw on a
   persona x age susceptibility hazard rebuilt AT THE CASE DATE; ATO
   drops and impostor payees draw UNIFORMLY, because those accounts are
   the attacker's and neither hazard bears on being one.
4. THE DRAW ORDER REVERSED: the case date is drawn before the victim, so
   a scam happens at a time and then finds someone susceptible at that
   time. Persona and age are not person constants in this model, and the
   old order could not express a life course.
5. TWO OPPOSITE GRADIENTS, both asserted: incidence falls with age,
   severity rises ~3x. `scamWireAmount` (median $900, sigma 1.3, clamped
   [$50, $50k]) takes the era scale AND the age severity, both applied to
   median and clamps so only the level moves.
6. MEMBERSHIP AT THE CASE DATE: joined for every rail; ALIVE for the
   scam rails only, which preserves the declared deceased-account-fraud
   exemption for card/ato rather than reversing it in passing.
7. NO REIMBURSEMENT on either authorized rail (Reg E covers unauthorized
   transfers; the UK code postdates the window) — gated, not assumed.
8. `tests/test_card_scam_rail.cpp`: a pure layer (the model's shape) and
   a world layer (the fold actually exercises it), including an
   exact — sampling-free — per-band hazard measurement over the real
   population at both ends of the window.

### DEPARTURE FROM THE PLAN: severity does NOT touch the gift-card rail

Part 3 D2 and an earlier draft of this record both said severity buys
MORE CARDS on the gift-card rail, since a rack caps a single card at
$500. **That was implemented and then removed in the same round, and the
plan is wrong, not just the code.**

Grading the card COUNT by the victim's severity multiplier put an
80-year-old at up to 13 × $500 = $6,500 of gift cards inside four hours,
out of a retail checking account. Most of those rows are unfundable, so
the ledger discards them: the visible effect is not a larger loss but a
burst of declines that no FTC spotlight describes. The research supports
LOSS rising with age, and the impostor rail already carries that with a
continuous amount.

So severity applies to the **impostor amount alone**. The gift-card rail
is ungraded in both denomination and count — the denomination lattice
stays fixed-nominal per authority U-6, and `targetSpan` is a plain
U{2..6} for that rail (`injector.cpp`, where the reasoning is recorded
next to the constant).

### WHAT LANDING V3 EXPOSED: the arch-equivalence world-shape trap

V3 is the first thing on the corpus path to read `Pack::joinDays`, via
the case-date membership gate. That turned a latent, silent asymmetry
into a hard `test_arch_equivalence` failure:

- `SimulationPipeline::buildEntities()` ALWAYS sizes the join cohort
  against its window (`simulate.cpp` sets `identity.windowDays`), so the
  monolithic reference leg had FOUR joiners at population 300 / 730 days;
- the GateWorld harness DEFAULTED `windowDays` to 0 — no joiners — a
  deliberate H3 3c-ii choice to keep existing gate worlds byte-identical.

The two legs were therefore comparing **different worlds**, and the
failure presented as a "SEMANTIC divergence" pointing at the settlement
path, which was innocent. `test_production_windowed` (production
monolith vs production windowed, both cohort-shaped) stayed green
throughout — that pairing is what proves the two ENGINES agree.

Fixed by giving the harness a `withJoinCohort` option, setting it on the
equivalence leg, and PINNING the resulting joiner count on both legs
before any corpus comparison runs, so a world-shape mismatch can never
again be reported as an architecture divergence.

**CLOSED by the join-cohort round (authority U-13).** The equivalence fix
above repaired the ORACLE half of the defect and left the MEASUREMENT
half standing: the four behavioural gates on the same harness
(`test_card_baselines`, `test_card_prevalence`,
`test_card_merchant_overlap`, `test_econ_wiring`) went on reporting bands
against a joinerless population production never generates. The harness
default is now INVERTED — `withJoinCohort` defaults TRUE in both
`WorldSpec` and `LegOptions`, so every gate world is the production
shape and there is nothing left to opt into; `false` survives only as a
bisect knob for separating a world-shape move from a model move. All four
gates now PIN `leg.joiners > 0` before reading a row, and
`checkLegMatches` reports a WORLD SHAPE MISMATCH ahead of any corpus
diagnostic. BEA-sized cohorts at N=300: 8 joiners over 730 days from
1991, 15 over 1,461 days, 2 over 730 days from 2019 — so
`test_econ_wiring`'s two legs are perturbed UNEQUALLY, by design, and
both counts are printed.

**NO BAND WAS WIDENED OR RE-CENTRED, and one was REMOVED as
mis-specified.** On the first measured run the world shape moved every
statistic and 56 of 57 tests passed; `test_card_prevalence`'s
deflated-fraud-amount sub-gate went red at 2.69x against its 2.50x
bound, and the nominal-spread discriminator shipped alongside it read
2.47x — the two moving TOGETHER, which is the "amount-MIX" verdict, not
the "CPI-wiring" one. (Exactly: deflated/nominal = 1.0883 =
priceScale(1994)/priceScale(1991), so the deflation was working
correctly.) Reading the amount model then showed the sub-gate had been
wrong since it was written: `unauthorized.cpp` applies `priceScale` only
to the CONTINUOUS samplers, while `cardTestCharge` and
`giftCardScamAmount` are FIXED-NOMINAL by the owner-approved U-6 lattice
CHOICE — so deflating the card view's *combined* mean and asserting
flatness asserts the opposite of U-6, and `test_econ_wiring` had already
excluded that same rail for that same reason. Independently, the per-year
mean of 42–92 draws from lognormal(σ=1.2) carries 19–28% sampling error,
so a 2.50x max/min envelope over four years was under-powered anyway and
the old 1.79x reading was luck. The statistic is now PRINTED and
DECOMPOSED (lattice vs CPI-scaled, nominal and deflated, per year, plus a
class-F clamp-ceiling ratio). Every other band on the four gates absorbed
the re-roll untouched; the yearly RATE spread — the gate that actually
carries the budget law — moved 1.12x → 1.85x against a 4.00x bound.
Per-band before/after tables live in each gate's header.

### THE REPLACEMENT, LANDED: `tests/test_card_class_f.cpp`

Withdrawing that sub-gate left the suite with **zero** coverage of "U-6
class F reaches the CARD rail", which is a declared law — so the gap was
closed in the following round rather than carried. Two era legs at N=900
(1991/1461d, 2019/730d) compare the **75th percentile** of non-lattice
card-fraud amounts, each row deflated by its own year's `priceScale`.

The design is shaped entirely by how the previous attempt failed:

- The two fixed-nominal contaminants are handled DIFFERENTLY because
  only one can be. Gift-card rows are resolvable by `FraudType` and are
  EXCLUDED. Card-test probes carry the same type as the spend they
  precede — there is no filter to write.
- So the STATISTIC dodges what cannot be filtered. A quantile of a scale
  family scales exactly with the scale, and the probes are bounded at $5
  nominal, i.e. the bottom of the axis. `p = 0.75` never sees them, and
  the gate ASSERTS the probe share stays below it rather than assuming.
- The comparison is CROSS-ERA (a ~1.8x effect) instead of within-era
  flatness (a null effect against the same heavy-tailed noise).
- The band is not hand-picked: ±3σ of the REALIZED two-leg quantile
  standard error, from the analytic CV = 1.635/√n.
- **The gate checks its own power** — it fails as UNDER-POWERED unless
  that band excludes both the fixed-nominal null (≈0.55) and the
  double-scaled null (≈1.81). The repair for that is a larger leg, never
  a tighter band.

Carried forward, declared: the attacker session on an authorized push is
wrong (the victim operates their own device); fixing it needs a
victim-own-device carrier, because routing through `infra::Router` from
the fraud planner would advance that person's sticky device index and
perturb legitimate routing in later windows. Tracked with the
ownerId-prefix finding in docs/card_fraud_v2_roadmap.md step d.
