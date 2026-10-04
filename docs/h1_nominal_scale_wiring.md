# H1 nominal-scale wiring contract (macro-history-v1, step 2b)

Status: delivered and verified 2026-07-25, one re-pin of all four goldens.
Authority: U-6, now
[Nominal-scale wiring classes](fraud_model_audit.md#m-3-nominal-scale-wiring-classes).

`synth/econ/nominal.hpp` (`priceScale`, `wageScale`, `scaleFrozen`) is
level-anchored at the 2019 calibration year and freezes outside 1990-2024
(freeze-and-declare). Step 2a landed it unwired, pinned by
`tests/test_econ_scale.cpp`. This contract classifies every dollar
surface and pre-registers the invariants and gates; wiring never precedes
its authority rows. Owner decision: the mortgage counterparty defect stays
unbundled (step 2b moves amounts only).

## The five semantic classes

Every dollar amount is in exactly one class. Order is always draw, scale,
`roundMoney`, spool/emit, so RNG streams and lanes are untouched.

### W: wage-indexed (`wageScale`, realization year)

| Surface | Files (verified 2026-07-24) | Wiring |
|---|---|---|
| Salary | `activity/recurring/{employment,payroll}.hpp`, `transfers/legit/routines/paychecks.hpp` | base (2019 dollars) × wageScale × Π(1+realRaise) since contract start. The `.025` `annualInflation` constant retires; `salary_real_raise` lanes survive as career progression (CHOICE: mean drifts (1+μ)^tenure above AWI, inside the gate band) |
| Freelancer / business revenue | `activity/income/revenue/{draw,flows,generate}.hpp` | draw × wageScale; CHOICE |
| SSA / government benefits | `transfers/channels/government/{retirement,disability,recipients,monthly_deposit_emitter}.hpp` | level × wageScale. CHOICE with deviation: real SSA wage-indexes at award, then CPI-COLAs per cohort (per-cohort indexing: an H2/H3 refinement) |

### P: price-indexed (`priceScale`, realization year)

| Surface | Files | Wiring |
|---|---|---|
| Rent | `activity/recurring/lease.hpp` | as salary, with `rent_real_raise`; `.025` retires |
| Session tickets | `transfers/legit/routines/spending/behavior.hpp`, `transfers/channels/credit_cards/detail/session.hpp` | × priceScale(event year) |
| Subscriptions | `transfers/channels/subscriptions/{prices,bundle,debits,schedule}.hpp` (`kPricePool`, 18 prices) | × priceScale(debit year). CHOICE: per-contract pricing would freeze 1991 prices for decades |
| Insurance premiums, claims | `transfers/channels/insurance/{rates,premiums,claims}.hpp` | × priceScale |
| Family routines | `transfers/legit/routines/family/*.hpp`, `inheritance.hpp` (median $25k) | × priceScale(event year); inheritance interim until H3 |
| ATM / cash | `transfers/legit/routines/atm.hpp` | × priceScale, then denomination rounding (a 1991 withdrawal is fewer $20s) |
| Card statements | `transfers/channels/credit_cards/{statement,cycle,payment}.hpp` | flow-through. `minPaymentDollars` $25 and `lateFee` $32 scale at the cycle date (session.cpp); $0.01 interest de-minimis fixed |
| Opening balances, credit limits | `synth/accounts/*`, `transfers/legit/ledger/limits.hpp` | stocks: × priceScale(window-start year) once at build; scaled flows keep liquidity and utilization ratios coherent |

### D: origination-anchored debt

Principals scale by priceScale(origination year), then stay fixed nominal:
`synth/products/sampling/amounts.hpp`,
`synth/products/terms/{mortgage,auto_loan,student_loan}.hpp`,
`synth/products/installments.hpp`, `transfers/channels/obligations/*`.
Originations before 1990 clamp to 1990. Tax (`synth/products/terms/tax.hpp`)
uses priceScale(realization year), since brackets index annually (CHOICE).

### S: statutory fixed-nominal (no scaling)

- Structuring (≤$9,950 band, `transfers/fraud/typologies/structuring.hpp`)
  and the BSA/CTR $10,000 threshold, unindexed since the 1970s. In 1991 it
  bit at about 2x today's real value: historically correct.
- The $20 ATM note.
- `cardTestCharge` anchors and `giftCardScamAmount` rack denominations
  (`transfers/fraud/typologies/amounts.hpp`; owner-approved 2026-07-25):
  the round amount is the typology. ATM, self-transfer and invoice
  lattices re-snap after scaling.

### F: fraud (`priceScale`, event year)

- Medians in `transfers/fraud/typologies/amounts.hpp` ($79 σ1.2, $180
  σ1.5) scale for every typology except class S.
- Chain math (haircuts, splits, floors) runs in 2019 dollars; scale
  applies once per Draft at emission (`typologies::nominalAt`), so
  behavioral floors bind identically in every era.
- Samplers scale median and clamps together; scaled bounds land on
  sub-cents, so band checks need a one-cent tolerance (test_fraud_amounts).
- Camouflage uses the index of the flow it mimics (bills, p2p: price;
  salary: wage).
- The prevalence target (0.11675% when H1 landed) is a count rate;
  funnel floors scale with amounts, so funnel geometry is scale-invariant.

## Invariants

1. Scale multiplies after the existing draw: no new draws, lanes or CLI.
2. Scaling precedes spooling, so replay and resume stay exact.
3. Engines share channel code: test_arch_equivalence and
   test_production_windowed stay green unchanged.
4. Every dollar-literal screen is classified: statutory fixed, behavioral
   scales with what it screens. Done: paycheck $50 minimum and revenue
   floors (wage); ATM reserve $40-$120, `kCashRefFloor` $75 per day, card
   $25/$32 per cycle, funnel floors $50/$5 (price). CTR, structuring,
   $0.01 and $1 de-minimis fixed.
5. One deterministic stderr notice, outside the corpus stream, when a
   window touches frozen years (`app::frozenEraNotice` in options.hpp,
   called from main.cpp; pinned by test_app_options).
6. One named re-pin of all four goldens (internal fixtures only; the
   public corpus waits for the owner's regeneration decision).

## Acceptance gates

Serverless `tests/test_econ_wiring.cpp`, 730-day GateWorld legs at 1991
and 2019.

- Spend per active person, 2019 over 1991: CPI band (about 1.88x).
- Salary ratio: AWI band (about 2.48x) plus the real-raise drift allowance.
- 2019 window: every subscription debit is a verbatim `kPricePool` price.
- Structuring stays in [., 9950] at 1991; other fraud amounts scale.
- A frozen-year run (the 2025 default) completes with the notice and
  2024-frozen scales.
- ATM amounts are positive multiples of $20 in both eras.
- Drift parity: the pre-registered "deflated spend flat" gate measured a
  harness fact (any 300-person world drains liquidity in year two, about
  0.73 deflated y/y at 1991). The live gate is the 1991 y/y ratio over the
  2019 one, inside [0.80, 1.25]: the drain cancels, era-scale distortion
  would not. Flatness is H4's job; H4 later restated this gate in
  calibration-level units ([the harness drain fact](h4_macro_modulation.md#the-harness-drain-fact-analysis-not-a-fix)).

## Verified outcome (owner, 2026-07-25)

`make test` all green (38 serverless). The default-2025 smoke prints one
frozen-era notice. Card-fraud 2k/60d/seed-7 at 1991-01-01: 137,825 ledger
rows, 78,883 Payment-view rows, 116 flagged (0.1471%), against pre-2b
143,336 / 82,720 / 121 (0.14628%). Counts fell 4% because fixed-nominal
minimums ($20 notes, $25 self-transfer snap, rack denominations) bind
harder at 1991 scale; the fraud rate held. Later rounds moved these figures.

## Authority

The owner ran `merge_authority_nominal_wiring_2026_07.py` (no longer in the
repository) on 2026-07-25 before wiring. It added the W/P/D/S/F rows (each
a CHOICE), the BSA $10k unindexed MEASUREMENT row, and the `.025`
retirement.
