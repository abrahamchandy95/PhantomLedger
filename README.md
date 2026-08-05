# PhantomLedger

A synthetic bank transaction generator. PhantomLedger produces realistic, research-grounded retail-bank ledger data.
People, accounts, salaries, rent, bills, loans, insurance, credit cards, fraud rings, devices, IPs are created
suitable for training ML models for mule detection, AML graph analytics, and general transaction-pattern research.

The **legitimate activity is structurally rich enough that the fraud signal isn't trivially separable**. Every behavioral
dimension the ML models need to learn like temporal bursts, high fan-in, device sharing and rapid forwarding also exists 
in milder form in legitimate accounts. Mules differ in *degree*, not in *kind*.

## Table of Contents

1. [Overview](#overview)
2. [Building](#building)
3. [Usage](#usage)
4. [Pipeline](#pipeline)
5. [Entity Generation](#entity-generation)
6. [Personas](#personas)
7. [Financial Products](#financial-products)
8. [Banking Mechanics](#banking-mechanics)
9. [Mathematical Models](#mathematical-models)
10. [Legitimate Transaction Flows](#legitimate-transaction-flows)
11. [Family and Social Flows](#family-and-social-flows)
12. [Fraud Typologies](#fraud-typologies)
13. [Infrastructure and Device Attribution](#infrastructure-and-device-attribution)
14. [Chronological Replay and Screening](#chronological-replay-and-screening)
15. [Export Formats](#export-formats)
16. [Configuration](#configuration)
17. [References](#references)

## Overview

PhantomLedger generates a complete bank-internal view of a synthetic population over a configurable time window.

Each person is assigned:
- a persona archetype (student, retired, salaried, freelancer, small business, HNW)
- one or more accounts, possibly including a credit card and a business operating or brokerage account
- PII (phone, email, deterministic address)
- a family graph (household, spouse, parents, children, supported relatives)
- a social graph (weighted contact list for P2P flows)
- device(s) and IP(s) with session history
- a portfolio of financial products (mortgage, auto loan, student loan, tax profile, insurance holdings)

On top of that static universe, the pipeline emits transactions across several channels like
salary, rent (five variants by landlord type), merchant purchases, bills, subscriptions, ATM,
self-transfers, credit card lifecycle events, government benefits, insurance premiums and claims,
loan payments, tax payments, family transfers (allowance, tuition, support, spouse, parent gifts,
sibling transfers, grandparent gifts, funerals and estates), and fraud (classic, layering, funnel, structuring, invoice, mule, solo).

Output formats include a standard transaction graph (vertices + edges), an ML-ready schema for mule detection,
a full TigerGraph AML_Schema_V1 export with MinHash-based entity resolution, a transaction-edges variant of the
AML schema with derived graph features, and a card-fraud corpus for TigerGraph's TF_GNN_v3
schema aimed at temporal graph neural networks (TGN) predicting transaction-level fraud. Every
table lands directly in PostgreSQL during the run — PhantomLedger writes no files.

### Design Principles

- **Research-grounded.** Every probability, median, and sigma has a published citation or an explicitly documented modeling choice.
- **Era-correct dollars.** All calibrated dollar constants are denominated in the pinned 2019 calibration year; realized amounts scale to each event's year through the embedded CPI-U (prices) and SSA AWI (wages) history, 1990–2024. A 1991 window opens at ≈0.53× prices and ≈0.40× wages; a 2019 window reproduces the calibrated magnitudes exactly. Outside coverage the scales freeze at the nearest measured year and the run declares it (never silent extrapolation). Statutory amounts — the BSA/CTR $10,000 threshold, note/rack denominations — deliberately do NOT scale.
- **Era-correct lifecycles.** Personas are lifecycle states, not fixed labels: each person carries a deterministic timeline derived from their single modeled birth date — students start careers at 19–28, workers retire on SSA claiming-shaped dates (the exact 1983-Amendments full-retirement-age schedule), small businesses close on a memoryless median-five-year hazard, and everyone eventually DIES on an SSA-life-table hazard — and payroll, Social Security onset, business revenue, the spending level, and the AML customer view all follow the timeline. The population both persists and dies: deaths produce funerals and estates, accounts close after settlement, and a BEA-sized join cohort replenishes the customer base (macro-history H3).
- **Era-correct activity levels.** Dollars are only half of era realism: how OFTEN people transact also moved. The discretionary spending session's per-day transaction rate is modulated by the measured real per-capita consumption level — nominal BEA PCE per capita deflated by CPI-U, anchored at the 2019 calibration year — so a 1991 window runs at ≈0.67× today's session volume, 2009 and 2020 carry their measured downturns, and a 2019 window reproduces calibrated volumes exactly. The modulation is COUNT-ONLY: ticket amounts stay on the price index, so the denomination law above is untouched, and the fraud budget rides the realized candidate count, so fraud DENSITY stays era-stable while fraud VOLUME follows the economy (macro-history H4; see [docs/h4_macro_modulation.md](docs/h4_macro_modulation.md)).
- **Structurally realistic.** Balance ledgers enforce affordability; overdraft protection is a per-account product with tier-specific fees; LOC interest accrues on a dollar-seconds integral; merchants receive business-checking seeds; credit cards operate as liability accounts.
- **Strict validation.** Configuration structs participate in a `validate::Report` framework; misconfigured rules surface as `validate::Error` at construction time rather than as silent zeros mid-simulation.

## Building

PhantomLedger is a C++23 project built with CMake. A `Makefile` wraps the common workflows.

### Prerequisites

- CMake ≥ 3.23
- A C++23-capable compiler (GCC 13+, Clang 17+, or MSVC 19.38+)
- Git (for the `faker-cxx` fetch)
- PostgreSQL client headers and `libpq` (for the build)
- A reachable writable PostgreSQL database for production runs

[`faker-cxx`](https://github.com/cieslarmichal/faker-cxx) v4.3.2 is fetched automatically through CMake's `FetchContent`; PostgreSQL's client library is a system dependency.

### Make targets

```sh
make build       # configure + build (Release by default)
make test        # build + run the CTest suite
make run         # build + run the binary (silent: warnings and errors only)
make run-help    # build + print --help
make run-fast    # incremental build, then run (skips reconfigure)
make run-info    # run with progress-level diagnostics
make run-debug   # run with per-day diagnostics; narrow with TOPICS=...
make run-trace   # run with everything the logger can say
make run-mem     # run with per-stage peak-RSS reporting only
make rebuild     # clean + build
make clean       # remove the build directory
```

### Variables

All targets accept the following overrides:

| Variable | Default | Description |
|---|---|---|
| `CONFIG` | `Release` | CMake build type (`Debug`, `Release`, `RelWithDebInfo`). |
| `BUILD_DIR` | `build` | Out-of-tree build directory. |
| `TESTS` | `ON` | `PL_BUILD_TESTS` — build the C++ test suite. |
| `BIN` | `phantomledger` | Binary name (used by the `run` targets). |
| `ARGS` | *(empty)* | Arguments forwarded to the binary by `make run` / `make run-fast`. |
| `TOPICS` | `all` | Topic filter for the diagnostics run targets (comma-separated; see [docs/debugging.md](docs/debugging.md)). |

Examples:

```sh
make build CONFIG=Debug
make test BUILD_DIR=build-debug CONFIG=Debug
make run ARGS="--usecase standard --days 120 --population 200000"
```

`ARGS` is the single Make variable that forwards the existing CLI surface. The
validated 5,000-person, 1999-through-2019 card-fraud invocation is:

```sh
make run ARGS="--start 1999-01-01 --days 1070 --population 50000 --seed 42 --usecase card-fraud"
```

Runtime diagnostics are **silent by default**; the `run-info` / `run-debug` / `run-trace` /
`run-mem` targets switch them on. [docs/debugging.md](docs/debugging.md) is the debugging
playbook: every topic and level, the golden-baseline triage protocol, and what to run for
each class of problem.

Headers and sources live in two mirrored trees, LLVM-style: `include/phantomledger/<layer>/…`
beside `src/<layer>/…`. The project prefix appears exactly once, the top level of each tree is
the dependency layering (enforced at configure time by the include-layer lint), and a module
reviews as the mirrored pair of folders. The configure step also audits the source list against
every `src/*.cpp` on each run; adding a translation unit without registering it in
`CMakeLists.txt` is a hard configure-time error rather than a silent missing-symbol surprise
at link.

## Usage

```sh
phantomledger [options]
```

All output lands in PostgreSQL: the corpus streams into the `transactions` table during settlement
and every exporter writes its tables directly during the run. No files are written; reruns with the
same seed and config rewrite byte-identical content.

| Option | Default | Description |
|---|---|---|
| `--usecase {standard,mule-ml,aml,aml-txn-edges,card-fraud}` | `standard` | Exporter to run. |
| `--days N` | `365` | Simulation length in days. |
| `--population N` | `70000` | Total population. |
| `--seed N` | `0xDEADBEEF` | Top-level RNG seed. |
| `--start YYYY-MM-DD` | `2025-01-01` | Simulation start date. |
| `--help`, `-h` | — | Print the usage message. |

Environment: `PL_PG='host=... port=... dbname=...'` overrides the PostgreSQL
connection. When `PL_PG` is unset or empty, the code uses
`dbname=phantomledger`. A reachable server is required—the run fails fast
before any generation when none answers. (`PL_FILE_ONLY=1` is test
infrastructure only: a serverless escape that produces just the corpus stream
digest; `aml-txn-edges` cannot run this way.)

Windows touching years outside the measured 1990–2024 era coverage (including
the 2025 default start) run with the nominal dollar scales AND the real
activity level FROZEN at the nearest covered year's level; the binary
prints one declared stderr notice. On the activity axis the 2024 freeze
sits ≈9% ABOVE the 2019 calibration level, so a default 2025 run
emits somewhat more session volume than a 2019 one.
The card-fraud use case additionally requires its whole window inside
coverage (the era lock).

Each `--usecase` writes into its own schema, so runs with the same `--seed` against the same
database compose into a coherent multi-format dataset without colliding (identifiers are canonical
across use cases):

- `standard` → `public` (unprefixed tables, next to the shared `transactions` stream)
- `mule-ml` → `mule_ml.ml_ready_*`
- `aml` → `aml.aml_*`
- `aml-txn-edges` → `aml_txn_edges.aml_txn_edges_*`
- `card-fraud` → `card_fraud.cf_*`

## Pipeline

The top-level orchestrator (`PhantomLedger::pipeline::SimulationPipeline`) runs three stages in order.

### Stage 1: entities

1. **People.** Person IDs are created; fraud-ring membership, mule roles, victim roles, and solo fraudsters are sampled (see [Fraud Typologies](#fraud-typologies)).
2. **Accounts.** One to N accounts per person (binomial, `maxPerPerson` default 3); the first account is always the primary deposit account.
3. **PII.** Deterministic phone and email derived from person ID.
4. **Merchants.** A core merchant pool (density per 10k people) plus a sparse long tail of external-only merchants; core merchants are split into internal (on-us) vs external based on `inBankP`.
5. **Landlords.** Typed pool (individual / small LLC / corporate) drawn from the RHFS 2021 unit-weighted distribution; each landlord independently assigned in-bank or external by type.
6. **Counterparty pools.** Employers, client payers, platforms, processors, owner businesses, brokerages. Employers and clients are split internal/external.
7. **Institutional externals.** SSA, disability, insurance carriers, lenders, IRS, and bank fee books are registered from a fixed catalog.
8. **Planned external family accounts.** Deterministic `XF…` accounts for family members who bank elsewhere.
9. **Personas.** Each person gets an archetype, a per-person perturbed `Persona` (lognormal noise around archetype values, beta-distributed paycheck sensitivity), a membership interval (a BEA-sized join cohort joins mid-window; accounts close after death — see [Personas](#personas)), a birth date drawn on an isolated lane at the person's own anchor (the single age axis every exporter renders), and a lifecycle timeline including a death date.
10. **Planned owned income accounts.** Freelancers/smallbiz get a `BOP…` business operating account; HNW gets a `BRK…` brokerage/custody account. These are **internal**, same-customer accounts, not externals.
11. **Credit cards.** Each eligible person draws card approval (persona-dependent); issued cards get APR, credit limit, cycle day, autopay mode.
12. **Portfolios.** Mortgage / auto loan / student loan / tax profile / insurance holdings are assigned by persona priors; insurance rates for non-financed collateral owners are back-calculated so the overall persona-level rate stays close to target.

### Stage 2: infra

1. **Ring plans.** Burst window and participating members for shared device/IP per fraud ring.
2. **Devices.** 1–2 personal devices per person; sparse legit shared-device groups (household/family ambient noise); ring-shared flagged devices.
3. **IPs.** 1–3 IPs per person; legit noise; ring-shared flagged IPs.
4. **Router.** Sticky current device/IP per person, occasional switching.
5. **Shared infra.** Map from ring ID to shared device/IP with high-probability usage during fraud transactions.

### Stage 3: transfers

1. The legit-transfer builder produces the candidate ledger in semantic order:
   - **Income pass.** Salary (payroll cadences, job tenure, compound raises), government benefits (SSA Wednesday cohorting, disability), non-payroll revenue (client ACH, platform payouts, card settlements, owner draws, investment inflows). Every income stream ends at death.
   - **Routines pass.** Direct-deposit splits, rent (with landlord-type-aware channel routing), subscriptions, ATM withdrawals, intra-person self-transfers, day-to-day discretionary spending (market simulator with AR(1) momentum, dormancy, paycheck-cycle boost, seasonality, counterparty evolution). Behavioral flows stop at death; contractual flows stop at account closure.
   - **Family pass.** Allowances, tuition, retiree support, spouse transfers, parent gifts, sibling transfers, grandparent gifts, funerals, and death-caused estates.
   - **Credit pass.** Credit-card lifecycle (purchase → refund/chargeback, interest, late fees, payments), serviced until account closure.

   The builder also returns a starting clearing house with per-account balances, overdraft products, and credit limits applied.
2. Insurance premium and claim events are merged in.
3. Financial-product obligations (mortgage/auto/student/tax) are emitted through a unified obligation emitter that models late/missed/partial/cure cycles with delinquency clustering.
4. **Authoritative pre-fraud chronological replay.** The combined stream is stable-sorted by the composite audit key `(timestamp, source, target, amount, fraud flag, ring, channel, device, IP)` and replayed against the starting ledger. This pass is the only place where balance-gated drops are recorded, and the only place that **emits liquidity events** — overdraft fees (one per courtesy-tap, capped at 3/day) and monthly LOC interest (dollar-seconds integral × APR / seconds-per-year).
5. **Fraud injection.** Camouflage and illicit transactions are added to the draft ledger based on the target illicit ratio (`targetIllicitP`, default 0.5%).
6. **Post-fraud replay.** Same ledger replayed with fraud included, but liquidity-event emission disabled so the fees/interest from the first pass aren't double-emitted.
7. **Account-registry validation.** Every referenced account ID must be in the registry.

## Entity Generation

### IDs

All IDs are fixed-width prefixed strings:

| Prefix | Entity | Width |
|--------|--------|-------|
| `C` | Customer (person) | 10 |
| `A` | Account | 10 |
| `L` | Credit-card account | 9 |
| `M` / `XM` | Merchant internal / external | 8 |
| `E` / `XE` | Employer internal / external | 8 |
| `LI`/`LS`/`LC` / `XLI`/`XLS`/`XLC` | Landlord individual/small-LLC/corporate, internal/external | 7 |
| `IC` / `XC` | Client payer | 8 |
| `XP` | Platform (external) | 8 |
| `XS` | Processor (external) | 8 |
| `XO` | Owner business (external) | 8 |
| `XB` | Brokerage (external) | 8 |
| `XF…` | External family account | hash-derived |
| `BOP…` / `BRK…` | Same-customer business operating / brokerage (internal) | hash-derived |
| `XGOV…` / `XINS…` / `XLND…` / `XIRS…` / `XBNK…` | Government / insurance / lender / IRS / bank servicing | fixed |

**Leading `X` signals external.** `BOP`/`BRK` are intentionally non-`X` because a freelancer's business account at the same bank is an internal book-to-book transfer destination, not an interbank counterparty (NFIB 2023: 56% of small business owners keep personal and business at the same bank).

These prefixed strings are the export-time rendering only. Internally, every account and counterparty is a 16-byte plain-old-data `entity::Key` `{role, bank, number}`; the simulation core never allocates or compares ID strings.

### Merchants

Density is configured per 10k people (`per10kPeople` = 120 core, `longTailExternalPer10kPeople` = 400). Core merchants are weighted by a lognormal size distribution (`sizeSigma` = 1.2); the long tail carries 18% of the total weight with a heavier sigma (1.8) and is always external.

Categories: grocery, fuel, utilities, telecom, ecommerce, restaurant, pharmacy, retail_other, insurance, education.

**In-bank probability** (`internalP` = 0.02): this is the current explicit
modeling choice. The older 0.06 documentation value was stale; calibrating the
on-us merchant share against an issuer/acquirer portfolio remains a model
round rather than a documentation inference from deposit share.

### Landlords

Unit-weighted shares from the 2021 Rental Housing Finance Survey (HUD/Census):

| Type | Share | Rationale |
|------|-------|-----------|
| Individual | 38% | RHFS "Individual investors" (37.6%). |
| Small LLC | 15% | The "mom-and-pop-in-LLC-wrapper" slice of the RHFS LLP/LP/LLC bucket (CRS R47332, Harvard JCHS "LLC gray zone"). |
| Corporate | 47% | Remainder of RHFS LLC bucket + institutional / REIT / trustee / real-estate-corporation share. |

**Payment channel mix** (Baselane 2024, TurboTenant, and the documented pattern that individual landlords rely on Zelle/check while corporate property management uses portal ACH):

| Type | rent_p2p | rent_check | rent_ach | rent | rent_portal |
|------|----------|------------|----------|------|-------------|
| Individual | 40% | 25% | 25% | 10% | — |
| Small LLC | 15% | 30% | 55% | — | — |
| Corporate | — | — | 5% | — | 95% |

**In-bank probability by type**: individual 6%, small LLC 4%, corporate 1%. Corporate/REIT landlords use commercial banking with national scope; very low overlap with any single retail bank.

### Counterparty Pools

Densities per 10k people:

| Pool | Default density | In-bank p |
|------|----------------|-----------|
| Employers | 25 | 4% (large employers use ADP/Paychex; smaller ones bank locally) |
| Client payers | 250 | 2% (geographically diverse businesses) |
| Owner businesses | 200 | — (always external by design) |
| Brokerages | 40 | — |
| Platforms | 2 | — |
| Processors | 1 | — |

## Personas

| Persona | Share | Rate mult. | Amount mult. | Initial balance | Card p | CC share | Credit limit | Paycheck sensitivity β(α,β) |
|---------|-------|------------|--------------|-----------------|--------|----------|--------------|------------------------------|
| student | 12% | 0.7 | 0.7 | $200 | 0.25 | 0.55 | $800 | Beta(4, 2) — high |
| retiree | 10% | 0.6 | 0.9 | $1,500 | 0.55 | 0.55 | $2,500 | Beta(3, 3) — moderate |
| freelancer | 10% | 1.1 | 1.1 | $900 | 0.65 | 0.65 | $4,000 | Beta(2, 4) — lower |
| smallBusiness | 6% | 2.4 | 1.8 | $8,000 | 0.80 | 0.75 | $7,000 | Beta(2, 5) — lower |
| highNetWorth | 2% | 1.3 | 2.8 | $25,000 | 0.92 | 0.80 | $15,000 | Beta(1, 8) — very low |
| salaried | 60% (residual) | 1.0 | 1.0 | $1,200 | 0.70 | 0.70 | $3,000 | Beta(2, 3) — moderate |

Shares are computed at compile time: the five non-salaried personas have fixed shares (12/10/10/6/2 = 40%) and salaried takes the remainder (60%). This is enforced by a `consteval` check — any future edit that makes the non-salaried shares exceed 1.0 is a compile error.

Dollar columns (initial balance, credit limit) are calibration-year values;
the realized stocks anchor once at the window-start year's price level.

### Persona timelines (macro-history H2)

The seed assignment above is the persona AT THE PERSON'S ANCHOR — simulation
start for the seed roster, the join date for the join cohort (below). Each
person also carries a deterministic lifecycle timeline
(`synth/personas/timeline.hpp`), derived once on an isolated RNG lane from
their modeled birth date:

- **student → working** (salaried 85% / freelancer 15%) at a work-start age
  drawn over 19–28;
- **salaried / freelancer → retiree** at an SSA claiming-shaped date: 30%
  claim at 62, 10% uniformly before full retirement age, 45% at FRA (the
  exact 1983-Amendments schedule, 65→67 by birth cohort), 5% between FRA and
  70, 10% at 70, plus a 0–60-day jitter;
- **smallBusiness → working** when the business closes (memoryless
  exponential residual lifetime, median 5 years, per BLS BED five-year
  survival) — retirement dominates when it comes first;
- **retiree** seeds carry a backdated claim date; **highNetWorth** is exempt
  (no transitions);
- **everyone → deceased** on an SSA 2023 period-life-table hazard (below).

Generation follows the timeline: paychecks span [career start, min(claiming
date, death)), Social Security deposits begin at max(window start, claim date)
and end at death, business revenue stops at the close, spending steps down
~12% at the claim (see [Day-to-Day Spending](#day-to-day-spending)), and the
AML Customer export reports the end-of-window persona. Spending ARCHETYPES
(the rate/amount/timing profiles above) remain seed-based — the full
age-profile re-anchor is a registered upgrade.

### Mortality, estates, and membership (macro-history H3)

Every person carries a death date, derived once on the isolated
`{"mortality", personId}` lane from their birth date and the EMBEDDED SSA
2023 period life table (sex-specific annual death probabilities inverted at
one uniform; sex is a latent 50/50 mortality attribute — the ~2.7-year
male/female gap is retained as real signal). Everyone on the roster is alive
at their anchor by construction (the hazard is conditional on survival to
it), and deaths anchor to birth dates, never to the window.

**Behavioral flows stop at death**: spending person-days, ATM withdrawals,
internal self-transfers, rent (the lease dies with the tenant), family
gifts (either party dead drops the row), insurance claim filing, and every
income stream. **Contractual flows keep posting against the estate until
ACCOUNT CLOSURE** at death + 120 days: subscriptions, insurance premiums,
loan/tax obligations, and card cycles (card servicing stops one final
statement early so its payment tail lands before closure). Estates really do
keep getting billed — the interval is the modeled settlement period.

**Every in-window death produces a funeral and an estate**: one bill-channel
funeral payment from the decedent's account at death+3–10 days (lognormal
median $6,300 calibration dollars — the NFDA 2019 General Price List blend
of burial and cremation-with-viewing at the ~55% cremation rate), and an
estate distribution to the heirs (children, else supporting children) at
death+30–90 days, both before the accounts close.

**Membership is the interval [joinTs, closeTs)**: the seed roster is present
from window start; a JOIN COHORT — sized so the customer base tracks the
embedded BEA population series (≈0.5–1.4%/yr across 1990–2024, rate-frozen
outside coverage) — joins on days drawn proportional to each year's
population growth, one draw per joiner on the isolated `{"join-cohort"}`
lane. Joiners' ages, timelines, and lifespans anchor at their JOIN date (a
2015 joiner draws 2015-appropriate ages — this closed the declared joiner
age-axis error). The standard exporter's visible corpus filters every row on
both endpoint owners' intervals; customer tables carry `created_at` and
`closed_at`; the AML Customer status flips `active → closed` at closure.

## Financial Products

Every person gets a portfolio of optional products. Ownership is conditioned on persona.
Payment medians below are calibration-year (2019) dollars; loan payments anchor
at their origination year's price level and stay fixed nominal through the term
(real loans are nominal contracts), while tax amounts realize at each due
date's price level.

### Mortgage

**Ownership** (persona-conditioned):

| Persona | p(mortgage) |
|---------|-------------|
| student | 0% |
| retired | 35% |
| salaried | 55% |
| freelancer | 35% |
| smallbiz | 50% |
| hnw | 70% |

**Terms**:
- Monthly payment: lognormal, median $1,672 (Census AHS 2023), σ = 0.45. Treated as all-in (P&I + escrow).
- Payment day: 85% on the 1st, remainder on days 2–5.
- Term: 30 years contractual, but observed loan age sampled triangular(0.5, 5, 10) years to reflect effective duration under refi/moves (Freddie Mac 2024).
- Late rate: 3.5% (MBA Q3 2024 30+ day delinquency). Late-day range 1–15. Miss 0.3%, partial 0.2%, cure 70%, cluster multiplier 1.6×.

### Auto Loan

**Ownership**:

| Persona | p |
|---------|---|
| student | 15% |
| retired | 25% |
| salaried | 45% |
| freelancer | 40% |
| smallbiz | 50% |
| hnw | 30% |

**New vs used split** (35% new / 65% used, modeling choice):

| Segment | Payment median | Payment σ | Term mean | Term σ |
|---------|---------------|-----------|-----------|--------|
| New | $734 | 0.28 | 68 mo | 8 mo |
| Used | $525 | 0.32 | 72 mo | 10 mo |

Anchors: Experian State of the Automotive Finance Market, Q2 2024 (average payments $734 new / $525 used; average terms 68.5 / 67.4 months; ~80% of new and ~36% of used purchases financed). Term range [36, 84] months.

Late rate 4% (broader than the 2.1% 60+ day stat from Fed NY Q4 2024), late window 1–10 days, miss 1%, partial 2%, cure 58%.

### Student Loan

**Ownership**:

| Persona | p |
|---------|---|
| student | 60% |
| retired | 2% |
| salaried | 25% |
| freelancer | 20% |
| smallbiz | 15% |
| hnw | 5% |

**Plan types** (modeling mix, not exact portfolio shares):
- Standard (60%): 120-month term.
- Extended (10%): 300-month term.
- IDR-like (30%): 55% get 240 months, 45% get 300 months. Forgiveness horizons per StudentAid.gov.

Grace period: 6 months after school exit (federal standard). Student-persona holders have 60% probability of still being in deferment at sim start. Origination is sampled 180 days – 4 years before school exit. Graduation season biased to May/June with secondary December.

Late rate 9%, late window 1–15 days, miss 3%, partial 5%, cure 45%, cluster multiplier 2.0×. Anchors: FSA FY 2024 Annual Report (~45M borrowers, $1.6T outstanding).

### Tax Profile

**Ownership of quarterly estimated payments**:

| Persona | p |
|---------|---|
| student | 2% |
| retired | 8% |
| salaried | 6% |
| freelancer | 70% |
| smallbiz | 85% |
| hnw | 45% |

Separate from ownership, annual filing produces one of three outcomes: refund (55%, median $900, IRS Feb–May), balance due (18%, median $1,200, April), or no visible settlement. Quarterly median $1,800. Anchors: JPMorgan Chase Institute (large income swings in Feb–Apr and Dec), IRS 2024–25 refund stats (~72% of filers receive refunds averaging $3,500+).

**Quarterly filing calendar (IRS Form 1040-ES)**: estimated tax payments fire on fixed dates — January 15 (prior-year Q4), April 15 (Q1), June 15 (Q2), and September 15 (Q3) — at 10:00 local time per filing convention. Weekends and federal holidays are not currently rolled.

### Insurance

Persona ownership rates:

| Persona | Auto | Home | Life |
|---------|------|------|------|
| student | 55% | 0% | 5% |
| retired | 80% | 80% | 40% |
| salaried | 90% | 55% | 55% |
| freelancer | 85% | 35% | 30% |
| smallbiz | 92% | 60% | 60% |
| hnw | 95% | 90% | 80% |

**Premium distributions** (monthly, calibration-year dollars; each billing
realizes at the billing month's price level):
- Auto: median $225, σ = 0.35 (Bankrate 2026).
- Home: median $163, σ = 0.40 (Ramsey 2025).
- Life: median $40, σ = 0.50.

**Collateral anchoring**: mortgaged households receive home coverage with p = 0.998; auto-loan holders receive auto with p = 0.997. For the non-financed remainder, the issuance probability is back-calculated so the overall persona rate matches the target.

**Claim behavior**:
- Auto: 4.2% annual claim rate (III 2024), median payout $4,700, σ = 0.80.
- Home: 5.5% annual (Triple-I 2024), median payout $15,750, σ = 0.90.
- Life: death-benefit payouts remain a REGISTERED upgrade — deaths ARE
  modeled (macro-history H3) and the estates carry the wealth transfer; a
  policy payout to the estate would double-count until sized together.

Annual rates are converted to a window probability via `1 - (1 - p)^(n_months/12)`. Claim filing stops at the policyholder's death; premiums keep billing the estate until account closure.

**Escrow interaction**: if a homeowner has a mortgage, the home-insurance premium is treated as already embedded in the mortgage payment and is NOT emitted separately (the Census AHS median payment is already all-in). Claims still fire.

### Credit Cards

At issuance the policy draws:
- APR: lognormal, median 22%, σ = 0.25, clamped to [8%, 36%] (Fed G.19 Q4 2024 average 21.5%).
- Limit: lognormal around persona credit limit, σ = 0.65, anchored at the window-start year's price level. (Context: Experian Q3 2023 average total credit limit across all of a consumer's cards was $29,855; per-card persona limits sit deliberately below that aggregate.)
- Cycle day: uniform [1, 28].
- Autopay mode: 40% full, 10% minimum, 50% manual.

Stats anchors: 82% of US adults have ≥1 card (Fed SHED 2023); average balance carried $6,580 (TransUnion Q4 2024).

**Lifecycle generator** processes each billing cycle:
1. Purchases accumulate on the card.
2. Each purchase probabilistically produces a refund (0.6%, 1–14 day delay) or chargeback (0.1%, 7–45 day delay) **from the same merchant that received the charge** — no synthetic refund counterparty.
3. Cycle-end: compute average balance via piecewise-constant integration; if out of grace and there is a debt integral, charge interest at `APR × interval_days / 365`.
4. Minimum due = max(2% of statement, $25). Autopay mode drives payment amount (full / min / manual). Manual splits into pay-full (35%), partial Beta(2, 5) (30%), minimum (25%), miss (10%). Late by cycle has 8% probability with 1–20 day delay.
5. Late fee $32 fires if not paid by due date (+grace_days default 25). ($32 is the CARD Act safe-harbor level restored when the CFPB's $8 cap was vacated in April 2025; real schedules step to ~$43 for repeat violations, which the model simplifies to a flat fee.) The $25 minimum-due floor and the $32 fee are calibration-year dollars, realized at each cycle date's price level.

Card servicing stops with the owner's account: the statement-close ladder truncates 50 days before ACCOUNT CLOSURE (death + 120 days), so the final cycle's payment and late-fee tail settle against the estate strictly before the accounts close.

## Banking Mechanics

### Balances

Every internal account carries:

1. **Balance** (seeded at simulation start; the calibration-year draw anchors at the window-start year's price level, so 1991 accounts open with era-correct dollars).
2. **Overdraft protection product** — exactly one of NONE / COURTESY / LINKED / LOC, drawn from a per-persona multinomial.
3. **Bank tier** — ZERO_FEE (15%) / REDUCED_FEE (10%) / STANDARD_FEE (75%) (Bankrate 2025, Consumer Reports 2024 account-share composition).
4. **Per-account overdraft fee** — drawn from a tier-specific lognormal at init (window-start anchored, like the balance).

Exclusivity matters: real customers don't stack all three buffer products. Available liquidity is `balance + overdraft + linked + courtesy`, but at most one of those buffers is non-zero per account.

#### Protection Mix by Persona

Per-persona shares for (courtesy / linked / LOC); the remainder is NONE (hard decline at zero):

| Persona | Courtesy | Linked | LOC | NONE |
|---------|----------|--------|-----|------|
| student | 12% | 8% | 2% | 78% |
| retiree | 16% | 22% | 4% | 58% |
| salaried | 18% | 24% | 12% | 46% |
| freelancer | 16% | 18% | 12% | 54% |
| smallBusiness | 20% | 24% | 20% | 36% |
| highNetWorth | 22% | 30% | 28% | 20% |

Protection type is drawn via a single 4-way categorical (courtesy / linked / loc / none). Mutual exclusivity is a hard invariant enforced by the sampler — no account can simultaneously hold multiple buffer types.

Anchors: ~20% of US consumers opt into courtesy (CFPB 2024); ~40% link a savings sweep when offered (Bankrate 2025); formal OD LOCs are less common overall but more prevalent among freelancers, SMBs, and HNW (SoFi / NerdWallet 2025, private-banking patterns).

#### Buffer Sizes (lognormal medians by persona)

Courtesy buffer: ~$100–300. Linked sweep: $225 (student) → $10,000 (HNW). LOC credit line: $500 (student) → $7,500 (HNW). Sigmas: 0.45 / 0.90 / 0.60 respectively.

#### LOC Interest Accrual

For each LOC-protected account the ledger tracks:
- `dollarSecondsIntegral[idx]` — running ∫ max(0, -cash) dt in dollar-seconds, updated on every balance-touching event using the pre-transfer cash value.
- `apr[idx]` — annualized rate, drawn globally from `Normal(0.18, 0.04)` clamped at 0.
- `billingDay[idx]` — day of month uniform [1, 28]. **Reserved for future calendar-aware billing; the current implementation bills on a rolling 30-day period from `lastBillingTs`, not on the calendar billing day.**

Billing uses a rolling 30-day sweep (`kBillingPeriodSeconds = 30 × 86400`). When the elapsed time since the previous billing exceeds the period, the tracker emits:

$$
\text{interest} = \frac{\text{integral\_seconds} \times \text{APR}}{365.25 \times 86400}
$$

Interest is debited directly from cash (bypassing the funding check so the fee always posts even over-limit) and emitted as a `LOC_INTEREST` transaction to `XBNK00000002`. The integral is reset to zero and `lastBillingTs` is advanced to `now`. On the very first sweep for an account, `lastBillingTs = 0` triggers a silent anchor — the clock starts but no interest is emitted.

#### Overdraft Fees

After any accepted debit that leaves a COURTESY-protected account negative, the replay emits a fee transaction to `XBNK00000001` (BANK_FEE_COLLECTION). The amount comes from the per-account lognormal sampled at init:

| Tier | Median | σ | Example banks |
|------|--------|---|---------------|
| ZERO_FEE | $0 | — | Capital One, Citibank, Ally |
| REDUCED_FEE | $15 | 0.25 | Huntington / BMO / Santander $15 (2022 cuts); BofA $10 |
| STANDARD_FEE | $35 | 0.20 | Chase $34, Wells $35, US Bank $36, PNC $36 |

Cap: 3 fees per account per calendar day (Wells Fargo / industry standard). Liquidity-event channels bypass the insufficient-funds check so the fee always posts.

#### Merchant Balance Seeding

Internal merchants (`M…`) default to the SALARIED persona's $1,200 initial balance because no person maps to them. That's too low for a business that must honor refunds. The init pass overrides merchant balances with a business-checking lognormal: median $8,000, σ = 0.90 (Bluevine 2025: 39% of SMBs have < 1 month of operating expenses; healthy ones hold 2–3 months). Merchants carry no personal protection products and are ZERO_FEE tier.

#### Credit Card Liability Accounts

`setCreditLimit(card, limit)` repurposes the `overdrafts` slot to hold the credit line, sets protection to NONE, tier to ZERO_FEE, and clears any LOC registration. This makes `availableToSpend(card) = creditLimit` at init while ensuring the card never accrues LOC interest or courtesy fees on top of its own CC_INTEREST / CC_LATE_FEE lifecycle events.

## Mathematical Models

### Amount Distributions

All channel → amount mappings live in a single amount-model table. Each channel has a single declared model; lookup failure is a hard runtime error.

**Denomination**: every median and floor below is a CALIBRATION-YEAR (2019)
dollar. Realized amounts multiply by the event year's index — SSA AWI for
labor income (salary, revenue, benefits), CPI-U for everything price-like —
from the embedded 1990–2024 era series (macro-history-v1 H1; see
[docs/era_data_provenance.md](docs/era_data_provenance.md) and
[docs/h1_nominal_scale_wiring.md](docs/h1_nominal_scale_wiring.md)). Behavioral
dollar screens scale with the amounts they screen; statutory amounts (the CTR
$10,000 threshold) and physical denominations (the $20 note, gift-card racks)
deliberately stay fixed — scaled amounts RE-SNAP to their lattices, so a 1991
ATM withdrawal is fewer $20s, never scaled $20s.

The era's REAL growth rides the COUNT axis instead (macro-history H4):
the medians below are what a transaction COSTS, and the measured
per-capita consumption path decides HOW MANY transactions happen — see
[Counts](#counts--gamma-poisson-mixture).

Channel-level lognormals (median, σ, floor):

| Channel | Median | σ | Floor | Source |
|---------|--------|---|-------|--------|
| salary | $3,000 | 0.35 | $50 | BLS QCEW 2024 (per-paycheck) |
| rent (all variants) | Γ(k=2, θ=400)+$50 | — | — | Census AHS 2023 |
| P2P | $45 | 0.80 | $1 | Fed Diary 2024 |
| bill | Γ(k=2, θ=400)+$50 | — | — | BLS CPI housing |
| external_unknown | $120 | 0.95 | $5 | Fed Payments Study 2024 (non-card remote) |
| ATM | $80 | 0.30 | $20 | Fed Payments Study, ATM Marketplace |
| self_transfer | $250 | 0.80 | $10 | |
| subscription | $15 | 0.40 | $5 | |
| client_ach_credit | $1,500 | 0.75 | $50 | |
| card_settlement | $650 | 0.60 | $20 | |
| platform_payout | $400 | 0.65 | $10 | |
| owner_draw | $2,500 | 0.80 | $100 | |
| investment_inflow | $5,000 | 1.00 | $100 | |
| fraud_classic | $900 | 0.70 | $50 | FATF 2022 |
| fraud_cycle | $600 | 0.25 | $1 | |

Merchant category lognormals (median, σ) — ClearlyPayments 2025, FMI 2024, Fed Diary 2024:

| Category | Median | σ |
|----------|--------|---|
| grocery | $50 | 0.55 |
| fuel | $45 | 0.35 |
| restaurant | $28 | 0.60 |
| pharmacy | $25 | 0.65 |
| ecommerce | $85 | 0.70 |
| retail_other | $45 | 0.75 |
| utilities | $120 | 0.40 |
| telecom | $75 | 0.30 |
| insurance | $150 | 0.35 |
| education | $200 | 0.60 |

A sample from a lognormal with median $m$ and sigma σ uses `mu = ln(m)`, giving `X ~ exp(Normal(mu, σ²))` with median exactly $m$ and mean `m · exp(σ²/2)`.

### Counts — Gamma-Poisson Mixture

Transaction counts per account-day are drawn from:

$$
\lambda_{\text{day}} \sim \text{Gamma}(k, \theta = \text{base\_rate}/k), \quad n \sim \text{Poisson}(\lambda_{\text{day}})
$$

This is a Negative Binomial in disguise — it produces overdispersed, bursty counts matching observed human transaction behavior. Default shape `k = 1.5`, weekend multiplier 0.8.

**Era modulation (macro-history H4).** The per-day rate additionally
carries `realPceLevel(year)` — the measured real per-capita
consumption index (nominal BEA PCE per capita ÷ CPI-U, both embedded,
anchored to exactly 1.0 at the 2019 calibration year). One lookup per
simulated day frame multiplies into the same combined multiplier that
carries seasonality and momentum, so the count axis follows the measured
path: ≈0.67 in 1991, the 1990-91 and 2008-09 dips, the 2020 collapse
and 2021 rebound, and ≈1.09 at the frozen 2024 level. The 2001
recession is deliberately FLAT — that downturn slowed consumption
GROWTH without a per-capita level dip, and the model inherits exactly
what the series shows, nothing more. The window-level transaction budget
keeps its meaning as a CALIBRATION-LEVEL target: realized volume is
target × the year's real level, so a 2019 window reproduces today's
volumes exactly. Amounts are untouched by this factor (the channel split
under [Amount Distributions](#amount-distributions)), and because the
fraud budget is F = pL/(1−p) on the realized candidate count L, fraud
density stays era-stable while fraud volume follows the economy.

### Timing Profiles

Three hour-of-day profiles:
- **consumer**: peaks at 18:00, tapers toward midnight.
- **consumer_day**: peaks at 10:00, drops after 14:00 (retired / stay-at-home).
- **business**: peaks 9:00–12:00, vanishes after business hours.

Each profile's 24-element PMF is normalized; a CDF is precomputed at compile time (`consteval`) so the hour sample is an O(log 24) walk over a constexpr array. Minute and second are uniform.

### Momentum — AR(1)

Per-person daily rate multiplier:

$$
m_t = \phi \cdot m_{t-1} + (1 - \phi) \cdot 1.0 + \varepsilon_t, \quad \varepsilon_t \sim \mathcal{N}(0, \sigma^2)
$$

Default φ = 0.45, σ = 0.15, clamped to [0.20, 3.00]. At daily resolution this produces effective weekly persistence of ~0.35–0.50, a calibration choice rather than a published estimate; Tovanich et al. (2021, *EPJ Data Science* 10:24) motivate treating temporal spending persistence and burstiness as stable per-person features. Grounded in Barabási (2005, *Nature* 435:207–211) on bursty human dynamics; Goh & Barabási (2008, *EPL* 81:48002) formal memory coefficient; Karsai et al. (2012, *Scientific Reports* 2:397) universality across activity domains.

### Dormancy — Three-State Machine

States: ACTIVE → (p=0.0012/day) → DORMANT → WAKING → ACTIVE. Calibrated so ~35% of accounts experience ≥1 dormant spell per year: `1 - (1 - 0.0012)^365 ≈ 0.35`.

- Dormant duration: uniform [7, 45] days (vacation 7–21, hospital 5–30, seasonal 30–90).
- Wake ramp: uniform [2, 5] days, linear interpolation from dormant rate (0.05) back to 1.0.
- Dormant rate: 0.05 — not zero because subscriptions and bills still fire.

Rationale: ~35% of accounts show ≥14 consecutive zero-txn days per year (Fed Payments Study frequency data). Mules show "dormant → tester → spike"; legitimate accounts show "quiet → gradual ramp" (LexisNexis 2025; Unit21 2026 found reactivation transactions average 17× rule threshold, but legitimate reactivation ramps gradually while fraud reactivation is immediate).

### Paycheck Cycle Boost

On payday, trigger a `max_residual_boost × paycheck_sensitivity` multiplier that linearly decays over `active_days` (default 4). Max residual boost 10%. Residual is intentionally small because the day-to-day engine already isolates discretionary flows — this captures the leftover effect beyond recurring bills.

### Counterparty Evolution — Monthly

The configured monthly evolution profile contains:
- `merchantAddP` = 0.35 — add one new favorite, weighted by global merchant CDF.
- `merchantDropP` = 0.10 — drop a random existing favorite.
- `contactAddP` = 0.08 — add a new P2P peer.
- `contactDropP` = 0.03 — replace a contact slot with a duplicate (reduces effective diversity).

Only contact evolution is wired in production today. Merchant favorite
add/drop remains a structural TODO, so the two merchant probabilities above
must not be interpreted as realized monthly turnover. Tovanich et al. (2021)
motivates category diversity, persistence, and turnover as useful features;
the listed rates are modeling choices, not estimates from that paper.

### Seasonal Multipliers

Normalized to unit annual mean so enabling seasonality redistributes but doesn't inflate volume:

| Month | Multiplier | Rationale |
|-------|-----------|-----------|
| Jan | 0.88 | Post-holiday trough, "dry January" |
| Feb | 0.94 | Tax refund wave starts late |
| Mar | 1.04 | Refund spending peak |
| Apr | 1.02 | Late refund + final tax season |
| May | 1.00 | Mother's Day offsets Memorial Day drag |
| Jun | 0.98 | Summer, Father's Day modest bump |
| Jul | 0.97 | Mid-summer, Independence Day |
| Aug | 1.05 | Back-to-school ramp-up |
| Sep | 1.02 | Tail of back-to-school |
| Oct | 0.99 | Halloween + holiday pre-season |
| Nov | 1.16 | Black Friday + early holiday |
| Dec | 1.22 | Peak holiday spending |

Sources: NRF ($976B 2024 holiday, $1.01T projected 2025); Bank of America Consumer Checkpoint (2024–25); JPMorgan Chase Institute (income swings Feb/Mar/Apr/Dec); S&P Market Intelligence (2026) on refund → retail spending elasticity; NRF Back-to-School 2025 ($128B total); Visa Holiday Retail 2025 (+4.2% YoY). Deliberately moderate — bills/rent/salary don't seasonally fluctuate.

### Liquidity Multiplier

During day-to-day simulation each person gets a liquidity multiplier combining:
- Days-since-payday **relief** (boost for first `reliefDays`) and **stress** (ramp over `stressRampDays` after `stressStartDay`).
- Cash-on-hand ratio (clipped; the $75 cash-reference floor scales with the day's price level).
- Fixed-burden ratio (larger fixed monthly obligations → greater stress).

Product is clipped to [0, 1.10] with an absolute floor. Count-stage liquidity shaping is **soft** (never zeros out an entire person-day) — the authoritative ledger is what truly enforces affordability.

## Legitimate Transaction Flows

### Salary (Payroll)

Persona-conditioned salary probability, then scaled by policy `paidFraction = 0.74`
(the working-type weighted mean of the table below, re-derived when selection
moved to the timeline at macro-history H2):

| Persona | p(salary) |
|---------|-----------|
| salaried | 98% |
|freelancer | 8% |
| smallbiz | 4% |
| hnw | 12% |
| student | 12% |
| retired | 2% |

Selection keys on the WORKING-LIFE persona from the timeline: a seed student
uses their career destination's probability, a business owner who closes uses
their post-business type, and seed retirees draw no payroll at all. Paychecks
are clipped to the working span — students start at their career onset,
post-close owners at the business end, and everyone stops at their claiming
date or their death, whichever comes first.

Each recipient gets an employer, a payroll cadence (20% weekly, 55% biweekly, 15% semimonthly, 10% monthly), and a job tenure drawn from uniform [2, 10] years. The base salary is a calibration-year draw; paychecks realize at the pay-date year's WAGE index (SSA AWI), so nominal pay follows the measured economy-wide path. Idiosyncratic career progression compounds ON TOP as seeded annual real raises `Normal(0.015, 0.02)`; job switches trigger a `Normal(0.08, 0.06)` bump. Semimonthly pay days are 1/15 or 15/31; monthly is day 28/30/31. Weekend falls roll to previous business day. Posting lag is 0–1 days. The salary amount model is interpreted as one monthly paycheck; annualizing and dividing by `payPeriodsInYear` gives the per-paycheck amount at any cadence. The weekly/biweekly pay lattices exist in EVERY era — the fixed 2025 anchor date is a weekday/fortnight-parity reference only, so a 1991 window pays exactly as many paychecks as a 2025 one (this repaired a defect where 75% of employer cadences were silent before 2025).

### Rent

Persona-conditioned renter probability (given not a homeowner), scaled by `rentFraction = 0.55`:

| Persona | p(rent) |
|---------|---------|
| student | 50% |
| retired | 18% |
| salaried | 62% |
| freelancer | 58% |
| smallbiz | 35% |
| hnw | 10% |

Leases have tenure uniform [2, 10] years. Base rent is a calibration-year draw realized at each payment's year through the CPI price index; idiosyncratic lease progression compounds on top as seeded annual real raises `Normal(0.02, 0.015)`. On lease turnover the base is re-sampled. Monthly payment timestamps are jittered in days 0–5 with hours 7–22. Channel is selected by the rent router using the landlord-type-specific CDF described above — so the same tenant paying the same landlord produces a consistent Zelle/check/ACH/portal signature. The lease dies with the tenant: rent stops at death.

### Day-to-Day Spending

A day-by-day market simulator drives discretionary spending:

1. **Market build.** Each person gets `favK` ∈ [8, 30] favorite merchants (weighted by global merchant CDF) and `billK` ∈ [2, 6] billers. Exploration propensity ~ Beta(1.6, 9.5). Burst windows (optional) ~ 8% of people get a 3–9 day high-spending burst at a random point in the window.
2. **Per-day.** Build seasonal × momentum × dormancy × paycheck × weekday × day-shock × liquidity × era-level multiplier — the last is H4's measured real consumption index (see [Counts](#counts--gamma-poisson-mixture)). Target count per person-day is back-calculated from the monthly target, inverting the suppressors; that target is a CALIBRATION-LEVEL quantity, so realized volume is target × the year's real level. Dead spenders' person-days are skipped — the dead spend nothing.
3. **Per-transaction.** Sample channel from `(merchant, bill, P2P, external_unknown)` CDF (weights: 0.82/0.10/0.08 of the non-unknown split, plus `unknownOutflowP = 0.05` carved out). Ticket draws realize at the day's CPI level.
4. **Merchant routing.** 82% of the time pick from favorites; else explore (with rejection if the explored merchant is already a favorite). Payment method is card (if the person holds one and `ccShare` rolls) or deposit account.
5. **Monthly boundary.** The commerce evolver adds/drops a merchant favorite and shuffles P2P contacts per the evolution config.

From a spender's Social Security claiming day onward, every ticket draw
additionally scales by 0.88 — the Aguiar–Hurst retirement consumption step
(~−12%, macro-history H2). It applies only to working-seed archetypes retiring
in-window; seed retirees are exempt because their archetype already carries
retired-calibrated rate/amount multipliers. The payday-anchored liquidity
machinery (relief, stress, paycheck boost) re-anchors automatically at
retirement: payday days derive from each person's ACTUAL inbound deposits and
the SSA channels count as payday inbounds, so the cycle follows the deposit
stream from paychecks to benefit Wednesdays with no special-case code.

### Subscriptions

Each person gets 4–8 subscription "intents" with 55% actual debit probability. Prices are drawn from a pool of realistic SaaS / streaming amounts ($6.99, $9.99, $14.99, $17.99, $49.99, $99.99, etc. — calibration-year prices; each debit realizes at the DEBIT month's price level, so subscription pricing tracks the era). Billing day uniform [1, 28] with ±1 day jitter per cycle. Subscriptions are contractual: they keep debiting the estate after death and stop at ACCOUNT CLOSURE (death + settlement).

### ATM

88% of people are ATM users. Monthly withdrawals uniform [1, 6]. Amounts drawn from a pool weighted toward round multiples of 20/40/60/100 (matches real ATM UX), scaled to the event year's price level and RE-SNAPPED to the $20 note lattice — a 1991 withdrawal is fewer $20s. 75% of withdrawals bias to days 0–18 of the month, 25% to days 18–28. Destination is the bank's ATM network hub account. Withdrawals stop at death.

### Self-Transfers

45% of multi-account holders actively self-transfer. Monthly [1, 3] transfers, 45% round amounts from pool, 55% lognormal($250, σ=0.80). Round picks scale to the event year then re-snap to a $25 round-amount lattice so the round-number signature survives every era. 70% bias to days 0–6 (post-payday "move to savings"). Source is the account with most available cash; destination is the leanest. The dead move no money between their own accounts.

### Direct-Deposit Splits (Paychecks)

For multi-account holders: 30% split their salary. 10–35% of each salary deposit is routed within 5–30 minutes to a secondary account. APA 2024. (Splits consume the payday-inbound stream, so they stop with the death-clipped income.)

### Government Benefits (SSA Wednesday Cohorting)

Post-1997 SSA/RSDI cycle payment rule:
- Birth day 1–10 → 2nd Wednesday of month
- Birth day 11–20 → 3rd Wednesday
- Birth day 21–31 → 4th Wednesday
- If Wednesday is a federal holiday, pay the preceding business day.

The Wednesday cohort derives from the person's REAL modeled birth day-of-month
— the same single birth-date carrier that renders every exported DOB — so the
payment calendar, the PII, and the retirement timeline all share one age axis
(macro-history H2 step 2a; the older synthetic hash-derived birth day is
retired).

**Social Security** (defaults, SSA 2026 COLA):
- Eligibility: 87% of eligible retirees (SSA Dec. 31, 2025 fact sheet). Selection is timeline-driven: anyone RETIRED BY WINDOW END — including mid-window claimants — is a candidate, and deposits begin at max(window start, claiming date), so a worker who claims mid-corpus starts drawing that month. Deposits end at death — benefits die with the beneficiary (survivor benefits are a registered upgrade).
- Monthly benefit: lognormal median $2,071 (estimated Jan 2026 avg retired-worker benefit), σ = 0.30, floor $900.

**Disability**:
- 4% of non-retired non-students.
- Monthly: median $1,630 (Jan 2026 avg disabled-worker benefit), σ = 0.25, floor $500.

Benefit levels are drawn once in calibration-year dollars; each monthly
deposit realizes at that month's wage index (one index era-wide and the
one-shot level draw are the declared simplifications — earnings-history-based
benefit levels and per-cohort COLA indexing are a registered upgrade).

The repo does NOT model the "paid on the 3rd" exceptions (pre-May-1997 entitlement, dual SSI, foreign residents) — all SSA-style benefits use the standard Wednesday rule.

### Non-Payroll Revenue

Freelancers, smallbiz, and HNW get a revenue engine (labor-income axis: draws realize at the month's wage index):

- **Freelancer**: 1–4 client ACH credits/month (median $1,400, σ = 0.70); 1–4 platform payouts (median $425); 1–2 owner draws (median $1,800). 12% quiet-month probability.
- **Smallbiz**: 0–3 client ACH (median $2,600); 0–3 platform (median $950); 4–12 card settlements (median $680); 1–2 owner draws (median $3,400). 6% quiet months.
- **HNW**: 0–2 investment inflows (median $6,500, σ = 1.05). 2% quiet months.

Revenue months are persona-gated by the timeline: a student plan stops
emitting at the career start, worker plans at the claiming date, business
plans at the close; retiree investment income and HNW inflows run until
death (those personas never transition away, but nobody earns past their
own funeral).

Routing: clients/platforms/processors land on the **business operating account (BOP…)** if the person has one; otherwise on personal. Investment inflows route to the **brokerage (BRK…)** if present. Business timestamps fall on business days (weekend rejection with retry). Cash takings stay on the $10 bill lattice after scaling, which keeps the exact-$10,000 CTR boundary reachable.

## Family and Social Flows

The family graph is built from three configs: household partitioning (`singleP` = 29%, Zipf α = 2.2, `spouseP` = 62%), dependent structure (65% student-dependent, 35% co-resident, 70% two-parent), and retiree support ties (35% has-adult-child, 35% supports).

Family amounts below are calibration-year dollars; each transfer realizes at
its event date's price level. Gift flows stop at death — a row whose source
or target owner is dead at its timestamp is dropped (external `XF…` family
members carry no modeled deaths — declared).

### Allowances (parent → student child)

60% weekly / 40% biweekly. Amount follows Pareto (xm = $15, α = 2.2).

### Tuition (parent → education merchant)

55% probability per student per scheduling cycle. 4–5 installments; total per semester is lognormal(μ = 8.7, σ = 0.35). Installments every ~30 days with ±5 day jitter.

### Retiree Support (adult child → retired parent)

Configured supporters with `hasChildP = 0.35` and `supportP = 0.35`. Amount = base Pareto × persona support-capacity weight (i.e. HNW child sends more than salaried child even to the same parent).

Supporter count sampling: 65% → 1 child, 27% → 2, 8% → 3. Spousal sync: if one retired parent is supported, there's an 85% chance the spouse shares the same supporters.

### Spouse Transfers

60% of couples keep at least partially separate accounts (Census 2023, Bankrate 2024). Those couples get 2–6 monthly transfers, lognormal($85, σ = 0.9). Breadwinner asymmetry 65% — higher earner (by persona amount multiplier) sends more often than vice versa. Pew 2023: 55% of marriages have husband as primary breadwinner, 29% egalitarian, 16% wife breadwinner — the repo uses amount multiplier as a gender-agnostic proxy.

### Parent Gifts (working parent → adult child)

HRS longitudinal data: 35% of parents 51+ transfer to adult children over a two-year window. Savings.com 2025: 50% support adult children, avg > $1,300/month among givers. Modeled as 12% monthly probability per eligible parent-child pair, Pareto(xm = $75, α = 1.6) scaled by parent's persona weight.

### Sibling Transfers

31% of family loans are between siblings (FinanceBuzz 2024); 76% have borrowed from a sibling at some point (JG Wentworth 2025). 15% of sibling pairs are "active"; those pairs transfer with 18% monthly probability, median $120, σ = 0.90, direction roughly 50/50.

### Grandparent Gifts

38–45% of 50+ households send money to younger generations (EBRI 2015). Implicit grandchildren found by two-hop traversal (`retired_person → children → their children`). 8% monthly probability, median $150, σ = 0.70.

### Funerals and Estates (death-caused)

Every in-window death produces, on the isolated family-inheritance lane:

- **A funeral**: one bill-channel payment from the decedent's account to the
  external service-merchant hub at death+3–10 days. Amount is lognormal,
  median $6,300 calibration dollars — the NFDA 2019 General Price List blend
  of a funeral with viewing and burial ($7,640) and cremation with viewing
  ($5,150) at the ~55% 2019 cremation rate — σ = 0.40, floor $1,000,
  realized at the death year's price level.
- **An estate**: the decedent's estate distributes to the heirs (direct
  children, else supporting children) at death+30–90 days (a declared
  probate-settlement window). Interim size lognormal median $25,000,
  σ = 1.0 — an SCF-anchored net-worth re-derivation is a registered
  upgrade. Heirless estates are not yet distributed (declared).

Both land strictly before the decedent's ACCOUNT CLOSURE (death + 120 days),
so the visible corpus always carries them. The old uncaused inheritance
hazard (0.15% of retirees per 180-day window, no death attached) is retired.

### Family External Accounts

`externalP = 0.18` — probability that a family counterparty banks at a different institution. Co-residing families overwhelmingly share a bank; non-co-residing adult children / siblings are more likely to bank elsewhere (FDIC 2023 + market fragmentation). Deterministic: a given person ID + coin-flip threshold always resolves the same way across runs, so every generator sees the same family member as internal or external without sharing state.

### Social Graph (P2P)

The social builder constructs a contact matrix: each person has `effectiveDegree` (default 12) contacts drawn from a community-aware weighted graph.

- Communities: contiguous blocks of `[6k, 24k]` people, where k is effective degree.
- Social capital: per-person lognormal(σ = 1.1), with configured hub people getting a 25× multiplier.
- Local probability: 70% within-block; remainder split between explicit cross-block (29%) and full global (1%).
- Tie strengths: Gamma(shape = 1.0) drawn per unique contact, turned into a CDF, sampled by the fixed-width contacts row so stronger ties appear more often.

## Fraud Typologies

Fraud population shape:

- Rings per 10k people: lognormal mean 6, σ = 0.4.
- Ring size: lognormal(μ = 2.0, σ = 0.7), clamped [3, 150].
- Mule fraction per ring: Beta(2, 4), clamped [10%, 70%].
- Victim count per ring: lognormal(μ = 3.0, σ = 0.8), clamped [3, 500].
- Solo fraudsters: 4 per 10k.
- Multi-ring mules: 6% (Merseyside OCG study: 63% of groups cooperated with at least one other — creates cross-community edges that break the "isolated cluster = fraud" assumption).
- Repeat victims: 10%.
- Ceiling: total fraud participants ≤ 6% of population.
- Target illicit transaction ratio: 0.5%.

Fraud steals era dollars: ring-rail amounts (and the continuous unauthorized
card/ATO draws) realize at the event year's price level. Structuring is the
deliberate exception — its band is anchored to the statutory CTR threshold,
which has been unindexed since the 1970s, so in 1991 the same $9,950 bites at
roughly twice today's real value. Card-testing anchors and gift-card rack
denominations also stay fixed-nominal (round amounts ARE the signature).

### Typologies

Each ring is assigned one typology by weighted choice (defaults: 30% classic, 15% layering, 10% funnel, 10% structuring, 5% invoice, 30% mule):

- **Classic** — victim → mule → fraud + optional cycle passes through the ring.
- **Layering** — victim → entry mule → 3–8 hops of mule/fraud nodes → exit cashout.
- **Funnel** — many sources → one collector → several cashouts.
- **Structuring** — victim splits N payments just under the $10k threshold (CTR avoidance), 3–12 splits of `amount = 10000 - Uniform(50, 400)`. Fixed-nominal in every era (class S).
- **Invoice** — fraud → biller (mimics business-to-vendor payments; $10-snapped era dollars).
- **Mule** — high fan-in (8–25 inbound sources) + rapid forwarding (1–12 hour delay, 90–95% passed through with 5–10% haircut). This is the textbook FATF 2022 profile: many small-to-medium inbounds, fewer larger outbounds, concentrated in a 5–14 day burst window.
- **Solo** — individual bad actor with no ring affiliation.

### Camouflage

Each participating ring account receives additional **legitimate-looking** transactions to raise the noise floor:
- Monthly bill payments: 35% probability per account per month.
- Daily small P2P: 3% per account per day.
- Recurring salary: 12% of ring accounts receive a plausible payroll stream.

Camouflage events fire with `isFraud = 0` and `ringId = -1` so they blend into the legitimate population for anyone looking only at flags. Camouflage scales with the index of the flow it mimics (bills/P2P ride the price index, the salary mimic rides the wage index) — a cover row scaled differently from its cover class would be a detectable artifact.

### Burst Window

Rings operate in a 7–14 day burst. Device and IP sharing is concentrated to this window; outside it the ring members behave legitimately. Rings never recruit the dead: each ring's bursts and camouflage clamp to its participants' alive horizon (the earliest death among its fraud actors and mules), so no ring member moves money after their own death. Card/ATO victims may be targeted during the modeled estate-settlement tail, but never outside their `[join, close)` membership interval; victim-authorized scams additionally require the victim to be alive. A compromise is accepted only when its complete sampled span fits before the earliest relevant victim/payee boundary, so boundary cases are rejected rather than compressed into artificial bursts.

### Shared Infrastructure

- Shared device probability (ring): 80%.
- Shared IP probability (ring): 75%.
- Legit shared-device noise: 1% (household ambient activity).
- During a fraud transaction, the transaction factory uses the shared ring device with probability 0.85 and shared IP with probability 0.80.

## Infrastructure and Device Attribution

### Devices

Each person has 1 device (80%) or 2 devices (20%). Device types are drawn uniformly from `{android, ios, web, desktop}`. Sparse legit shared groups (1% noise) model a family ambient device or a household tablet. In the card-fraud export, personal, shared, and attacker devices all use the same fixed-width opaque `D…` identifier namespace; owner role is not encoded in the prefix, width, or numeric range.

### IPs

Each person has 1–3 IPs (1 + Bernoulli(0.35) + Bernoulli(0.10)). Deterministic random IP avoiding invalid NANP ranges.

### Router

Sticky "current" device/IP per person. Default switch probability 5% per transaction. During fraud transactions the transaction factory consults the shared-infra map first and falls back to personal infra only if the ring device/IP roll misses.

## Chronological Replay and Screening

The authoritative pre-fraud replay is the single source of truth for balance-gated drops and liquidity-event emission. It stable-sorts by the composite audit key `(timestamp, source, target, amount, fraud flag, ring, channel, device, IP)` and processes each transaction:

1. Attempt transfer. On acceptance, check whether the debit tapped a COURTESY-protected account into negative — if so, emit an overdraft-fee transaction (capped at 3/day).
2. On insufficient-funds rejection, try to find a future inbound credit that would "cure" the shortfall:
   - Card-like channels (ATM, merchant, card_purchase, P2P): within 10 hours.
   - Retryable ACH-like channels (bill, rent, subscription, external_unknown, insurance, loans, tax): within 36 hours.

   Cure candidates are from the set of channels that legitimately top up an account: salary, government, insurance claims, refunds, self-transfers, incoming family support, etc.
3. If no cure exists, blind-retry with 55% probability after 18 hours (first retry) or 72 hours (second). Max retries: 1 for card-like, 2 for ACH-like.
4. For LOC accounts, billing events are pre-generated (23:55 on each account's cycle day) and merged into the stream ahead of the authoritative stable sort, whose composite audit key orders same-timestamp balance changes before the interest computation. Periodically the replay invokes the LOC accrual tracker, which sweeps every enabled account, rolls its dollar-seconds integral forward to the current timestamp using the current cash value, and for any account whose last billing was ≥ 30 days ago emits an interest accrual. The ledger debits interest from cash (bypassing the funding check) and forwards a liquidity event to the sink. Accounts whose `lastBillingTs = 0` are silently anchored on first sweep so billing does not fire retroactively over the simulation's pre-history.

The post-fraud replay uses the same logic but with liquidity-event emission disabled so overdraft fees and LOC interest already in the stream aren't double-posted.

Upstream **soft screens** in individual generators (ATM, subscriptions, self-transfers, day-to-day) use a scratch copy of the initial ledger to avoid glaringly unaffordable proposals. These don't replace the final replay — they just reduce the drop rate.

## Export Formats

### Standard

Vertex tables: `person`, `accountnumber`, `phone`, `email`, `device`, `ipaddress`, `merchants`, `external_accounts`.

Edge tables: `HAS_ACCOUNT`, `HAS_PHONE`, `HAS_EMAIL`, `HAS_USED`, `HAS_IP`, `HAS_PAID` (aggregated). The raw ledger is
the streamed `transactions` table, shared by every use case (`SELECT * FROM transactions ORDER BY row_seq`).

The VISIBLE corpus behind `HAS_PAID` and the flow aggregates filters every
row on both endpoint owners' membership intervals `[joinTs, closeTs)`: a
joiner's pre-join history and anything at/after a decedent's account closure
is invisible, exactly as a bank's own books would be. The entity-resolution
`customer.csv` carries `created_at` (the join instant) and `closed_at`
(the closure instant when it lands inside the window; empty while open).

### Mule-ML

Optimized schema for GraphSAGE / node-level mule detection:

- `Party` — one row per account with fraud label, phone, email, full deterministic identity (name, SSN, DOB, address, geo, country, canonical IP, canonical device).
- `Transfer_Transaction` — raw ledger.
- `Account_Device`, `Account_IP` — aggregated account-infra edges with counts and first/last seen.

Ages render from the single per-person birth-date carrier (persona
band-shaped draws — e.g. retired: Beta-weighted across 65–99; student:
16–34), shared by every exporter, the SSA payment calendar, and the persona
timelines. Joiners' ages anchor at their join date. Addresses use `faker-cxx`
together with deterministic zip-code lookups so addresses resolve to real US
cities, with a fallback list.

### AML — TigerGraph AML_Schema_V1

Full graph export with:

- **Vertices**: Customer, Account, Counterparty, Name, Address, Country, Watchlist, Device, Transaction, SAR, Bank, MinHash buckets (Name / Address / Street_Line1 / City / State), Connected_Component.
- **Edges**: customer_has_account / account_has_primary_customer, send/receive_transaction (customer side) + counterparty_send/receive_transaction (counterparty side) + sent/received_transaction_to/from_counterparty (aggregated), uses_device, logged_from, customer/account/counterparty/bank/address has_name / has_address / associated_with_country, customer_matches_watchlist, references (SAR → Customer with role), sar_covers (SAR → Account with activity amount), beneficiary_bank / originator_bank, resolves_to (counterparty → customer soft link), MinHash bucket edges.

**Customer persona and lifecycle**: the Customer vertex's `customer_type`
(and the demographic/occupation attributes derived from it) reports the
END-OF-WINDOW persona — the lifecycle state at the corpus's final timestamp —
so a worker who retires mid-window exports as a retiree, exactly as a bank's
CRM would show them at export time. The Customer status cell flips
`active → closed` once the person's ACCOUNT CLOSURE (death + settlement)
precedes the corpus end. The AML corpora remain FULL-WORLD exports (no
membership row filter — declared); the AML onboarding date is a synthetic
backdated derivation, not the membership joinTs (a declared inconsistency;
aligning them is registered).

**SAR generation**: one SAR per fraud ring (filed 30 days after the last illicit transaction, per BSA); one SAR per solo fraudster. Violation type inferred from dominant fraud channel: structuring → `structuring`, invoice → `suspicious_activity`, everything else → `money_laundering`. `activity_amount` per account is both-sides (in + out) throughput, not a share.

**MinHash**: byte-for-byte compatible with TigerGraph's reference `TokenBank.cpp`. Uses Austin Appleby's MurmurHash2 on byte shingles (k=3), plus the exact 101-element c1/c2 universal-hash coefficient tables from the reference. Bucket IDs include the band index (`{PREFIX}_{band}_{hash}`) to keep LSH bands independent. Reference C source is embedded as a comment in the implementation for migration / validation.

### AML — Transaction-Edges with Derived Features

A variant of the AML schema that swaps the aggregated `HAS_PAID` projection for a transaction-edge view and adds graph-derived account features (PageRank score, Louvain community ID, weakly-connected-component ID and component size, shortest path to mule, IP/device collision counts, in/out mule-ratio, multi-hop mule count, betweenness, in/out degree, clustering coefficient). Intended as input to graph-feature-aware AML models that consume both the ledger and the precomputed topology signals. Its Customer table reports the same end-of-window persona and `active`/`closed` lifecycle status as the AML export.

### Card-Fraud — TigerGraph TF_GNN_v3 (temporal graph learning)

A transaction-fraud corpus shaped for TigerGraph's TF_GNN_v3 GSQL schema. The primary downstream
use case is a **temporal graph neural network (TGN) predicting fraud at the transaction level**:
every payment carries a timestamp and timestamped device/IP session edges, so the corpus replays
as a continuous-time event stream rather than a static graph snapshot. 43 tables land in schema
`card_fraud` (prefix `cf_`):

- **`Payment_Transaction`** — the card view of the corpus: `card_purchase` rows (credit-card
  purchases plus unauthorized-card and gift-card-scam fraud rows) and `merchant` rows
  (account-paid POS, interpreted as debit-card transactions). Unauthorized rows currently use
  the victim's primary account and therefore derive a debit-card identity. They are not
  relabeled as issued-credit-card activity after generation: fraud is planned only after the
  credit-card lifecycle has closed its statements, so doing that honestly requires reordering
  fraud planning into the statement/payment/interest lifecycle.
  8 loaded columns: `id` (`T<row_seq>`, cross-referencing the streamed `transactions` table 1:1),
  timestamp, amount, `is_fraud`, unix time, merchant category, `use_chip`, and `error`. Streamed
  at row scale during settlement together with the `Card_Send_Transaction`,
  `Merchant_Receive_Transaction`, `Transaction_Uses_Device`, and
  `Transaction_Uses_IP` edges.
- **Cards and parties** — `Card` (credit cards resolve through the card registry, ≤1 per person;
  every other view source becomes the account's derived debit card), `Party` (canonical
  customer ids; `created_at` is the membership joinTs), and `Party_Has_Card`. Both carry an
  `is_fraud` column that is always **0** — see the label note below.
  `Is_Merchant` carries the beneficial-owner edge for view-observed merchants (~42% of the
  catalogue, draw-free from world state; `merchant-ownership-2026-07`). It asserts IDENTITY,
  not money flow — a card purchase still settles to the merchant's sink, not the proprietor.
- **Merchants and geography** — `Merchant`, `Merchant_Category`, `Merchant_Assigned`, and a
  consistent `City`/`State`/`Zipcode` chain (`Has_City`/`Has_State`/`Has_Zip`, `Assigned_To`,
  `Located_In`) read the merchant's world-modeled location. The current
  71-US-city catalogue is a runnable placeholder, not Census-complete ZCTA or
  establishment data.
- **PII investigative layer** — `Address`/`Phone`/`Email`/`IP`/`Device`/`ID`/`Full_Name`/`DOB`
  vertices plus the non-infrastructure `Has_*` edges from the PII synthesis; the `IP`/`Device`
  `is_blocked` column is always **0** — see the label note below. (Marked demo-only in
  TF_GNN_v3; PhantomLedger populates it.) `Has_Device` and `Has_IP` are POPULATED as of
  `attacker-infra-2026-07`: they are the institution's INCOMPLETE endpoint registry
  (~72% device / ~61% address coverage), not ground-truth ownership, so a missing edge is
  weak evidence rather than an attacker tell. Measured "not on file ⇒ fraud" precision is
  0.027 at 2.9x lift. Use them for graph structure; use the timestamped
  `Transaction_Uses_Device`/`_IP` edges for anything point-in-time.
- **Transaction-time infrastructure** — `Transaction_Uses_Device` and `Transaction_Uses_IP`
  connect every payment to the device and IP actually observed for that session, with
  `edge_unix_time`. Their endpoint vertices include exogenous attacker sessions as well as the
  synthesized customer roster. Score a transaction before appending its current session edges
  to temporal memory.
- **`Ground_Truth_Label`** — the investigative overlay: `(entity_type, entity_id, label)`,
  positives only, joinable 1:1 to the vertex tables. No TF_GNN_v3 loading job reads it and
  no edge points at it. It is one of the 37 physical tables but is outside the feature graph.

**Labels (card-fraud-realism-v2).** `Card.is_fraud`, `Party.is_fraud`,
`Device.is_blocked` and `IP.is_blocked` are full-window entity verdicts — "this
card ever carried a flagged row". Exported as features they answer the training
question before a model sees a transaction, so all four are **written as 0**.
The columns are kept, not dropped, because TF_GNN_v3 loading jobs map
positionally. The one supervised target in the graph is
`Payment_Transaction.is_fraud`, which is observable at its own row's timestamp;
the entity verdicts live in `cf_Ground_Truth_Label`, outside the graph, for
evaluation only.

A 10,000-person/60-day realism audit found that every one of 748 fraud rows
targeted a merchant with no legitimate card-view transaction, and that every
unauthorized-fraud row carried a TEST-NET-2 attacker IP that no legitimate row
could collide with. **Both shortcuts are now closed in generation**: fraud
card-rail destinations are drawn from the same merchant acceptance catalogue as
legitimate spend, modality-conditioned through the same distance-decay kernel,
and attacker IPs come from the same sampler as everyone else's. A
merchant-ID-only baseline is gated in the suite (`test_card_baselines`), which
columns a model may read is a written and tested contract
([docs/card_fraud_feature_contract.md](docs/card_fraud_feature_contract.md),
pinned by a truncation experiment in `test_card_point_in_time`), and per-year
prevalence, channel, typology, amount and episode bands are gated by
`test_card_prevalence`. This is suitable for building a point-in-time temporal
GNN pipeline, but it is not yet a public calibrated online benchmark. Remaining
gates include fraud-level calibration, integrating unauthorized credit-card
activity into the statement/payment/interest lifecycle, effective-dated
card/device/residence lifecycles, era-varying fraud technology and rail mix,
operationally delayed labels, and an executable, tested TigerGraph
GSQL/training/evaluation pipeline.
Follow the causal feature, split, metric, and minimum-realism gates in
[docs/card_fraud_online_gnn.md](docs/card_fraud_online_gnn.md).

The generator emits **loaded attributes only**: the pagerank/community slots, the engineered
Payment_Transaction features, and TF_GNN_v3's interaction/co-occurrence/community edges are
in-graph TigerGraph query work. The repository does not yet ship the production GSQL feature
query or a complete training/evaluation runner. Column order matches the TF_GNN_v3
loaded-attribute order so loading jobs map positionally; to hand a table to TigerGraph as CSV:
`\copy card_fraud."cf_Payment_Transaction" TO 'Payment_Transaction.csv' WITH (FORMAT csv, HEADER true)`.

## Configuration

The CLI exposes a small set of top-level knobs (`--days`, `--population`, `--seed`, `--start`, `--usecase`). Everything beneath that — persona shares, channel medians, fraud weights, family probabilities — is declared in code as constants or as `Rules` / `Flow` / `Profile` structs that travel with the subsystem that consumes them. Reference data is embedded as constexpr tables (the era macro history in `synth/econ/era_data.hpp`, the geographic catalogue in `synth/geo/geo_data.hpp`) — the repo carries no external data files and the generator never fetches at run time.

All configuration structs participate in a uniform validation pass: each carries a `void validate(validate::Report&) const` method that posts named, source-located checks. The orchestrator collects a single `Report` and throws a `validate::Error` if any check fails, so misconfigured rules surface at construction time rather than as silent zeros mid-simulation.

Three guarantees are enforced at compile time rather than at validation time:

- Persona shares: a `consteval` check ensures non-salaried shares never exceed 1.0.
- Indexable enums: `static_assert(enums::isIndexable(...))` on any enum used as an array index.
- Timing PMFs: hour-of-day distributions are normalized and CDF'd via `consteval` so the hot path is a constexpr table lookup.

## References

**Banking & Payments**
- Bankrate 2025 — overdraft fee averages; linked savings sweep adoption.
- Bankrate 2026 — auto insurance premiums.
- CFPB 2024 — consumer opt-in rates for courtesy overdraft.
- Consumer Reports 2024 — bank overdraft fee composition.
- FDIC 2023 Survey of Household Use of Banking Services — family cross-institution rates.
- FDIC Summary of Deposits 2024 — national bank deposit market share.
- Fed Diary of Consumer Payment Choice 2024 — P2P, ATM distributions.
- Federal Reserve G.19 Q4 2024 — credit card APR (21.5%).
- Federal Reserve Payments Study 2024 — non-card remote payment medians, transaction frequency distributions.
- Federal Reserve SHED 2023: 82% of US adults have ≥1 credit card.
- Federal Reserve Bank of New York Q4 2024 — 60+ day auto-loan delinquency (2.1%).
- NFIB 2023 Small Business Survey — 56% of small businesses bank personal+business at same bank.
- SoFi / NerdWallet 2025 — overdraft LOC prevalence.

**Macro History (era-correct dollars, activity levels, lifecycles & mortality)**
- FRED CPIAUCNS (BLS CUUR0000SA0 mirror) — CPI-U annual averages 1990–2024, verified exact.
- SSA Average Wage Index (ssa.gov) — AWI levels 1990–2024, verified exact.
- BEA A794RC0A052NBEA / B230RC0A052NBEA (FRED) — nominal per-capita PCE and population. Deflated by CPI-U, the PCE series is the H4 real activity level that modulates session transaction counts (≈0.67 at 1991, 1.0 at the 2019 calibration year); the population series sizes the H3 join-cohort replenishment.
- BLS LNS14000000 (cross-checked via FRED UNRATENSA) — unemployment annual averages.
- SSA Period Life Table 2023 (table 4.C6) — embedded mortality; drives the H3 lifespan hazard (one period table era-wide, declared).
- SSA 1983 Amendments — the statutory full-retirement-age schedule (65→67 by birth cohort; `timeline::fraMonths`).
- SSA Annual Statistical Supplement — claiming-age distribution (the H2 claiming mixture: 62 / pre-FRA / FRA / post-FRA / 70).
- NCES / BLS — school-exit and labor-market-entry ages (the student work-start band, 19–28).
- BLS Business Employment Dynamics — establishment survival (~50% at five years; the business-close hazard).
- Aguiar & Hurst 2005, *Journal of Political Economy* 113(5) — the retirement consumption drop (the ~−12% spending step at the claim).
- NFDA 2019 General Price List survey; NFDA/CANA cremation rate — the funeral cost blend (burial $7,640 / cremation-with-viewing $5,150 at ~55% cremation → $6,300 median).
- 31 CFR 1010.311 — the $10,000 CTR threshold, statutory and unindexed (class S).
- Provenance and refresh contract: [docs/era_data_provenance.md](docs/era_data_provenance.md); wiring contracts: [docs/h1_nominal_scale_wiring.md](docs/h1_nominal_scale_wiring.md), [docs/h2_persona_timeline.md](docs/h2_persona_timeline.md), [docs/h3_mortality_estate.md](docs/h3_mortality_estate.md), [docs/h4_macro_modulation.md](docs/h4_macro_modulation.md).

**Credit Cards**
- Experian: avg 3.9 cards/holder (2023 data); avg total credit limit across all cards $29,855 (Q3 2023, aggregate rather than per-card).
- TransUnion Q4 2024 CIIR: avg carried balance $6,580 (Q2 2024 was $6,329).

**Mortgages & Auto Loans**
- Census AHS 2023: median mortgage payment $1,672 (within the $1,520–$1,960 band across ACS/AHS/NMDB measures). Census/ACS: ~60% of owner-occupied homes carry a mortgage (59.7% in the 2024 ACS).
- Experian State of the Automotive Finance Market Q2 2024: payments $734 new / $525 used; ~80% of new and ~36% of used purchases financed.
- Freddie Mac 2024 — effective mortgage duration ~7–10 years.
- Insurance Information Institute 2024 — 4.2 auto claims/100 drivers/year; avg collision claim $4,700.
- MBA Q3 2024 — 3.5% of mortgages 30+ days delinquent.
- Triple-I 2024 — 5–6% of home policies claim/year; avg payout $15,749.

**Student Loans**
- FSA FY 2024 Annual Report — ~45M borrowers, $1.6T outstanding.
- StudentAid.gov — standard 10-year plan, grace period 6 months, IDR forgiveness 20/25 years.

**Insurance**
- Census 2024 — 92% of US households own a vehicle.
- NW Mutual 2024 — 52% of Americans have life insurance; 26% expect to leave inheritance.
- Ramsey 2025 — avg home insurance $163/month.

**Social Security & Disability**
- SSA 2026 COLA fact sheet — Jan 2026 avg retired-worker benefit $2,071; avg disabled-worker $1,630.
- SSA Dec. 31, 2025 fact sheet — 87% of 65+ receive benefits.
- SSA payment calendar & handbook — post-1997 RSDI Wednesday cohort rule.

**Housing & Rent**
- 2021 Rental Housing Finance Survey (HUD/Census) — unit-weighted landlord typology.
- CRS R47332 — LLP/LP/LLC "mom-and-pop wrapper" discussion.
- Harvard JCHS — LLC gray-zone analysis.
- Baselane 2024 — landlord rent-acceptance behaviors.
- TurboTenant 2024 — 65% of digital rent flows over ACH.

**Tax & Income**
- IRS 2024–25 — refund volumes, ~72% of filers receive refunds averaging $3,500+.
- JPMorgan Chase Institute 2024 — household income swings Feb/Mar/Apr/Dec.
- S&P Market Intelligence 2026 — refund → retail spending elasticity (~2% per 10%).
- BLS Quarterly Census of Employment and Wages 2024 — salary distributions.
- IRS Form 1040-ES — quarterly estimated tax filing dates (Jan 15, Apr 15, Jun 15, Sep 15).

**Family Transfers**
- Census 2023 — 60% of couples have partially separate accounts.
- EBRI 2015 — 38–45% of 50+ households transfer to younger generations.
- FinanceBuzz 2024 — 31% of family loans are between siblings.
- HRS longitudinal data — 35% of parents 51+ transfer to adult children over 2 years.
- JG Wentworth 2025 — 76% have borrowed from a sibling.
- LendingTree 2021 — median sibling-owed amount ~$400.
- Pew 2023 — marriage breadwinner composition; 24% say siblings have financial responsibility for each other.
- Savings.com 2025 — 50% of parents financially support adult children; avg > $1,300/month.

**Seasonality**
- National Retail Federation 2024/2025 — $976B 2024 holiday retail; $1.01T 2025 projection; Back-to-School $128B 2025.
- Bank of America Consumer Checkpoint 2024–2025 — seasonal spending troughs/peaks.
- Visa Holiday Retail Report 2025 — +4.2% YoY holiday retail.

**Human Dynamics & Spending Behavior**
- Barabási 2005 — "The origin of bursts and heavy tails in human dynamics," *Nature* 435:207–211.
- Goh & Barabási 2008 — memory coefficient formalization, *EPL* 81:48002.
- Karsai et al. 2012 — universal correlations in human activity, *Scientific Reports* 2:397.
- Tovanich, Centellegher, Bennacer Seghouani, Gladstone, Matz et al. 2021: "Inferring psychological traits from spending categories and dynamic consumption patterns," *EPJ Data Science* 10:24. Motivates the persistence/burstiness/turnover feature framing; note the paper's own conclusion is that trait inference from spending is hard, and the specific rates used in this repo are modeling choices.

**Fraud & AML**
- FATF 2022 — "Money Laundering Through Money Mules" typology report.
- Europol EMSC — operational mule-detection data.
- UK National Crime Agency — money mule behavioral profiles.
- LexisNexis 2025 — "Money Mule Detection Strategies."
- Merseyside OCG study — 63% of organized criminal groups cooperate with ≥1 other group.
- Unit21 2026 — legitimate reactivation transactions average 17× rule thresholds but ramp gradually.
- FATF 2020 — AML Red Flag Indicators.
- Rossi, Chamberlain, Frasca, Eynard, Monti & Bronstein 2020 — "Temporal Graph
  Networks for Deep Learning on Dynamic Graphs," arXiv:2006.10637. The
  continuous-time event framing the card-fraud corpus is built to feed.

**Small Business Banking**
- Bluevine 2025 — 39% of SMBs have < 1 month of operating expenses; healthy ones hold 2–3 months.

**Hash Reference**
- Austin Appleby — MurmurHash2 32-bit.
- TigerGraph DevLabs — `TokenBank.cpp` MinHash reference implementation (embedded as a comment in the AML MinHash module).
