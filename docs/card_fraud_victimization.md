# Victimization: who gets defrauded, and how PhantomLedger decides

Status: delivered (V1, V2, V4, V3). Authority: `docs/fraud_model_audit.md`
rows U-11 (V1, V2) and U-12 (V3). The audit, research, design and plan are the
original record; round 6 and 7 notes and the delivered sections record what
changed and what shipped.

Origin: a 500-person, 30-year corpus had zero fraud victims, yet a scammer
abroad can talk anyone into paying $1,500 for a "sick relative" however many
fraudsters the roster holds.

## Audit (the code when this arc began)

Line numbers in `src/transfers/fraud/injector.cpp` are as audited.

- F1. One line blocked everything: `injector.cpp:577` returned `{}` when
  `rings_.topology->rings.empty()`. Ring count
  `round(lognormal(6.0, 0.4) × population/10000)` has no floor
  (`rings.hpp:38`), so below about population 833 there are no rings, and
  camouflage, ring laundering, card fraud, gift-card scams and ATO all
  vanished. No rings is realistic at that size; rings silencing scams is a
  defect.
- F2. The unauthorized/scam rail already uses exogenous attackers (only the
  AML ring typologies derive victims from fraudsters). `buildCompromisePlans`
  (`:348`) draws victims via `rng.choiceIndex(personLimit)` over the whole
  roster, excluding ring participants and ring victims (`:374`). Source: the
  victim's own account; destination: a merchant (card-fraud-realism-v2 b-2);
  IP: random. Round 7: every card-fraud positive therefore looks like a
  derived debit card. Moving it onto the victim's issued credit card was
  reverted: fraud is planned after `CardCycleDriver` closes statements and
  generates payments and interest, so the swap created unserviced debt. This
  needs lifecycle reordering and is open.
- F3. The budget is window-scaled:
  `txnFraudBudget = targetTxnFraudP × (realizedBaseCount + camouflage + illicit)`,
  and `realizedBaseCount` grows with the window. Ring victim count is
  window-invariant (sampled once in `make()`) but feeds only AML typologies.
- F4. The level is about right. Population 500, 30 years, without F1:
  0.0012 × 4.2M rows ≈ 5,000 fraud rows; ÷ U{5..14} events per case
  (mean ≈ 9) ≈ 550 cases; 500 × (1 − e^−1.1) ≈ 333 distinct victims (67%).
  Real world: about 69% victimized at least once in 29 years at a 4%/year
  hazard. This justified decoupling visibility from ring count; it is not a
  calibration. Card/scam prevalence and CNP share against a named issuer
  series remain open benchmark gates.
- F5. Victim selection was uniform (`rng.choiceIndex(personLimit)`), so every
  victim-side feature was noise and a GNN had nothing to learn there.

## Research: the two families run in opposite directions

| | Authorized scams | Unauthorized card fraud |
|---|---|---|
| What | victim deceived into paying (imposter, family-emergency, tech support, romance, job); the owner's example | third-party misuse of an existing account |
| Incidence | skews younger: FTC Consumer Sentinel shows people in their 20s and 30s reporting a loss more often than 70+ | about 1 in 10 aged 16+ per year (BJS NCVS Identity Theft Supplement); existing credit-card misuse is the largest category |
| Age and driver | severity skews older: median loss highest at 70-79 and 80+; elder typologies (grandparent, tech support) per FinCEN advisories and CFPB elder-exploitation SAR analyses | exposure: cards held, volume, e-commerce, income (higher-income households report more); mild age gradient peaking in prime working years, lower for 65+ |
| Payment method | gift cards and wires skew older; P2P and crypto younger, crypto with the largest losses | |
| Reimbursed | no: the victim authorized it | yes (Reg E / Reg Z), already modelled by the p=.85 report-and-reimburse layer (scam-fraud-2026-07) |

So retirees are low-exposure for card fraud and high-severity for scams; one
"vulnerability" knob would model this backwards.

