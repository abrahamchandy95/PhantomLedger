# H2 persona-timeline contract (macro-history-v1)

**STATUS: STEP 1 VERIFIED (2026-07-25, U-7 merged); STEP 2a VERIFIED
(single age axis; goldens recaptured); STEP 2b GATES GREEN
(test_persona_wiring passed after the payroll era fix + the retiree-
revenue test correction; full-suite verification folds into the 2c
re-pin); STEP 2c DELIVERED (owner verification pending) — wiring items
6-7 executed: AML end-of-window persona, the retirement spending step,
and the payday re-anchor pinned as a code-fact. MODEL-MOVING: the four
goldens recapture with the 2b+2c re-pin (one recapture covers both).
Owner decisions 2026-07-25: staged delivery; single age axis; AML
Customer = end-of-window persona; wiring includes the spending step.**

## ERA-AXIS PAYROLL DEFECT (pre-existing; found by the 2b gates; FIXED)

`paydatesForProfile` (activity/recurring/payroll.hpp) treated the
payroll anchor — the fixed 2025-01-01 weekday/parity reference set in
`samplePayrollProfile` — as a START BOUND: the weekly branch began at
`max(start, anchorDate)` and the biweekly branch only stepped FORWARD
from the anchor. Result: in every window BEFORE the anchor year,
weekly + biweekly employer profiles (75% of the cadence mix) emitted
ZERO paydates. This predates H2 entirely and is visible in the
historical smokes: at the same 2,000 population the 2025-start
standard run carried `income screened=13590` while the 1991-start run
carried `6394` — pre-2025 eras have been running on roughly HALF
their income (monthly + semimonthly only) since the era lock moved
runs in-era. The H1 econ gates could not see it because both of their
legs (1991, 2019) sat before the anchor and were equally starved, so
every RATIO held.

FIX (payroll.hpp): the lattices are now era-agnostic — weekly pays on
its weekday in every week; biweekly aligns the window's first
on-weekday date to the anchor's FORTNIGHT PARITY (the anchor is a
parity reference only, valid in both directions). For windows at or
after the anchor the emitted dates are UNCHANGED (backward-compatible
with the 2025-start standard golden config). Pre-2025-era corpora gain
the missing 75% of payroll — 1991-era income roughly DOUBLES, with
downstream spending following; the fraud COUNT rate is count-targeted
and holds. This lands inside the step-2b re-pin. The 2025 anchor
constant itself is retained as a documented parity reference (a fixed
lattice phase, not a present-time model anchor).

THE DEFECT THIS ARC STAGE CLOSES: personas are STATIC. The seed
assignment (shares salaried .60 / student .12 / retiree .10 /
freelancer .10 / smallBusiness .06 / HNW .02) is sampled once and
never changes, so a 29-year canonical window carries a 29-year
student cohort, retirees seeded 65-99 who would reach 94-128, and
nobody ever stops working or starts drawing Social Security
mid-corpus.

## The timeline model (step 1, VERIFIED)

`timeline::derive(factory, {person, seed, dob, simStart})` draws
EXACTLY EIGHT values, unconditionally, in a fixed documented order, on
the isolated `{"persona-era", personId}` lane, and returns transition
DATES anchored to the person's BIRTH DATE. `personaAt(timeline, date)`
is the pure persona-AT-DATE.

| Seed | Transitions |
|---|---|
| student | -> working (salaried .85 / freelancer .15) at a work-start age drawn over 19-28 (mass 22-26), then -> retiree at the claiming date |
| salaried, freelancer | -> retiree at an SSA-claiming-shaped date (freelancers are SECA-covered) |
| smallBusiness | -> working (salaried .70 / freelancer .30) when the business ends — memoryless exponential residual lifetime, median 5 years (BLS BED ~50% five-year survival); retirement DOMINATES |
| retiree | none — the claim date backdates (clamped <= simStart) |
| highNetWorth | NONE at H2 — declared exemption |

**Claiming-age mixture:** `.30 at 62 | .10 uniform [63y, FRA) | .45 at
FRA | .05 uniform (FRA, 70y) | .10 at 70` + 0-60d birthday jitter; FRA
= the exact 1983-Amendments schedule (`timeline::fraMonths`).
Per-cohort claiming shares = registered upgrade.

**Seed-consistency invariant (pinned):** personaAt(simStart) == seed;
clamps bind ONLY on past dates. **Monotone irreversibility (pinned):**
student -> working -> retiree, never backwards.

## The wiring (2a/2b/2c EXECUTED)

1. **Age carrier — EXECUTED (step 2a).** `Pack::birthDates` on
   isolated {"dob", personId} lanes; PII renders FROM the carrier; SSA
   cohorts from the REAL birth day-of-month (`syntheticBirthDay`
   retired). Gates: test_dob_carrier.
2. **Timeline carrier — EXECUTED (step 2b).** `Pack::timelines` =
   `timeline::deriveAll(...)`; filled in buildPersonas + the blueprint
   fallback; rides the pack into the fold (both engines).
3. **Salary — EXECUTED (step 2b).** Selection keys on the WORKING-LIFE
   type (`probabilityFor(tl.working)`); `paidFraction` re-derived .65
   -> .74 (working-type weighted mean; arithmetic in salary.hpp);
   `Paymaster::pay` clips spans to [max(windowStart, payrollStart(tl)),
   min(windowEnd, tl.retirement)) — seed retirees return before the
   salary-level draw, students anchor at workStart, post-close owners
   at businessEnd. Study-period student jobs OUT OF SCOPE (declared);
   student->freelancer destinations get payroll at .08 and no revenue
   plan (declared ~1.8% gap, H3).
