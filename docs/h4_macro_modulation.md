# H4 macro modulation contract (macro-history-v1)

Status: closed 2026-07-26, delivered and verified. The owner adopted all
four decisions below. Step 1 verified (U-9 merged, realPceLevel(1991) =
0.668); step 2 verified (RateSampler seam, all gates green, all four
goldens re-pinned). Authority: U-9, now
[Macro modulation](fraud_model_audit.md#m-4-macro-modulation-the-real-consumption-level).
Declared deviations: the 2008-09 recession leg is subsumed (see the gates),
and the README sweep was deferred to the H5 arc-close round.

Golden effect: the default standard golden grew 184,988 to 197,245 rows
(+6.63%). Its window starts in 2025, beyond coverage, so it runs at the
frozen 2024 level, realPceLevel(2024) = 1.0915; +9.15% on the
session-routed share gives +6.6%. The 1991-start fraud and card-fraud
corpora shrank, as designed.

## The defect closed

H1 made dollars era-correct and H3 made people age and die, but a 1991
person still transacted as often, and bought as much in real terms, as a
2019 person. Over [1991, 2020) nominal per-capita consumption grew about
2.9x and prices about 1.88x, so real consumption grew about 1.5x, with
dips in 1991 and 2008-09 and the COVID collapse and rebound (2020-21). The
2001 recession slowed growth without a per-capita dip; the model inherits
the series as measured.

## Inputs: already embedded

`synth/econ/era_data.hpp` (U-4/U-5 lineage, pinned by test_econ_catalog,
exported as `econ.macro_annual`) holds `pcePerCapitaDollars` (BEA A794RC,
nominal), `cpiU`, `unemploymentRatePct` (U-3 annual), `recessionMonths`
(NBER months per year) and `populationThousands`. No new data.

## The model

Two pure level functions in `synth/econ/nominal.hpp`, exactly 1.0 at the
calibration year, freeze-and-declare outside coverage:

    pceScale(year)      = pcePerCapita(year) / pcePerCapita(calibrationYear)
    realPceLevel(year)  = pceScale(year) / priceScale(year)

realPceLevel is the real per-capita consumption index: about 0.67 at 1991,
1.0 at 2019, about 1.09 at the frozen 2024 level.

Decision 1, the channel: real consumption moves counts, not amounts. The
session's per-day rate is multiplied by realPceLevel(day's year); tickets
stay calibration draw × priceScale.

- The U-6 denomination law is untouched.
- Per-capita payment counts grew on this order (Fed Payments Study
  lineage); ticket medians are calibration-anchored.
- The fraud budget F = pL/(1-p) rides the candidate count L, so fraud
  density stays proportional with no fraud-side wiring. Verified: the
  injector's illicit and transaction-fraud budgets chain off
  `realizedBaseCount`, the realized legit row count.
- A mixed count/amount split is registered; it needs a real-quantity
  decomposition and there is no 1991 ticket-size anchor.

The seam: `RateSampler` computes `dayPriceScale_` once per day frame on
both engines. Step 2 adds `dayRealLevel_ = realPceLevel(year(frame.day.start))`
and multiplies it into `combinedMultiplierFor()` beside
`frame_.seasonalMult`. No draws, lanes or CLI; only sampled counts (rows,
L, ledger trajectory) change. The window budget (`PreparedRun::Budget`
targetTotalTxns) is the calibration-level target: realized = target ×
realPceLevel(y). 2019 is bit-identical (IEEE ×1.0); 1991 runs at about 67%.

Decision 2, scope: the session only. Salary, revenue and benefits ride AWI;
rent, subscriptions, premiums, obligations and card terms are contractual
and carry the price level. ATM cadence (uniform [1,6] per month), the
cash-versus-card mix and family gift cadences are declared era-flat; a
cash-share era model is registered.

Decision 3, unemployment: declared, not modeled. A job-separation model
would touch payroll and timelines for a second-order effect that PCE
already carries. U-3 stays embedded and exported. A separation-spell model
and within-year NBER shading (8 peak/trough dates, 1990-2020) are
registered.

Decision 4, COVID: windows crossing 2020-21 get the measured collapse and
rebound through realPceLevel (pinned in test_econ_catalog). The Economic
Impact Payments (CARES, Apr 2020, $1,200 per adult; Dec 2020-Jan 2021,
$600; ARPA, Mar 2021, $1,400) are registered as a future class-S statutory
table, deferred because the canonical card-fraud window ends 2020-01-01.

## The harness drain fact (analysis, not a fix)

300-person gate legs lose about 27% deflated spend in year two, in every
era and since before the macro rounds: small worlds under-provision
income, balances fall, and the liquidity multiplier suppresses counts.
Production populations do not share it. H4 gates are cross-era ratios of
same-position years, so the drain cancels. A gate-world budget calibration
round is registered.

The H1 drift-parity gate assumed equal real growth across eras, which H4
makes false (1991 to 1992 grows, 2019 to 2020 falls). It now compares y/y
in calibration-level units (each year's total over priceScale ×
realPceLevel), a declared amendment with its U-9 row, like the H3
fraud-band amendment.

Liquidity feedback is a model property: a quieter 1991 session drains
balances more slowly, lifting the volume ratio to 0.705 against 0.668. The
±15% band allows for it.

## Gates

Step 1 (test_econ_scale): exactly 1.0 at the calibration year;
pceScale(1991) about 0.36, realPceLevel(1991) about 0.67; dips (1991 below
1990, 2009 below 2008, 2020 below 2019) and the 2021 rebound; 2024 above
calibration; scale ratios equal series ratios; freeze clamping.

Step 2 (test_econ_wiring), observed at close on one gate seed with
300-person 730-day legs at 1991 and 2019:

| Gate | Rule | Observed |
|---|---|---|
| Volume | year-1 session count ratio 1991/2019 within ±15% of realPceLevel 0.668 (year 1 so the drain cancels) | 0.7049 |
| Ticket band (unchanged) | mean ticket 2019/1991 within ±15% of CPI 1.877: counts did not move amounts | 1.845 |
| Salary (unchanged) | 2019/1991 within ±15% of AWI 2.480 | 2.395 |
| Calibration identity | 2019 subscription debits are verbatim `kPricePool` prices; counts exact since realPceLevel(2019) == 1.0 | exact |
| Drift parity | calibration-level y/y, 1991 pair over 2019 pair, in 0.80-1.25 (includes the COVID 2020 dip) | 0.927 |
| Fraud rides L | flagged-row ratio over legit-row ratio in 0.80-1.25 | 0.9504 |
| Ring-rail mean | directional band (1.4, 2.6) | 2.012 (H3: 2.418) |

Recession direction is subsumed (declared): the 2008-09 dip is 1-3% at
annual resolution, invisible under the 27% drain at N=300. Its direction
is pinned in test_econ_scale and its count transmission by the volume
gate. Later rounds re-rolled these readings and replaced the single-seed
fraud-rides-L estimator with a 12-seed-pair mean; current values are in
`tests/test_econ_wiring.cpp`.

## Rules kept

No new CLI, draws, lanes or data. Frozen years hold the last measured PCE
level, like the H1 scales, under the existing frozen-era notice. A default
2025+ window runs about 9% above calibration volume. Any long window
starting before 2019 emits fewer session rows than before H4 (the
1991-start card-fraud run integrates about 0.67 to 1.0), while the budget
law keeps the fraud count rate. U-9 rows landed at step 1 (authority rows
first).