Anchoring discipline:

- Measurement (defensible): the directions above and order-of-magnitude
  prevalence (single-digit percent per cardholder-year; about 1 in 10 for
  identity theft), robust across FTC, BJS, FinCEN and CFPB.
- Choice (declared): every per-persona and per-age-band multiplier. Sources
  give form and sign, not values.
- A multiplier counts as measurement only after the owner verifies the FTC
  Consumer Sentinel age-band loss table and the BJS ITS
  victimization-by-income table. Until then each ships as a choice with a
  named comparator, like `kCardNotPresentShare = 0.70`.

Proposed persona mapping (directions defensible, magnitudes a choice):

| Persona | Card exposure | Scam incidence | Scam severity |
|---|---|---|---|
| `student` | low: thin file, few cards | high: job, fake-check, online-shopping | low |
| `salaried` | high: most cards and e-commerce | baseline | baseline |
| `freelancer` | moderate-high | elevated: invoice, job | moderate |
| `smallBusiness` | high: business card, volume | elevated: BEC-adjacent | high |
| `highNetWorth` | highest: card count, volume, ticket size | moderate | highest |
| `retiree` | low: fewer cards, less online | moderate | highest |

## Design

- D1. Weight unauthorized victims by realized card activity, not a persona
  table. Exposure already exists per person (persona spending, H4 era volume,
  card ownership), so this follows derive-don't-store, needs no new anchors,
  and the persona gradient emerges (a retiree has fewer card rows). The
  injector got only the scalar `realizedBaseCount`, so exposure needs a
  carrier (D-Q1).
- D2. Authorized scams get their own fraud type. `Rail::giftCardScam` /
  `FraudType::scamGiftCard` is one payment method of the class,
  authorized-push-payment fraud. Proposed, append-only per the
  designated-initializer law:
  - `FraudType::scamImpostor = 6`: deceived wire, P2P or bank transfer;
    `scamGiftCard = 5` stays as the family's gift-card method;
  - hazard `persona-at-date × age-at-date`, normalized so total prevalence is
    unchanged (the budget law `F = pL/(1−p)` still sets volume), as the
    registered Bettencourt b′ tilt does;
  - amount scaled by age band on top of CPI realization;
  - method mix age-graded: gift card and wire older, P2P younger;
  - no reimbursement (the card rail has p=.85): a real, learnable asymmetry.
- D3. Anti-shortcut condition. A tilt makes persona and age correlate with the
  label, which is realistic and is how this arc's original defect happened.
  As in `test_card_baselines`, persona-only and age-only classifiers must stay
  below a stated recall at precision ≥ 0.90. If a tilt clears it, fix the
  exponent, not the gate. The gate ships in the same round as the tilt.

## Round plan

| Round | Content | Golden impact |
|---|---|---|
| V1 | per-family guards replace the blanket `rings.empty()` return; low-population fraud-visibility gate (the coverage hole that hid F1) | likely zero: configs with ≥1 ring are bit-identical, so a moved golden was pinning the bug |
| V2 | exposure-weighted unauthorized victims (D1); named lane | model-moving, four goldens |
| V3 | `scamImpostor`, persona/age scam hazard, age-graded severity, method mix (D2) | model-moving, four goldens |
| V4 | persona-only and age-only baselines (D3); prevalence suite covers victim-side distribution by persona | zero |

V1 lands first regardless. V4's harness ships with V3 and measures the
pre-tilt world, or the tilt has no gate (the ordering error the Bettencourt b′
plan records).

## Owner decisions requested

- D-Q1. Exposure carrier: per-person realized card-row count through the legit
  stage (truer), or `holdings.creditCards` × persona-at-date (cheaper)?
- D-Q2. V3 methods: wire/P2P beside gift cards, or also the bank-visible
  `crypto_ramp_out` boundary? That rail must not imply native-token or wallet
  semantics, which do not exist.