4. **SSA benefits — EXECUTED (step 2b).** Timeline-mode selection:
   RETIRED BY WINDOW END, eligibleP .87 kept, `Recipient.onset` =
   max(window start, claiming date); deposits skip pre-onset months.
   Disability persona-static (declared). Benefit LEVEL stays the
   one-shot draw (earnings-history benefits = registered upgrade).
5. **Revenue — EXECUTED (step 2b).** Months emit only while
   personaAt(monthStart) == the plan's seed persona: student plans
   stop at workStart, worker plans at the claiming date, business
   plans at the close; RETIREE and HNW plans are perpetual (their
   persona never transitions away — the first gate run's "violations"
   were this legitimate retiree revenue, and the TEST was corrected,
   not the model). Each month is its own content-keyed lane.
6. **AML Customer export — EXECUTED (step 2c).**
   `resolveEndOfWindowPersonas` (exporter/aml/vertices.hpp, inline,
   gate-testable): both streaming sinks (aml + aml_txn_edges) call it
   from `takeArtifacts()`, resolving `ctx.personaByPerson` to
   `personaAt(lastTs)` where `lastTs` is the corpus MAXIMUM timestamp
   — accumulated in `append()` from the replay-sorted stream's final
   row, exactly as `firstTs`/simStart is derived from its first row.
   One seam, two engines, no window threading through configs. Empty
   streams and packs without the timeline lane leave the seed
   assignment (the pre-2c behavior). ONLY the Customer-table bytes
   move (persona feeds customer type / demographic / occupation
   cells); the golden_tables_aml re-pin absorbs it.
7. **Retirement spending step — EXECUTED (step 2c).** From the
   claiming day, spending-session ticket draws scale by
   `actors::kRetiredSpendScale = 0.88` (Aguiar-Hurst ~-12%, U-7)
   through the SAME seam as the H1 day-frame priceScale: the census
   carries per-person retirement day-indices
   (`Census::retirementDays`, computed by the transfers layer from
   the BLUEPRINT pack's timeline lane — identical on both engines,
   unlike the homeAreas carrier there is no empty-on-oracle mode) ->
   population View -> `Spender::retireDay` -> the emission loop sets
   `Event.consumptionScale` per spender-day (pure derived data, NO
   draws) -> the payment router multiplies it into the bill /
   external / p2p / merchant draws alongside priceScale.
   **SEED-RETIREE + HNW EXEMPTION (declared):** a seed retiree's
   archetype already encodes retired-calibrated spending (rate x0.6 /
   amount x0.9 in the audited persona table) — stacking the step
   would double-count; the step models the IN-WINDOW transition of
   working-archetype spenders (whose archetype swap is explicitly NOT
   at H2). **PAYDAY RE-ANCHOR (code-fact, pinned not written):**
   `buildPaydaysByPerson` screens by `isPaydayInbound`, which admits
   gov_social_security/pension/disability — so a mid-window retiree's
   liquidity relief/stress cycle re-anchors from salary days to SSA
   deposit days automatically once the 2b income switch moves their
   inbound stream. Zero new code; pinned in test_persona_wiring.
8. **Family/tuition and card issuance** stay seed-based at H2.

**Step-2b gates (LIVE, tests/test_persona_wiring.cpp; diagnostics
retained):** income-only 300-person GateWorld, 4 years at 1991 — seed
retirees emit ZERO paychecks; no paycheck after the claiming date
(+10d); no student paycheck before workStart (>=1 in-window career
start); no SSA deposit before max(window start, claim); in-window
retirees exist, >=1 draws deposits, >=1 full worked->retired->
deposited arc; revenue never outlives its persona gate (retiree/HNW
exempt; +45d month-granularity grace); >=1 closed-business owner
takes a job.

**Step-2c gates (LIVE, same test):** (A) the end-of-window resolver
echoes personaAt(corpus end) exactly, covers at least the in-window
retiree cohort, and leaves the assignment on empty streams /
carrier-less packs; (B) every post-claim payday of a full-arc,
revenue-free retiree IS a government deposit day (>=1 validated); (C)
a 300-person 3-year spending leg (base routines + the real simulator):
the interior in-window retiree cohort's mean ticket steps DOWN vs a
non-retiring salaried control, difference-in-differences around each
claiming day in (0.60, 0.97) — generous band because the 0.88 level
factor compounds with the SSA-income liquidity response.

**Law compliance:** NO new CLI; new randomness ONLY on
`{"persona-era"}`/`{"dob"}` lanes; 2c adds ZERO draws (the step and
the resolver are pure derived data — RNG streams and entity order are
byte-identical; only post-claim ticket AMOUNTS and aml Customer bytes
move); selection draws one coin per candidate; `transactions/` never
includes synth.

## Authority

U-7 (merged 2026-07-25) carries the classed rows (FRA MEASUREMENT;
claiming mixture CHOICE; student work-start CHOICE; business hazard
TYPOLOGY on the BED anchor; HNW exemption; seed-consistency clamps;
single age axis + real-birth-day cohorts; end-of-window AML persona;
retirement spending step; timeline-consequent selection with the
paidFraction re-derivation — executed .65 -> .74). THE PAYROLL
ERA-AXIS DEFECT FIX is a correctness repair on the U-7 lineage
(code-fact; the cadence mix and anchor constants are unchanged — only
the lattice's era coverage was repaired). The H2 arc-close merge
script (`merge_authority_h2_close_2026_07.py`) appends the U-7
addendum: the payroll fix note, the seed-retiree/HNW exemption on the
spending step, and the payday re-anchor code-fact.
