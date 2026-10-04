# PhantomLedger

A synthetic retail-bank ledger generator for mule detection, AML graph analytics and transaction-pattern research. Legitimate accounts show every fraud behavior (temporal bursts, high fan-in, device sharing, rapid forwarding) in milder form, so fraud is not trivially separable: mules differ in degree, not in kind.

## Overview

PhantomLedger generates a bank-internal view of a synthetic population over a configurable window. Each person has a persona, accounts (possibly a credit card and a business operating or brokerage account), PII (phone, email, deterministic address), a family graph (household, spouse, parents, children, supported relatives), a social graph (weighted P2P contacts), devices and IPs with session history, and financial products. All tables are written to PostgreSQL during the run; no files are written.

- Research-grounded: every probability, median and sigma has a citation or a documented modeling choice.
- Era-correct: dollars scale to the event year ([Amount Distributions](#amount-distributions)), counts follow real consumption ([Counts](#counts-gamma-poisson-mixture)), and personas follow lifecycles with retirement, death and a join cohort ([Personas](#personas)).
- Structurally realistic: the ledger enforces affordability; overdraft protection is a per-account product with tier fees; LOC interest accrues on a dollar-seconds integral; merchants get business-checking seeds; credit cards are liability accounts.
- Strictly validated: misconfiguration fails at construction ([Configuration](#configuration)).

## Building

Requires CMake ≥ 3.23, a C++23 compiler (GCC 13+, Clang 17+ or MSVC 19.38+), Git (CMake `FetchContent` pulls [`faker-cxx`](https://github.com/cieslarmichal/faker-cxx) v4.3.2), and PostgreSQL client headers and `libpq` (a system dependency). Production runs need a reachable, writable PostgreSQL database.

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

| Variable | Default | Meaning |
|---|---|---|
| `CONFIG` | `Release` | CMake build type (`Debug`, `Release`, `RelWithDebInfo`). |
| `BUILD_DIR` | `build` | Out-of-tree build directory. |
| `TESTS` | `ON` | `PL_BUILD_TESTS`: build the C++ tests. |
| `BIN` | `phantomledger` | Binary name for the `run` targets. |
| `ARGS` | *(empty)* | CLI arguments for `make run` and `make run-fast`; the only variable that forwards the CLI. |
| `TOPICS` | `all` | Comma-separated topic filter for the diagnostics targets. |

```sh
make build CONFIG=Debug
make test BUILD_DIR=build-debug CONFIG=Debug
make run ARGS="--usecase standard --days 120 --population 200000"
# validated card-fraud invocation
make run ARGS="--start 1999-01-01 --days 1070 --population 50000 --seed 42 --usecase card-fraud"
```

Diagnostics are silent except under `run-info`, `run-debug`, `run-trace` and `run-mem`. [docs/debugging.md](docs/debugging.md) has every topic and level, golden-baseline triage and what to run per problem class.

Layout (LLVM-style): `include/phantomledger/<layer>/…` mirrors `src/<layer>/…` (the project prefix appears once), and a module is reviewed as the folder pair. Top-level folders are the dependency layers, enforced by the configure-time include-layer lint. Configure also fails if a `src/*.cpp` is not registered in `CMakeLists.txt`.

## Usage

```sh
phantomledger [options]
```

| Option | Default | Description |
|---|---|---|
| `--usecase {standard,mule-ml,aml,aml-txn-edges,card-fraud,mule-temporal}` | `standard` | Exporter to run. |
| `--days N` | `365` | Simulation length in days. |
| `--population N` | `70000` | Total population. |
| `--seed N` | `0xDEADBEEF` | Top-level RNG seed. |
| `--start YYYY-MM-DD` | `2025-01-01` | Simulation start date. |
| `--help`, `-h` | | Print usage. |

- The corpus streams into the `transactions` table during settlement; each exporter writes its tables during the run. The same seed and config rewrite byte-identical content.
- `PL_PG='host=... port=... dbname=...'` sets the connection (unset or empty: `dbname=phantomledger`). Without a reachable server the run fails before generating anything.
- `PL_FILE_ONLY=1` is test infrastructure only: a serverless mode producing just the corpus stream digest; `aml-txn-edges` cannot use it.
- Era coverage is 1990–2024. Outside it (including the default 2025 start) dollar scales and the real activity level freeze at the nearest covered year, with one stderr notice; the 2024 activity level is ≈9% above 2019. Card-fraud needs its whole window inside coverage (the era lock), so the default start fails for it.

Each use case writes its own schema, so same-seed runs in one database compose without collisions. IDs are canonical across use cases; `mule-temporal` pseudonymizes them.

| `--usecase` | Schema and tables |
|---|---|
| `standard` | `public` (unprefixed, beside the shared `transactions` stream) |
| `mule-ml` | `mule_ml.ml_ready_*` |
| `aml` | `aml.aml_*` |
| `aml-txn-edges` | `aml_txn_edges.aml_txn_edges_*` |
| `card-fraud` | `card_fraud.cf_*` |
| `mule-temporal` | `mule_temporal.mt_*` (opaque entity IDs, temporal schema) |

## Pipeline

`PhantomLedger::pipeline::SimulationPipeline` runs three stages.

Entities:

1. People with fraud roles: ring members, mules, victims, solo fraudsters ([Fraud Typologies](#fraud-typologies)).
2. Accounts: 1 to N per person (binomial, `maxPerPerson` default 3); the first is the primary deposit account.
3. PII: phone and email derived from the person ID.
4. [Merchants](#merchants), [landlords](#landlords) and [counterparty pools](#counterparty-pools), including billers, geographically placed cash and check points and crypto ramp venues (typed, external, ownerless contexts, not customer accounts).
5. Fixed institutions (SSA, disability, IRS), the bank's four income GLs ([Banking Mechanics](#banking-mechanics)), and `XF…` accounts for relatives who bank elsewhere.
6. Personas: archetype, a perturbed per-person `Persona` (lognormal noise, beta paycheck sensitivity), membership interval, birth date (isolated lane, at the person's anchor; the one age axis every exporter renders) and a lifecycle timeline with a death date ([Personas](#personas)).
7. Internal same-customer income accounts: `BOP…` business operating (freelancers, small businesses), `BRK…` brokerage (HNW).
8. [Credit cards](#credit-cards).
9. Portfolios by persona priors ([Financial Products](#financial-products)). Each loan or policy draws its servicer or carrier once at issuance, on its own RNG lane, from a national market-share table (NAIC for auto, home and life insurers; FSOC for mortgage servicers; institutional-providers amendment in `docs/fraud_model_audit.md`). Only providers in use are registered, as external ownerless accounts.

Infra: ring plans (burst window, members sharing a device or IP); devices, IPs and a sticky per-person router, with sparse legitimate sharing and ring-shared flagged devices and IPs ([Infrastructure](#infrastructure-and-device-attribution)); a ring → shared device/IP map used with high probability in fraud transactions.

Transfers:

1. The legit-transfer builder emits candidates in semantic order (income, routines, family, credit) plus a starting clearing house (balances, overdraft products, credit limits). Income and behavioral flows stop at death; contractual flows and card servicing stop at account closure.
2. Insurance premiums and claims are merged in.
3. One obligation emitter posts mortgage, auto, student-loan and tax payments with late, missed, partial and cure cycles and delinquency clustering.
4. The authoritative pre-fraud replay ([Chronological Replay and Screening](#chronological-replay-and-screening)) is the only place that records balance-gated drops and emits liquidity events (overdraft fees, LOC interest).
5. Fraud injection adds camouflage and illicit transactions up to `targetIllicitP` (default 0.5%).
6. A post-fraud replay runs with liquidity emission off, so nothing is emitted twice.
7. Every referenced account ID must be registered.

## Entity Generation

### IDs

| Prefix | Entity | Width |
|--------|--------|-------|
| `C` / `A` | Customer (person) / account | 10 |
| `L` | Credit-card account | 9 |
| `M` / `XM` | Merchant, internal / external | 8 |
| `E` / `XE` | Employer, internal / external | 8 |
| `LI`/`LS`/`LC` / `XLI`/`XLS`/`XLC` | Landlord individual / small LLC / corporate, internal / external | 7 |
| `IC` / `XC` | Client payer | 8 |
| `XP` / `XS` / `XO` / `XB` | Platform / processor / owner business / brokerage (external) | 8 |
| `XF…` | External family account | hash-derived |
| `BOP…` / `BRK…` | Same-customer business operating / brokerage (internal) | hash-derived |
| `XE09000001` / `XE09000002` / `XO1000000004` | SSA / disability / IRS | fixed |
| `XO1000100001` to `XO1000699999` | Lender and insurer pools, one 100,000-serial block per market (mortgage, auto loan, student loan, auto, home, life insurance) | fixed layout |
| `XO1001000001` to `XO1001001000` | Check-payee banks (bank of first deposit), one per external bank, ranked by FDIC Summary of Deposits share | fixed layout |
| `XP1000000001` / `XP1000000002` | P2P platforms (Venmo / Cash App) | fixed |
| `XM1100000001` onward | Funeral homes (MCC 7261): `1,100,000,000 + area * 10,000 + ordinal` | fixed layout |
| `GL00000001` to `GL00000004` | Bank income GLs: card interest, card fees (late fees), deposit fees (overdraft fees), credit-line interest; internal, ownerless | fixed |
| `XM1000000001` | Retired external-unknown catch-all: never registered, no row | fixed |
| `XO3000000001` / `XO4294967041` / `XO4294967042` | Retired card issuer / fee collection / overdraft line of credit: never registered, no rows | fixed |

- A leading `X` means external. `GL` is the bank's own internal account. `BOP`/`BRK` are internal: a same-bank business account is a book-to-book transfer, not an interbank counterparty (NFIB 2023: 56% of small-business owners keep personal and business at the same bank).
- The retired catch-all's flows now pay check-payee banks, identified remote merchants, P2P platforms and funeral homes, appended after the per-person payees.
- High-range `XS…` IDs include ATM terminal/acceptor, cash-depository and check-capture endpoints; `XP…` includes crypto ramp venues. They are registered so references stay valid, but are not customer accounts and never get a balance-bearing posting index. Clearing enforces channel, endpoint kind and direction ([Customer-ledger boundary flows](docs/customer_ledger_boundaries.md)).
- Strings are export rendering only. Internally an ID is a 16-byte plain-old-data `entity::Key` `{role, bank, number}`; the simulation core never allocates or compares ID strings.

### Merchants

Per 10k people: `per10kPeople` = 120 core and `longTailExternalPer10kPeople` = 400 long-tail merchants. Core size weights are lognormal (`sizeSigma` = 1.2); the long tail carries 18% of total weight (sigma 1.8) and is always external. Core merchants are in-bank with `internalP` = 0.02, an explicit modeling choice (the old documented 0.06 was stale); calibrating against an issuer/acquirer portfolio is a future model round. Categories: grocery, fuel, utilities, telecom, ecommerce, restaurant, pharmacy, retail_other, insurance, education.

### Landlords

A size law (`synth/counterparties/size_law.hpp`) over eight classes: the seven 2021 Rental Housing Finance Survey (HUD/Census) property-size columns (1, 2-4, 5-24, 25-49, 50-99, 100-149, 150+ units; CRS R47332 Tables 1 and 3), with the NMHC 2024 Top-50 owners carved out of 150+ as a rank-size row. Class c keeps `clamp(round(P x 0.35 x m_c), 1, F_c)` landlords (m_c: share of rental units; F_c: real property count), about one per expected renter in small classes and capped at real size in large ones. A lease picks its landlord by the same law (one uniform: class, then member). 200,000 people give 66,658 landlords; the largest, a Top-50 owner, has about 150 tenants.

Type comes from the class's unit mix (renter-weighted 43.3% / 13.8% / 42.9%). Channels follow Baselane 2024 and TurboTenant (individuals use Zelle and checks, corporate managers portal ACH); corporate and REIT landlords bank commercially with national scope.

| Type | RHFS owners | rent_p2p / rent_check / rent_ach / rent / rent_portal | In-bank |
|------|-------------|------------------|---------|
| Individual | Individual investor, trustee for estate, tenant in common (72.5% of single-unit, 4.9% of 150+ unit rental units) | 40 / 25 / 25 / 10 / 0% | 6% |
| Small LLC | LLC/LP/LLP and general partnership under 25 units: "mom-and-pop in an LLC wrapper" (CRS R47332, Harvard JCHS "LLC gray zone") | 15 / 30 / 55 / 0 / 0% | 4% |
| Corporate | All other forms, including LLCs and partnerships at 25+ units (professionally managed, portal-paying) | 0 / 0 / 5 / 0 / 95% | 1% |

### Counterparty Pools

Employers follow a size law over 17 classes (13 SUSB 2022 enterprise-size rows, the 20,000+ row as a rank-size tail, and federal, state and local government payrolls from QCEW 2022): class c keeps `clamp(round(P x 0.74 x m_c), 1, F_c)` employers (m_c: share of jobs; F_c: real firm count), and every job and job switch picks through it. 200,000 people give 89,231 employers, all external; the 148,000 workers use about 58,600; the federal payroll pays about 2,800 and the largest private employer about 1,700. The law and its metro reconciliation: counterparty-sizes-2026-09 amendment in `docs/fraud_model_audit.md`.

Other pools per 10k people (in-bank p): client payers 250 (2%, geographically diverse); owner businesses 200 (always external); brokerages 40; platforms 2; processors 1.

## Personas

| Persona | Share | Rate mult. | Amount mult. | Initial balance | Card p | CC share | Credit limit | Paycheck sensitivity |
|---------|-------|------------|--------------|-----------------|--------|----------|--------------|----------------------|
| student | 12% | 0.7 | 0.7 | $200 | 0.25 | 0.55 | $800 | Beta(4, 2), high |
| retiree | 10% | 0.6 | 0.9 | $1,500 | 0.55 | 0.55 | $2,500 | Beta(3, 3), moderate |
| freelancer | 10% | 1.1 | 1.1 | $900 | 0.65 | 0.65 | $4,000 | Beta(2, 4), lower |
| smallBusiness | 6% | 2.4 | 1.8 | $8,000 | 0.80 | 0.75 | $7,000 | Beta(2, 5), lower |
| highNetWorth | 2% | 1.3 | 2.8 | $25,000 | 0.92 | 0.80 | $15,000 | Beta(1, 8), very low |
| salaried | 60% (residual) | 1.0 | 1.0 | $1,200 | 0.70 | 0.70 | $3,000 | Beta(2, 3), moderate |

A `consteval` check fails compilation if the fixed non-salaried shares exceed 1.0. Dollars are 2019 values anchored at the window-start price level.

### Persona timelines (macro-history H2)

The table gives the persona at the person's anchor (window start for the seed roster, join date for joiners). A deterministic timeline (`synth/personas/timeline.hpp`), drawn once on an isolated lane from the birth date, moves people on:

- student → salaried (85%) or freelancer (15%) at a work-start age in 19–28
- salaried or freelancer → retiree at an SSA claiming-shaped date: 30% at 62, 10% uniform before full retirement age (FRA), 45% at FRA (1983-Amendments schedule, 65→67 by birth cohort), 5% between FRA and 70, 10% at 70, plus 0–60 days of jitter
- smallBusiness → working at business close (memoryless exponential, median 5 years, BLS BED five-year survival), unless retirement comes first
- retiree seeds carry a backdated claim date; highNetWorth never transitions
- everyone → deceased (below)

Paychecks run over [career start, min(claim, death)); Social Security from max(window start, claim) to death; business revenue stops at close; spending drops ~12% at the claim ([Day-to-Day Spending](#day-to-day-spending)); the AML Customer export shows the end-of-window persona. Spending archetypes stay seed-based; a full age-profile re-anchor is a registered upgrade.

### Mortality, estates, and membership (macro-history H3)

- Death is drawn once on the isolated `{"mortality", personId}` lane from the birth date and the embedded SSA 2023 period life table (sex-specific annual probabilities inverted at one uniform; sex is a latent 50/50 attribute, keeping the ~2.7-year male/female gap). The hazard is conditional on survival to the anchor; deaths anchor to birth dates, not the window.
- Stop at death: spending, ATM, self-transfers, rent, family gifts (either party dead drops the row), insurance claim filing, all income.
- Run until account closure at death + 120 days (the settlement period; estates keep getting billed): subscriptions, insurance premiums, loan and tax obligations, card cycles (servicing ends one statement early so its payment tail lands first).
- Each in-window death produces a funeral and an estate before closure ([Funerals and Estates](#funerals-and-estates-death-caused)).
- Membership is [joinTs, closeTs). The seed roster is present from window start. A join cohort tracks the embedded BEA population series (≈0.5–1.4%/yr over 1990–2024, rate frozen outside coverage), joining on days drawn in proportion to each year's growth, one draw per joiner on the isolated `{"join-cohort"}` lane. Joiners' ages, timelines and lifespans anchor at join (a 2015 joiner draws 2015 ages, closing the joiner age-axis error). The standard exporter filters rows on both owners' intervals; customer tables carry `created_at` and `closed_at`; AML Customer status flips `active → closed` at closure.

## Financial Products

Medians are 2019 dollars. Loan payments fix at the origination year's price level for the term (nominal contracts); tax amounts realize at each due date. Ownership:

| Persona | Mortgage | Auto loan | Student loan | Quarterly tax | Auto ins. | Home ins. | Life ins. |
|---------|----------|-----------|--------------|---------------|-----------|-----------|-----------|
| student | 0% | 15% | 60% | 2% | 55% | 0% | 5% |
| retired | 35% | 25% | 2% | 8% | 80% | 80% | 40% |
| salaried | 55% | 45% | 25% | 6% | 90% | 55% | 55% |
| freelancer | 35% | 40% | 20% | 70% | 85% | 35% | 30% |
| smallbiz | 50% | 50% | 15% | 85% | 92% | 60% | 60% |
| hnw | 70% | 30% | 5% | 45% | 95% | 90% | 80% |

Loan delinquency parameters:

| Loan | Late | Late days | Miss | Partial | Cure | Cluster |
|------|------|-----------|------|---------|------|---------|
| Mortgage | 3.5% (MBA Q3 2024, 30+ days) | 1–15 | 0.3% | 0.2% | 70% | 1.6× |
| Auto | 4% (broader than Fed NY Q4 2024's 2.1% 60+ day rate) | 1–10 | 1% | 2% | 58% | |
| Student | 9% | 1–15 | 3% | 5% | 45% | 2.0× |

### Mortgage

Payment lognormal, median $1,672 (Census AHS 2023), σ = 0.45, all-in (P&I + escrow); 85% on the 1st, else days 2–5. Term 30 years; observed loan age triangular(0.5, 5, 10) years for effective duration under refinancing and moves (Freddie Mac 2024).

### Auto Loan

35% new / 65% used (modeling choice); term range [36, 84] months.

| Segment | Payment median | Payment σ | Term mean | Term σ |
|---------|---------------|-----------|-----------|--------|
| New | $734 | 0.28 | 68 mo | 8 mo |
| Used | $525 | 0.32 | 72 mo | 10 mo |

Experian State of the Automotive Finance Market Q2 2024: average payments $734 / $525, terms 68.5 / 67.4 months, ~80% of new and ~36% of used purchases financed.

### Student Loan

Plans (a modeling mix): standard 60% (120 months), extended 10% (300), IDR-like 30% (240 months for 55%, 300 for 45%; StudentAid.gov forgiveness horizons). 6-month federal grace after school exit; 60% of student-persona holders are still in deferment at start. Origination 180 days to 4 years before exit; graduation mostly May and June, then December. FSA FY 2024 Annual Report: ~45M borrowers, $1.6T outstanding.

### Tax Profile

Quarterly estimated payments (median $1,800) on the IRS Form 1040-ES dates (January 15 for prior-year Q4, April 15, June 15, September 15) at 10:00 local; weekends and federal holidays are not rolled. Annual filing gives a refund (55%, median $900, Feb–May), a balance due (18%, median $1,200, April) or no visible settlement. Anchors: JPMorgan Chase Institute (income swings Feb–Apr and Dec); IRS 2024–25 (~72% of filers get refunds averaging $3,500+).

### Insurance

- Monthly premiums at the billing month's price level: auto median $225, σ = 0.35 (Bankrate 2026); home $163, σ = 0.40 (Ramsey 2025); life $40, σ = 0.50.
- Mortgage holders get home cover with p = 0.998, auto-loan holders auto with p = 0.997; the non-financed rate is back-calculated to hit the persona target. A mortgaged home's premium is inside the all-in mortgage payment and is not emitted; its claims still fire.
- Claims: auto 4.2%/yr (III 2024), median $4,700, σ = 0.80; home 5.5%/yr (Triple-I 2024), median $15,750, σ = 0.90; window probability `1 - (1 - p)^(n_months/12)`. Life payouts are a registered upgrade: estates already carry the wealth transfer, and a payout would double-count until both are sized together.
- Claim filing stops at death; premiums bill the estate until closure.

### Credit Cards

Persona-dependent approval. At issuance: APR lognormal median 22%, σ = 0.25, clamped [8%, 36%] (Fed G.19 Q4 2024 average 21.5%); limit lognormal around the persona limit, σ = 0.65, at the window-start price level (below Experian's Q3 2023 $29,855 average total across a consumer's cards); cycle day uniform [1, 28]; autopay 40% full, 10% minimum, 50% manual. Anchors: 82% of US adults hold a card (Fed SHED 2023); average carried balance $6,580 (TransUnion Q4 2024).

Each cycle:

1. Purchases accumulate; each may refund (0.6%, 1–14 days later) or charge back (0.1%, 7–45 days) from the same merchant (no synthetic refund counterparty).
2. At cycle end the average balance comes from piecewise-constant integration; out of grace with a debt integral, interest `APR × interval_days / 365` posts to `GL00000001`.
3. Minimum due = max(2% of statement, $25). Manual payers: full 35%, partial Beta(2, 5) 30%, minimum 25%, miss 10%. A cycle is late with probability 8%, by 1–20 days.
4. A $32 late fee posts to `GL00000002` if unpaid by the due date + `grace_days` (default 25): the CARD Act safe harbor restored when the CFPB's $8 cap was vacated in April 2025, with repeat-violation fees (~$43) simplified to flat. The $25 and $32 are 2019 dollars realized per cycle date.

Statements stop 50 days before account closure (death + 120 days), so the last payment and fee settle first.

## Banking Mechanics

The clearing book projects only the internal customer ledger: a `Bank::external` source credits only the internal destination, and an external destination debits only the internal source. External keys stay registered but are absent from the ledger's account map, so no boundary counterparty (salary source, merchant, ATM, biller) holds a balance. External-to-external and unknown internal postings are rejected as unbooked. CSV export rejects non-finite numeric cells.

Bank income is the only bank-side leg: each fee or interest posting debits the customer and credits its income GL (`GL00000001` card interest, `GL00000002` card fees, `GL00000003` deposit fees, `GL00000004` credit-line interest), the double-entry shape of core systems (FLEXCUBE CHG_BOOK debit, CHG_INCOME credit). GLs are internal and ownerless, never seeded or debited (a GL's balance is its window income), never fraud, mule, victim or camouflage, and their postings carry no device or IP. Exporters type them `gl` (mule-temporal) or `general_ledger` (AML).

### Balances

Each internal account has a balance seeded at the window-start price level, exactly one protection product (NONE, COURTESY, LINKED or LOC; one 4-way categorical draw), a bank tier (ZERO_FEE 15%, REDUCED_FEE 10%, STANDARD_FEE 75%; Bankrate 2025, Consumer Reports 2024) and an overdraft fee drawn at init from its tier's lognormal (window-start anchored). Available liquidity is `balance + overdraft + linked + courtesy`, with at most one buffer non-zero.

| Persona | Courtesy | Linked | LOC | NONE (hard decline at zero) |
|---------|----------|--------|-----|------|
| student | 12% | 8% | 2% | 78% |
| retiree | 16% | 22% | 4% | 58% |
| salaried | 18% | 24% | 12% | 46% |
| freelancer | 16% | 18% | 12% | 54% |
| smallBusiness | 20% | 24% | 20% | 36% |
| highNetWorth | 22% | 30% | 28% | 20% |

Anchors: ~20% opt into courtesy (CFPB 2024); ~40% link a savings sweep when offered (Bankrate 2025); formal OD LOCs are rarer but more common for freelancers, SMBs and HNW (SoFi / NerdWallet 2025, private-banking patterns). Buffer lognormal medians: courtesy ~$100–300 (σ 0.45); linked sweep $225 (student) to $10,000 (HNW) (σ 0.90); LOC line $500 to $7,500 (σ 0.60).

### LOC Interest Accrual

Per LOC account: `dollarSecondsIntegral[idx]`, the running ∫ max(0, -cash) dt updated at each balance-touching event from pre-transfer cash; `apr[idx]` ~ `Normal(0.18, 0.04)` clamped at 0; and `billingDay[idx]` uniform [1, 28], reserved for calendar billing. Billing is a rolling 30-day period (`kBillingPeriodSeconds = 30 × 86400`) from `lastBillingTs`:

$$
\text{interest} = \frac{\text{dollar-seconds integral} \times \text{APR}}{365.25 \times 86400}
$$

Interest debits cash directly (posting even over limit) as `loc_interest` to `GL00000004`; the integral resets and `lastBillingTs` moves to now. The first sweep (`lastBillingTs = 0`) only starts the clock. A draw is the deposit account going negative within LOC capacity; a separate per-customer Regulation Z credit account is a registered limitation.

### Overdraft Fees

When an accepted debit leaves a COURTESY account negative, the replay posts a fee to `GL00000003` (no device or IP), at most 3 per account per calendar day (Wells Fargo, industry standard), bypassing the insufficient-funds check.

| Tier | Median | σ | Example banks |
|------|--------|---|---------------|
| ZERO_FEE | $0 | | Capital One, Citibank, Ally |
| REDUCED_FEE | $15 | 0.25 | Huntington / BMO / Santander $15 (2022 cuts); BofA $10 |
| STANDARD_FEE | $35 | 0.20 | Chase $34, Wells $35, US Bank $36, PNC $36 |

### Merchant and Card Accounts

- Internal merchants (`M…`) would inherit the salaried $1,200 seed (no person maps to them), too low for refunds, so init reseeds them lognormal median $8,000, σ = 0.90 (Bluevine 2025: 39% of SMBs hold under 1 month of operating expenses, healthy ones 2–3 months), with no protection and ZERO_FEE.
- `setCreditLimit(card, limit)` stores the line in the `overdrafts` slot, sets protection NONE and tier ZERO_FEE and clears LOC registration, so `availableToSpend(card) = creditLimit` at init and cards get only their CC_INTEREST and CC_LATE_FEE events.

## Mathematical Models

### Amount Distributions

Each channel has one declared model; a missing lookup is a runtime error. Medians and floors are 2019 dollars, multiplied by the event year's index from embedded 1990–2024 series: SSA AWI for labor income (salary, revenue, benefits), CPI-U for prices (macro-history-v1 H1; [docs/era_data_provenance.md](docs/era_data_provenance.md), [docs/h1_nominal_scale_wiring.md](docs/h1_nominal_scale_wiring.md)). A 1991 window opens at ≈0.53× prices and ≈0.40× wages; 2019 reproduces calibration exactly. Dollar screens scale with what they screen. Statutory amounts (the BSA/CTR $10,000 threshold) and physical denominations (the $20 note, gift-card racks) stay fixed, and scaled amounts re-snap to their lattice (a 1991 ATM withdrawal is fewer $20s). Real growth changes counts, not amounts ([Counts](#counts-gamma-poisson-mixture)).

| Channel | Median | σ | Floor | Source |
|---------|--------|---|-------|--------|
| salary | $3,000 | 0.35 | $50 | BLS QCEW 2024 (per paycheck) |
| rent (all variants) | Γ(k=2, θ=400)+$50 | | | Census AHS 2023 |
| P2P | $45 | 0.80 | $1 | Fed Diary 2024 |
| bill | Γ(k=2, θ=400)+$50 | | | BLS CPI housing |
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

Merchant categories (median, σ; ClearlyPayments 2025, FMI 2024, Fed Diary 2024): grocery $50, 0.55; fuel $45, 0.35; restaurant $28, 0.60; pharmacy $25, 0.65; ecommerce $85, 0.70; retail_other $45, 0.75; utilities $120, 0.40; telecom $75, 0.30; insurance $150, 0.35; education $200, 0.60.

A lognormal with median m uses `mu = ln(m)`: `X ~ exp(Normal(mu, σ²))` has median m and mean `m · exp(σ²/2)`.

### Counts: Gamma-Poisson Mixture

$$
\lambda_{\text{day}} \sim \text{Gamma}(k, \theta = \text{base rate}/k), \quad n \sim \text{Poisson}(\lambda_{\text{day}})
$$

A negative binomial per account-day (overdispersed, bursty); shape `k = 1.5`, weekend multiplier 0.8.

Era modulation (macro-history H4; [docs/h4_macro_modulation.md](docs/h4_macro_modulation.md)): the day rate is also multiplied by `realPceLevel(year)`, BEA nominal PCE per capita ÷ CPI-U (embedded; 1.0 at 2019), looked up once per simulated day into the multiplier that carries seasonality and momentum. It is ≈0.67 in 1991, dips in 1990-91 and 2008-09, collapses in 2020, rebounds in 2021, and is ≈1.09 at the frozen 2024 level. 2001 is flat: that recession slowed growth without a per-capita level dip. The window budget is a calibration-level target (realized volume = target × the year's level); amounts are unaffected. The fraud budget is F = pL/(1−p) on the realized candidate count L, so fraud density is era-stable while fraud volume follows the economy.

### Timing Profiles

Hour-of-day PMFs: consumer (peak 18:00, tapering to midnight), consumer_day (peak 10:00, dropping after 14:00; retired, stay-at-home) and business (peak 9:00–12:00, nothing after hours). `consteval`-precomputed CDFs make an hour draw an O(log 24) walk over a constexpr array; minute and second are uniform.

### Momentum: AR(1)

$$
m_t = \phi \cdot m_{t-1} + (1 - \phi) \cdot 1.0 + \varepsilon_t, \quad \varepsilon_t \sim \mathcal{N}(0, \sigma^2)
$$

φ = 0.45, σ = 0.15, clamped [0.20, 3.00]: weekly persistence ~0.35–0.50, a calibration choice. Motivation: Tovanich et al. 2021 (persistence and burstiness as stable per-person features); Barabási 2005 (bursty dynamics), Goh & Barabási 2008 (memory coefficient), Karsai et al. 2012 (universality across activity domains).

### Dormancy: Three-State Machine

ACTIVE → DORMANT at p = 0.0012/day (`1 - (1 - 0.0012)^365 ≈ 0.35` of accounts a year) → WAKING → ACTIVE. Dormancy lasts uniform [7, 45] days (vacation 7–21, hospital 5–30, seasonal 30–90) at rate 0.05, not zero, since bills and subscriptions still fire; waking ramps linearly back to 1.0 over [2, 5] days. Evidence: ~35% of accounts have ≥14 consecutive zero-transaction days a year (Fed Payments Study). Mules go dormant → tester → spike, legitimate accounts ramp gradually (LexisNexis 2025; Unit21 2026: reactivation transactions average 17× the rule threshold, gradual when legitimate, immediate when fraud).

### Paycheck Cycle Boost

A payday multiplier `max_residual_boost × paycheck_sensitivity` (max 10%) decays linearly over `active_days` (default 4). It is small because discretionary flows are already separated from recurring bills.

### Counterparty Evolution: Monthly

`merchantAddP` = 0.35 (add a live favorite by the reach law), `merchantDropP` = 0.10 (drop a random favorite), `contactAddP` = 0.08 (add a P2P peer), `contactDropP` = 0.03 (replace a contact slot with a duplicate, lowering diversity). Both merchant and contact evolution run (merchant churn since `merchant-churn-2026-07`). The rates are modeling choices; Tovanich et al. 2021 motivate diversity, persistence and turnover as features.

### Seasonal Multipliers

Unit annual mean, so seasonality redistributes without inflating volume. Moderate on purpose: bills, rent and salary are not seasonal.

| Month | × | Driver |
|-------|---|--------|
| Jan | 0.88 | Post-holiday trough, "dry January" |
| Feb | 0.94 | Tax refunds start late |
| Mar | 1.04 | Refund spending peak |
| Apr | 1.02 | Late refunds, end of tax season |
| May | 1.00 | Mother's Day offsets Memorial Day drag |
| Jun | 0.98 | Summer, small Father's Day bump |
| Jul | 0.97 | Mid-summer, Independence Day |
| Aug | 1.05 | Back-to-school ramp |
| Sep | 1.02 | Back-to-school tail |
| Oct | 0.99 | Halloween, holiday pre-season |
| Nov | 1.16 | Black Friday, early holiday |
| Dec | 1.22 | Holiday peak |

Sources: NRF ($976B 2024 holiday, $1.01T projected 2025); Bank of America Consumer Checkpoint (2024–25); JPMorgan Chase Institute (income swings Feb/Mar/Apr/Dec); S&P Market Intelligence 2026 (refund → retail spending elasticity); NRF Back-to-School 2025 ($128B); Visa Holiday Retail 2025 (+4.2% YoY).

### Liquidity Multiplier

The product of payday relief (boost for the first `reliefDays`) and stress (ramp over `stressRampDays` after `stressStartDay`), the cash-on-hand ratio (clipped; its $75 floor scales with the day's price level) and the fixed-burden ratio, clipped to [0, 1.10] with an absolute floor. It is soft (never zeroes a whole person-day); the ledger enforces affordability.

## Legitimate Transaction Flows

### Salary (Payroll)

| | salaried | freelancer | smallbiz | hnw | student | retired |
|---|---|---|---|---|---|---|
| p(salary) | 98% | 8% | 4% | 12% | 12% | 2% |

Scaled by `paidFraction = 0.74` (the working-type weighted mean, re-derived in H2). Selection uses the working-life persona: a seed student takes their career destination's rate, a closed business owner their post-business type; seed retirees get no payroll. Pay runs from career onset (or business end) to the claim or death, whichever is first.

- Employer: drawn through the employer size law ([Counterparty Pools](#counterparty-pools)); tenure uniform [2, 10] years.
- Cadence, drawn once per employer: weekly 20%, biweekly 55%, semimonthly 15% (1/15 or 15/31), monthly 10% (day 28, 30 or 31). Weekend paydays roll back to the prior business day; posting lag 0–1 days. The fixed 2025 anchor only sets weekday and fortnight parity, so weekly and biweekly lattices exist in every era (fixing 75% of cadences being silent before 2025).
- Amount: a 2019 draw paid at the pay date's AWI, with compounding seeded real raises `Normal(0.015, 0.02)` a year and `Normal(0.08, 0.06)` per job switch. The model is one monthly paycheck; annualize and divide by `payPeriodsInYear` for other cadences.

### Rent

| | student | retired | salaried | freelancer | smallbiz | hnw |
|---|---|---|---|---|---|---|
| p(rent) | 50% | 18% | 62% | 58% | 35% | 10% |

For non-homeowners, scaled by `rentFraction = 0.55`. Leases last uniform [2, 10] years. Base rent is a 2019 draw paid at each payment's CPI level with compounding real raises `Normal(0.02, 0.015)` a year, re-sampled at turnover. Payments jitter over days 0–5 and hours 7–22. The channel comes from the landlord-type CDF ([Landlords](#landlords)), so a tenant paying one landlord keeps one Zelle, check, ACH or portal signature. Rent stops at the tenant's death.

### Day-to-Day Spending

1. Market build: `favK` ∈ [8, 30] favorite merchants per person, drawn from merchants live at window start through the reach law (popularity flattened by a solved exponent) and home-conditioned (online merchants nationally, physical ones through the home area's distance-decay pool); `billK` ∈ [2, 6] billers; exploration propensity ~ Beta(1.6, 9.5); optional 3–9 day high-spending bursts at `burstsPerYear` = 0.487, scaled by segment span / 365.25 (the former 8% per 60 days, made window-independent).
2. Per day: seasonal × momentum × dormancy × paycheck × weekday × day-shock × liquidity × era-level multiplier. The person-day target inverts the suppressors from the calibration-level monthly target. Dead spenders are skipped.
3. Per transaction: merchant / bill / P2P at 0.82 / 0.10 / 0.08, with `unknownOutflowP = 0.05` carved out for `external_unknown`; tickets at the day's CPI.
   - `external_unknown` (deposit-funded remote spending): at the DCPC check share of consumer payments for its year (7% in 2016, 3% in 2024; 60% of the slot from 2024, all of it through 2020) a paid check keyed by the payee's bank (4 payees per person, each at a bank drawn from FDIC Summary of Deposits 2024 shares); otherwise an identified remote merchant (an external online or national-service outlet live at that time).
   - P2P with a missing or unusable contact goes to the person's platform (Venmo or Cash App, split by monthly actives), since Zelle needs an enrolled contact.
   - Every choice is a draw-free hash, so only destinations differ from the old catch-all build.
4. Routing: favorites carry ~99.2% of picks (`baseExploreP` = 0.02); an explored merchant that is already a favorite is rejected. Payment is by card if the person holds one and `ccShare` rolls, else by deposit account.
5. Monthly: favorites and contacts evolve ([Counterparty Evolution](#counterparty-evolution-monthly)).

From the Social Security claim day, tickets scale by 0.88 (the Aguiar–Hurst retirement consumption step, ~−12%, H2), for working seeds retiring in-window only; seed retirees already carry retired multipliers. Paydays come from actual inbound deposits, SSA included, so liquidity relief, stress and paycheck boost follow benefit Wednesdays after retirement with no special case.

### Subscriptions

4–8 intents per person, each debiting with probability 55%, priced from a SaaS and streaming pool ($6.99, $9.99, $14.99, $17.99, $49.99, $99.99, etc.) at the debit month's price level. Billing day uniform [1, 28], ±1 day jitter. Contractual: they bill the estate until closure.

### ATM

88% of people withdraw 1–6 times a month (uniform), from a pool weighted to multiples of 20/40/60/100, price-scaled and re-snapped to $20; 75% on days 0–18 of the month, 25% on days 18–28. Withdrawals stop at death.

Cash points: 13.5 per 10,000 people (at least two) over the home-area distribution. Each withdrawal uses the customer's current, relocation-aware area: a stable primary point 82% of the time, else one of up to three nearby. The `XS…` endpoint is a combined terminal/acceptor in the graph and an external boundary in clearing (customer debited, endpoint never credited, no single cash node). Vault-cash and interbank-settlement GLs are out of scope. Not yet modeled: the ISO 8583 terminal/acceptor split (DE41/DE42), on-us versus off-us, an interchange-fee leg, exported cash-point coordinates.

### Cash and Check Deposits

Household cash and settled paper-check deposits, on isolated deterministic lanes, credit only the customer; capture points come from the event-time, relocation-aware area. Business cash takings (non-payroll revenue) use the same routing. Check rows are posted credits after capture; holds and returns are not separate events yet.

### Crypto Fiat Ramps

`crypto_ramp_out` is a USD debit to an external venue, `crypto_ramp_in` a USD credit back; there is no blockchain ledger. Ramps start no earlier than 2013 in a conservative modern adopter cohort, and each ramp-in is capped by the account's remaining accepted ramp-out inventory. Out of scope: token quantities, wallet-to-wallet recipients, gains and losses, fees, transaction hashes.

### Self-Transfers

45% of multi-account holders move money 1–3 times a month from their richest account to their leanest: 45% round amounts (price-scaled, re-snapped to a $25 lattice), 55% lognormal($250, σ = 0.80); 70% on days 0–6 (post-payday saving). Stops at death.

### Direct-Deposit Splits (Paychecks)

30% of multi-account holders send 10–35% of each salary deposit to a secondary account within 5–30 minutes (APA 2024); splits end with the death-clipped income.

### Government Benefits (SSA Wednesday Cohorting)

Post-1997 SSA/RSDI rule: birth day 1–10 pays the 2nd Wednesday, 11–20 the 3rd, 21–31 the 4th; a holiday Wednesday pays the preceding business day. The cohort uses the real modeled birth day, the same date as every exported DOB (H2 step 2a retired the hash-derived day). The "paid on the 3rd" exceptions (pre-May-1997 entitlement, dual SSI, foreign residents) are not modeled.

- Social Security (SSA 2026 COLA): 87% of eligible retirees (SSA Dec. 31, 2025 fact sheet). Anyone retired by window end is a candidate, paid from max(window start, claim) to death; survivor benefits are a registered upgrade. Median $2,071 (estimated Jan 2026 average retired-worker benefit), σ = 0.30, floor $900.
- Disability: 4% of non-retired non-students; median $1,630 (Jan 2026 average disabled-worker benefit), σ = 0.25, floor $500.

Levels are drawn once in 2019 dollars and paid at each month's AWI (a declared simplification; earnings-history levels and per-cohort COLA are a registered upgrade).

### Non-Payroll Revenue

Monthly, at the month's AWI:

| Persona | Streams per month (median) | Quiet months |
|---------|--------------------------|--------------|
| Freelancer | 1–4 client ACH ($1,400, σ = 0.70); 1–4 platform payouts ($425); 1–2 owner draws ($1,800) | 12% |
| Smallbiz | 0–3 client ACH ($2,600); 0–3 platform ($950); 4–12 card settlements ($680); 1–2 owner draws ($3,400) | 6% |
| HNW | 0–2 investment inflows ($6,500, σ = 1.05) | 2% |

Student plans stop at career start, worker plans at the claim, business plans at close; retiree investment income and HNW inflows run until death. Client, platform and processor income lands on `BOP…` and investment inflows on `BRK…` when present, else on the personal account. Timestamps fall on business days (weekend rejection with retry). Cash takings stay on the $10 lattice, so an exact-$10,000 CTR amount stays reachable.

## Family and Social Flows

The family graph has households (`singleP` = 29%, Zipf α = 2.2, `spouseP` = 62%), dependents (65% student-dependent, 35% co-resident, 70% two-parent) and retiree support ties (35% have an adult child, 35% of those support). Amounts are 2019 dollars at event-date prices. A gift whose source or target owner is dead at its timestamp is dropped (external `XF…` relatives have no modeled deaths, declared).

| Flow | Model | Evidence |
|------|-------|----------|
| Allowance (parent → student child) | 60% weekly, 40% biweekly; Pareto(xm = $15, α = 2.2) | |
| Retiree support (adult child → retired parent) | `hasChildP` = 0.35, `supportP` = 0.35; base Pareto × the child's persona support-capacity weight; 1 / 2 / 3 supporters at 65 / 27 / 8%; the spouse shares supporters with p = 0.85 | |
| Spouse | Couples with partly separate accounts (60%) make 2–6 transfers a month, lognormal($85, σ = 0.9); breadwinner asymmetry 65% (the higher earner, by persona amount multiplier, sends more often) | Census 2023, Bankrate 2024; Pew 2023 (husband primary breadwinner 55%, egalitarian 29%, wife 16%; the multiplier is a gender-agnostic proxy) |
| Parent gift (working parent → adult child) | 12% monthly per eligible pair, Pareto(xm = $75, α = 1.6) × the parent's persona weight | HRS (35% of parents 51+ give over two years); Savings.com 2025 (50% support adult children, over $1,300/month among givers) |
| Sibling | 15% of pairs active; 18% monthly, median $120, σ = 0.90, direction ~50/50 | FinanceBuzz 2024 (31% of family loans); JG Wentworth 2025 (76% have borrowed from a sibling) |
| Grandparent gift | Grandchildren by two hops (`retired_person → children → their children`); 8% monthly, median $150, σ = 0.70 | EBRI 2015 (38–45% of 50+ households give to younger generations) |

### Tuition (parent → the student's school)

65% of students get one payment plan per run, starting 0–9 days after the first day of the window's first calendar month: 4–5 installments 30 days apart, each posted 0–4 days late between 08:00 and 17:59. The total is lognormal(μ = 8.95, σ = 0.35), median about $7,712, split evenly with 3% noise per installment. One parent's own account pays every installment to one school: an education merchant open on every installment date, picked uniformly in the student's home city if it has one, else among all open education merchants, on the student's own random lane (`family/schools.cpp`).

### Funerals and Estates (death-caused)

On the isolated family-inheritance lane, every in-window death produces:

- A funeral: one bill-channel payment from the decedent's account at death + 3–10 days to a funeral home (MCC 7261) in the decedent's city at death. Each city has `max(1, round(population * 15,375 / 334,017,321))` homes (Census CBP 2022 establishments per resident); the person ID picks one. Lognormal median $6,300 (NFDA 2019 General Price List: funeral with viewing and burial $7,640, cremation with viewing $5,150, at the ~55% 2019 cremation rate), σ = 0.40, floor $1,000, at the death year's prices.
- An estate: paid to heirs (direct children, else supporting children) at death + 30–90 days (a declared probate window). Interim size lognormal median $25,000, σ = 1.0; an SCF net-worth re-derivation is a registered upgrade. Heirless estates are not yet distributed (declared).

Both land before account closure (death + 120 days), so the visible corpus has them. The old uncaused inheritance hazard (0.15% of retirees per 180-day window) is retired.

### Family External Accounts

`externalP = 0.18` is the chance a family counterparty banks elsewhere: co-residents mostly share a bank, non-co-resident adult children and siblings less so (FDIC 2023, market fragmentation). It is deterministic from the person ID and a threshold, so every generator agrees without shared state.

### Social Graph (P2P)

`effectiveDegree` (default 12) contacts per person in a community-aware weighted graph: contiguous communities of `[6k, 24k]` people (k the effective degree); lognormal(σ = 1.1) social capital, 25× for configured hubs; 70% of ties within the block, 29% explicit cross-block, 1% global; Gamma(shape = 1.0) tie strengths per unique contact, sampled as a CDF by the fixed-width contacts row, so strong ties recur.

## Fraud Typologies

- Rings per 10k people: lognormal mean 6, σ = 0.4. Size lognormal(μ = 2.0, σ = 0.7) in [3, 150]. Mule fraction Beta(2, 4) in [10%, 70%]. Victims per ring lognormal(μ = 3.0, σ = 0.8) in [3, 500].
- Solo fraudsters 4 per 10k; repeat victims 10%; fraud participants ≤ 6% of the population; target illicit transaction ratio 0.5%.
- Multi-ring mules 6% (Merseyside OCG study: 63% of groups cooperated with at least one other), adding cross-community edges that break the "isolated cluster = fraud" assumption.
- Ring-rail amounts and continuous unauthorized card/ATO draws use event-year prices. Fixed nominal: structuring (tied to the statutory CTR threshold, unindexed since the 1970s, so $9,950 in 1991 is roughly twice today's real value), card-testing anchors and gift-card denominations (round amounts are the signature).

| Typology | Default weight | Pattern |
|----------|---------------|---------|
| Classic | 30% | victim → mule → fraud, with an optional cycle through the ring |
| Layering | 15% | victim → entry mule → 3–8 mule/fraud hops → exit cashout |
| Funnel | 10% | many sources → one collector → several cashouts |
| Structuring | 10% | the victim splits 3–12 payments of `10000 - Uniform(50, 400)`, under the $10k CTR threshold; fixed nominal in every era (class S) |
| Invoice | 5% | fraud → biller, mimicking business-to-vendor payments ($10-snapped era dollars) |
| Mule | 30% | fan-in from 8–25 sources, forwarded after 1–12 hours with 90–95% passed through (5–10% haircut), in a 5–14 day burst: the FATF 2022 profile of many small-to-medium inbounds and fewer, larger outbounds |

Solo fraudsters act alone, with no ring.

### Camouflage

Ring accounts get cover with `isFraud = 0` and `ringId = -1` (invisible to anyone reading only flags): bills (35% per account-month), small P2P to a customer deposit account, the only destination legitimate P2P pays (3% per account-day), and payroll for 12% of ring accounts from a size-law employer on its own schedule (pay dates, posting lag, posting hours). Cover scales with the index of the flow it mimics (prices for bills and P2P, wages for salary); otherwise it would be a detectable artifact.

### Burst Window

Rings operate in a 7–14 day burst where device and IP sharing concentrates; outside it members behave legitimately. Bursts and camouflage clamp to the participants' alive horizon (earliest death among fraud actors and mules). Card/ATO victims may be hit in the estate-settlement tail but never outside `[join, close)`; victim-authorized scams need a living victim. A compromise is accepted only if its whole sampled span fits before the earliest victim or payee boundary; edge cases are rejected, not compressed.

### Shared Infrastructure

Ring shared device 80%, shared IP 75%; legitimate shared-device noise 1%. A fraud transaction uses the ring device with probability 0.85 and the ring IP with 0.80.

## Infrastructure and Device Attribution

- Devices: 1 (80%) or 2 (20%) per person, type uniform over `{android, ios, web, desktop}`; 1% legitimate shared groups (family or household devices). In card-fraud, personal, shared and attacker devices share one fixed-width opaque `D…` namespace; owner role is not encoded in prefix, width or range.
- IPs: 1 + Bernoulli(0.35) + Bernoulli(0.10) per person; seeded random IPv4 with first octet 11–222 and last octet 1–254.
- Router: sticky current device and IP, switching with probability 5% per transaction. Fraud transactions try the ring's shared infra first and fall back to personal infra if that roll misses.

## Chronological Replay and Screening

The pre-fraud replay stable-sorts by `(timestamp, source, target, amount, fraud flag, ring, channel, device, IP)` and for each transaction:

1. Posts it; if an accepted debit takes a COURTESY account negative, emits an overdraft fee (max 3/day).
2. On insufficient funds, looks for a future inbound credit that cures the shortfall (salary, government, insurance claims, refunds, self-transfers, incoming family support, etc.): within 10 hours for card-like channels (ATM, merchant, card_purchase, P2P), 36 hours for retryable ACH-like ones (bill, rent, subscription, external_unknown, insurance, loans, tax).
3. With no cure, blind-retries with probability 55% after 18 hours, then 72 hours; max 1 retry card-like, 2 ACH-like.
4. Accrues LOC interest through each posting's timestamp: a due-time min-heap pops accounts last billed ≥ 30 days ago, rolls their integral forward lazily from current cash, debits the interest (bypassing the funding check) and sends a liquidity event to the sink. Accounts with `lastBillingTs = 0` are anchored first, so billing never covers pre-history.

The post-fraud replay repeats this with liquidity emission off. Soft screens in the ATM, subscription, self-transfer and day-to-day generators use a scratch copy of the initial ledger only to lower the drop rate.

## Export Formats

### Standard

Vertices `person`, `accountnumber`, `phone`, `email`, `device`, `ipaddress`, `merchants`, `external_accounts`; edges `HAS_ACCOUNT`, `HAS_PHONE`, `HAS_EMAIL`, `HAS_USED`, `HAS_IP`, `HAS_PAID` (aggregated); the raw ledger is the shared `transactions` stream (`SELECT * FROM transactions ORDER BY row_seq`). `HAS_PAID` and the flow aggregates keep only rows inside both endpoint owners' `[joinTs, closeTs)`, as a bank's books would. The entity-resolution `customer.csv` carries `created_at` (join) and `closed_at` (closure if inside the window, else empty).

### Mule-ML

For GraphSAGE and node-level mule detection: `Party` (one row per account with fraud label, phone, email and full deterministic identity: name, SSN, DOB, address, geo, country, canonical IP, canonical device), `Transfer_Transaction` (the raw ledger), and `Account_Device` and `Account_IP` (aggregated account-infra edges with counts and first and last seen). Ages come from the per-person birth date (persona-band draws, e.g. retired Beta-weighted over 65–99, student 16–34; joiners anchored at join). Addresses use `faker-cxx` with deterministic zip-code lookups to real US cities, plus a fallback list.

### Mule-Temporal (TigerGraph Mule_Pattern_Learner)

`--usecase mule-temporal` exports [schemas/mule_temporal.gsql](schemas/mule_temporal.gsql) into schema `mule_temporal` of the `phantomledger` database: eight vertex tables, seven association tables with discriminated half-open tenures, and twelve payment-participation tables, with payments and association changes in one chronological sequence. Zelle payments appear only in `Zelle_Transfer`, others only in `Payment_Transaction`.

```sh
PL_PG='dbname=phantomledger' make run ARGS="--usecase mule-temporal --start 2024-01-01 --days 366 --population 200000 --seed 42"
```

- Zelle activity, token relationships and simulator labels are included. Zelle follows [Federal Reserve survey estimates and published bank limits](docs/research/zelle_model.md), with separate sending-user and payment-choice probabilities, and includes external-bank consumers and eligible businesses. With no source-confirmed Zelle rail or investigation-arrival feed, rail assignments and immediate oracle labels are synthetic.
- Entity metadata is immutable. Labels, source fraud typologies and whole-history aggregates are excluded from features. Zelle `fraud_label` is a separate payment target.
- Account carries MulePatternLearner's fifteen-column Account label contract: `is_mule` (integer account role: 1 mule, 0 other synthetic account), then the label's mask, PU label, effective and availability clocks, ring (`mule_ring_id`, the mule's home ring) and source. Every label is masked and none marked known, so MulePatternLearner's one-time reveal runs.

[The temporal contract](docs/mule_temporal.md) has mappings, sampling rules, supervision boundaries and current limitations.

### AML (TigerGraph AML_Schema_V1)

- Vertices: Customer, Account, Counterparty, Name, Address, Country, Watchlist, Device, Transaction, SAR, Bank, MinHash buckets (Name, Address, Street_Line1, City, State), Connected_Component.
- Edges: customer_has_account / account_has_primary_customer; send/receive_transaction (customer side), counterparty_send/receive_transaction (counterparty side), sent/received_transaction_to/from_counterparty (aggregated); uses_device; logged_from; customer/account/counterparty/bank/address has_name / has_address / associated_with_country; customer_matches_watchlist; references (SAR → Customer with role); sar_covers (SAR → Account with activity amount); beneficiary_bank / originator_bank; resolves_to (counterparty → customer soft link); MinHash bucket edges.
- Customer: `customer_type` and its derived demographic and occupation attributes show the end-of-window persona, as a CRM would. Status flips `active → closed` once account closure precedes the corpus end. AML corpora are full-world, with no membership row filter (declared). The onboarding date is a synthetic backdated derivation, not joinTs (a declared inconsistency; aligning them is registered).
- SARs: one per ring, filed 30 days after its last illicit transaction (BSA), and one per solo fraudster. Violation type from the dominant fraud channel: structuring → `structuring`, invoice → `suspicious_activity`, else `money_laundering`. Per-account `activity_amount` is in + out throughput, not a share.
- MinHash: byte-for-byte compatible with TigerGraph's reference `TokenBank.cpp` (Austin Appleby's MurmurHash2 on byte shingles, k = 3, with the reference's exact 101-element c1/c2 coefficient tables). Bucket IDs include the band (`{PREFIX}_{band}_{hash}`) so LSH bands stay independent. The reference C source is embedded as a comment for migration and validation.

### AML Transaction-Edges with Derived Features

The AML schema with a transaction-edge view instead of the aggregated `HAS_PAID` projection, plus derived account features for models that use both ledger and topology: PageRank, Louvain community ID, weakly connected component ID and size, shortest path to a mule, IP and device collision counts, in/out mule ratio, multi-hop mule count, betweenness, in/out degree, clustering coefficient. Customer persona and `active`/`closed` status match the AML export.

### Card-Fraud (TigerGraph TF_GNN_v3, temporal graph learning)

A transaction-fraud corpus for TigerGraph's TF_GNN_v3 GSQL schema, built for temporal graph networks (TGN) that score each transaction: payments carry timestamps and timestamped device and IP session edges, so the corpus replays as a continuous-time event stream. 43 tables in schema `card_fraud` (prefix `cf_`):

- `Payment_Transaction`: `card_purchase` rows (credit-card purchases plus unauthorized-card and gift-card-scam fraud) and `merchant` rows (account-paid POS, read as debit card). Unauthorized rows use the victim's primary account and so derive a debit-card identity; treating them as credit-card activity needs fraud planning moved into the statement, payment and interest lifecycle, which closes first today. 8 loaded columns: `id` (`T<row_seq>`, 1:1 with `transactions`), timestamp, amount, `is_fraud`, unix time, merchant category, `use_chip`, `error`. Streamed during settlement with the `Card_Send_Transaction`, `Merchant_Receive_Transaction`, `Transaction_Uses_Device` and `Transaction_Uses_IP` edges.
- `Card` (registry credit cards, ≤1 per person; any other view source becomes the account's derived debit card), `Party` (canonical customer IDs; `created_at` is joinTs), `Party_Has_Card`. `Is_Merchant` links view-observed merchants (~42% of the catalogue, draw-free from world state; `merchant-ownership-2026-07`) to their beneficial owner; it asserts identity, not money flow.
- `Merchant`, `Merchant_Category`, `Merchant_Assigned` and a consistent `City`/`State`/`Zipcode` chain (`Has_City`/`Has_State`/`Has_Zip`, `Assigned_To`, `Located_In`) from the merchant's world location; the 71-US-city catalogue is a runnable placeholder, not Census-complete ZCTA or establishment data.
- PII vertices `Address`/`Phone`/`Email`/`IP`/`Device`/`ID`/`Full_Name`/`DOB` and non-infrastructure `Has_*` edges (TF_GNN_v3 marks the layer demo-only; PhantomLedger fills it). Since `attacker-infra-2026-07`, `Has_Device` and `Has_IP` are the institution's incomplete endpoint registry (~72% device, ~61% address coverage), not true ownership, so a missing edge is weak evidence ("not on file ⇒ fraud" precision 0.027 at 2.9× lift). Use them for structure and the timestamped edges for anything point-in-time.
- `Transaction_Uses_Device` and `Transaction_Uses_IP` link each payment to its session's device and IP with `edge_unix_time`, including exogenous attacker sessions. Score a transaction before adding its session edges to temporal memory.
- `Ground_Truth_Label` `(entity_type, entity_id, label)`: the investigative overlay, positives only, joinable 1:1 to vertex tables, one of the 43 tables but outside the feature graph (TF_GNN_v3 does not load it; no edge points to it).

Labels (card-fraud-realism-v2): `Card.is_fraud`, `Party.is_fraud`, `Device.is_blocked` and `IP.is_blocked` are full-window verdicts ("this card ever carried a flagged row") that would leak the answer, so they are written as 0; the columns stay because TF_GNN_v3 maps columns by position. The one supervised target is `Payment_Transaction.is_fraud`, observable at its own timestamp. Entity verdicts are in `cf_Ground_Truth_Label` for evaluation only.

Realism: a 10,000-person, 60-day audit found all 748 fraud rows at merchants with no legitimate card-view transaction, and every unauthorized-fraud row on a TEST-NET-2 attacker IP. Both are fixed: fraud card-rail destinations come from the legitimate acceptance catalogue through the same modality-conditioned distance-decay kernel, and attacker IPs from the shared sampler. Gates: `test_card_baselines` (merchant-ID-only baseline); [docs/card_fraud_feature_contract.md](docs/card_fraud_feature_contract.md) (readable columns, pinned by a truncation experiment in `test_card_point_in_time`); `test_card_prevalence` (per-year prevalence, channel, typology, amount and episode bands).

The corpus supports a point-in-time temporal GNN pipeline but is not yet a public calibrated online benchmark. Open: fraud-level calibration; unauthorized credit-card activity in the statement, payment and interest lifecycle; effective-dated card, device and residence lifecycles; era-varying fraud technology and rail mix; operationally delayed labels; an executable, tested TigerGraph GSQL, training and evaluation pipeline. Causal feature, split, metric and minimum-realism gates: [docs/card_fraud_online_gnn.md](docs/card_fraud_online_gnn.md).

Only loaded attributes are emitted, in TF_GNN_v3's loaded-attribute order. PageRank and community slots, engineered `Payment_Transaction` features and TF_GNN_v3's interaction, co-occurrence and community edges are in-graph TigerGraph query work; the production GSQL feature query and a full training and evaluation runner are not shipped yet. Export a table as CSV with `\copy card_fraud."cf_Payment_Transaction" TO 'Payment_Transaction.csv' WITH (FORMAT csv, HEADER true)`.

## Configuration

The CLI exposes only `--days`, `--population`, `--seed`, `--start` and `--usecase`. Everything else (persona shares, channel medians, fraud weights, family probabilities) is code: constants or `Rules` / `Flow` / `Profile` structs that live with the subsystem using them. Reference data is embedded as constexpr tables (era history in `synth/econ/era_data.hpp`, geography in `synth/geo/geo_data.hpp`); the generator reads no external data files and never fetches at run time.

Each config struct has `void validate(validate::Report&) const`, posting named, source-located checks; the orchestrator collects one `Report` and throws `validate::Error` if any fail, so errors surface at construction, not as silent zeros mid-simulation. Compile-time checks: the `consteval` persona-share check, `static_assert(enums::isIndexable(...))` on every enum used as an array index, and `consteval`-normalized hour PMFs and CDFs.

## References

Sources cited inline with their figures are not repeated. Additional details:

- Payments: Bankrate 2025 (overdraft fee averages); Fed Diary 2024 (ATM distributions); FDIC 2023 Survey of Household Use of Banking Services; FDIC Summary of Deposits (June 30, 2024 deposits size the check-payee pool: top 25 exact, anchors to rank 1,000); Fed Diary of Consumer Payment Choice, 2025 Findings (check share by number); FRB adoption of DSTU X9.37 (a paid check's only structured payee key is the bank-of-first-deposit routing number); Zelle FAQ and Venmo Help Center (Zelle needs an enrolled email or US mobile number, unclaimed payments expire after 14 days; Venmo posts as a named ACH counterparty); Block, Inc. Form 10-K FY2024 (Cash App 57M monthly transacting actives) and PayPal Q4 2024 earnings call (Venmo over 64M monthly active accounts); 12 CFR 1005.17(a), Regulation E (an overdraft line of credit is a Regulation Z credit account, not overdraft service).
- Bank-owned income GLs: Oracle FLEXCUBE Universal Banking Interest and Charges User Guide 14.5.3 (2021; debit interest debits ICDB-BOOK and credits ICDB-PNL); Temenos Transact accounting events (CATEG entries to P&L categories, internal accounts with no customer); Fiserv DNA FCRM extract (2023; GL accounts with a blank customer number, GL legs kept in AML profiling); Oracle Behavior Detection (a back-office transaction is an Account plus an Offset Account).
- Macro history: FRED CPIAUCNS (BLS CUUR0000SA0 mirror; CPI-U 1990–2024, verified exact); SSA Average Wage Index (1990–2024, verified exact); BEA A794RC0A052NBEA / B230RC0A052NBEA via FRED (per-capita PCE and population); BLS LNS14000000, cross-checked with FRED UNRATENSA (unemployment); SSA Period Life Table 2023, table 4.C6 (one table era-wide, declared); SSA 1983 Amendments (`timeline::fraMonths`); SSA Annual Statistical Supplement (claiming ages); NCES / BLS (school-exit and labor-entry ages); BLS Business Employment Dynamics (~50% five-year survival); Aguiar & Hurst 2005, *Journal of Political Economy* 113(5); NFDA/CANA cremation rate; Census County Business Patterns 2022, NAICS 812210 (15,375 funeral homes; MCC 7261 per the Visa Merchant Data Standards Manual); 31 CFR 1010.311 (CTR threshold). Provenance and refresh: [docs/era_data_provenance.md](docs/era_data_provenance.md). Wiring: [H1](docs/h1_nominal_scale_wiring.md), [H2](docs/h2_persona_timeline.md), [H3](docs/h3_mortality_estate.md), [H4](docs/h4_macro_modulation.md).
- Credit: Experian (3.9 cards per holder, 2023); TransUnion Q4 2024 CIIR (Q2 2024 balance $6,329).
- Loans and claims: Census AHS 2023 ($1,672 sits in the $1,520–$1,960 ACS/AHS/NMDB band); Census/ACS (~60% of owner-occupied homes mortgaged, 59.7% in 2024); Freddie Mac 2024 (effective duration ~7–10 years); Insurance Information Institute 2024 (4.2 auto claims per 100 drivers a year; average collision claim $4,700); Triple-I 2024 (5–6% of home policies claim a year; average payout $15,749); StudentAid.gov (10-year standard plan, 6-month grace, IDR forgiveness at 20/25 years).
- Households: Census 2024 (92% own a vehicle); NW Mutual 2024 (52% have life insurance, 26% expect to leave an inheritance); SSA Dec. 31, 2025 fact sheet (87% of people 65+ receive benefits); SSA payment calendar and handbook (Wednesday rule); TurboTenant 2024 (65% of digital rent over ACH); S&P Market Intelligence 2026 (~2% retail spending per 10% refund); LendingTree 2021 (median owed to a sibling ~$400); Pew 2023 (24% say siblings are financially responsible for each other).
- Human dynamics: Barabási 2005, "The origin of bursts and heavy tails in human dynamics," *Nature* 435:207–211; Goh & Barabási 2008, *EPL* 81:48002; Karsai et al. 2012, universal correlations in human activity, *Scientific Reports* 2:397; Tovanich, Centellegher, Bennacer Seghouani, Gladstone, Matz et al. 2021, "Inferring psychological traits from spending categories and dynamic consumption patterns," *EPJ Data Science* 10:24 (the paper finds trait inference from spending hard; this repo's rates are modeling choices).
- Fraud and AML: FATF 2022, "Money Laundering Through Money Mules"; FATF 2020, AML Red Flag Indicators; Europol EMSC (operational mule-detection data); UK National Crime Agency (mule profiles); LexisNexis 2025, "Money Mule Detection Strategies"; Rossi, Chamberlain, Frasca, Eynard, Monti & Bronstein 2020, "Temporal Graph Networks for Deep Learning on Dynamic Graphs," arXiv:2006.10637 (the continuous-time framing the card-fraud corpus feeds).
- Hashing: Austin Appleby, MurmurHash2 32-bit; TigerGraph DevLabs `TokenBank.cpp` MinHash reference (embedded as a comment in the AML MinHash module).