- D-Q3. Multipliers as declared choices now (faster), or after verifying the
  FTC/BJS tables (stronger; the audit convention prefers it for load-bearing
  values)?
- D-Q4. Own BEC/check-fraud typologies for `smallBusiness`, or consumer rails
  for now? (Registered either way.)

## V3 delivered (victimization-2026-07b)

Authority: U-12.

1. `FraudType::scamImpostor = 6` (`scam_impostor`) appended; no value moves.
2. `Rail::scamImpostor`: the victim authorizes a push to the attacker's payee,
   50/50 a wire-shaped `externalUnknown` transfer or a `p2p` push. Both carry
   heavy legitimate volume, so the channel cannot label the row. Crypto stayed
   out. The old reason (era lock ending 2020) is retired, since coverage
   reaches 2024 and a bank-visible USD crypto-ramp boundary exists; adding it
   needs an explicit prevalence, severity and labelling decision.
3. Per-rail picker: card/ATO keep the V2 exposure CDF (date-independent,
   built once); authorized rails use persona × age susceptibility rebuilt at
   the case date; ATO drops and impostor payees (attacker accounts) draw
   uniformly.
4. The case date is drawn before the victim, so a scam finds whoever is
   susceptible then. Persona and age change over life; the old order could
   not express that.
5. Incidence falls with age; severity rises about 3x. `scamWireAmount`
   (median $900, sigma 1.3, clamped [$50, $50k]) applies era scale and age
   severity to median and clamps alike, so only the level moves.
6. Membership at the case date: joined for every rail, alive for scam rails.
   Round 7 tightens this to `[joinTs, closeTs)`; card/ATO keep the
   deceased-account exemption only in the 120-day estate-settlement tail.
7. No reimbursement on either authorized rail (Reg E covers unauthorized
   transfers; the UK code postdates the window). Gated, not assumed.
8. `tests/test_card_scam_rail.cpp`: a pure layer (model shape) and a world
   layer (the fold exercises it), with an exact, sampling-free per-band
   hazard measurement over the real population at both window ends.

### Departure: severity does not touch the gift-card rail

The plan had severity buy more gift cards (a rack caps one at $500). That put
an 80-year-old at up to 13 × $500 = $6,500 within four hours from retail
checking; most rows were unfundable and discarded, leaving a burst of declines
no FTC spotlight describes. It was removed in the same round. Severity applies
only to the impostor amount; the gift-card rail keeps the fixed-nominal
denomination lattice (U-6) and a plain U{2..6} `targetSpan` (reasoning beside
the constant in `injector.cpp`).

### Exposed by V3: the arch-equivalence world-shape trap

V3 was the first corpus-path reader of `Pack::joinDays`, which surfaced a
world-shape mismatch as a `test_arch_equivalence` failure.
`SimulationPipeline::buildEntities()` sizes the join cohort to its window
(`simulate.cpp` sets `identity.windowDays`; four joiners at population 300 /
730 days), but the GateWorld harness defaulted `windowDays` to 0, an H3 3c-ii
choice to keep gate worlds byte-identical. The failure posed as a "SEMANTIC
divergence" in the innocent settlement path. `test_production_windowed` (both
legs cohort-shaped) stayed green and is what proves the engines agree. First
fix: a `withJoinCohort` option on the equivalence leg, with joiner counts
pinned on both legs before comparison.

The join-cohort round (U-13) then fixed the four gates still measuring a
joinerless world (`test_card_baselines`, `test_card_prevalence`,
`test_card_merchant_overlap`, `test_econ_wiring`):

- `withJoinCohort` defaults true in `WorldSpec` and `LegOptions`; `false` is
  only a bisect knob (world-shape move versus model move).
- All four pin `leg.joiners > 0`; `checkLegMatches` reports WORLD SHAPE
  MISMATCH before any corpus diagnostic.
