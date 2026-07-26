# H1 nominal-scale wiring contract (macro-history-v1, step 2b)

**STATUS: DELIVERED + VERIFIED (2026-07-25) — the wiring landed per
this contract in the named step-2b round (authority section U-6,
merged by the owner; one re-pin of all four goldens). Owner decisions
taken in that round:
(1) the mortgage counterparty defect stays UNBUNDLED — the step-2b diff
moves amounts only; (2) denomination-lattice fraud amounts
(`cardTestCharge`, `giftCardScamAmount`) stay FIXED-NOMINAL (declared
CHOICE, same rationale as the $20 ATM note; the ATM/self-transfer/
invoice lattices RE-SNAP after scaling); (3) the acceptance gates below
live as the serverless `tests/test_econ_wiring.cpp` (730-day GateWorld
legs at 1991 and 2019). Discovered-in-the-read classifications recorded
in U-6: card `minPaymentDollars` $25 + `lateFee` $32 scale at the cycle
date; the liquidity $75 cash-reference floor scales at the event day;
camouflage scales with the index of the flow it mimics. GATE AMENDMENT
(first verification run): the pre-registered "deflated spend ~flat"
gate measured a HARNESS fact, not the scale — a 300-person world drains
liquidity over its second year in ANY era (measured ≈0.73 deflated y/y
at 1991). The live gate is DRIFT PARITY: the deflated year-over-year
ratio of the 1991 pair divided by the 2019 pair ∈ [0.80, 1.25] — the
structural drain cancels, only era-scale distortion can break it. True
flatness arrives with H4's macro modulation. VERIFICATION (owner,
2026-07-25): second `make test` ALL GREEN (38 serverless, both amended
gates included); default-2025 standard smoke prints exactly ONE
frozen-era notice and completes; the 1991 card-fraud smoke holds the
Payment-view fraud rate (0.1471% vs 0.14628% pre-2b) with counts −4%
(the fixed-nominal $20/$25/rack lattice minimums bind relatively
harder against 1991-scale amounts — direction-realistic, declared).**

The scale primitive (`synth/econ/nominal.hpp`: `priceScale(year)` /
`wageScale(year)` / `scaleFrozen(year)`, level-anchored at the pinned
2019 calibration year, freeze-and-declare outside 1990–2024 coverage)
landed UNWIRED in step 2a, pinned by `tests/test_econ_scale.cpp`. THIS
document is the contract for the step-2b MODEL-MOVING wiring round: it
classifies every dollar-realization surface, fixes each class's index
and anchor date, and pre-registers the invariants and gates. Wiring
NEVER precedes this contract's authority rows.

## The five semantic classes

Every dollar amount in PhantomLedger belongs to exactly one class.
"Anchor year" = the year whose scale multiplies the site's existing
draw (always: draw → scale → roundMoney → spool/emit; never reordered
around the draw, so RNG streams and entity lanes are untouched).

### W — WAGE-INDEXED (`wageScale`, realization year)

Labor income and its statutory proxies track the AWI path.

| Surface | Files (verified 2026-07-24) | Wiring shape |
|---|---|---|
| Salary paychecks | `activity/recurring/employment.hpp` (:49 `CompoundRules{.annualInflation=.025}`, :188 `growthSince` via `growth::compoundGrowth`), `activity/recurring/payroll.hpp`, `transfers/legit/routines/paychecks.hpp` | Base salary is CALIBRATION-YEAR dollars. nominal(t) = base × wageScale(year(t)) × Π(1+realRaise) since contract start. The constant `.025` inflation term RETIRES (the index IS the inflation+economy-wide path); the seeded `salary_real_raise` lanes SURVIVE as idiosyncratic career progression on top — declared CHOICE (population mean drifts (1+μ)^tenure above pure AWI; the aggregate gate band allows it) |
| Freelancer / business revenue | `activity/income/revenue/{draw,flows,generate}.hpp` | draws × wageScale(realization year) — labor-income axis, CHOICE |
| SSA / government benefit levels | `transfers/channels/government/{retirement,disability,recipients,monthly_deposit_emitter}.hpp` | level draws × wageScale(realization year). CHOICE with stated deviation: real SSA wage-indexes at award then CPI-COLAs per cohort; one index era-wide is the H1 simplification (per-cohort indexing = H2/H3 refinement when personas retire IN-model) |

### P — PRICE-INDEXED (`priceScale`, realization year)

Consumption and contract prices track the CPI-U path.

