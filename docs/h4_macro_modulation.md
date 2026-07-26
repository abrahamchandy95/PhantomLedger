# H4 macro modulation contract (macro-history-v1)

**STATUS (2026-07-26): CLOSED — DELIVERED AND VERIFIED.** Decisions
adopted (owner, all four recommendations). Step 1 verified (U-9
merged, script deleted, realPceLevel(1991) = 0.668). Step 2 verified:
the RateSampler seam landed, every meaning gate passed, and the owner
re-pinned all four goldens. OBSERVED at the gate seed (300-person
730-day legs at 1991 and 2019):

| Gate | Observed | Expectation |
|------|----------|-------------|
| Session volume ratio 1991/2019 | **0.7049** | realPceLevel 0.668, ±15% |
| Ticket mean ratio 2019/1991 | **1.845** | CPI 1.877, ±15% (unmoved) |
| Salary mean ratio 2019/1991 | **2.395** | AWI 2.480, ±15% (unmoved) |
| Fraud-rides-L parity | **0.9504** | 1.0, band 0.80–1.25 |
| Calibration-level drift parity | **0.927** | 1.0, band 0.80–1.25 |
| Ring-rail fraud mean ratio | **2.012** | band 1.4–2.6 (was 2.418) |

Two step-2 deviations from the original gate list, both declared: (a)
the separate 2008-09 recession-direction leg is SUBSUMED (below); (b)
the README sweep moved to the H5 arc-close round (pacing; nothing
commits between the rounds).

Corpus-level effect of the re-pin, for the record: the default-window
standard golden grew 184,988 → 197,245 rows (+6.63%). That window
starts in 2025 — beyond coverage — so every frame carries the FROZEN
2024 level, realPceLevel(2024) = 1.0915; +9.15% on the session-routed
share of the stream reproduces the observed +6.6%. The 1991-start
fraud and card-fraud corpora moved the other way, as designed.

THE DEFECT THIS ARC CLOSES (the last stationary axis): the world's
REAL activity level was era-flat. H1 made every dollar era-correct
(prices ride CPI, wages ride AWI) and H3 made the population persist
and die — but a 1991 person still transacted exactly as OFTEN and
bought exactly as MUCH (in real terms) as a 2019 person. Measured
reality: over the canonical window [1991, 2020), nominal per-capita
personal consumption grew ≈2.9x while prices grew ≈1.88x — REAL
per-capita consumption grew ≈1.5x, and the path is not smooth: the
level dips in 1991 and 2008-09 and collapses then rebounds through
COVID (2020-21). (The 2001 recession slowed consumption GROWTH
without a per-capita level dip — the measured series says so, and the
model inherits exactly that.) None of this existed in the corpus.

## The one series, already embedded

Everything H4 consumes is ALREADY in `synth/econ/era_data.hpp`
(U-4/U-5 provenance lineage; pinned by test_econ_catalog; exported as
econ.macro_annual): `pcePerCapitaDollars` (BEA A794RC, nominal),
`cpiU`, `unemploymentRatePct` (U-3 annual), `recessionMonths` (NBER
months per year), `populationThousands`. NO new embedded data was
required for the adopted scope.

## The model

Two pure level functions beside priceScale/wageScale in
`synth/econ/nominal.hpp` (step 1), both level-anchored at the
calibration year (exactly 1.0 there) and freeze-and-declare outside
coverage:

    pceScale(year)      = pcePerCapita(year) / pcePerCapita(calibrationYear)
    realPceLevel(year)  = pceScale(year) / priceScale(year)

`realPceLevel` is the REAL per-capita consumption index: ≈0.67 at
1991, 1.0 at 2019, ≈1.09 at the frozen 2024 level, with the measured
dips and the COVID swing encoded at annual resolution by the series
itself.

**THE CHANNEL (decision 1, ADOPTED): real consumption enters the
COUNT axis, not the amount axis.** The discretionary spending
session's per-day transaction rate is modulated by
`realPceLevel(day's year)`; ticket AMOUNTS stay exactly as H1 wired
them (calibration draw × priceScale). Rationale:

- The U-6 denomination law survives untouched — an amount is a
  calibration-year draw realized at the price level, full stop.
- The quantity axis IS the real axis: per-capita payment COUNTS grew
  on this order over the era (Fed Payments Study lineage), while
  category ticket medians are calibration-anchored.
- The fraud budget F = pL/(1-p) rides the candidate count L, so fraud
  DENSITY automatically stays proportional across eras — no separate
  fraud-side wiring. (VERIFIED: the injector's illicit and
  transaction-fraud budgets chain off `realizedBaseCount` — the
  realized legit row count — not off population or window constants.
  The gate measured parity 0.9504.)
- test_econ_wiring's CPI ticket band (~1.88x) remained CORRECT as
  written; only the volume-side gates changed (below).

A mixed count/amount split is a REGISTERED refinement (it would
require re-deriving the ticket bands against a real-quantity
decomposition; no anchor for 1991 ticket sizes exists in the current
catalog).

**THE SEAM (delivered exactly as contracted):** `RateSampler` already
computed `dayPriceScale_` ONCE per day frame (the H1 pattern, shared
by both engines — oracle parity automatic). Step 2 added the parallel
`dayRealLevel_ = realPceLevel(year(frame.day.start))` beside it and
multiplies it into `combinedMultiplierFor()` alongside
`frame_.seasonalMult`. Pure derived data — NO draws moved, NO lanes,
NO new CLI; only sampled counts (and therefore emitted rows, L, and
the ledger trajectory) change. The window-level budget
(`PreparedRun::Budget` targetTotalTxns) keeps its meaning as the
CALIBRATION-LEVEL target: realized volume = target × realPceLevel(y)
per year — a 2019 window reproduces today's volumes exactly (the
multiply is an IEEE ×1.0 no-op there, bit-identical frames), 1991 runs
at ≈67%.

**SCOPE (decision 2, ADOPTED): the session only.** Salary/revenue/
benefits already ride AWI (the labor axis is measured, not modeled
twice). Rent, subscriptions, premiums, obligations, card terms are
CONTRACTUAL — their era axis is the price level they already carry.
ATM withdrawal frequency and the cash-vs-card mix are genuinely
era-varying in reality (cash declined; PhantomLedger's ATM cadence is
uniform [1,6]/month) — DECLARED era-flat here; a cash-share era model
is REGISTERED. Family gift cadences likewise declared era-flat.

**Unemployment (decision 3, ADOPTED): DECLARED, not modeled.** A true
labor-market channel (recession job-separation spells interrupting
salary) would touch the payroll machinery and the persona timelines —
a large model for a second-order corpus effect, given that the DEMAND
side already carries the downturns through the PCE series. The U-3
series stays embedded + exported; a separation-spell model is
REGISTERED. Within-year NBER shading (the 8 peak/trough dates,
1990-2020, a tiny constexpr table) is likewise REGISTERED — annual
resolution is what the measured series gives us for free.

**COVID/EIP (decision 4, ADOPTED): PCE swing free, EIP registered.**
Windows crossing 2020-21 automatically get the measured collapse and
rebound through realPceLevel (the COVID axis facts are already pinned
in test_econ_catalog). The three Economic Impact Payments (CARES Apr
2020 $1,200/adult; Dec 2020-Jan 2021 $600; ARPA Mar 2021 $1,400)
remain REGISTERED as a future class-S statutory table — the canonical
card-fraud window ends 2020-01-01, so no current probe config reaches
the EIP dates; wiring unreachable events buys nothing testable.

## THE HARNESS DRAIN FACT (analysis, not a fix)

The ≈27% deflated year-over-year drain in 300-person second-year legs
(pinned as a DRIFT PARITY fact in test_econ_wiring) predates every
macro-history round: small gate worlds under-provision income relative
to the spending target, balances decline, the liquidity multiplier
suppresses counts, and year 2 runs lean. It is a HARNESS-WORLD budget
artifact, not an era-model defect — production populations don't show
the same geometry.

H4 interacts with it only through gate design: every H4 gate is a
CROSS-ERA RATIO of same-position years, so the structural drain
cancels in the division. H4 does NOT recalibrate the harness world; a
gate-world income/spending budget calibration round is REGISTERED.

