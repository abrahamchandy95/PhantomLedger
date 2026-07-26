# H3 mortality + estate + replenishment contract (macro-history-v1)

**STATUS (2026-07-26): PART 3c-ii DELIVERED — H3 IS CODE-COMPLETE.
Owner verification pending: run the U-8 ADDENDUM merge script
(`merge_authority_h3_membership_2026_07.py`), `make test` (serverless
44; MODEL-MOVING — delete and recapture all four goldens), and
`graphify update .`. Everything through part 3b-i was verified in
prior rounds; 3c-i (U-8 authority + docs) verified 2026-07-25.**

THE DEFECT THIS ARC CLOSED: nobody died and the population only ever
grew — retirees seeded 65-99 would have reached 94-128 over the
canonical window, the inheritance hazard was detached from any death,
joiners aged as of sim start (the declared JOINER AGE AXIS ERROR),
membership was joiners-only at a flat 2%/yr, and every account
persisted forever.

## The lifespan primitive (step 1, VERIFIED)

`lifespan::derive` — EXACTLY THREE draws per person on the isolated
`{"mortality", personId}` lane: latent sex (50/50, declared), one
uniform inverting the annual hazard walk over the EMBEDDED SSA 2023
period life table (4.C6, log-linear interpolation), and the
within-year placement. ALIVE-AT-ANCHOR invariant (conditional survival
from the current age at the person's anchor — sim start for seeds, the
JOIN date for the 3c-ii join cohort; death strictly after it); deaths
anchor to BIRTH dates; age-120 cap. Declared: one period table
era-wide, no SES gradients, uniform within-year timing. The Timeline
carries `death`/`male` (filled inside timeline::derive — the
persona-era eight draws are byte-identical), so every H2 consumer
reads death with zero new threading. Gates: test_lifespan.

## THE BEHAVIORAL/CONTRACTUAL LINE (declared)

**BEHAVIORAL flows STOP at death (all VERIFIED through 3b-i):**

| Flow | Mechanism |
|---|---|
| Salary | active interval ends at min(retirement, death) |
| SSA / disability | Recipient.end = death (survivor benefits registered) |
| Revenue | months stop at death (perpetual retiree/HNW plans included) |
| Spending session | Census::deathDays → the emission loop skips the person-day |
| ATM | emission-side filter (no draws in the loop — stream byte-identical) |
| Internal transfers | skip AFTER the source/destination draws burn (stream byte-identical) |
| Rent | the lease dies with the tenant (declared shared-stream shift) |
| Family gifts | dropDeadPartyRows — either party dead drops the row (external XF members unmodeled) |
| Insurance claims | (3c-ii) post-draw filter at DEATH — claim filing is behavioral |
| Split deposits | CODE-FACT: they consume the payday-inbound stream, which death-clipped income ends |

**CONTRACTUAL flows keep posting against the estate until ACCOUNT
CLOSURE at death + `pii::kSettlementDays` (120d) — DELIVERED 3c-ii:**

| Flow | Stop mechanism (all post-draw / lane-isolated) |
|---|---|
| Subscriptions | emission skip at closeTs (candidates' month draws burn first) — PLUS the H1 CPI DEFECT FIX below |
| Insurance premiums | emission skip at closeTs AFTER the hour/minute draws |
| Loan/tax obligations | draft skip at closeTs AFTER draftFor's draws |
| Card cycles | statement ladder truncates at closeTs − 50d (`kCardSettleTailDays` — grace 25d + late tail 20d + fee morning); per-card lanes keep every other card byte-identical |

The 120-day settlement strictly contains the funeral (death+3-11d)
and the estate distribution (death+30-90d), so every estate row is
corpus-visible before the accounts close.

**H1 WIRING DEFECT FIX (found + fixed at 3c-ii):** production
subscriptions never scaled — the routines DebitEmitter (the ONLY
production path, passes::addSubscriptions) drafted raw
calibration-dollar amounts while the U-6 CPI wiring sat in the
unreferenced channels emitter; test_econ_wiring's calibration gate
pins 2019 rows, where scale == 1.0 hides the difference. Fixed:
screen + draft realize sub.amount × priceScale(debit month). Gate:
the deflated pair identity in test_membership.

## Death-caused estates + funerals (part 2b, VERIFIED)

The uncaused hazard (0.15% of retirees per 180-day sweep) is RETIRED.
Every in-window death produces, in a FIXED per-decedent draw order on
the `{"family","inheritance"}` lane:

- **FUNERAL** at death+3-10d: one bill-channel payment from the
  decedent's account to the external service-merchant hub (dedicated
  counterparty/channel = registered), lognormal median **$6,300
  calibration dollars** — the NFDA 2019 GPL blend (burial $7,640 /
  cremation-with-viewing $5,150 at the ~55% 2019 cremation rate),
  sigma .40, floor $1,000, CPI-realized.