| Surface | Files | Wiring shape |
|---|---|---|
| Rent | `activity/recurring/lease.hpp` (:42 `.025`, :200–204) | same shape as salary: base × priceScale(year(t)) × real-raise lanes (`rent_real_raise` survives); `.025` retires |
| Spending session tickets | `transfers/legit/routines/spending/behavior.hpp`, `transfers/channels/credit_cards/detail/session.hpp` | ticket draw × priceScale(event year) |
| Subscriptions | `transfers/channels/subscriptions/{prices,bundle,debits,schedule}.hpp` (`kPricePool` 18 fixed prices) | pool price × priceScale(DEBIT year) — subscription prices track the era level (CHOICE; per-contract fixed pricing would freeze 1991 prices for decades) |
| Insurance premiums + claims | `transfers/channels/insurance/{rates,premiums,claims}.hpp` | draw × priceScale(realization year) |
| Family routines | `transfers/legit/routines/family/*.hpp` (support, tuition, gifts, allowances, siblings, spouse) + `inheritance.hpp` (`InheritanceEvent` median $25k) | draw × priceScale(event year). Inheritance scales here as an interim fix; H3 retires the hazard for death-caused estates and re-anchors size |
| ATM / cash | `transfers/legit/routines/atm.hpp` | draw × priceScale(event year), THEN the existing denomination rounding (a 1991 withdrawal is fewer $20s, not scaled $20s) |
| Card statements / payments | `transfers/channels/credit_cards/{statement,cycle,payment}.hpp` | FLOW-THROUGH — no direct scaling; they aggregate already-scaled purchases. WIRING-READ FINDING: two independent dollar constants DID hide here — `BillingTerms.minPaymentDollars` ($25) and `lateFee` ($32) — both behavioral, both realized at the cycle date's priceScale (session.cpp per-cycle effective terms); the $0.01 billable-interest de-minimis stays fixed |
| Account opening balances; credit limits | `synth/accounts/*`, `transfers/legit/ledger/limits.hpp` | STOCKS: × priceScale(WINDOW-START year) once at world build; they then evolve through scaled flows, keeping liquidity/utilization ratios coherent era-wide |

### D — ORIGINATION-ANCHORED DEBT (`priceScale` at origination year, fixed nominal after)

Real loans are nominal contracts: the payment fixed at origination
does not track later inflation. Principal draws scale by the
ORIGINATION year; the installment math then runs unchanged.

| Surface | Files |
|---|---|
| Mortgage / auto / student principals + installments | `synth/products/sampling/amounts.hpp`, `synth/products/terms/{mortgage,auto_loan,student_loan}.hpp`, `synth/products/installments.hpp`, `transfers/channels/obligations/*` |
| Origination years BEFORE 1990 (backdated contracts in an early window) | clamp to 1990 per freeze-and-declare — declared, deterministic |
| Tax | `synth/products/terms/tax.hpp` | priceScale(realization year) — IRL brackets index annually; CHOICE |

### S — STATUTORY FIXED-NOMINAL (NO scaling — deliberately)

The BSA/CTR $10,000 threshold has been UNINDEXED since the 1970s;
holding it fixed while everything else scales is HISTORICALLY CORRECT
(in 1991 the threshold bit at ~2x today's real value — a true fact of
the era, and a realism *feature*).

| Surface | Files | Rule |
|---|---|---|
| Structuring amounts (≤$9,950 band) + CTR filing threshold | `transfers/fraud/typologies/structuring.hpp`, CTR logic | UNCHANGED — threshold-anchored, not price-anchored |
| ATM note denominations ($20) | rounding rules | UNCHANGED |
| Card-test anchors + gift-card rack denominations | `transfers/fraud/typologies/amounts.hpp` (`cardTestCharge`, `giftCardScamAmount`) | UNCHANGED (owner-approved 2026-07-25) — round probe amounts and rack denominations are physical artifacts; the round-amount signature IS the typology |

### F — FRAUD (`priceScale`, realization year — EXCEPT class S)

Fraud steals era dollars: `transfers/fraud/typologies/amounts.hpp`
(medians $79/σ1.2, $180/σ1.5) × priceScale(event year); every
typology EXCEPT structuring (and the class-S denomination lattices
above). Chain math (haircuts, splits, floors) runs in CALIBRATION
dollars; the scale applies once at each Draft's emission
(`typologies::nominalAt`), so behavioral floors bind identically in
every era. The continuous samplers scale median AND clamps together —
the scaled clamp bounds land on sub-cent values and the samplers round
to cents, so band checks against `bound × scale` need a one-cent
tolerance (test_fraud_amounts lesson). Camouflage scales with the
index of the flow it MIMICS (bills/p2p → priceScale; the salary mimic
→ wageScale). The 0.11675% calibration target is a COUNT rate —
unaffected by amount scaling; the funnel's dollar floors scale with
their amounts, keeping funnel geometry scale-invariant.