**Consequence for the existing gate (AMENDED):** the DRIFT PARITY gate
compared deflated y/y across eras assuming era-equal real growth.
Under H4 that assumption is intentionally false when the legs'
year-pairs have different measured real growth (1991→1992 positive vs
2019→2020 negative). The gate now compares y/y in CALIBRATION-LEVEL
units — each year's total divided by priceScale × realPceLevel — a
declared amendment with its U-9 row, exactly like the H3 fraud-band
amendment. Observed parity 0.927.

The liquidity feedback is worth naming as a model property, not a
defect: a 0.67×-quiet 1991 session drains its balances more slowly,
which lifts the realized volume ratio slightly above the pure level
(0.705 observed vs 0.668 nominal). The ±15% band is sized for exactly
this second-order coupling.

## Acceptance gates (all green)

**Step 1 (test_econ_scale):** exact 1.0 at the calibration year on
both new levels; pceScale(1991) ≈ 0.36 and realPceLevel(1991) ≈ 0.67
direction bands; the measured level dips (1991 below 1990, 2009 below
2008, 2020 below 2019) and the 2021 rebound; 2024 real level back
above calibration; scale-ratio == series-ratio consistency;
freeze-and-declare clamping.

**Step 2 (test_econ_wiring):**

1. **VOLUME band** — ANCHOR-YEAR session ticket COUNT ratio 1991/2019
   inside ±15% of the realPceLevel ratio (≈0.67). Anchor-year form
   (year 1 of each leg, same harness position) rather than
   window-integrated: the drain cancels cleanly and the liquidity
   feedback stays inside the declared allowance. Observed 0.7049.
2. **TICKET band unchanged (channel-separation pin)** — the CPI
   ~1.88x mean-ticket band stayed exactly as pinned; the count
   modulation did not move amounts. Observed 1.845.
3. **CALIBRATION identity** — the amount half stays the corpus gate
   (every 2019 subscription debit is a verbatim kPricePool price); the
   COUNT half is primitive-exact (realPceLevel(2019) == 1.0 in IEEE
   and x × 1.0 == x, so 2019 frames sample bit-identically to the
   unmodulated loop).
4. **R-aware drift parity (the declared amendment)** — y/y spend in
   calibration-level units, 1991 pair over 2019 pair, inside
   0.80-1.25. The COVID 2020 dip rides inside this gate. Observed
   0.927.
5. **Recession direction — SUBSUMED (declared deviation)** — the
   separate 2007-2010 leg is dropped: the 2008-09 per-capita dip is
   1-3% at annual resolution, unresolvable under the ~27% harness
   drain at N=300 (the sparse-band lesson: don't gate what the harness
   cannot measure). The dip DIRECTION is pinned at the primitive
   (test_econ_scale) and the count-axis TRANSMISSION is pinned by the
   33% volume gate; their composition is the recession behavior.
6. **Fraud rides L** — the flagged-row count ratio tracks the legit-row
   count ratio inside 0.80-1.25 (the budget law F = pL/(1-p) reads
   realized L). Observed 0.9504. The ring-rail mean band stays
   DIRECTIONAL (1.4, 2.6): observed 2.012, which moved AWAY from the
   2.6 edge that H3 had approached (2.418) — the recomposition
   improved the margin.

## Law compliance

NO new CLI; NO new rng draws or lanes (pure level factors); NO new
embedded data; freeze-and-declare outside coverage (frozen years hold
the last measured PCE level — consistent with the H1 scales, and the
app's existing frozen-era notice already covers the window). NOTE the
freeze DIRECTION: a default 2025+ window runs ≈9% ABOVE calibration
volume, at the frozen 2024 level.

MODEL-MOVING: counts moved, so all four goldens were re-pinned in the
step-2 round. Corollary for run planning: any long window STARTING
before the calibration year now emits FEWER session rows than pre-H4
(the canonical 1991-start card-fraud run integrates realPceLevel
≈0.67→1.0 across its years), while the fraud COUNT RATE is preserved
by the budget law. U-9 authority rows landed at step 1, per the
authority-rows-first law.