- BEA-sized cohorts at N=300: 8 joiners (730 days from 1991), 15 (1,461
  days), 2 (730 days from 2019), so `test_econ_wiring`'s legs differ by
  design and both counts print.

No band was widened or re-centred; one was removed as mis-specified. The first
run passed 56 of 57: `test_card_prevalence`'s deflated-fraud-amount sub-gate
read 2.69x against 2.50x, with the nominal-spread discriminator at 2.47x.
Moving together means amount mix, not CPI wiring (deflated/nominal = 1.0883 =
priceScale(1994)/priceScale(1991)). The sub-gate had always been wrong:

- `unauthorized.cpp` applies `priceScale` only to continuous samplers, while
  `cardTestCharge` and `giftCardScamAmount` are fixed-nominal under the
  owner-approved U-6 lattice choice, so a flat deflated combined mean
  contradicts U-6 (`test_econ_wiring` already excluded the rail for this).
- Per-year means of 42-92 lognormal(σ=1.2) draws carry 19-28% sampling error,
  so a four-year 2.50x max/min envelope was under-powered and the earlier
  1.79x was luck.

The statistic is now printed and decomposed (lattice versus CPI-scaled,
nominal and deflated, per year, plus a class-F clamp-ceiling ratio). Other
bands absorbed the re-roll; the yearly rate spread (the budget-law gate) moved
1.12x → 1.85x against 4.00x. Before/after tables are in each gate's header.

### Replacement gate: `tests/test_card_class_f.cpp`

Restores coverage of the declared law "U-6 class F reaches the card rail". Two
era legs at N=900 (1991/1461d, 2019/730d) compare the 75th percentile of
non-lattice card-fraud amounts, each deflated by its own year's `priceScale`.

- Gift-card rows are excluded by `FraudType`. Card-test probes share their
  spend's type and cannot be filtered, but sit at ≤ $5 nominal, below
  p = 0.75 (a scale family's quantile scales exactly with the scale); the gate
  asserts their share stays below it.
- Cross-era (about a 1.8x effect), not within-era flatness (a null effect
  against heavy-tailed noise).
- Band: ±3σ of the realized two-leg quantile standard error (analytic
  CV = 1.635/√n).
- It fails as UNDER-POWERED unless the band excludes the fixed-nominal null
  (≈0.55) and the double-scaled null (≈1.81). Fix with a larger leg, never a
  tighter band.

### Rounds 6 and 7: session and membership closure

Round 6: `transactions::Factory` already routed the victim's own device/IP for
authorized rows, but `unauthorized.cpp` overwrote it with the attacker's. The
overwrite now stops on gift-card and impostor rails (no new randomness,
carrier or slot-0 device pattern); card/ATO keep the attacker session. An
earlier "carried forward" design had overstated this fix.

Round 7:

- every fraud rail requires membership in `[joinTs, death + 120d)`; authorized
  scams also need the victim alive, and the full case span must fit before
  the earliest victim/payee boundary rather than be truncated into a burst;
- unauthorized card rows stay derived-debit; a gate rejects the late
  credit-liability swap until fraud planning joins card-cycle servicing;
- legitimate credit-card keys join router ownership, so their purchases carry
  the owner's device/IP session;
- every device owner type renders in one opaque fixed-width `D` namespace, not
  role-revealing `FD`/person/shared layouts;
- the card graph emits timestamped transaction→device/IP edges and makes every
  observed endpoint a vertex. (Round 7 left `Has_Device`/`Has_IP` header-only
  so Party adjacency could not reveal attacker role; attacker-infra-2026-07
  populated both and removed the asymmetry in the generator, leaving a
  2.9x-lift residual.)

Still open, as benchmark work rather than victimization claims: issued-card
fraud servicing, effective card expiry/reissue histories, era-varying attacker
behaviour, delayed labels, real-modality calibration, and the external
GSQL/training/evaluation pipeline.