## Cross-cutting invariants (pre-registered for step 2b)

1. **Draw order untouched:** scale multiplies AFTER the site's
   existing draw — RNG streams, lanes, and entity ordering are
   byte-identical to today; ONLY amounts move. No new draws, no new
   lanes, no new CLI.
2. **Spool/replay:** scaling happens BEFORE candidate rows spool, so
   spooled bytes are final nominal amounts — replay and resume stay
   exact.
3. **Oracle parity:** both engines share the channel code, so
   windowed == monolith holds automatically; test_arch_equivalence /
   test_production_windowed must stay green UNCHANGED (they compare
   engines to each other, not to baselines).
4. **Dollar-literal screens:** every in-model comparison of an amount
   against a dollar constant must be classified in the wiring read
   (statutory → fixed; behavioral 2019-calibrated → scale the
   threshold with the same index as the amounts it screens). Grep
   surface: liquidity cutoffs, delinquency triggers, fraud funnel
   bands, CTR (fixed). EXECUTED: paycheck $50 min (wage), revenue rule
   floors (wage), ATM reserve $40–$120 (price), liquidity
   kCashRefFloor $75 (price, per day), card $25/$32 (price, per
   cycle), funnel floors $50/$5 (price); CTR/structuring/$0.01/$1
   de-minimis fixed.
5. **Freeze-and-declare notice:** the app layer prints ONE stderr
   notice when a run's window touches frozen years (uses
   `scaleFrozen`); deterministic, not part of the corpus stream.
   EXECUTED: `app::frozenEraNotice` (options.hpp) + main.cpp, pinned
   by test_app_options; VERIFIED LIVE on the default-2025 smoke.
6. **ONE named re-pin:** all four goldens move together in the step-2b
   commit; internal fixtures only (public corpus unchanged until the
   owner's regeneration decision).

## Acceptance gates (step 2b) — LIVE as tests/test_econ_wiring.cpp

* Aggregate nominal consumer spend per active person: 2019-window /
  1991-window ratio inside a CPI band (≈1.88x ± tolerance).
* Aggregate salary level ratio inside an AWI band (≈2.48x ± the
  declared idiosyncratic-drift allowance).
* A calibration-year (2019) window reproduces today's magnitudes
  (scale ≡ 1.0 — realized as the exact kPricePool-verbatim
  subscription check on 2019-dated debits).
* Structuring rows: band [., 9950] intact in an 1991 window (class S
  untouched), while non-structuring fraud amounts scale.
* Frozen-year run (e.g. standard 2025 default) completes with the
  declared notice and 2024-frozen scales (notice pinned by
  test_app_options; the smoke run verified live 2026-07-25).
* DRIFT PARITY (amended from "deflated spend ~flat" after the first
  verification run): deflated y/y spend falls ~27% in year 2 of ANY
  300-person leg — a harness liquidity-drain fact independent of era.
  The live gate divides the 1991 year-pair ratio by the 2019 year-pair
  ratio (∈ [0.80, 1.25]); the drain cancels, era-scale distortion
  would not. Corpus-level flatness is H4's deliverable, not H1's.
* ADDED in the round: ATM amounts are positive multiples of $20 in
  BOTH eras (denomination re-snap preservation).

## Verified corpus-level outcome (owner smokes, 2026-07-25)

Card-fraud 2k/60d/seed-7 at 1991-01-01, fresh database: 137,825
ledger rows / 78,883 Payment-view rows / 116 flagged (0.1471%) vs the
pre-2b 143,336 / 82,720 / 121 (0.14628%). COUNTS drifted −4%: the
class-S fixed-nominal minimums ($20 notes, $25 self-transfer snap,
rack denominations) bind relatively harder against 1991-scale amounts
and balances, so affordability screens reject slightly more candidates
— direction-realistic and declared. The Payment-view fraud RATE (the
COUNT-rate calibration axis) held. These are the post-2b smoke
reference figures.

## Authority

Step 2b opens with a U-6 merge script adding classed rows: the W/P/D/
S/F classification (each a CHOICE with stated deviation where noted),
the statutory-threshold MEASUREMENT row (BSA $10k unindexed), and the
retirement of the `.025` constants. Wiring lands only after those rows.
EXECUTED: `merge_authority_nominal_wiring_2026_07.py` (owner ran it
2026-07-25; delete the script if it still exists).
