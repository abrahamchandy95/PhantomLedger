# H2 persona-timeline contract (macro-history-v1)

Status: delivered in stages, 2026-07-25. Step 1 (U-7 merged) and step 2a
(single age axis) verified; step 2b gates green after the payroll fix and
a corrected retiree-revenue test; step 2c added the end-of-window AML
persona and the retirement spending step. One recapture of the four
goldens covers 2b and 2c. Authority: U-7, now
[Persona timeline, mortality and membership](fraud_model_audit.md#m-5-persona-timeline-mortality-and-membership).
Owner decisions: staged delivery; single age axis; the AML Customer shows
the end-of-window persona; wiring includes the spending step.

The defect closed: personas were static. Seed shares (salaried .60,
student .12, retiree .10, freelancer .10, smallBusiness .06, HNW .02)
never changed, so a 29-year window kept a 29-year student cohort, retirees
seeded at 65-99 reached 94-128, and nobody retired mid-corpus.

## Payroll era-axis defect (pre-existing, fixed)

`paydatesForProfile` (`activity/recurring/payroll.hpp`) treated the fixed
2025-01-01 anchor from `samplePayrollProfile` as a start bound, so windows
before 2025 emitted no weekly or biweekly paydates (75% of the cadence
mix). At 2,000 people, `income screened` was 13590 for a 2025 start and
6394 for 1991. The H1 econ gates missed it because both of their legs
(1991, 2019) sat before the anchor and were equally starved, so every
RATIO held.

Fix: weekly pays its weekday every week; biweekly aligns to the anchor's
fortnight parity in both directions. Dates from 2025 on are unchanged
(2025 golden config unaffected). 1991-era income roughly doubles, spending
follows, the fraud count rate holds. The 2025 constant stays as a parity
reference. Landed in the 2b re-pin.

## The timeline model (step 1)

`timeline::derive(factory, {person, seed, dob, simStart})` draws exactly
eight values in a fixed order on the isolated `{"persona-era", personId}`
lane and returns transition dates anchored to the birth date.
`personaAt(timeline, date)` is the pure persona at a date.

| Seed | Transitions |
|---|---|
| student | working (salaried .85 / freelancer .15) at age 19-28 (mass 22-26), then retiree at claiming |
| salaried, freelancer | retiree at an SSA-claiming date (freelancers are SECA-covered) |
| smallBusiness | working (salaried .70 / freelancer .30) at close: exponential, median 5 years (BLS BED, about 50% five-year survival); retirement dominates |
| retiree | none; claim date backdates (≤ simStart) |
| highNetWorth | none (declared exemption) |

Claiming age: .30 at 62, .10 uniform [63, FRA), .45 at FRA, .05 uniform
(FRA, 70), .10 at 70, plus 0-60 days jitter. FRA follows the 1983
Amendments (`timeline::fraMonths`). Per-cohort shares: registered upgrade.
Pinned: `personaAt(simStart) == seed` (clamps bind only on past dates);
student, working, retiree, never backwards.

## The wiring (steps 2a to 2c)

1. Age (2a): `Pack::birthDates` on `{"dob", personId}` lanes; PII renders
   from it; SSA cohorts use the real birth day (`syntheticBirthDay`
   retired). Gate: test_dob_carrier.
2. Timelines (2b): `Pack::timelines = timeline::deriveAll(...)`, filled in
   buildPersonas and the blueprint fallback, on both engines.
3. Salary (2b): selection keys on `probabilityFor(tl.working)`;
   `paidFraction` .65 to .74 (working-type mean, see salary.hpp).
   `Paymaster::pay` clips to [max(windowStart, payrollStart(tl)),
   min(windowEnd, tl.retirement)): seed retirees skip the salary draw,
   students start at workStart, former owners at businessEnd. Declared gaps: no study-period student
   jobs; student-to-freelancer gets payroll at .08 and no revenue plan
   (about 1.8%).
4. SSA (2b): retired by window end, eligibleP .87; `Recipient.onset` =
   max(window start, claim). Disability stays static; the level stays a
   one-shot draw (earnings-history benefits registered).
5. Revenue (2b): a month emits only while `personaAt(monthStart)` is the
   plan's seed persona; retiree and HNW plans are perpetual (the first gate
   run wrongly flagged them; the test was fixed). One content-keyed lane
   per month.
6. AML Customer (2c): `resolveEndOfWindowPersonas`
   (`exporter/aml/vertices.hpp`), called by the aml and aml_txn_edges
   sinks from `takeArtifacts()`, sets `ctx.personaByPerson` to
   `personaAt(lastTs)`, the corpus maximum timestamp taken in `append()`.
   Empty streams and timeline-less packs keep the seed. Only Customer
   bytes (type, demographic, occupation) move; the golden_tables_aml re-pin absorbs it.
7. Retirement spending (2c): from the claiming day, session tickets scale
   by `actors::kRetiredSpendScale = 0.88` (Aguiar-Hurst, about -12%) via
   the H1 priceScale seam: `Census::retirementDays` (from the blueprint
   timeline, same on both engines), `Spender::retireDay`,
   `Event.consumptionScale` per spender-day (no draws), then the payment
   router's bill, external, p2p and merchant draws. Seed retirees and HNW
   are exempt: their archetype already encodes retired spending (rate ×0.6,
   amount ×0.9). Payday re-anchor needs no code: `buildPaydaysByPerson`
   screens with `isPaydayInbound`, which admits gov_social_security,
   pension and disability, so a new retiree's liquidity cycle follows SSA
   deposits (pinned in test_persona_wiring).
8. Family/tuition and card issuance stay seed-based.

## Gates (`tests/test_persona_wiring.cpp`)

Step 2b (income-only, 300 people, 4 years at 1991): seed retirees get no
paychecks; none after claiming (+10d) or before a student's workStart; no
SSA deposit before max(window start, claim); at least one in-window
career start, one deposit-drawing retiree, one full worked-retired-paid
arc, and one closed-business owner taking a job; revenue never outlives
its persona (retiree/HNW exempt, +45d grace).

Step 2c: the resolver returns `personaAt(corpus end)`, covers the
in-window retirees, and keeps the seed when it has no data; every post-claim payday of a full-arc, revenue-free
retiree is a government deposit day; over 300 people and 3 years, the
in-window retirees' mean ticket falls against a salaried control, with a
difference-in-differences ratio in (0.60, 0.97). The band is wide because
0.88 compounds with the SSA-income liquidity response.

## Rules kept

No new CLI. New randomness only on `{"persona-era"}` and `{"dob"}`. Step
2c adds no draws (only post-claim ticket amounts and aml Customer bytes
move). Selection draws one coin per candidate. `transactions/` never
includes synth.

## Authority

U-7 (merged 2026-07-25) classes: FRA (MEASUREMENT); claiming mixture and
student work-start (CHOICE); business hazard (TYPOLOGY on BED); the HNW
exemption; seed-consistency clamps; single age axis; end-of-window AML
persona; the spending step; timeline selection with paidFraction .74. The
payroll fix is a correctness repair (cadence mix and anchor unchanged).
`merge_authority_h2_close_2026_07.py`, no longer in the repository,
appended the payroll note, the spending-step exemption and the payday
re-anchor.
