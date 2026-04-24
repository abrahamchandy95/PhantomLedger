# PhantomLedger

A synthetic bank transaction generator. PhantomLedger produces realistic, research-grounded retail-bank ledger data — people, accounts, salaries, rent, bills, loans, insurance, credit cards, fraud rings, devices, IPs — suitable for training ML models for mule detection, AML graph analytics, and general transaction-pattern research.

The defining property of the data is that **legitimate activity is structurally rich enough that the fraud signal isn't trivially separable**. Every behavioral dimension the ML models need to learn — temporal bursts, high fan-in, device sharing, rapid forwarding — exists in milder form in legitimate accounts. Mules differ in *degree*, not in *kind*.

---

## Table of Contents

1. [Overview](#overview)
2. [Repository Layout](#repository-layout)
3. [Pipeline](#pipeline)
4. [Entity Generation](#entity-generation)
5. [Personas](#personas)
6. [Financial Products](#financial-products)
7. [Banking Mechanics](#banking-mechanics)
8. [Mathematical Models](#mathematical-models)
9. [Legitimate Transaction Flows](#legitimate-transaction-flows)
10. [Family and Social Flows](#family-and-social-flows)
11. [Fraud Typologies](#fraud-typologies)
12. [Infrastructure and Device Attribution](#infrastructure-and-device-attribution)
13. [Chronological Replay and Screening](#chronological-replay-and-screening)
14. [Export Formats](#export-formats)
15. [Configuration Reference](#configuration-reference)
16. [Research Citations](#research-citations)

---

## Overview

PhantomLedger generates a complete bank-internal view of a synthetic population over a configurable time window (default 365 days, 80,000 people). Each person is assigned:

- a persona archetype (student, retired, salaried, freelancer, small business, HNW)
- one or more accounts, possibly including a credit card and a business operating or brokerage account
- PII (phone, email, deterministic address via pgeocode)
- a family graph (household, spouse, parents, children, supported relatives)
- a social graph (weighted contact list for P2P flows)
- device(s) and IP(s) with session history
- a portfolio of financial products (mortgage, auto loan, student loan, tax profile, insurance holdings)

On top of that static universe, the pipeline emits transactions across ~40 canonical channels — salary, rent (five variants by landlord type), merchant purchases, bills, subscriptions, ATM, self-transfers, credit card lifecycle events, government benefits, insurance premiums and claims, loan payments, tax payments, family transfers (allowance, tuition, support, spouse, parent gifts, sibling transfers, grandparent gifts, inheritance), and fraud (classic, layering, funnel, structuring, invoice, mule, solo).

Output formats include a standard transaction graph (vertices + edges CSVs), an ML-ready schema for mule detection, and a full TigerGraph AML_Schema_V1 export with MinHash-based entity resolution.

### Design Principles

- **Deterministic**: every stochastic decision derives from a seed via `blake2b` hashing, so a given `(seed, config)` yields bit-identical output.
- **Research-grounded**: every probability, median, and sigma has a published citation or an explicitly documented modeling choice.
- **Structurally realistic**: balance ledgers enforce affordability; overdraft protection is a per-account product with tier-specific fees; LOC interest accrues on a dollar-seconds integral; merchants receive business-checking seeds; credit cards operate as liability accounts.
- **Strict typing**: dataclasses with metadata-driven validation; pyright-strict throughout.

---

## Repository Layout

```
common/          Shared utilities: config, channels, IDs, calendar, random, schema, math
entities/        Static population: people, accounts, merchants, landlords, counterparties,
                 credit cards, personas, financial products (portfolio)
infra/           Devices, IPs, routing, shared fraud-ring infrastructure
math_models/    Amount / count / timing / seasonal / momentum / dormancy / paycheck /
                 counterparty-evolution distributions
relationships/   Family graph, recurring employment & lease lifecycle, social network
transfers/       Transaction generation: balances, credit cards, day-to-day spending,
                 family, fraud, government, insurance, legitimate routines, obligations
pipeline/        Orchestration stages and state containers
export/          Standard, mule-ml, and AML exporters
main.py          CLI entry point
```

---

## Pipeline

The top-level orchestrator `pipeline.generate_ledger.simulate` runs three stages in order:

### Stage 1: `build_entities`

1. `generate_people` — creates person IDs; assigns fraud-ring membership, mule roles, victim roles, solo fraudsters via `FraudSampler` (Section: Fraud Typologies).
2. `build_accounts` — one to N accounts per person (binomial, `max_per_person` default 3); first account is always the "primary" deposit account.
3. `generate_pii` — deterministic phone and email derived from person ID.
4. `build_merchants` — core merchant pool (density per 10k people) plus a sparse long tail of external-only merchants; core merchants split into internal (on-us) vs external based on `in_bank_p`.
5. `build_landlords` — typed pool (individual / small LLC / corporate) drawn from the RHFS 2021 unit-weighted distribution; each landlord independently assigned in-bank or external by type.
6. `build_pools` (counterparties) — employers, client payers, platforms, processors, owner businesses, brokerages. Employers and clients are split internal/external.
7. Registration of institutional externals (SSA, disability, insurance carriers, lenders, IRS, bank fee books) from `common.externals.ALL`.
8. `planned_external_family_accounts` — deterministic `XF…` accounts for family members who bank elsewhere.
9. `assign_personas` + `build_persona_objects` — each person gets an archetype and a per-person perturbed `Persona` (lognormal noise around archetype values, beta-distributed paycheck sensitivity).
10. `planned_owned_income_accounts` — freelancers/smallbiz get a `BOP…` business operating account; HNW gets a `BRK…` brokerage/custody account. These are **internal**, same-customer accounts, not externals.
11. `build` (credit cards) — each eligible person draws card approval (persona-dependent); issued cards get APR, credit limit, cycle day, autopay mode.
12. `build_portfolios` — assigns mortgage / auto loan / student loan / tax profile / insurance holdings based on persona priors; insurance rates for non-financed collateral owners are back-calculated so the overall persona-level rate stays close to target.

### Stage 2: `build_infra`

1. `build_ring_plans` — burst window and participating members for shared device/IP per fraud ring.
2. `build_devices` — 1–2 personal devices per person; sparse legit shared-device groups (household/family ambient noise); ring-shared flagged devices.
3. `build_ips` — 1–3 IPs per person; legit noise; ring-shared flagged IPs.
4. `Router.build` — sticky current device/IP per person, occasional switching.
5. `SharedInfra` — map from ring ID to shared device/IP with high-probability usage during fraud transactions.

### Stage 3: `build_transfers`

1. `LegitTransferBuilder.build` — produces the candidate ledger in semantic order:
   - **Income pass**: salary (payroll cadences, job tenure, compound raises), government benefits (SSA Wednesday cohorting, disability), non-payroll revenue (client ACH, platform payouts, card settlements, owner draws, investment inflows).
   - **Routines pass**: direct-deposit splits, rent (with landlord-type-aware channel routing), subscriptions, ATM withdrawals, intra-person self-transfers, day-to-day discretionary spending (market simulator with AR(1) momentum, dormancy, paycheck-cycle boost, seasonality, counterparty evolution).
   - **Family pass**: allowances, tuition, retiree support, spouse transfers, parent gifts, sibling transfers, grandparent gifts, inheritance.
   - **Credit pass**: credit-card lifecycle (purchase → refund/chargeback, interest, late fees, payments).
   The builder also returns a starting `ClearingHouse` with per-account balances, overdraft products, and credit limits applied.
2. Insurance premium and claim events are merged in (reads from portfolio).
3. Financial-product obligations (mortgage/auto/student/tax) are emitted through `transfers.obligations.emit`, which models late/missed/partial/cure cycles with delinquency clustering.
4. **Authoritative pre-fraud chronological replay** (`ChronoReplayAccumulator`): the combined stream is sorted by `(timestamp, source, target, amount)` and replayed against the starting ledger. This pass is the only place where balance-gated drops are recorded, and the only place that **emits liquidity events** — overdraft fees (one per courtesy-tap, capped at 3/day) and monthly LOC interest (dollar-seconds integral × APR / seconds-per-year).
5. `inject` (fraud) — camouflage and illicit transactions are added to the draft ledger based on the target illicit ratio (`target_illicit_p`, default 0.5%).
6. **Post-fraud replay**: same ledger replayed with fraud included, but `emit_liquidity_events=False` so the fees/interest from the first pass aren't double-emitted.
7. `validate_transaction_accounts` — every referenced account ID must be in the registry.

---

## Entity Generation

### IDs

All IDs are fixed-width prefixed strings from `common.ids`:

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

**Leading `X` signals external**. `BOP`/`BRK` are intentionally non-`X` because a freelancer's business account at the same bank is an internal book-to-book transfer destination, not an interbank counterparty (NFIB 2023: 56% of small business owners keep personal and business at the same bank).

### Merchants

Density is configured per 10k people (`per_10k_people` = 120 core, `long_tail_external_per_10k_people` = 400). Core merchants are weighted by a lognormal size distribution (`size_sigma` = 1.2); the long tail carries 18% of the total weight with a heavier sigma (1.8) and is always external.

Categories: grocery, fuel, utilities, telecom, ecommerce, restaurant, pharmacy, retail_other, insurance, education.

**In-bank probability** (`in_bank_p` = 0.06): a large retail bank holds ~10-12% of US deposits (FDIC SOD 2024), but NFIB 2023 shows 56% of small business owners keep personal and business at the same bank. Blended, ~6% of a customer's merchant counterparties bank at the same institution.

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

---

## Personas

| Persona | Share | Rate mult. | Amount mult. | Initial balance | Card p | CC share | Credit limit | Paycheck sensitivity β(α,β) |
|---------|-------|------------|--------------|-----------------|--------|----------|--------------|------------------------------|
| student | 12% | 0.7 | 0.7 | $200 | 0.25 | 0.55 | $800 | Beta(4, 2) — high |
| retiree | 10% | 0.6 | 0.9 | $1,500 | 0.55 | 0.55 | $2,500 | Beta(3, 3) — moderate |
| freelancer | 10% | 1.1 | 1.1 | $900 | 0.65 | 0.65 | $4,000 | Beta(2, 4) — lower |
| smallBusiness | 6% | 2.4 | 1.8 | $8,000 | 0.80 | 0.75 | $7,000 | Beta(2, 5) — lower |
| highNetWorth | 2% | 1.3 | 2.8 | $25,000 | 0.92 | 0.80 | $15,000 | Beta(1, 8) — very low |
| salaried | 60% (residual) | 1.0 | 1.0 | $1,200 | 0.70 | 0.70 | $3,000 | Beta(2, 3) — moderate |

Shares are computed at compile time: the five non-salaried personas have fixed shares (12/10/10/6/2 = 40%) and salaried takes the remainder (60%). This is enforced by a `consteval` check — any future edit that makes the non-salaried shares exceed 1.0 is a compile error.---

## Financial Products

Every person gets a `Portfolio` of optional products. Ownership is conditioned on persona.

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

Anchors: Experian Q3 2024 (85% of new cars and 55% of used financed; average payments; average terms). Term range [36, 84] months.

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

Separate from ownership, annual filing produces one of three outcomes: refund (55%, median $900, IRS Feb–May), balance due (18%, median $1,200, April), or no visible settlement. Quarterly median $1,800. Anchors: JPMorgan Chase Institute (large income swings in Feb–Apr and Dec),
IRS 2024–25 refund stats (~72% of filers receive refunds averaging $3,500+).

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

**Premium distributions** (monthly):
- Auto: median $225, σ = 0.35 (Bankrate 2026).
- Home: median $163, σ = 0.40 (Ramsey 2025).
- Life: median $40, σ = 0.50.

**Collateral anchoring**: mortgaged households receive home coverage with p = 0.998; auto-loan holders receive auto with p = 0.997. For the non-financed remainder, the issuance probability is back-calculated so the overall persona rate matches the target.

**Claim behavior**:
- Auto: 4.2% annual claim rate (III 2024), median payout $4,700, σ = 0.80.
- Home: 5.5% annual (Triple-I 2024), median payout $15,750, σ = 0.90.
- Life: not modeled (death events out of scope).

Annual rates are converted to a window probability via `1 - (1 - p)^(n_months/12)`.

**Escrow interaction**: if a homeowner has a mortgage, the home-insurance premium is treated as already embedded in the mortgage payment and is NOT emitted separately (the Census AHS median payment is already all-in). Claims still fire.

### Credit Cards

At issuance the policy draws:
- APR: lognormal, median 22%, σ = 0.25, clamped to [8%, 36%] (Fed G.19 Q4 2024 average 21.5%).
- Limit: lognormal around persona `credit_limit`, σ = 0.65 (Experian 2024 avg $29,855).
- Cycle day: uniform [1, 28].
- Autopay mode: 40% full, 10% minimum, 50% manual.

Stats anchors: 84% of US adults have ≥1 card (Fed 2023); average balance carried $6,580 (TransUnion Q3 2024).

**Lifecycle generator** (`transfers/credit_cards/generator.py`) processes each billing cycle:
1. Purchases accumulate on the card.
2. Each purchase probabilistically produces a refund (0.6%, 1–14 day delay) or chargeback (0.1%, 7–45 day delay) **from the same merchant that received the charge** — no synthetic refund counterparty.
3. Cycle-end: compute average balance via piecewise-constant integration; if out of grace and there is a debt integral, charge interest at `APR × interval_days / 365`.
4. Minimum due = max(2% of statement, $25). Autopay mode drives payment amount (full / min / manual). Manual splits into pay-full (35%), partial Beta(2, 5) (30%), minimum (25%), miss (10%). Late by cycle has 8% probability with 1–20 day delay.
5. Late fee $32 fires if not paid by due date (+grace_days default 25).

---

## Banking Mechanics

### Balances (`transfers/balances.py`)

Every internal account carries:
1. **Balance** (seeded at simulation start).
2. **Overdraft protection product** — exactly one of NONE / COURTESY / LINKED / LOC, drawn from a per-persona multinomial.
3. **Bank tier** — ZERO_FEE (15%) / REDUCED_FEE (10%) / STANDARD_FEE (75%) (Bankrate 2025, Consumer Reports 2024 account-share composition).
4. **Per-account overdraft fee** — drawn from a tier-specific lognormal at init.

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
| REDUCED_FEE | $15 | 0.25 | Bank of America (2022 policy) |
| STANDARD_FEE | $35 | 0.20 | Chase $34, Wells $35, US Bank $36, PNC $36 |
Cap: 3 fees per account per calendar day (Wells Fargo / industry standard). Liquidity-event channels bypass the insufficient-funds check so the fee always posts.

#### Merchant Balance Seeding

Internal merchants (M…) default to the SALARIED persona's $1,200 initial balance because no person maps to them. That's too low for a business that must honor refunds. The init pass overrides merchant balances with a business-checking lognormal: median $8,000, σ = 0.90 (Bluevine 2025: 39% of SMBs have < 1 month of operating expenses; healthy ones hold 2–3 months). Merchants carry no personal protection products and are ZERO_FEE tier.

#### Credit Card Liability Accounts

`set_credit_limit(card, limit)` repurposes the `overdrafts` slot to hold the credit line, sets protection to NONE, tier to ZERO_FEE, and clears any LOC registration. This makes `available_to_spend(card) = credit_limit` at init while ensuring the card never accrues LOC interest or courtesy fees on top of its own CC_INTEREST / CC_LATE_FEE lifecycle events.

---

## Mathematical Models

### Amount Distributions

All channel → amount mappings live in `math_models/amount_model.py`. Each channel has a single declared model; lookup failure is a loud `KeyError`.

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

### Timing Profiles

Three hour-of-day profiles:
- **consumer**: peaks at 18:00, tapers toward midnight.
- **consumer_day**: peaks at 10:00, drops after 14:00 (retired / stay-at-home).
- **business**: peaks 9:00–12:00, vanishes after business hours.

Each profile's 24-element PMF is normalized; a CDF is precomputed so `searchsorted` samples hour in O(log 24). Minute and second are uniform.

### Momentum — AR(1)

Per-person daily rate multiplier:

$$
m_t = \phi \cdot m_{t-1} + (1 - \phi) \cdot 1.0 + \varepsilon_t, \quad \varepsilon_t \sim \mathcal{N}(0, \sigma^2)
$$

Default φ = 0.45, σ = 0.15, clamped to [0.20, 3.00]. Vilella et al. (2021) measured weekly spending autocorrelations of 0.3–0.6 across personality types. At daily resolution this produces effective weekly persistence of ~0.35–0.50. Grounded in Barabási (2005, *Nature* 435:207–211) on bursty human dynamics; Goh & Barabási (2008, *EPL* 81:48002) formal memory coefficient; Karsai et al. (2012, *Scientific Reports* 2:397) universality across activity domains.

### Dormancy — Three-State Machine

States: ACTIVE → (p=0.0012/day) → DORMANT → WAKING → ACTIVE. Calibrated so ~35% of accounts experience ≥1 dormant spell per year: `1 - (1 - 0.0012)^365 ≈ 0.35`.

- Dormant duration: uniform [7, 45] days (vacation 7–21, hospital 5–30, seasonal 30–90).
- Wake ramp: uniform [2, 5] days, linear interpolation from dormant rate (0.05) back to 1.0.
- Dormant rate: 0.05 — not zero because subscriptions and bills still fire.

Rationale: ~35% of accounts show ≥14 consecutive zero-txn days per year (Fed Payments Study frequency data). Mules show "dormant → tester → spike"; legitimate accounts show "quiet → gradual ramp" (LexisNexis 2025; Unit21 2026 found reactivation transactions average 17× rule threshold, but legitimate reactivation ramps gradually while fraud reactivation is immediate).

### Paycheck Cycle Boost

On payday, trigger a `max_residual_boost × paycheck_sensitivity` multiplier that linearly decays over `active_days` (default 4). Max residual boost 10%. Residual is intentionally small because the day-to-day engine already isolates discretionary flows — this captures the leftover effect beyond recurring bills.

### Counterparty Evolution — Monthly

At month boundaries:
- `merchant_add_p` = 0.35 — add one new favorite, weighted by global merchant CDF.
- `merchant_drop_p` = 0.10 — drop a random existing favorite.
- `contact_add_p` = 0.08 — add a new P2P peer.
- `contact_drop_p` = 0.03 — replace a contact slot with a duplicate (reduces effective diversity).

Vilella et al. (2021) found monthly category turnover of 0.15–0.30 is a stable individual trait correlated with Openness to Experience. This matters for mule detection: mules show SUDDEN counterparty explosion (10–25 new counterparties in days, per FATF 2022); legitimate accounts show 1–2 new merchants/month.

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
- Days-since-payday **relief** (boost for first `relief_days`) and **stress** (ramp over `stress_ramp_days` after `stress_start_day`).
- Cash-on-hand ratio (clipped).
- Fixed-burden ratio (larger fixed monthly obligations → greater stress).

Product is clipped to [0, 1.10] with an absolute floor. Count-stage liquidity shaping is **soft** (never zeros out an entire person-day) — the authoritative ledger is what truly enforces affordability.

---

## Legitimate Transaction Flows

### Salary (Payroll)

Persona-conditioned salary probability, then scaled by policy `salary_fraction = 0.65`:

| Persona | p(salary) |
|---------|-----------|
| salaried | 98% |
| freelancer | 8% |
| smallbiz | 4% |
| hnw | 12% |
| student | 12% |
| retired | 2% |

Each recipient gets an employer, a payroll cadence (27% weekly, 43% biweekly, 20% semimonthly, 10% monthly), and a job tenure drawn from uniform [2, 10] years. Raises compound: `inflation + Normal(0.02, 0.01)` annually. Job switches trigger a `Normal(0.08, 0.05)` bump. Semimonthly pay days are 1/15 or 15/31; monthly is day 28/30/31. Weekend falls roll to previous business day. Posting lag is 0–1 days. The salary amount model is interpreted as one monthly paycheck; annualizing and dividing by `pay_periods_in_year` gives the per-paycheck amount at any cadence.

### Rent

Persona-conditioned renter probability (given not a homeowner), scaled by `rent_fraction = 0.55`:

| Persona | p(rent) |
|---------|---------|
| student | 50% |
| retired | 18% |
| salaried | 62% |
| freelancer | 58% |
| smallbiz | 35% |
| hnw | 10% |

Leases have tenure uniform [2, 10] years. Base rent compounds at `inflation + Normal(0.03, 0.02)` annually. On lease turnover the base is re-sampled. Monthly payment timestamps are jittered in days 0–5 with hours 7–22. Channel is selected by the `RentRouter` using the landlord-type-specific CDF described above — so the same tenant paying the same landlord produces a consistent Zelle/check/ACH/portal signature.

### Day-to-Day Spending (`transfers/day_to_day/`)

The `simulate` function runs a day-by-day market simulator:

1. **Market build**: each person gets `fav_k` ∈ [8, 30] favorite merchants (weighted by global merchant CDF) and `bill_k` ∈ [2, 6] billers. Exploration propensity ~ Beta(1.6, 9.5). Burst windows (optional) ~ 8% of people get a 3–9 day high-spending burst at a random point in the window.
2. **Per-day**: build seasonal × momentum × dormancy × paycheck × weekday × day-shock × liquidity multiplier. Target count per person-day is back-calculated from monthly target, inverting the suppressors.
3. **Per-transaction**: sample channel from `(merchant, bill, P2P, external_unknown)` CDF (weights: 0.82/0.10/0.08 of the non-unknown split, plus `unknown_outflow_p = 0.05` carved out).
4. **Merchant routing**: 82% of the time pick from favorites; else explore (with rejection if the explored merchant is already a favorite). Payment method is card (if the person holds one and `cc_share` rolls) or deposit account.
5. **Monthly boundary**: `evolve_all` adds/drops a merchant favorite and shuffles P2P contacts per the evolution config.

### Subscriptions

Each person gets 4–8 subscription "intents" with 55% actual debit probability. Prices are drawn from a pool of realistic SaaS / streaming amounts ($6.99, $9.99, $14.99, $17.99, $49.99, $99.99, etc.). Billing day uniform [1, 28] with ±1 day jitter per cycle.

### ATM

88% of people are ATM users. Monthly withdrawals uniform [1, 6]. Amounts drawn from a pool weighted toward round multiples of 20/40/60/100 (matches real ATM UX). 75% of withdrawals bias to days 0–18 of the month, 25% to days 18–28. Destination is the bank's ATM network hub account.

### Self-Transfers

45% of multi-account holders actively self-transfer. Monthly [1, 3] transfers, 45% round amounts from pool, 55% lognormal($250, σ=0.80). 70% bias to days 0–6 (post-payday "move to savings"). Source is the account with most available cash; destination is the leanest.

### Direct-Deposit Splits (Paychecks)

For multi-account holders: 30% split their salary. 10–35% of each salary deposit is routed within 5–30 minutes to a secondary account. APA 2024.

### Government Benefits (SSA Wednesday Cohorting)

Post-1997 SSA/RSDI cycle payment rule:
- Birth day 1–10 → 2nd Wednesday of month
- Birth day 11–20 → 3rd Wednesday
- Birth day 21–31 → 4th Wednesday
- If Wednesday is a federal holiday, pay the preceding business day.

Since the transfer generator doesn't carry DOB, the synthetic day-of-month is derived from the same blake2b hash used by `export/mule_ml/party.py`, so the Wednesday cohort stays aligned with the exported fake DOB.

**Social Security** (cfg defaults, SSA 2026 COLA):
- Eligibility: 87% of retirees (SSA Dec. 31, 2025 fact sheet).
- Monthly benefit: lognormal median $2,071 (estimated Jan 2026 avg retired-worker benefit), σ = 0.30, floor $900.

**Disability**:
- 4% of non-retired non-students.
- Monthly: median $1,630 (Jan 2026 avg disabled-worker benefit), σ = 0.25, floor $500.

The repo does NOT model the "paid on the 3rd" exceptions (pre-May-1997 entitlement, dual SSI, foreign residents) — all SSA-style benefits use the standard Wednesday rule.

### Non-Payroll Revenue

Freelancers, smallbiz, and HNW get a revenue engine (`transfers/legit/inflows/revenue/`):

- **Freelancer**: 1–4 client ACH credits/month (median $1,400, σ = 0.70); 1–4 platform payouts (median $425); 1–2 owner draws (median $1,800). 12% quiet-month probability.
- **Smallbiz**: 0–3 client ACH (median $2,600); 0–3 platform (median $950); 4–12 card settlements (median $680); 1–2 owner draws (median $3,400). 6% quiet months.
- **HNW**: 0–2 investment inflows (median $6,500, σ = 1.05). 2% quiet months.

Routing: clients/platforms/processors land on the **business operating account (BOP…)** if the person has one; otherwise on personal. Investment inflows route to the **brokerage (BRK…)** if present. Business timestamps fall on business days (weekend rejection with retry).

---

## Family and Social Flows

Family graph is built by `relationships.family` from three configs: household partitioning (single_p = 29%, Zipf α = 2.2, spouse_p = 62%), dependent structure (65% student-dependent, 35% co-resident, 70% two-parent), and retiree support ties (35% has-adult-child, 35% supports).

### Allowances (parent → student child)

60% weekly / 40% biweekly. Amount follows Pareto (xm = $15, α = 2.2).

### Tuition (parent → education merchant)

55% probability per student per scheduling cycle. 4–5 installments; total per semester is lognormal(μ = 8.7, σ = 0.35). Installments every ~30 days with ±5 day jitter.

### Retiree Support (adult child → retired parent)

Configured supporters with `has_child_p = 0.35` and `support_p = 0.35`. Amount = base Pareto × persona support-capacity weight (i.e. HNW child sends more than salaried child even to the same parent).

Supporter count sampling: 65% → 1 child, 27% → 2, 8% → 3. Spousal sync: if one retired parent is supported, there's an 85% chance the spouse shares the same supporters.

### Spouse Transfers

60% of couples keep at least partially separate accounts (Census 2023, Bankrate 2024). Those couples get 2–6 monthly transfers, lognormal($85, σ = 0.9). Breadwinner asymmetry 65% — higher earner (by persona amount_multiplier) sends more often than vice versa. Pew 2023: 55% of marriages have husband as primary breadwinner, 29% egalitarian, 16% wife breadwinner — the repo uses amount_multiplier as a gender-agnostic proxy.

### Parent Gifts (working parent → adult child)

HRS longitudinal data: 35% of parents 51+ transfer to adult children over a two-year window. Savings.com 2025: 50% support adult children, avg > $1,300/month among givers. We model as 12% monthly probability per eligible parent-child pair, Pareto(xm = $75, α = 1.6) scaled by parent's persona weight.

### Sibling Transfers

31% of family loans are between siblings (FinanceBuzz 2024); 76% have borrowed from a sibling at some point (JG Wentworth 2025). 15% of sibling pairs are "active"; those pairs transfer with 18% monthly probability, median $120, σ = 0.90, direction roughly 50/50.

### Grandparent Gifts

38–45% of 50+ households send money to younger generations (EBRI 2015). Implicit grandchildren found by two-hop traversal (`retired_person → children → their children`). 8% monthly probability, median $150, σ = 0.70.

### Inheritance

26% of Americans plan to leave one (NW Mutual 2024). Modeled as a rare one-shot event: per 180-day window, 0.15% of retirees trigger an inheritance. Amount lognormal median $25,000, σ = 1.0, floor $1,000. Split equally among heirs (direct children, or supporting_children as fallback).

### Family External Accounts

`external_p = 0.18` — probability that a family counterparty banks at a different institution. Co-residing families overwhelmingly share a bank; non-co-residing adult children / siblings are more likely to bank elsewhere (FDIC 2023 + market fragmentation). Deterministic: a given person_id + coin-flip threshold always resolves the same way across runs, so every generator sees the same family member as internal or external without sharing state.

### Social Graph (P2P)

`relationships.social.build` constructs a contact matrix: each person has `effective_degree` (default 12) contacts drawn from a community-aware weighted graph.

- Communities: contiguous blocks of `[6k, 24k]` people, where k is effective_degree.
- Social capital: per-person lognormal(σ = 1.1), with configured hub people getting a 25× multiplier.
- Local probability: 70% within-block; remainder split between explicit cross-block (29%) and full global (1%).
- Tie strengths: Gamma(shape = 1.0) drawn per unique contact, turned into a CDF, sampled by the fixed-width contacts row so stronger ties appear more often.

---

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

### Typologies

Each ring is assigned one typology by weighted choice (defaults: 30% classic, 15% layering, 10% funnel, 10% structuring, 5% invoice, 30% mule):

- **Classic** — victim → mule → fraud + optional cycle passes through the ring.
- **Layering** — victim → entry mule → 3–8 hops of mule/fraud nodes → exit cashout.
- **Funnel** — many sources → one collector → several cashouts.
- **Structuring** — victim splits N payments just under the $10k threshold (CTR avoidance), 3–12 splits of `amount = 10000 - Uniform(50, 400)`.
- **Invoice** — fraud → biller (mimics business-to-vendor payments).
- **Mule** — high fan-in (8–25 inbound sources) + rapid forwarding (1–12 hour delay, 90–95% passed through with 5–10% haircut). This is the textbook FATF 2022 profile: many small-to-medium inbounds, fewer larger outbounds, concentrated in a 5–14 day burst window.
- **Solo** — individual bad actor with no ring affiliation.

### Camouflage

Each participating ring account receives additional **legitimate-looking** transactions to raise the noise floor:
- Monthly bill payments: 35% probability per account per month.
- Daily small P2P: 3% per account per day.
- Recurring salary: 12% of ring accounts receive a plausible payroll stream.

Camouflage events fire with `is_fraud = 0` and `ring_id = -1` so they blend into the legitimate population for anyone looking only at flags.

### Burst Window

Rings operate in a 7–14 day burst. Device and IP sharing is concentrated to this window; outside it the ring members behave legitimately.

### Shared Infrastructure

- Shared device probability (ring): 80%.
- Shared IP probability (ring): 75%.
- Legit shared-device noise: 1% (household ambient activity).
- During a fraud transaction, `TransactionFactory` uses the shared ring device with probability 0.85 and shared IP with probability 0.80.

---

## Infrastructure and Device Attribution

### Devices

Each person has 1 device (80%) or 2 devices (20%). Device types uniformly from `{android, ios, web, desktop}`. Ring-shared flagged devices get ID `FD0000`, `FD0001`, … Sparse legit shared groups (1% noise) model a family ambient device or a household tablet.

### IPs

Each person has 1–3 IPs (1 + Bernoulli(0.35) + Bernoulli(0.10)). Deterministic random IP avoiding invalid NANP ranges.

### Router

Sticky "current" device/IP per person. Default switch probability 5% per transaction. During fraud transactions the `TransactionFactory` consults `SharedInfra` first and falls back to personal infra only if the ring device/IP roll misses.

---

## Chronological Replay and Screening

The authoritative pre-fraud replay (`ChronoReplayAccumulator`) is the single source of truth for balance-gated drops and liquidity-event emission. It sorts by `(timestamp, source, target, amount)` and processes each transaction:

1. Attempt transfer. On acceptance, check whether the debit tapped a COURTESY-protected account into negative — if so, emit an overdraft-fee transaction (capped at 3/day).
2. On insufficient-funds rejection, try to find a future inbound credit that would "cure" the shortfall:
   - Card-like channels (ATM, merchant, card_purchase, P2P): within 10 hours.
   - Retryable ACH-like channels (bill, rent, subscription, external_unknown, insurance, loans, tax): within 36 hours.
   Cure candidates are from the set of channels that legitimately top up an account: salary, government, insurance claims, refunds, self-transfers, incoming family support, etc.
3. If no cure exists, blind-retry with 55% probability after 18 hours (first retry) or 72 hours (second). Max retries: 1 for card-like, 2 for ACH-like.
4. For LOC accounts, billing events are pre-generated (23:55 on each account's cycle day)
and interleaved into the heap so same-timestamp ties process balance changes before interest
computation.4. For LOC accounts, the replay invokes `accrueLocInterestThrough(timestamp)` periodically.
The `LocAccrualTracker` sweeps every enabled account, rolls its dollar-seconds integral forward to
`timestamp` using the current cash value, and for any account whose last billing was ≥ 30 days ago
emits an `InterestAccrual`. The ledger debits interest from cash (bypassing the funding check) and
forwards a `LiquidityEvent` to the sink. Accounts whose `lastBillingTs = 0` are silently anchored on
first sweep so billing does not fire retroactively over the simulation's pre-history.

The post-fraud replay uses the same logic but with `emit_liquidity_events=False` so overdraft fees and LOC interest already in the stream aren't double-posted.

Upstream **soft screens** in individual generators (ATM, subscriptions, self-transfers, day-to-day) use a scratch copy of the initial ledger to avoid glaringly unaffordable proposals. These don't replace the final replay — they just reduce the drop rate.

---

## Export Formats

### Standard (`export/standard/`)

Vertex CSVs: `person.csv`, `accountnumber.csv`, `phone.csv`, `email.csv`, `device.csv`, `ipaddress.csv`, `merchants.csv`, `external_accounts.csv`.

Edge CSVs: `HAS_ACCOUNT`, `HAS_PHONE`, `HAS_EMAIL`, `HAS_USED`, `HAS_IP`, `HAS_PAID` (aggregated), optional raw `transactions.csv`.

### Mule-ML (`export/mule_ml/`)

Optimized schema for GraphSAGE / node-level mule detection:

- `Party.csv` — one row per account with fraud label, phone, email, full deterministic identity (name, SSN, DOB, address, geo via pgeocode, country, canonical IP, canonical device).
- `Transfer_Transaction.csv` — raw ledger.
- `Account_Device.csv`, `Account_IP.csv` — aggregated account-infra edges with counts and first/last seen.

Ages are drawn from persona-specific band distributions (e.g. retired: Beta-weighted across 65–99; student: 16–34). Addresses use `Faker` + `pgeocode` so zip codes resolve to real US cities, with a fallback list.

### AML — TigerGraph AML_Schema_V1 (`export/aml/`)

Full graph export with:

- **Vertices**: Customer, Account, Counterparty, Name, Address, Country, Watchlist, Device, Transaction, SAR, Bank, MinHash buckets (Name / Address / Street_Line1 / City / State), Connected_Component.
- **Edges**: customer_has_account / account_has_primary_customer, send/receive_transaction (customer side) + counterparty_send/receive_transaction (counterparty side) + sent/received_transaction_to/from_counterparty (aggregated), uses_device, logged_from, customer/account/counterparty/bank/address has_name / has_address / associated_with_country, customer_matches_watchlist, references (SAR → Customer with role), sar_covers (SAR → Account with activity amount), beneficiary_bank / originator_bank, resolves_to (counterparty → customer soft link), MinHash bucket edges.

**SAR generation** (`export/aml/sar.py`): one SAR per fraud ring (filed 30 days after the last illicit transaction, per BSA); one SAR per solo fraudster. Violation type inferred from dominant fraud channel: structuring → `structuring`, invoice → `suspicious_activity`, everything else → `money_laundering`. `activity_amount` per account is both-sides (in + out) throughput, not a share.

**MinHash** (`export/aml/minhash.py`): byte-for-byte compatible with TigerGraph's reference `TokenBank.cpp`. Uses Austin Appleby's MurmurHash2 on byte shingles (k=3), plus the exact 101-element c1/c2 universal-hash coefficient tables from the reference. Bucket IDs include the band index (`{PREFIX}_{band}_{hash}`) to keep LSH bands independent. Reference C source embedded at the bottom of the module for migration / validation.

---

## Configuration Reference

All configuration lives in `common/config/` and composes into a single `World` dataclass:

```python
from common.config import World, Window

world = World(
    window=Window(start="2025-01-01", days=365),
    # population, accounts, hubs, personas, merchants, landlords, counterparties,
    # households, dependents, allowances, tuition, retiree_support, spouses,
    # parent_gifts, sibling_transfers, grandparent_gifts, inheritance,
    # family_routing, government, insurance, events, infra, fraud, patterns
)
```

Every numeric field uses dataclass `metadata={"ge": 0}` or similar; `validate_metadata(self)` runs in `__post_init__` so invalid configs fail at construction.

### Window

- `start`: ISO YYYY-MM-DD.
- `days`: positive integer.

### Population

- `seed`: 7 (default). Derives every downstream RNG via blake2b.
- `size`: 80,000 (default).

### CLI

```
python main.py \
    --usecase {standard, mule-ml, aml} \
    --days 365 \
    --out out_bank_data \
    [--show-transactions] \
    [--progress] \
    [--ml-only]
```

- `--ml-only` skips the standard export tables when combined with `--usecase mule-ml`.
- `--show-transactions` emits the raw ledger alongside aggregated edges.

---

## Research Citations

Consolidated reference list.

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
- Federal Reserve 2023 Survey — 84% of US adults have ≥1 credit card.
- Federal Reserve Bank of New York Q4 2024 — 60+ day auto-loan delinquency (2.1%).
- NFIB 2023 Small Business Survey — 56% of small businesses bank personal+business at same bank.
- SoFi / NerdWallet 2025 — overdraft LOC prevalence.

**Credit Cards**
- Experian 2024 — avg 3.9 cards/holder; avg credit limit $29,855.
- TransUnion Q3 2024 — avg carried balance $6,580.

**Mortgages & Auto Loans**
- Census AHS 2023 — median mortgage payment $1,672; 65% of homeowners have mortgages.
- Experian Q3 2024 — auto loan payments, terms, financing rates.
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
- Vilella et al. 2021 — spending persistence & category turnover as stable individual traits (correlated with Openness), *EPJ Data Science* 10:25.

**Fraud & AML**
- FATF 2022 — "Money Laundering Through Money Mules" typology report.
- Europol EMSC — operational mule-detection data.
- UK National Crime Agency — money mule behavioral profiles.
- LexisNexis 2025 — "Money Mule Detection Strategies."
- Merseyside OCG study — 63% of organized criminal groups cooperate with ≥1 other group.
- Unit21 2026 — legitimate reactivation transactions average 17× rule thresholds but ramp gradually.
- FATF 2020 — AML Red Flag Indicators.

**Small Business Banking**
- Bluevine 2025 — 39% of SMBs have < 1 month of operating expenses; healthy ones hold 2–3 months.

**Hash Reference**
- Austin Appleby — MurmurHash2 32-bit.
- TigerGraph DevLabs — `TokenBank.cpp` MinHash reference implementation (embedded in `export/aml/minhash.py` as `_TG_REFERENCE_C_SOURCE`).

---

## License

MIT. See `LICENSE`.