- **ESTATE** at death+30-90d (probate, declared): the interim
  lognormal($25k, sigma 1.0) split over the heirs (children, else
  supporting children; heirless estates undistributed — declared);
  SCF-anchored size re-derivation = registered.

Gates: test_estates (causation, timing windows, exactly one funeral
per decedent, the dead-party filter over 9k+ gift rows, existence).

## Membership + replenishment (part 3c-ii, DELIVERED)

**MEMBERSHIP [joinTs, closeTs)** (`pii::Membership`, rewritten; the
flat-`Growth` model retired): joinTs = window start for seeds, the
drawn join day for the cohort; closeTs = death + 120d. Constructed
through THE one path — `join_cohort::membershipOf(pack, window)` — by
exportAll/exportEntities, the streaming twin (main), and card_fraud.

**JOIN COHORT** (`synth/personas/join.hpp`): joinerCount = population
× Σ over window days of r(year(day)) / 365.2425 (linear, declared),
r(y) from the EMBEDDED BEA population series (era_data.hpp — no new
data), RATE-CLAMPED at coverage edges (frozen years read the last
measured year-over-year rate). Joiners are the LAST K ids (seed
roster byte-stable — gated); join day = EXACTLY ONE draw per joiner
on {"join-cohort", personId}, inverse-CDF ∝ r(year(day)).
`Pack::joinDays` carries the schedule; production fills it via
`identity.windowDays` (simulate.cpp) and the blueprint fallback pack
mirrors it.

**THE AGE-AXIS REPAIR:** joiners' dob, persona timeline, and lifespan
anchor at their JOIN date (dob.hpp / timeline::deriveAll /
lifespan::derive share the per-person anchor) — a 2015 joiner draws
2015-appropriate ages, personaAt(join) == seed, and death lands
strictly after joining. Generation still emits joiners from window
start (the pre-existing joiners-generate, exporter-hides model); the
standard exporter's [joinTs, closeTs) filter is the visibility line.

**FRAUD-SCHEDULING INTERVALS:** each ring plan carries
`participantsAliveEndEpoch` = min death over its fraud + mule
participants (rings.hpp, from the timeline carrier threaded through
InjectorRingView); the injector clamps each ring's typology window
AND its camouflage window to that horizon minus 22 days
(`kRingScheduleGuardDays` — invoice's weekly lattice can spill ≤21d
past its base range; the other typologies' tail paddings already
contain their bursts). Victims exempt (deceased-account fraud is a
real typology — declared); solo/unauthorized rail exempt.

**EXPORTER LIFECYCLE:** standard customer.csv gains `closed_at`
(schema kErCustomer 2→3 columns); the visible corpus filters both
endpoints on [joinTs, closeTs); the AML/aml_txn_edges Customer status
cell flips active → closed when the corpus end reaches closeTs
(resolveEndOfWindowPersonas fills SharedContext::closedByPerson); the
card_fraud Party created_at reports the membership joinTs. AML
corpora stay FULL-WORLD (no row filter — declared); AML onboarding
dates remain the synthetic derivation (declared inconsistency,
alignment registered).

Gates: tests/test_membership.cpp (serverless 44) — join sizing vs the
BEA index, declining-growth placement skew, seed-roster byte
stability, joiner ages at join, alive-at-join, interval semantics,
the AML closure resolution, ATM/internal death stops (strict), the
subscription/premium/obligation closure stops + estate-servicing
windows, the claims death stop, card-ladder truncation, ring fraud
never recruiting the dead, and the membership filter's
pre-join-hides / zero-post-close invariant.

**Law compliance:** NO new CLI; new randomness ONLY on the
{"mortality"}, {"family","inheritance"} and {"join-cohort"} lanes;
the ATM/internal/subscription/premium/obligation stops shift NO
shared stream; rent's is the one declared shared-stream shift; card
truncation and ring clamps live on isolated lanes. MODEL-MOVING:
all four goldens recapture; everything rides the single wind-up
commit.

## Authority

U-8 (merged 2026-07-25) carries every row verified through 3b-i. The
U-8 ADDENDUM merge script (`merge_authority_h3_membership_2026_07.py`,
delivered 3c-ii; owner runs then deletes) appends the membership /
replenishment / closure / defect-fix / fraud-interval rows. No fresh
research anchor: the BEA population series is already embedded and
provenance-pinned (U-4/U-5 lineage).

## Registered upgrades (the H3 ledger)

Historical-period mortality tables; SES-differential mortality;
surfacing sex to PII with a measured ratio; survivor benefits;
SCF-anchored estate sizes; heirless-estate distribution; life-policy
death benefits (sized together with estates); per-bank
customer-acquisition series for join sizing; AML onboarding aligned
to the membership axis; external family-member deaths.
