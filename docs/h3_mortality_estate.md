# H3 mortality + estate + replenishment contract (macro-history-v1)

Status: code-complete 2026-07-26 (part 3c-ii); earlier parts verified by
2026-07-25; all four goldens recaptured. Authority: U-8 and addendum, now
[Persona timeline, mortality and membership](fraud_model_audit.md#m-5-persona-timeline-mortality-and-membership).

The defect closed: nobody died and the population only grew. Retirees
seeded at 65-99 reached 94-128, inheritance ignored death, joiners aged as
of sim start, membership was joiners-only at a flat 2%/yr, and accounts
lived forever.

## The lifespan primitive (step 1)

`lifespan::derive` makes three draws per person on `{"mortality",
personId}`: latent sex (50/50, declared), one uniform inverting the annual
hazard walk over the embedded SSA 2023 period table (4.C6, log-linear
interpolation), and within-year placement. Death lands strictly after the
person's anchor (sim start for seeds, join date for joiners), counted from
birth, capped at 120. Declared: one table era-wide, no SES gradient,
uniform within-year timing. The Timeline carries `death` and `male`
(filled in `timeline::derive`; the eight persona-era draws are unchanged),
so H2 consumers read death without new threading. Gate: test_lifespan.

## The behavioral/contractual line (declared)

Behavioral flows stop at death:

| Flow | Mechanism |
|---|---|
| Salary | ends at min(retirement, death) |
| SSA / disability | `Recipient.end` = death (survivor benefits registered) |
| Revenue | stops at death, perpetual retiree/HNW plans included |
| Session | `Census::deathDays`; the loop skips the person-day |
| ATM | emission-side filter, stream byte-identical |
| Internal transfers | skip after the draws burn |
| Rent | lease dies with the tenant (declared shared-stream shift) |
| Family gifts | `dropDeadPartyRows` drops rows with a dead party (external family unmodeled) |
| Insurance claims | post-draw filter at death (filing is behavioral) |
| Split deposits | follow the payday inbound stream, which death ends |

Contractual flows post against the estate until closure at death +
`pii::kSettlementDays` (120d), always after the site's draws burn:

| Flow | Stop |
|---|---|
| Subscriptions, insurance premiums | emission skip at closeTs |
| Loan/tax obligations | draft skip at closeTs |
| Card cycles | statements end at closeTs - 50d (`kCardSettleTailDays`: grace 25d, late tail 20d, fee morning); per-card lanes |

120 days contain the funeral (death+3-11d) and estate (death+30-90d), so
every estate row is visible before closure.

## H1 subscription defect (found and fixed in 3c-ii)

Production subscriptions never scaled: the routines `DebitEmitter`, the
only production path (`passes::addSubscriptions`), drafted raw
calibration dollars, while the U-6 CPI wiring sat in the unreferenced
channels emitter. test_econ_wiring's calibration gate pins 2019 rows,
where scale == 1.0 hides the difference. Fix: screen and draft use
sub.amount × priceScale(debit month). Gate: the deflated pair identity in
test_membership.

## Estates and funerals (part 2b)

The uncaused hazard (0.15% of retirees per 180-day sweep) is retired. Each
in-window death draws, in fixed order on `{"family","inheritance"}`:

- Funeral at death+3-10d: one bill-channel payment from the decedent's
  account to a funeral home in their city as of death (unknown-counterparty-2026-09 amendment in
  `docs/fraud_model_audit.md`; a dedicated channel is registered).
  Lognormal median $6,300 calibration dollars (NFDA 2019: burial $7,640,
  cremation with viewing $5,150, about 55% cremation), sigma .40, floor
  $1,000, CPI-realized.
- Estate at death+30-90d (probate): interim lognormal($25k, sigma 1.0)
  split over children, else supporting children; heirless estates stay
  undistributed. SCF-anchored sizing registered.

Gate: test_estates (causation, timing, one funeral each, the dead-party
filter over 9k+ gift rows).

## Membership and replenishment (part 3c-ii)

- Membership [joinTs, closeTs) (`pii::Membership`, replacing flat
  `Growth`): joinTs is window start for seeds, else the join day; closeTs
  is death + 120d. Built only by `join_cohort::membershipOf(pack, window)`
  (exportAll/exportEntities, the streaming twin, card_fraud).
- Join cohort (`synth/personas/join.hpp`): joinerCount = population × Σ
  over window days of r(year(day)) / 365.2425, r from the embedded BEA
  population series; frozen years reuse the last measured rate. Joiners
  are the last K ids (seed roster byte-stable, gated), with one join-day
  draw each on `{"join-cohort", personId}`, inverse-CDF ∝ r. Carried in
  `Pack::joinDays` (from `identity.windowDays` in simulate.cpp; mirrored
  by the blueprint fallback).
- Age axis: joiners' dob, timeline and lifespan anchor at the join date
  (dob.hpp, `timeline::deriveAll`, `lifespan::derive`), so a 2015 joiner gets 2015 ages and `personaAt(join) == seed`. Joiners
  still generate from window start; the standard exporter's filter
  decides visibility.
- Fraud: each ring carries `participantsAliveEndEpoch`, the earliest death
  among its fraud and mule participants (rings.hpp). Typology and
  camouflage windows end 22 days before it (`kRingScheduleGuardDays`;
  invoice can spill 21 days). Victims and the solo/unauthorized rail are
  exempt (deceased-account fraud is real).
- Exporters: standard customer.csv adds `closed_at` (kErCustomer 2 to 3
  columns) and filters on [joinTs, closeTs). aml/aml_txn_edges Customer
  status turns closed when the corpus end reaches closeTs
  (`SharedContext::closedByPerson`). card_fraud Party `created_at` is
  joinTs. AML corpora stay full-world; AML onboarding dates stay synthetic
  (declared inconsistency, alignment registered).

Gate: `tests/test_membership.cpp` covers each rule above plus the death
and closure stops, card truncation, rings never recruiting the dead, and
the filter (nothing before join, nothing after close).

## Rules kept

No new CLI. New randomness only on `{"mortality"}`,
`{"family","inheritance"}` and `{"join-cohort"}`. Rent is the one shared
stream that shifts; card truncation and ring clamps use isolated lanes.

## Authority

U-8 (merged 2026-07-25) covers work through 3b-i. Its addendum script
(`merge_authority_h3_membership_2026_07.py`, no longer in the repository)
added membership, replenishment, closure, the defect fix and fraud
intervals. The BEA series was already embedded (U-4/U-5 lineage).

## Registered upgrades

Historical-period mortality tables; SES-differential mortality; sex in PII
with a measured ratio; survivor benefits; SCF-anchored estates and heirless
distribution; life-policy death benefits (sized with estates); per-bank
acquisition series for join sizing; AML onboarding on the membership axis;
external family-member deaths.
