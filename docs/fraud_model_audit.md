# PhantomLedger — MASTER CITATION DOCUMENT (Model Ground Truth)

THE ONE DOCUMENT. Every research-sensitive number in PhantomLedger is a
row here: the value the code implements, the claim it makes about the
real world, and a citation slot. (Filename is historical; scope is the
whole model.)

## THE AUTHORITY RULE (owner directive, 2026-07-18)

This document is NORMATIVE, in two phases per row:

* **UNCITED row:** the value mirrors the code and is provisional. If
  doc and code disagree in this phase, the DOC is wrong — fix the doc.
* **CITED row (owner has verified a real source):** THE DOCUMENT
  GOVERNS. If the cited real-world value contradicts the model value,
  the row becomes **NONCONFORMING** and PhantomLedger's behavior is
  CHANGED TO FIT THIS DOCUMENT — always through the model-version
  pipeline (owner-approved ADJUST, one named commit, re-pin of every
  golden baseline touched; fraud-audit-2026-07 is the template), never
  a silent edit.
* **CHOICE rows** are normative too, but their normative content is the
  DOCUMENTED DEVIATION: cite the real-world value, state the deviation
  and its reason (e.g., fraud deliberately oversampled for ML label
  density — the older "~100×" figure was dropped per the F-1 Pass 1
  finding: the count-axis oversampling factor is unknown).
  A CHOICE row conforms when the deviation is explicit and justified —
  not when the raw number matches the world.

Classification: **MEASUREMENT** (value should match real data) /
**TYPOLOGY** (shape/structure should match documented patterns) /
**CHOICE** (deliberate deviation, documented). Never conflate
prevalence axes; labels (`is_fraud`, SARs, alerts/CTRs, chain/shell)
are calibrated separately.

## Citation protocol (the owner's pass)

For each row: find the CURRENT edition of the named source (or a
better one); record title, publisher, year/edition, URL, page/table in
the Citation slot; enter the real-world value; set Status:

* **CONFORMS** — cited value supports the model value (or shape).
* **DEVIATES-BY-CHOICE** — cited value recorded; deviation + reason
  stand next to it.
* **NONCONFORMING → ADJUST** — cited value contradicts the model; file
  the ADJUST proposal (owner decides; new model version; re-pins).

Statutes: verify against current eCFR. Every source named below is
knowledge-based (recalled, not retrieved) and may be stale — the pass
replaces recollection with retrieval. Status of every row starts
**UNCITED**.

Row schema used throughout:
**PL value | Class | Claim to verify | Suggested source | Citation
[fill] | Real-world value [fill] | Status [UNCITED]** — the last three
cells are the owner's; only non-default cells are printed below.

## CITATION PASS STATUS (Pass 1 executed 2026-07-18, retrieval-based)

Pass 1 filled the owner cells for the rows listed in the **Pass 1
results** blocks below each affected table. Everything not listed
remains **UNCITED**. Convention for filled rows:
`Citation | Real-world value | Status (proposed)`. Confidence tags:
[Certain] = read directly from the cited source this pass;
[Likely] = strong secondary inference, re-verify before ADJUST;
[Derived] = arithmetic on cited values. All statuses are proposals;
ADJUST decisions remain owner-gated per the AUTHORITY RULE.

Pass 1 headline verdicts: 2 statutory boundary defects found (CTR
trigger uses >= where 31 CFR 1010.311 says strictly more than
$10,000; CTR scope must be verified as currency-only), 2 economic
rows NONCONFORMING (rent, credit card full-payment share), 1
deviation note factually wrong as written (fraud budget p: US card
fraud is ~11 bp of value, not "a few bp"), 6 rows CONFORMS, 3 rows
re-classed DEVIATES-BY-CHOICE with corrected justifications. A
next-pass queue with at-risk flags is appended at the end of this

### Pass 2 (2026-07-18, same session)

Pass 2 executed the entire at-risk queue plus the L-8/L-9/L-11 tail.
Combined coverage after Pass 2: every table in the document now has
at least one cited row; remaining UNCITED rows are enumerated in the
REMAINING OPEN ITEMS section at the end (they are the thin tail:
family-transfer subrows, L-10 revenue profiles, scaffolding
densities, and a few unit-definition checks that require reading the
code, not the literature). One Pass 1 verdict is REVISED in place:
rent (L-3) moves from NONCONFORMING to CONTESTED after DCPC Table 13
produced a per-transaction rent average of $824 that sits on the PL
mean; see the L-3 Pass 2 block. New NONCONFORMING verdicts from Pass
2: student employment (L-4), allowances (L-9), lease tenure (L-5).
document.

### C0 code reads + C1 conformance-statutory (2026-07-18, same session)

The owner adopted the CONFORMANCE PROGRAM: PhantomLedger's numbers are
progressively corrected to fit this document. C0 (the code reads that
settled every CONTESTED/unit-definition verdict blocked on the code)
is DONE — results are written into the affected sections below, marked
"C0 code read". C1 (the statutory correctness batch) is SHIPPED as
model version **conformance-statutory** (re-pin #8) — see the F-6 C1
block and the change history.

### cash-deposits-2026-07 + cash-split-2026-07 (2026-07-18)

C1's statutory scoping exposed a REALISM GAP: the world had no
legitimate large-currency behavior, so a correct CTR rule produced a
near-empty CTR table. **cash-deposits-2026-07** (re-pin #9, measured:
117 CTRs) closed it with a business cash-takings revenue source on the
new `cash_deposit` channel. **cash-split-2026-07** (owner-requested
research round) replaced the provisional persona shares with a
research-anchored CASH-HANDLING SPLIT across ALL personas — sources,
derivations and confidence tags in the L-10 block; every figure is a
named-source recalled value awaiting the owner's retrieval pass, per
the standing protocol.

### scam-fraud-2026-07 (2026-07-18 — victim scams + fraud reporting)

Owner-requested round: model REPORTED transaction fraud and
victim-side scams. Two additions to the unauthorized-fraud family
(F-4 block): (1) a **gift-card scam rail** — victim-AUTHORIZED
impostor scams where the victim is coached into buying several
max-denomination gift cards (new `scam_gift_card` fraud_type label);
(2) a **reporting/reimbursement layer** — reported card compromises
are made whole by merchant chargeback credits (Reg Z zero-liability),
while gift-card scams are never reimbursed. The authorized-vs-
unauthorized contrast is the modelable "scandal" signature.

### household-econ-2026-07 (2026-07-18 — C2 household economics)

The C2 batch of the conformance program: every NONCONFORMING and
owner-call row in the legitimate-economy tables shipped in ONE model
version — rent level, renter share, lease tenure, student employment
(+ the L-10 student cash-share recompute), allowances, fuel ticket,
seasonality amplitude, P2P amount, home insurance premium, home claim
severity, tax refund. TWO code findings landed with it (details in
the L-3 and L-4 C2 blocks): (1) the salary selector's .95 fit target
scale-clamped EFFECTIVE employment to ~100% for every persona except
retirees — the L-4 table described base weights, not behavior; (2) no
homeowner exclusion is wired into rent selection and the .80 fit
target made ~4 in 5 people full-rent payers, ~2.3× the household
renter share. Both fixed. A ledger COVERAGE GAP also closed: the
government-benefits stream (SSA retirement + disability deposits) was
never tabulated here — now at L-4b, UNCITED with recalled anchors.

### card-behavior-2026-07 (2026-07-18 — C3 card behavior + per-case calls)

The C3 batch. THE HEADLINE IS A REVERSAL: the Pass 1 payment-mixture
NONCONFORMING verdict was an AXIS MISREAD of the same class C2 found
in employment — the mixture {full .35 …} applies only to MANUAL
payers (~50% of cards); autopay-full (.40) and autopay-minimum (.10)
cards bypass it, so PL's EFFECTIVE full-payment share was already
.40 + .50×.35 = **.575 against the measured ~.58 — CONFORMS, no code
change** (L-7 C3 block). Shipped code: **cardP trimmed** to weighted
≈ .826 vs credit adoption 82.3% (L-1) and **ATO events-per-case
raised** U{2..5} → U{3..8} so per-case drain ≈ $3.0k vs the UK ~$3.5k
(F-4). Card-rail per-case density re-classed **CHOICE** (label
density; F-4). The ATO **Reg E remediation design** is now written
(F-4 C3 block) but stays unimplemented pending the owner's
entity/channel decision.

### c4-definitions-2026-07 (2026-07-18 — C4 definition writes, doc-only)

The C4 batch closes the definition tasks: seven rows gained the
operational definition that makes them falsifiable — spousal-separate,
tuition COA, subscription comparator (Bango 2025), repeat-victimization
window, accounts-per-person scope, autopay-split comparator, and the
miss-share axis mapping. Each definition was verified against code
before writing (file references in the affected rows). No code
changed; no golden moved. With C4 the conformance program's WRITE
side is COMPLETE: everything remaining is the owner's retrieval/
verify pass and the owner-gated designs in REMAINING OPEN ITEMS.

═══════════════════════════════════════════════════════════════════════
# PART I — FRAUD / AML MODEL
═══════════════════════════════════════════════════════════════════════

### F-1. Prevalence & fraud budget

| Parameter | PL value | Class | Claim to verify | Suggested source |
|---|---|---|---|---|
| Fraud rings per 10k customers | mean 6.0, lognormal σ 0.4 | CHOICE | real organized-group density per customer base is far lower; we oversample for label density | Europol EMMA cycles; UK Finance Annual Fraud Report |
| Solo fraudsters per 10k | 4.0 | CHOICE | lone-actor density (oversampled) | FTC Consumer Sentinel; FBI IC3 |
| Max fraud participation / illicit persons | 6% / ≤0.5% of population | CHOICE | internal caps | — |
| Fraud budget p | 0.0012 of TRANSACTIONS (F = pL/(1−p), exact L) | CHOICE | PL runs fraud at 12 bp of transaction COUNT; published US benchmarks are value-based (~11–18 bp of value in 2023); count-based incidence benchmarks are not published in these sources, so the oversampling factor vs. count is UNKNOWN (deviation text rewritten per the Pass 1 finding below) | Federal Reserve Payments Study; Nilson Report |

**Pass 1 results (F-1):**
* Fraud budget p | Citation: Nilson Report issue on 2023 card fraud
  (nilsonreport.com, "Card Fraud Losses Worldwide in 2023", Dec 2024)
  and Nilson press release Jan 7 2026 (2024 figures); Federal Reserve
  Board, biennial Reg II debit interchange/fraud report (2023 data,
  published 2025). | Real-world value: worldwide gross card fraud
  6.58 cents per $100 of total volume in 2023 [Certain]; US-issued
  cards $14.32B fraud on $13.007T volume in 2023 = ~11.0 bp of VALUE
  [Certain, Derived]; worldwide losses $33.41B in 2024 [Certain];
  covered-issuer debit fraud, all parties, 17.6 bp of value in 2023
  [Certain]. | Status: DEVIATES-BY-CHOICE, but the deviation text is
  NONCONFORMING as written and must be rewritten: (a) the claim
  column's "a few bp" understates US value-based rates (~11 bp card,
  17.6 bp debit); (b) PL's p = 12 bp is a share of TRANSACTION COUNT,
  a different axis from every cited value-based rate. Correct
  deviation statement: "PL runs fraud at 12 bp of transaction count;
  published US benchmarks are value-based (~11-18 bp of value in
  2023); count-based incidence benchmarks are not published in these
  sources, so the oversampling factor vs. count is UNKNOWN, not
  ~100x." The ~100x oversample language in the AUTHORITY RULE
  example should be re-derived or dropped.
  *(C1 doc ride: the row's claim cell and the AUTHORITY RULE example
  now carry exactly this rewrite.)*

### F-2. Ring topology

| Parameter | PL value | Class | Suggested source |
|---|---|---|---|
| Ring size | lognormal μ2.0 σ0.7, clamp [3,150], mean ≈ 9.4 | TYPOLOGY | Europol EMMA (mules per network); FATF |
| Mule fraction of ring | Beta(2,4) → [0.10,0.70], mean ≈ 0.30 | TYPOLOGY | Europol EMMA |
| Mule multi-ring reuse | p 0.06 | TYPOLOGY | Europol EMMA (recurring mules) |
| Victims per ring | lognormal μ3.0 σ0.8, clamp [3,500], mean ≈ 27.7 | TYPOLOGY | IC3 / FTC |
| Repeat victimization | p 0.10 — DEFINITION (C4, code: `synth/people/fraud.hpp Victims::repeatP`, applied in `people/make.hpp`): per victim slot of each ring, p .10 that the slot is filled by a victim of an EARLIER ring (cross-ring reuse) instead of a fresh person; window = the whole simulated period (≤12 months at standard configs), so the usual 12-month revictimization definition applies naturally | MEASUREMENT | FTC Sentinel; revictimization literature — comparator: same person defrauded again within 12 months |

**Pass 2 results (F-2):**
* Ring size / Mule fraction / Mule reuse | Citation: Europol EMMA
  press results: EMMA 7 (2021) 18,351 mules and 324 recruiters;
  EMMA 8 (2022) 8,755 mules and 222 recruiters; EMMA 9 (2023) 10,759
  mules and 474 recruiters (europol.europa.eu newsroom). |
  Real-world value: EMMA publishes network-level totals, not per-ring
  size distributions; mule-to-recruiter ratios ran ~23-57 across
  cycles [Certain on the totals, Derived on the ratios]. | Status:
  DEVIATES-BY-CHOICE for density (deliberate oversample) and the
  per-ring size/mule-fraction distributions are UNVERIFIABLE against
  published data; document that the lognormal(mu 2.0, sigma .7) is a
  modeling convenience anchored only to qualitative typology. No
  ADJUST possible or needed; fix the row's class expectation.
* Repeat victimization p .10 | Status: UNCITED. Literature reports
  revictimization anywhere from ~10% to 45% depending on window,
  fraud type, and definition [Guessing]; pick the definition (same
  fraud type within 12 months is the usual one) before citing. PL's
  .10 will land inside almost any band, so this is a definition task,
  not a number risk.
* Repeat victimization DEFINITION WRITTEN (C4, 2026-07-18) | Code
  read: PL's p .10 is cross-ring victim REUSE at world build — a
  ring's victim slot re-targets a prior ring's victim with p .10
  (`people/make.hpp`), i.e. organized-fraud repeat targeting within
  the simulated period (≤12 months at standard configs). The
  definition task is closed; the number awaits the owner's citation
  (any value in the 10–45% literature band lands, per the note
  above).

### F-3. Playbook mix (17 playbooks, weights sum 1.00)

classic .12, pureMule .12, placementToIntegration .12 (structuring
.25→layering .55→invoice .20), rapidFunnelMule .10 (.20/.65/.15),
smurfThenLayer .08 (.40/.60), shellLaundering .06 (.65/.35),
pureScatterGather .05, pureLayering/pureFunnel/pureStructuring/
pureCycle/pureBipartite/classicWithLayering/scatterGatherWithLayering
.04 each, bipartiteWeb .03, pureInvoice/mixingService .02 each.
CHOICE weights over the FATF placement→layering→integration TYPOLOGY
skeleton. Structuring-phase mass 0.36 (⇒ P(no structuring per ring)
≈ 0.64). Verify skeleton: FATF stage model; FinCEN mule advisories.

**Pass 2 note (F-3):** The placement/layering/integration skeleton is
the canonical FATF three-stage model (fatf-gafi.org, "What is Money
Laundering"; FATF, Professional Money Laundering, 2018) [Likely on
the exact edition, Certain on the framework]. The 17-playbook weight
vector is CHOICE and has no external comparator; its normative
content is the skeleton plus the documented weights, which is already
how the row is written. Status: skeleton CONFORMS; weights
DEVIATES-BY-CHOICE by construction. Pull page cites from the 2018
report at ADJUST time only.

### F-4. Typology structure & fraud amounts

| Parameter | PL value | Class | Suggested source |
|---|---|---|---|
| CTR threshold | $10,000 — the CTR files strictly ABOVE this, currency-only (C1; see F-6) | MEASUREMENT (statutory) | 31 CFR §1010.311 (eCFR) |
| Layering hops | 3–8 | TYPOLOGY | FATF Professional ML (2018) |
| Structuring ε below threshold | U[$50, $1,500] (fraud-audit-2026-07 F1) | TYPOLOGY | FinCEN structuring guidance; FFIEC BSA/AML manual |
| Structuring profile mix | 60% threshold ($8.5k–$9.95k) / 25% medium ($3–7k) / 15% small ($300–1.5k) | CHOICE | FinCEN SAR narratives |
| Splits per victim burst | 3–12; burst 3–8 d; sub-burst 1–2 d; 08–22 h; secondary target p .20 | CHOICE | — |
| Classic-fraud amount | LN($900, .70) floor $50 | CHOICE (re-classed per Pass 2) — targets money-movement/bank-drain scams (phone-contact median $1,400 in 2022; APP per-case ~$2,950), deliberately above the all-fraud CSN median ~$497 | UK Finance APP losses; FTC medians |
| Cycle amount | LN($600, .25). The boost-cycle companion LN($500, .20) was RETIRED (boost-cycle-retire-2026-07): the constant existed in code but was never wired — no Typology enumerator, no fraud channel, no playbook phase, no sampler consumed it | CHOICE | — |
| Card-test charge | U[$0.50,$5.00], ~40% anchors {.50,1,2,5} (test-pinned) | MEASUREMENT | Nilson; issuer advisories |
| Card fraud spend | median ≈ $79, mean ≈ $162, clamp [$1,$5k] (test-pinned; PER TRANSACTION). Per-CASE: targetEvents U{5..14} ⇒ ≈ $1.2–1.5k ≈ 8× the UK per-case average — CHOICE, re-classed by card-behavior-2026-07 (dense compromise events for label density; the per-txn distribution is the measured object) | MEASUREMENT (per-txn) / CHOICE (per-case) | Fed Payments Study; UK Finance |
| ATO drain | median ≈ $180, mean ≈ $554, clamp [$10,$85k], ~0.4% ≥ $10k (test-pinned; PER DRAIN TRANSACTION). Per-CASE: targetEvents U{3..8} (card-behavior-2026-07 — was U{2..5}) ⇒ ≈ $3.0k vs UK remote-banking ~$3.5k/case — CONFORMS as a band | MEASUREMENT | FTC Sentinel; IC3; UK Finance |
| Unauthorized rail mix | card compromise .60 / gift-card scam .12 / ATO .28 (scam-fraud-2026-07; the scam share carved from the card rail) | MEASUREMENT-adjacent | FTC CSN payment-method report mix |
| Gift-card scam (victim-AUTHORIZED) | 2–6 cards/case in ONE 1–4 h coached burst; denominations 75% {$100,$200,$500 triple-weighted} else $50–$500 in $10 steps (mean ≈ $339/card ⇒ ≈ $700–2,000/case, test-pinned); retail merchants; channel card_purchase; fraud_type `scam_gift_card`; NEVER reimbursed | MEASUREMENT-adjacent | FTC gift-card Data Spotlights |
| Card-fraud reporting | per-case reported p .85 → every fraudulent SPEND made whole by a merchant chargeback credit (flag-0, cc_chargeback, lag 1–10 d, outside the fraud budget); sub-$5 test charges never reimbursed; ATO (Reg E) remediation UNMODELED — design written in the C3 block below, owner-gated | MEASUREMENT-adjacent | Reg Z / 15 U.S.C. §1643; network zero-liability; Security.org |

**Pass 1 results (F-4):**
* CTR threshold | Citation: eCFR, 31 CFR 1010.311 (current, retrieved
  2026-07-18), https://www.ecfr.gov/current/title-31/subtitle-B/chapter-X/part-1010/subpart-C/section-1010.311 ;
  aggregation: 31 CFR 1010.313(b). | Real-world value: report required
  for each deposit/withdrawal/exchange/transfer involving a
  transaction IN CURRENCY of MORE THAN $10,000; same-business-day
  currency transactions aggregate when the institution knows they are
  by or on behalf of one person [Certain]. | Status: NONCONFORMING ->
  ADJUST (two defects): (1) boundary: PL triggers at >= $10,000 but
  the regulation is strictly greater; a transaction of exactly
  $10,000.00 must NOT produce a CTR row; (2) scope check: 1010.311
  covers transactions in currency (cash) only; verify the label layer
  files CTRs only on cash-channel rows, never on ACH/card/wire.
  Structuring profile "threshold" band top of $9.95k remains valid
  either way.
  *(RESOLVED by C1 conformance-statutory — see the F-6 C1 block.)*
* Classic-fraud amount | Citation slot (partial): FTC press releases
  2025-03 and 2026-06 (Consumer Sentinel 2024/2025 totals),
  ftc.gov/news-events. | Real-world anchors: reported fraud losses
  $12.5B in 2024, ~$16B in 2025 [Certain]; 38% of 2024 fraud reports
  involved a money loss [Certain]; median loss for victims 80+
  exceeded $1,600 in 2024 [Certain]. Overall median individual loss
  ~$500 [Likely, from prior CSN Data Books, NOT re-verified this
  pass]. | Status: UNCITED (anchors logged). AT RISK: if the ~$500
  overall median holds in the 2025 Data Book, PL's $900 median is
  ~1.8x high and this row goes NONCONFORMING or re-classes to CHOICE.
  Pull the exact median from the CSN Data Book 2025 next pass.
* Card-test charge, Card fraud spend, ATO drain | Status: UNCITED,
  queued (Fed Payments Study fraud tables / UK Finance 2026 edition /
  CSN Data Book medians).

**Pass 2 results (F-4):**
* Classic-fraud amount | Citation: FTC Consumer Sentinel Network Data
  Book 2024 (ftc.gov/reports, Mar 2025) plus secondary transcription
  of its loss tables. | Real-world value: overall median individual
  fraud loss $497 in 2024 ($500 in 2021-2023); average per
  loss-report $12,651; 63% of loss reports were under $1,000
  [Certain]. UK Finance 2026 report: APP fraud averaged GBP 2,324 per
  case in 2025 (576.4M over 248,070 cases) [Certain, Derived]. |
  Status: re-class MEASUREMENT to CHOICE and document the target
  population: PL's $900 median sits 1.8x the all-fraud CSN median but
  well under money-movement scam medians (phone-contact median
  $1,400 in 2022; APP per-case average ~$2,950). As a bank-drain
  playbook the intermediate value is defensible ONLY with that
  sentence written into the row. Alternative: ADJUST median to ~$500
  and accept all-fraud calibration.
  *(C1 doc ride: the row now carries the re-class and the sentence.)*
* Card fraud spend | Citation: UK Finance Annual Fraud Report 2026
  (May 2026 data on 2025). | Real-world value: remote purchase card
  fraud GBP 423.5M across 3.2M cases = ~GBP 132 (~$167) average per
  case [Certain, Derived]. | Status: CONFORMS: PL mean $162 (median
  $79, lognormal) is within 3% of the UK per-case average; the
  test-pinned distribution stands.
  *(RE-OPENED by the C0 axis flag below: this compared per-txn to
  per-case. CLOSED by card-behavior-2026-07: per-case re-classed
  CHOICE — see the C3 block.)*
* ATO drain | Citation: same report. | Real-world value: remote
  banking fraud GBP 104.4M across 37,646 cases = ~GBP 2,773 (~$3.5k)
  average per CASE [Certain, Derived]. | Status: UNIT CHECK REQUIRED
  before a verdict: PL's mean $554 is per drain transaction; if a
  modeled ATO case executes ~5-7 drains the case totals reconcile
  with the UK average. Read the code, write the per-case expectation
  into the row, then set CONFORMS or ADJUST. Do not compare per-txn
  to per-case again.
  *(CLOSED by card-behavior-2026-07: targetEvents raised to U{3..8}
  ⇒ per-case ≈ $3.0k, CONFORMS as a band — see the C3 block.)*
* Card-test charge | Status: UNCITED, low risk: sub-$5 authorization
  testing is qualitatively described in issuer and network advisories
  [Likely]; pull one named advisory (Visa card-testing bulletin) for
  the citation slot. The pattern, not the exact bounds, is the claim.

**C0 code reads (F-4, 2026-07-18):**
* ATO drain UNIT (closes the Pass 2 unit check) | One ATO case = one
  `CompromisePlan` executing `plan.targetEvents` drain transactions
  (`transfers/fraud/typologies/unauthorized.cpp`). The sampler
  (`injector.cpp` budget-split loop): NON-CARD (bank-drain) plans
  draw targetEvents ~ U{2..5}, mean 3.5, each event an
  atoDrainAmount draw (mean $554) ⇒ implied per-case mean ≈ 3.5 ×
  $554 ≈ **$1.9k vs the UK remote-banking ~$3.5k per case**
  [Derived] — same order of magnitude, low side (~55%). Owner call
  (rides with C3): CONFORMS-as-band, or ADJUST (raise non-card
  targetEvents toward ~4–7 or the drain median).
  *(RESOLVED by card-behavior-2026-07: ADJUST shipped, U{3..8}.)*
* Card fraud spend AXIS FLAG (re-opens the Pass 2 CONFORMS) | CARD
  plans draw targetEvents ~ U{5..14}, mean 9.5 (≤2 sub-$5 test
  charges at p .7, the remainder spends at the pinned per-txn
  distribution, mean $162) ⇒ implied per-case spend ≈ $1.2–1.5k.
  The Pass 2 verdict compared PL's PER-TRANSACTION mean ($162) to
  the UK PER-CASE average (~$167) — an axis mismatch; the near-match
  is coincidence. On the per-case axis PL runs ~8× the UK
  remote-purchase average. The per-txn distribution remains
  test-pinned (`test_fraud_amounts`) and individually plausible; the
  per-case total is the open question. Owner call (rides with C3):
  re-class CHOICE (dense drains per compromise for label density) or
  ADJUST card-rail targetEvents downward.
  *(RESOLVED by card-behavior-2026-07: re-classed CHOICE.)*
* CTR trigger | Both statutory defects were confirmed in code
  (`derived.cpp TxnSweep::observe`: `>= 10000.0`, no channel filter)
  and FIXED by C1 — see the F-6 C1 block.

**scam-fraud-2026-07 (F-4) — sources & derivations. Every figure is a
NAMED-SOURCE recalled value with a confidence tag, awaiting the
owner's retrieval pass (assistant browser unavailable this session):**
* GIFT-CARD SCAM TYPOLOGY | Sources: FTC Data Spotlight series on
  gift-card scams ("Scammers prefer gift cards", 2021; follow-ups
  through 2023): gift cards the MOST-REPORTED scam payment method for
  several years; reported gift-card scam losses ≈ $217M in 2023;
  victims coached by phone to buy multiple MAX-DENOMINATION cards,
  often across 1–2 stores in one trip; Target, Apple and Google Play
  the most-named brands; median per-scam losses in the $500–$1,000
  band [Likely on exact vintages/figures]. Retailer per-card caps
  commonly $500 [Likely]. | PL model: rail share .12 of compromise
  plans (carved from the card rail; FTC CSN payment-method REPORT
  mix: credit cards the largest reported method, gift cards ≈ 10–15%
  [Likely]); 2–6 cards per case in one 1–4 h burst; amounts 75%
  {100, 200, 500×3} else $50–$500 on the $10 lattice (analytic mean
  ≈ $339/card, test-pinned in `test_fraud_amounts`); per-case ≈
  $700–2,000, bracketing the FTC medians [Derived]. Rows ride the
  LEGITIMATE card_purchase channel (the purchases are real) —
  detectability comes from the burst pattern + round denominations +
  the `scam_gift_card` label, not the rail.
* REPORTING / REIMBURSEMENT LAYER | Sources: Reg Z / 15 U.S.C. §1643
  caps cardholder liability for UNAUTHORIZED card use at $50, and
  network zero-liability policies waive even that [Certain on the
  statute]; Security.org card-fraud reports: the large majority of
  card-fraud victims detect the charges and are made whole [Likely
  on the share]. | PL model: per-case reported p .85 (card rail
  only); each fraudulent SPEND is reimbursed by a merchant
  chargeback credit (channel cc_chargeback, flag-0, fraud_type none,
  lag 1–10 d) — sub-$5 test charges go unnoticed and are not
  reimbursed. Gift-card scams: NO reimbursement (authorized
  payments; FTC: recovery rare once the codes are read out). ATO
  (Reg E, unauthorized EFT) remediation is UNMODELED — logged as a
  known gap for a future round (needs a bank-remediation entity).
* BUDGET MECHANICS (engineering note): reimbursement credits are
  flag-0 remediation rows — like camouflage rows, they live OUTSIDE
  the exact fraud budget F = pL/(1−p); `unauthorized::generate`
  bounds only flag-1 rows against the budget (`fraudEmitted`
  counter). Fraud density on the flag axis is unchanged.

**card-behavior-2026-07 (F-4, C3) — per-case calls closed + the Reg E
design:**
* CARD-RAIL PER-CASE → CHOICE (re-classed) | targetEvents stays
  U{5..14}. Documented deviation: PL runs dense compromise events per
  case (~8× the UK per-case remote-purchase average) DELIBERATELY —
  compromise sessions need enough rows for device/IP/burst pattern
  learning, the same label-density rationale as the F-1 fraud budget.
  The per-txn distribution (median $79, mean $162) remains the
  MEASUREMENT and is test-pinned. The row conforms through this
  paragraph.
* ATO PER-CASE → ADJUST SHIPPED | `injector.cpp` non-card
  targetEvents U{2..5} → **U{3..8}** (mean 5.5) ⇒ implied per-case
  drain ≈ 5.5 × $554 ≈ **$3.0k vs the UK remote-banking ~$3.5k/case
  (~87%) — CONFORMS as a band** [Derived]. The drain-amount sampler
  is untouched (its median $180 is the cited Security.org per-victim
  median; raising it would break that row's own anchor).
* REG E REMEDIATION DESIGN (written, NOT implemented — owner-gated) |
  Statute: Reg E / 12 CFR 1005.6 caps consumer liability for
  unauthorized EFTs ($50 if reported ≤2 business days, $500 ≤60
  days); 1005.11 requires investigation with PROVISIONAL CREDIT
  within 10 business days [Certain on the framework]. Model design:
  per-ATO-case reported p ≈ .90 (large drains get noticed); the
  VICTIM'S BANK (not a merchant) posts a credit for each drain,
  lag ~2–10 business days. Two prerequisites the owner must approve:
  (1) a bank-remediation counterparty account (the cash hub is
  semantically wrong; the card issuerAccount is card-side); (2) a
  dedicated credit channel — reusing `cc_chargeback` would conflate
  merchant-funded card chargebacks with bank-funded Reg E credits
  and corrupt the F-4 reporting row's semantics. New channel ⇒
  taxonomy byte + names + exporter purpose + `test_channels` re-pin;
  ship as its own model version when approved.

### F-5. Camouflage

Small P2P p .03/day; monthly bill p .35; salary inbound p .12 — CHOICE
(+ planned separability measurement).

### F-6. Detection & label layer

| Parameter | PL value | Class | Suggested source |
|---|---|---|---|
| Below-CTR alert band | [$9,000, $10,000] → sev 2, ALL channels (upper edge inclusive since C1, so exactly-$10,000 lands here) | CHOICE | FFIEC; TM vendor catalogs |
| CTR record | STRICTLY > $10,000 AND currency channel (`channels::isCurrency`: atm_withdrawal, cash_deposit, fraud_structuring) → sev 3 + CTR row (C1 + cash-deposits) | MEASUREMENT (statutory) | 31 CFR §1010.311 |
| Velocity alert | ≥5 txns/(account,day) → sev 2 | CHOICE | TM vendor docs |
| Alert→case escalation | 1 in 8 (content-hash) | CHOICE — no external claim (the "TM conversion stats 5–15%" source was uncitable vendor folklore; re-annotated per Pass 2) | — |
| SAR filing probability | 0.70 per group (content-keyed) | CHOICE (F3) | FinCEN SAR Stats |
| SAR monetary floor | ≥ $5,000 group total | MEASUREMENT (statutory) | 31 CFR §1020.320 |
| SAR filing lag | activity end + 30 days | MEASUREMENT | 31 CFR §1020.320(b)(3) |
| shell_score | round2(passThrough × (1 − organicShare)) | CHOICE (F2) | FATF shell typologies |

**Pass 1 results (F-6):**
* CTR record | Citation: eCFR, 31 CFR 1010.311 and 1010.313(b)
  (current, retrieved 2026-07-18). | Real-world value: strictly more
  than $10,000, currency only, same-day aggregation [Certain]. |
  Status: NONCONFORMING -> ADJUST, same two defects as the F-4
  threshold row (boundary at exactly $10,000; currency-only scope).
  Below-CTR alert band [$9,000, $10,000) is unaffected as CHOICE, but
  note the exactly-$10,000 edge currently falls into neither band
  correctly once the trigger is fixed: decide whether $10,000.00
  lands in the sev-2 band (recommended: extend band to [$9,000,
  $10,000]) or stays unalerted.
  *(RESOLVED by C1 — block below; the recommended band extension
  shipped.)*
* SAR monetary floor | Citation: eCFR, 31 CFR 1020.320(a)(2)
  (current, retrieved 2026-07-18). | Real-world value: reporting
  required when a transaction "involves or aggregates at least
  $5,000" and suspicion criteria are met [Certain]. Nuances not in
  PL: insider abuse reportable at ANY amount; $25,000+ tier
  reportable with no suspect identified. | Status: CONFORMS for the
  modeled money-laundering case; nuances optional, log as known
  simplification.
* SAR filing lag | Citation: eCFR, 31 CFR 1020.320(b)(3). |
  Real-world value: file no later than 30 CALENDAR DAYS after INITIAL
  DETECTION of facts constituting the basis; +30 days (60 total) if
  no suspect identified on detection date [Certain]. | Status:
  DEVIATES-BY-CHOICE (re-class from MEASUREMENT): PL keys the lag to
  activity end because a detection date is not modeled; document that
  proxy explicitly, and note the unmodeled 60-day no-suspect tier.
* SAR filing probability 0.70 | anchor only: FinCEN SAR Stats
  (fincen.gov/reports/sar-stats) remains the right source for the
  funnel calibration named in the F3 finding; UNCITED this pass.

**Pass 2 results (F-6, funnel anchors):**
* National BSA volumes | Citation: FinCEN FY2024 Year in Review (via
  ABA Banking Journal, Jun 2025). | Real-world value: 4.7M SARs and
  20.5M CTRs filed in FY2024 (12,870 and 56,160 per day); ratio ~4.4
  CTRs per SAR; fraud-typed SARs ~52% of filings [Certain]. | Use:
  these are the calibration anchors for the F3 funnel item (SAR p
  .70 and alert-to-case 1-in-8). PL's probe corpus (5 rings, 53
  structuring rows, 2 SARs, and its CTR row count) should be checked
  against the 4.4:1 CTR:SAR ratio as a sanity band, not a target,
  since PL oversamples fraud deliberately.
* Alert-to-case 1 in 8 | Status: UNCITED and the suggested source
  ("TM conversion stats 5-15%") has no citable public origin; it is
  vendor folklore [Guessing]. Either find a named survey (some
  regulator speeches quote false-positive rates above 90%, implying
  under-10% conversion) or re-annotate the row as CHOICE with no
  external claim.
  *(C1 doc ride: row re-annotated exactly so.)*

**C1 conformance (2026-07-18) — model version `conformance-statutory`
SHIPPED (re-pin #8):**
* Both statutory defects are FIXED to the cited 1010.311 text: the
  CTR (alert + row) fires on **strictly more than $10,000** and
  **only on currency channels** — `channels::isCurrency` in
  `taxonomies/channels/predicates.hpp`. At C1 the currency set was
  {`Legit::atm` ("atm_withdrawal", cash-out) and `Fraud::structuring`
  (structured cash deposits; structuring is definitionally a currency
  offense, 31 USC 5324)}; cash-deposits-2026-07 added
  `Legit::cashDeposit` (legitimate cash takings). No other tag in the
  taxonomy models physical cash (everything else is
  ACH/card/wire/check-like); widening the set is a model-version
  decision recorded here first.
* Band edge: the sev-2 band extended to **[$9,000, $10,000]
  inclusive** (the Pass 1 recommendation), so exactly-$10,000.00
  alerts at sev 2 and files nothing. The band stays ALL-channel by
  CHOICE (generic high-amount monitoring heuristic).
* Plumbing: `TxnSweep::observe` now reads `session.channel`, so the
  PostgreSQL read-back decode contract was extended — the scan
  decodes the `channel` column losslessly (`channels::parse` inverts
  `channels::name`, names validated unique). Pinned by
  `test_pg_readback` (channel-diverse fixture, exact Tag round-trip)
  and `test_derived_readback` (corpus-vs-readback bundle parity PLUS
  the statutory pins: an exactly-$10,000.00 currency row files
  nothing; a $15,000.77 merchant row files nothing).
* KNOWN SIMPLIFICATION (logged, unmodeled): same-business-day
  currency aggregation, 31 CFR 1010.313(b) — PL files
  single-transaction CTRs only, so a structurer's five same-day
  $2,500 cash deposits do not aggregate into a CTR.
* VOLUME CONSEQUENCE (at C1; RESOLVED same day by
  cash-deposits-2026-07): statutory scoping collapsed the CTR table
  from 340 rows (all non-currency ≥$10k rows — dominated by the
  salary tail, ~7% of salary credits above $10k under LN($4,500,.55))
  to ~0, because the world had NO legitimate large-currency behavior.
  cash-deposits-2026-07 added it — **measured at re-pin #9: 117 CTR
  rows**, all legitimate cash deposits, vs the FinCEN per-adult
  anchor ≈128 (within 9%).
* Status after C1: the F-4/F-6 CTR rows conform to the cited
  1010.311 text (boundary + scope); owner confirms CONFORMS at the
  re-pin commit.

### F-7. Measured emergent properties (from generated corpora)

Pre-cash-deposits (pop 10k/60d/seed 7): threshold splits below alert
band 0.375 (analytic 0.345); posted structuring mix 15/32/53 vs
sampler 60/25/15 (unfunded victim debits bounce at clearing —
emergent); SAR filing observed 2/9 and 0/2 groups; fraud rows 1,769 /
candidates L=909,116 (0.19%); CTR count pre-C1: 340 (all
non-currency).

**Measured at re-pin #9 (cash-deposits-2026-07, same config), read
from `tests/golden_tables_aml.md5`:** CTR rows **117** (anchor ≈128
[Derived]; analytic pre-attrition estimate was ≈129 — quiet months,
weekend rolls and window edges account for the haircut). Alerts
28,531 → **24,231** (the ~4,300 net drop = the old non-currency CTR
alerts removed, partially offset by new band/velocity alerts from
deposit activity). SARs unchanged at 2; ESCALATED_TO 46 → 42;
CONTAINS_ALERT 249 → 232. **Corpus 910,176 → 881,368 (−3.2%)** —
emergent liquidity knock-on: cash inflows raise business-account
liquidity, so fewer overdraft-fee rows and fewer retry rows post
(qualitative attribution; the invariance gates all passed, so the
shift is deterministic and internally consistent). Re-measure the
band composition (legit deposits vs structuring in [$9,000, $10,000])
at the next probe. **At the scam-fraud re-pin, measure:** the
fraud_type mix (scam_gift_card share ≈ .12 of unauthorized cases),
the flag-0 chargeback count, and the per-case gift-card totals.

**At the household-econ re-pin (#12), measure:** salary-credit count
(expect roughly −30%: the fit target moved .95 → .65); rent rowcount
(expect roughly −55% at ~1.76× the mean amount); CTR count (expect
≈ unchanged, 110–125 — the business cash-deposit source is untouched
by this round); total corpus size (expect a visible shift from the
income/rent liquidity knock-ons: more bounced debits for the
newly-unwaged personas, fewer rent debits overall); the posted
structuring mix and the [$9,000, $10,000] band composition.

**At the card-behavior re-pin (#13), measure:** issued-card count
(expect ≈ −3%: weighted cardP .855 → .826, content-keyed per-person
coins so ~all existing holders keep their cards minus the trimmed
margin); ATO case count and per-case drain totals (targetEvents mean
3.5 → 5.5 ⇒ FEWER cases at the same flag-1 budget, each ≈ $3.0k);
CTR count (expect ≈ unchanged); card-payment row mix (unchanged
mixture — only issuance moved).

═══════════════════════════════════════════════════════════════════════
# PART II — LEGITIMATE ECONOMY
═══════════════════════════════════════════════════════════════════════

*(card-behavior-2026-07 edits L-1 and L-7 below; household-econ-2026-07
edited L-2, L-3, L-4, L-4b, L-5, L-8, L-9, L-10; all other content is
preserved from the previous revision.)*

### L-1. Population & personas

Shares (CHOICE — verify demographic context): salaried .60, student
.12, retiree .10, freelancer .10, smallBusiness .06, highNetWorth .02.

| Persona | rate× | amt× | timing | init bal | cardP | ccShare | limit | weight | paySens |
|---|---|---|---|---|---|---|---|---|---|
| student | 0.7 | 0.7 | consumer | $200 | .60 | .55 | $800 | .18 | .67 |
| retiree | 0.6 | 0.9 | consumerDay | $1,500 | .82 | .55 | $2,500 | .30 | .50 |
| freelancer | 1.1 | 1.1 | consumer | $900 | .85 | .65 | $4,000 | .95 | .33 |
| smallBusiness | 1.2 | 1.4 | business | $8,000 | .95 | .75 | $7,000 | 1.50 | .29 |
| highNetWorth | 1.3 | 2.8 | consumer | $25,000 | .98 | .80 | $15,000 | 2.20 | .11 |
| salaried | 1.0 | 1.0 | consumer | $1,200 | .85 | .70 | $3,000 | 1.00 | .40 |

`cardP` gates CREDIT-card issuance specifically (`synth/cards/
issue.hpp` coins `persona.card.prob`); `ccShare` = credit share of
spend; `limit` = credit limit (C0 code read, 2026-07-18). cardP
column trimmed by card-behavior-2026-07: weighted mean .855 → **.826**
against S-DCPC credit adoption 82.3% (student .65→.60, retiree
.84→.82, freelancer .88→.85, salaried .88→.85; smallBusiness/HNW
unchanged).
Paycheck-sensitivity Beta(α,β): student (4,2), retiree (3,3),
freelancer (2,4), smallBusiness (2,5), HNW (1,8), salaried (2,3).
Heterogeneity: medians jittered LN σ.15; probabilities Normal σ.08
clamp [.01,.99]. Class MEASUREMENT-adjacent. Sources: Fed SCF
(balances, limits); CFPB/Fed card ownership.

**Pass 2 results (L-1):**
* Initial balances | Citation: Federal Reserve, 2022 Survey of
  Consumer Finances (transaction account tables, widely
  transcribed). | Real-world value: median HOUSEHOLD transaction
  account balance $8,000 (mean $62,410); median checking-only $2,800
  (mean $16,891); under-35 median $5,400; top income decile median
  $111,600 [Certain]. | Status: DEVIATES-BY-CHOICE, documented as
  initial conditions rather than steady state: PL's weighted initial
  balance is ~ $1,960 per person against a ~ $2,800 per-household
  checking median, defensible for day-zero checking. Two flags:
  retiree $1,500 is LOW (65-74 cohort medians are multiples of
  that) and HNW $25,000 is low against the top-decile $111,600
  unless HNW wealth is held off-ledger by design; state that.
* Card ownership | already cited in the L-7 Pass 1 block (S-DCPC
  Table 3: credit 82.3%, debit 90.3%).
* cardP definition (C0 code read, 2026-07-18) | cardP is strictly
  CREDIT-card issuance, so the right comparator is credit adoption
  82.3%: PL's weighted ~.85 is slightly HIGH (a couple of points),
  near-CONFORMS; final call rides with C3 (card-behavior batch).
  *(RESOLVED by card-behavior-2026-07: trimmed to weighted ≈ .826 —
  CONFORMS on the strictly-credit comparator.)*

### L-2. Spending engine

| Parameter | PL value | Class | Suggested source |
|---|---|---|---|
| Transaction load | 40 txns/person/month | MEASUREMENT | Fed Diary of Consumer Payment Choice |
| Daily counts | gamma-Poisson k=1.5; weekend ×0.8; day shock Gamma(shape 1.3, scale 1/1.3) — unit mean, fatter tail at lower shape (`actors/day.cpp`, C0) | TYPOLOGY | payment-count dispersion literature |
| Slot mix | merchant .82 / bills .10 / p2p .08 around external .05 ⇒ effective 77.9/9.5/7.6/5.0 (matches run-log attempt shares) | MEASUREMENT | Fed Diary payment purposes |
| Known-biller preference | .55; merchant retry limit 6; pick attempts 250 | CHOICE | — |
| Exploration | base .02/txn; per-person propensity Beta(1.6, 9.5); burst p .08 for 3–9 d | CHOICE | — |
| Seasonality (unit mean) | Jan .94, Feb .96, Mar 1.02, Apr 1.01, May 1.00, Jun .99, Jul .98, Aug 1.03, Sep 1.01, Oct 1.00, Nov 1.05, Dec 1.15 — damped to the Census NSA Dec/Jan ratio ~1.22 (household-econ-2026-07) | MEASUREMENT | Census monthly retail sales |
| Momentum | AR(1) φ .45, σ .15, clamp [.20, 3.00] | CHOICE | — |
| Dormancy | enter .0012/day; 7–45 d at ×.05; wake 2–5 d | CHOICE | — |
| Paycheck boost | ≤ +10% × sensitivity, 4-day decay | TYPOLOGY | payday-response literature (JPMC Institute) |
| Liquidity throttle | relief ≤2 d post-payday (+.04+.06·sens); stress from day 7 over 7 d (−.10−.15·sens); cash factor .85+.15·(avail/max($75,baseline)); burden max(.88, 1−.08·ratio); clamp [.70, 1.10]; count factor (.5+.5·liq)²; amount factor 1→.85 across liq .95→.70 | CHOICE (mechanism) | consumption-smoothing literature |
| Commerce evolution | merchant add .35 / drop .10 per day (max 40 favorites); contacts add .08 / drop .03 (max 20) | CHOICE | — |

**Pass 1 results (L-2):**
* Transaction load | Citation: Federal Reserve, 2026 Findings from
  the Diary of Consumer Payment Choice (Oct 2025 data),
  frbservices.org; Atlanta Fed, 2024 Survey and Diary of Consumer
  Payment Choice Tables, Table 6 (Oct 2024 data). | Real-world value:
  47 payments/consumer/month in Oct 2025 (16 credit, 15 debit, 6
  cash) [Certain]; 48.2 in Oct 2024, of which cash 6.7, check 1.2,
  debit 14.3, credit 16.6; average payment $142; average monthly
  value $6,867 [Certain, read from Table 6]. | Status: CONFORMS via
  documented derivation: PL models BANK-VISIBLE rows, so comparable
  count = total payments minus cash payments (47 - 6 = 41; 48.2 -
  6.7 = 41.5), with cash reaching the ledger as ATM withdrawals
  (modeled separately in L-6, 1-6/mo). PL's 40/mo sits within
  rounding of that derivation. Write this derivation into the row;
  without it the row reads as 17% low vs. the headline 48.
* Slot mix | Citation slot (located, not yet transcribed): same 2024
  S-DCPC Tables document, Tables 9-12 (purchases vs bill payments,
  levels and shares) and Table 13 (merchant type). | Status: UNCITED,
  AT RISK: bills at .10 of attempts is likely LOW vs. DCPC bill
  counts [Likely]; partial mitigation is definitional, PL books
  utilities/telecom/insurance as merchant tickets while DCPC counts
  them as bills, so the comparison must be made on a mapped basis.
  Transcribe exact Table 9-12 counts next pass and reconcile.
* Seasonality | Status: UNCITED, queued (Census Monthly Retail Trade
  Survey NSA monthly factors; verify the Nov 1.16 / Dec 1.22 shape).

**Pass 2 results (L-2), supersedes the Pass 1 load derivation:**
* Transaction load, corrected accounting | The Pass 1 derivation
  compared only the 40/mo engine to Diary non-cash payments; that
  was incomplete because L-5 through L-8 generators add rows on top.
  Full accounting: PL bank rows = 40 engine + ~2.5 subscriptions +
  ~3 ATM + ~1 internal transfers + ~2-5 loan/insurance/card
  payments = ~ 48-52 per person-month. Diary comparator: 48.2 total
  payments minus 6.7 cash = 41.5 non-cash, plus ATM withdrawals
  (not counted as payments) ~ 3-5 = ~ 45-46 bank rows. Verdict: PL
  runs ~ 5-15% above the Diary-derived bank-row count [Derived].
  Status stays CONFORMS as a band, but write THIS derivation into
  the row, not the Pass 1 one. (Cash-deposit rows add ~0.3
  rows/person-month on average — inside the band.)
* Slot mix, resolved | Citation: 2024 S-DCPC Tables 9a, 11, 13
  (fetched and transcribed 2026-07-18). | Real-world value: bills
  10.2 of 48.2 payments = 21.2% of count ($4,267 of $6,867 = 62% of
  value, avg $418/bill); purchases incl. P2P 38.0 (78.8%); merchant
  category "A person" 1.8/mo = 3.7% of count, avg $181/txn
  [Certain]. | Status: bills RECONCILE on a mapped basis: PL's .10
  engine-bills slot plus PL's out-of-engine recurring debits
  (subscriptions, loans, insurance, card payments, rent) is ~ 20-25%
  of total PL rows, matching the Diary's 21.2%. Write the mapping
  into the row. P2P is the residual problem: PL 7.6% engine share
  plus L-9 family transfers is roughly 2x the Diary's 3.7% count
  share; set DEVIATES-BY-CHOICE (P2P density feeds fraud typologies)
  or ADJUST the slot toward .04.
  *(household-econ-2026-07: the COUNT share stays
  DEVIATES-BY-CHOICE — P2P density feeds the fraud typologies; the
  AMOUNT row shipped, next bullet.)*
* P2P amount | Real-world value: Diary "A person" average $181/txn;
  mobile-app payment average $71.9/txn (Table 8) [Certain]. PL
  LN($45,.80) has mean $62, low against both. | Status: DEVIATES,
  owner call: raise median toward $55-70 (app-like) or document the
  small-social-payment choice.
  *(RESOLVED by household-econ-2026-07: median $45 → $55, σ .80 —
  implied mean ≈ $75.7 against the $71.9 mobile-app average; the
  app-like reading is the documented comparator.)*
* Merchant tickets vs Diary Table 13 per-txn averages [Derived,
  Certain inputs]: utilities $132.4 vs PL mean $130 CONFORMS;
  communications $78.7 vs $78.5 CONFORMS; education $250 vs $239
  CONFORMS; grocery cluster $52.2 vs PL grocery mean $58.2 CONFORMS;
  restaurant+fast food blended $27.8 vs PL restaurant mean $33.5
  acceptable; stores $82.0 vs PL ecommerce $108.6 / retailOther
  $59.6 blended CONFORMS. One failure: gas $32.8 vs PL fuel mean
  $47.8, ~ 46% high. Status: table CONFORMS overall; fuel
  NONCONFORMING -> ADJUST (drop fuel median toward $32-38; note gas
  prices move this row year to year).
  *(RESOLVED by household-econ-2026-07: fuel median $45 → $32, σ
  .35 — implied mean ≈ $34.0 vs the Diary $32.8.)*
* Seasonality | Citation: Census MARTS not-seasonally-adjusted
  levels as analyzed in trade press (Dec 2025 NSA retail $817B; Jan
  2025 $668B). | Real-world value: Census NSA Dec-to-Jan ratio ~
  1.22; PL's Dec 1.22 / Jan 0.88 implies a 1.39 ratio [Derived]. |
  Status: shape CONFORMS (Dec peak, Jan trough, Nov elevated);
  amplitude DEVIATES, PL's tail spread is ~ 1.7x the retail
  benchmark, and PL models total consumer spending where seasonality
  is flatter than retail. Owner: either damp the tails (Dec ~ 1.15,
  Jan ~ 0.94, Nov ~ 1.02-1.05) or re-class CHOICE (amplified for
  signal).
  *(RESOLVED by household-econ-2026-07: tails damped exactly as
  proposed — Dec 1.15, Jan 0.94, Nov 1.05; Dec/Jan ratio now 1.22 on
  the Census anchor; the consteval unit-mean normalization in
  `math/seasonal.hpp` preserves ratios.)*

### L-3. Amount catalog (LN = lognormal(median, σ); Γ(shape, scale)+add)

| Channel | PL model | Class | Suggested source |
|---|---|---|---|
| Salary (monthly) | LN($4,500, .55) floor $50, ×12 annual | MEASUREMENT | BLS OEWS median wages |
| Rent | Γ(2, 700)+$100 (mean $1,500; household-econ-2026-07 — was Γ(2,400)+$50, mean $850) | MEASUREMENT | Census/HUD median gross rent |
| P2P | LN($55, .80) (household-econ-2026-07 — was LN($45,.80)) | MEASUREMENT | Fed Diary P2P |
| Bill | Γ(2, 55)+$15 (mean $125) | MEASUREMENT | Fed Diary bills |
| External unknown | LN($120, .95) floor $5 | CHOICE | — |
| ATM | LN($80, .30) floor $20 | MEASUREMENT | Fed Diary cash |
| Self transfer | LN($250, .80) floor $10 | CHOICE | — |
| Subscription (fallback) | LN($15, .40) floor $5 | MEASUREMENT | subscription surveys |
| Client ACH credit | LN($1,500, .75) floor $50 | MEASUREMENT | freelance invoice data |
| Card settlement | LN($650, .60) floor $20 | CHOICE | — |
| Platform payout | LN($400, .65) floor $10 | CHOICE | — |
| Owner draw | LN($2,500, .80) floor $100 | CHOICE | — |
| Investment inflow | LN($5,000, 1.0) floor $100 | CHOICE | — |
| Cash deposit (takings/tips) | per-persona split — see the L-10 cash block (LN, $10-rounded, floor $100) | MEASUREMENT-adjacent | FinCEN CTR volume; Fed Diary; Yale Budget Lab; IRS ATG |

Merchant tickets LN(median, σ): grocery 50/.55, fuel 32/.35
(household-econ-2026-07 — was 45/.35), restaurant 28/.60, pharmacy
25/.65, ecommerce 85/.70, retailOther 45/.75, utilities 120/.40,
telecom 75/.30, insurance 150/.35, education 200/.60; default 45/.70.
MEASUREMENT — Fed Diary / BLS CE per-category average tickets.

**Pass 1 results (L-3):**
* Salary (monthly) | Citation: BLS OEWS May 2024 (news release USDL
  25-0451, Apr 2 2025; bls.gov/oes). | Real-world value: median
  annual wage, all occupations, $49,500 in May 2024 = $4,125/mo,
  covering all wage/salary workers including part-time [Certain];
  full-time median usual weekly earnings ~ $1,192 in late 2024 =
  ~$5,165/mo [Likely, CPS, re-verify exact quarter]. | Status:
  CONFORMS conditional on documenting the comparator: PL's $4,500
  median sits between the all-worker and full-time medians, which is
  coherent for a salaried persona that excludes gig/student income
  (modeled elsewhere). Add one sentence to the row stating which
  median the persona targets. May 2025 OEWS is now published; refresh
  the number when transcribing.
* Rent | Citation: Census ACS 1-year 2024, table B25064 median gross
  rent (data.census.gov/table/ACSDT1Y2024.B25064); CBPP analysis of
  2024 ACS (Sep 2025). | Real-world value: median gross rent $1,406
  in 2023 [Likely, re-read exact 2023 cell]; 2024 median rose 5.8%
  nominal per CBPP [Certain] giving ~ $1,487 [Derived]. | Status:
  NONCONFORMING -> ADJUST, confirming the row's own suspicion:
  PL's Gamma(2,400)+$50 has mean $850, roughly 57% of the real
  median. File the ADJUST: retarget mean to ~ $1,450-1,550 with
  right skew (renter-quality mix), re-pin goldens per
  fraud-audit-2026-07 template. Note the knock-on: rent is a large
  monthly debit, so liquidity-throttle and paycheck-sensitivity
  calibrations shift with it.

**Pass 2 REVISION (L-3 rent), supersedes the Pass 1 status:**
* Rent | New evidence: 2024 S-DCPC Table 13 shows rent payments at
  0.5/consumer-month and $412/consumer-month, an average of $824 per
  observed rent TRANSACTION [Certain, Derived]. That sits on PL's
  Gamma mean of $850. The ACS household median gross rent (~$1,487
  in 2024) remains true on its own axis. | Status: CONTESTED, unit
  definition decides: per renter HOUSEHOLD the PL mean is ~ 43% low
  (NONCONFORMING); per observed per-CONSUMER bank debit it CONFORMS
  (roommate splits, partial payments, and diary underreporting all
  push the transaction average below household gross rent). Required
  action: write into the row which unit PL claims. If PL landlords
  expect one debit per tenancy per month (household-like), ADJUST
  toward ~$1,450-1,550 as Pass 1 proposed; if multiple payers per
  tenancy exist in the generator, the current calibration stands
  with the DCPC citation. This is a code-reading decision, not a
  literature decision.

**C0 code read (L-3 rent, 2026-07-18) — CONTESTED resolved:
NONCONFORMING.**
* Unit: each renter is a SOLE TENANT of their own dwelling — one
  lease per payer, the FULL rent every month, and NO roommate/
  household split mechanism exists (`activity/recurring/lease.hpp`).
  The household axis therefore governs: the ACS median gross rent
  (~$1,487 in 2024) is the comparator, and the DCPC per-consumer
  transaction average does not apply. **ADJUST queued in C2:
  retarget the rent mean to ~$1,450–1,550 with right skew**, exactly
  as Pass 1 proposed, with the liquidity-throttle knock-on noted
  there (rent shifts the throttle calibration and the
  posted-structuring mix).
* Rent-LEVEL mechanics (correctly modeled — NOT part of the ADJUST):
  the monthly debit is CONSTANT within each lease year and steps up
  once per lease anniversary by 1 + 2.5% inflation + real raise
  N(2.0%, 1.5%) floor −1% (one content-keyed draw per lease-year;
  `growth::compoundGrowth` counts full 12-month anniversaries, zero
  anniversaries ⇒ factor exactly 1.0) — an annual renewal escalation
  of ~4.5%/yr nominal, not month-over-month compounding. Moving
  re-draws the base rent fresh; leases are BACKDATED at world
  creation so some tenants start mid-tenancy with past steps
  applied. The ~4.5%/yr escalation rate itself is the L-5 growth
  row (UNCITED; comparator: CPI rent of primary residence).
* Residual read at C2 time: the renter-selection share (who rents at
  all).

**C2 household-econ-2026-07 (L-3 rent) — SHIPPED, plus the renter
code-read result:**
* Rent AMOUNT | `math::amounts::kRent` Γ(2,400)+$50 → **Γ(2,700) +
  $100** — mean $1,500 (inside the queued $1,450–1,550 band), right
  skew preserved (shape 2), CV ≈ .66 as before. All five Rent::*
  channel variants share the model.
* RENTER-SELECTION SHARE (the queued C0 residual — read, and it came
  back NONCONFORMING) | The rent selector fits a scale so the MEAN
  selection probability across candidates equals
  `rent::Rules::paidFraction` = **.80**: four in five people paid a
  FULL household rent every month. No homeowner exclusion is wired
  in production (`RentRoll.isHomeowner` defaults to `noHomeowners`;
  `passes.cpp buildRentRoll` never sets it), so mortgage payers
  could simultaneously rent. Comparator on the household axis (each
  PL renter = sole tenant paying one full household rent): ACS
  renter share of households ~.34–.36 [Likely] — PL ran ~2.3× that.
  **ADJUST shipped: paidFraction .80 → .35.** At the .35 target the
  fitted scale ≈ .66 with NO clamping; effective persona renter
  shares: student ≈ .33, retiree ≈ .12, freelancer ≈ .38,
  smallBusiness ≈ .23, HNW ≈ .07, salaried ≈ .41. Aggregate
  reconciliation [Derived]: PL per-capita rent outflow ≈ .35 ×
  $1,500 = $525/person-month vs real ≈ .35 × $1,487 ≈ $520 — the
  amount and share ADJUSTs reconcile the aggregate that either alone
  would have broken (the old pair ran $680; amount-only would have
  run $1,200).
* KNOWN SIMPLIFICATION (logged): homeowner/renter overlap — the
  `isHomeowner` hook exists but is unwired, so a mortgage payer can
  also be selected as a renter (expected overlap ≈ .35 × mortgage
  adoption ≈ 16% of people). Owner call whether to thread account
  ownership into `RentRoll` in a later round.

### L-4. Income & employment

| Parameter | PL value | Class | Suggested source |
|---|---|---|---|
| Employment probability | EFFECTIVE per-persona table (household-econ-2026-07): salaried .98, student .40, retiree .02, freelancer .08, smallBiz .04, HNW .12; fit target .65 = the table's weighted mean, so the fitted scale ≈ 1.0 and the table IS the effective rate (see the C2 block — the old .95 target clamped every persona except retirees to ~100%) | MEASUREMENT | BLS employment-population ratios |
| Pay cadences | weekly .20 / biweekly .55 / semimonthly .15 / monthly .10 | DEVIATES-BY-CHOICE (re-classed from MEASUREMENT per Pass 1: BLS publishes establishment shares, PL needs worker-weighted shares) | BLS length-of-pay-period data |
| Payday mechanics | Friday default (25% Thu↔Fri); semimonthly {15,31} (35% {1,15}); monthly ∈ {28,30,31}; roll to previous business day; posting lag 0–1 d; salary posts 06:00–12:00 same day | MEASUREMENT | payroll-industry conventions |
| Job tenure | 1.5–4.0 y/job | MEASUREMENT | BLS median tenure |
| Wage growth | 2.5% inflation + real raise N(1.5%, 2.0%) floor −2%; switch bump N(+8%, 6%) floor −5% | MEASUREMENT | Atlanta Fed Wage Growth Tracker |
| Floors/jitter | ≥$50/paycheck; initial salary jitter LN σ.03 | CHOICE | — |

**Pass 1 results (L-4):**
* Pay cadences | Citation: BLS Current Employment Statistics,
  length-of-pay-period estimates, Feb 2023 reference (widely
  republished; bls.gov CES); BLS TED Jun 5 2014 for the size
  gradient. | Real-world value: share of PRIVATE ESTABLISHMENTS,
  Feb 2023: biweekly 43.0%, weekly 27.0%, semimonthly 19.8%, monthly
  ~10% [Certain]; 72.9% of establishments with 1,000+ employees pay
  biweekly (2013 vintage) [Certain]. | Status: DEVIATES-BY-CHOICE
  (re-class from MEASUREMENT): BLS publishes establishment shares,
  PL needs WORKER-weighted shares, and workers concentrate in large
  biweekly employers. PL's .55 biweekly / .20 weekly is directionally
  consistent with worker-weighting but cannot claim conformance to a
  published number. Document exactly that; alternatively locate a
  worker-weighted source next pass and re-class back to MEASUREMENT.
  *(C1 doc ride: row class cell re-classed exactly so.)*
* Employment probability, Job tenure, Wage growth | Status: UNCITED,
  queued. AT RISK flag on student employment .12: BLS reports roughly
  40% of full-time college students employed [Likely]; either the
  persona definition excludes employed students explicitly or this

**Pass 2 results (L-4):**
* Student employment .12 | Citation: NCES Condition of Education,
  College Student Employment (40% of full-time undergraduates
  employed, 2020); BLS USDL-25-0563 (Apr 2025): labor force
  participation 44.6% for full-time college students, 37.6% for
  recent-graduate full-time enrollees, Oct 2024. | Status:
  NONCONFORMING -> ADJUST (raise student employment probability
  toward ~ .35-.45) OR redefine the persona in the row as
  "non-working student funded by family transfers" and re-class
  CHOICE. As written, .12 is a third of the measured rate.
  (KNOCK-ON: the L-10 student cash-tips share is derived from this
  value — recompute it in the same C2 batch.)
  *(RESOLVED by household-econ-2026-07 — but see the C2 block: the
  base .12 was ALREADY clamped to ~1.0 at runtime, so the honest
  ADJUST was the fit-semantics fix plus base .40, which LOWERED
  effective student employment from ~100% to the cited ~40%.)*
* Job tenure 1.5-4.0y | Real-world value: BLS median employee tenure
  3.9 years (2024 release) [Likely, verify exact figure at ADJUST
  time]. PL's per-job range tops out at the national median, so PL
  workers churn faster than measured. | Status: DEVIATES-BY-CHOICE
  if documented (short sim windows need job-change events for
  signal); otherwise ADJUST the range upward (~ 2-7y).
* Wage growth | Real-world value: Atlanta Fed Wage Growth Tracker
  has run ~ 4-4.5% nominal median growth recently, with a modest
  job-switcher premium [Likely]. PL's 2.5% + N(1.5%, 2.0%) = 4.0%
  nominal median CONFORMS as a band; the +8% switch bump exceeds the
  tracker's switcher-stayer gap but is in line with payroll-data
  switcher raises [Guessing]. Status: CONFORMS (base), switch bump
  UNCITED with note.
  row goes NONCONFORMING.

**C2 household-econ-2026-07 (L-4) — the EFFECTIVE-EMPLOYMENT finding
and the shipped ADJUST:**
* FINDING (code read, `activity/income/salary.hpp` +
  `income/selection.hpp`) | The salary selector does not apply the
  persona table directly: `fitScale` solves for a scale s such that
  the MEAN of clamp(base × s, 0, 1) across all candidates equals
  `paidFraction`. At the old target .95 the solution was s ≈ 25,
  which CLAMPED every persona except retirees to probability 1.0 —
  effective employment was ~100% for students, freelancers,
  small-business owners, HNW AND salaried, and ~50% for retirees.
  The table this document printed (salaried .98, student .12, …) was
  the base-weight vector, not behavior: freelancers all drew
  paychecks on top of client revenue; half of retirees drew
  paychecks on top of Social Security. Every prior verdict on this
  row inherited that misread (the Pass 2 NONCONFORMING said student
  employment was a third of the measured rate; it was actually 2.5×
  the measured rate).
* ADJUST (shipped) | `paidFraction` .95 → **.65** — the persona
  table's weighted mean under the L-1 shares (Σ share × p = .6508),
  so the fitted scale ≈ 1.0, nothing clamps, and the table becomes
  the EFFECTIVE per-persona employment probability. Base table
  change: student .12 → **.40** (NCES 40% of full-time
  undergraduates employed; BLS student LFP 44.6% — the Pass 2
  citations). Retiree stays .02: the persona is FULLY retired and
  its income is the SSA benefits stream (L-4b), so the BLS 65+
  employment-population ratio (~19%) lives implicitly in the
  salaried persona, not here — definition written into the row.
  Freelancer .08 / smallBusiness .04 / HNW .12 unchanged as bases
  but now REAL: their income arrives through the L-10 revenue
  profiles, and the accidental blanket paychecks are gone.
* KNOCK-ONS (measure at re-pin #12) | Salary credits ≈ −30%;
  student/freelancer/smallBusiness liquidity now rests on family
  transfers and revenue profiles (expect more bounced debits — the
  emergent F-7 pattern); retiree income now = SSA (L-4b) + L-10
  draw/investment only. RESIDUAL: employed students draw the same
  LN($4,500,.55) salary model as everyone else — a part-time wage
  tier is a candidate future ADJUST, logged in REMAINING OPEN ITEMS.

### L-4b. Government benefits (ADDED by household-econ-2026-07 — a
ledger COVERAGE GAP: these streams existed in code but had no rows
here. Values mirror code, UNCITED; recalled anchors noted for the
owner's pass.)

| Parameter | PL value | Class | Suggested source |
|---|---|---|---|
| SSA retirement | retirees only; eligibleP .87; LN($2,071, .30) floor $900/mo; paid on the 3 SSA Wednesday cohorts by synthetic birth-day (`transfers/channels/government/retirement.hpp`) | MEASUREMENT | SSA Monthly Statistical Snapshot |
| SSDI disability | non-retiree/non-student personas; eligibleP .04; LN($1,630, .25) floor $500/mo (`government/disability.hpp`) | MEASUREMENT | SSA SSDI beneficiary statistics |

Recalled anchors [Likely, owner verifies]: ~90% of 65+ receive Social
Security (PL .87 of retirees CONFORMS-adjacent); average retired-
worker benefit ≈ $1,907/mo (Dec 2024), ≈ $1,976 after the Jan 2025
COLA — PL's LN median $2,071 implies mean ≈ $2,166, a few percent
high; average disabled-worker benefit ≈ $1,540/mo — PL median $1,630
implies mean ≈ $1,682, ~9% high; SSDI beneficiaries ≈ 7.2–7.4M ≈ 3–4%
of the working-age population (PL .04 CONFORMS-adjacent).

### L-5. Housing

Lease tenure 3–8 y (household-econ-2026-07 — was 1–3 y); on move: new
landlord, fresh base rent (jitter LN σ.05); growth 2.5% inflation +
real N(2.0%, 1.5%) floor −1%/y (applied once per lease anniversary ≈
4.5%/yr nominal — mechanics in the L-3 C0 block). MEASUREMENT — CPS
renter turnover ~15–22%/y (the overall CPS mover rate was 11.0% in
2017 and has run single-digit %/y in the 2020s; the stale "~13%/y"
parenthetical is removed per Pass 2), CPI rent index.

**Pass 2 results (L-5):**
* Lease tenure 1-3y | Citation: Census CPS ASEC mobility (renter
  mover rate 21.7% in 2017, a then-historic low); BLS working paper
  on continuing-tenant rents (new-tenant share declined to ~ 15%
  recently). | Real-world value: renters turn over at ~ 15-22% per
  year, implying mean renter stays of ~ 4.5-7 years [Certain on the
  rates, Derived on the implication]. PL's 1-3y tenure implies ~ 50%
  annual turnover, 2.5-3x reality. | Status: NONCONFORMING -> ADJUST
  (tenure ~ 3-8y, mean ~ 5-6y) or re-class CHOICE with the
  sim-window justification. Also: the row's own "(~13%/y)" overall
  mover-rate parenthetical is stale; the overall CPS mover rate was
  11.0% in 2017 and has run in the single digits in the 2020s
  [Likely]. Fix the parenthetical regardless of the tenure decision.
  *(C1 doc ride: parenthetical fixed in the header above.)*
  *(RESOLVED by household-econ-2026-07: tenure 1–3y → 3–8y, mean
  5.5y ⇒ ~18%/yr turnover, inside the cited band. Backdated initial
  leases keep mid-tenancy starts working unchanged.)*

### L-6. Recurring debits

| Routine | PL value | Class | Suggested source |
|---|---|---|---|
| Subscriptions | 4–8 candidates/person, 55% become debits; 18-point pool $6.99–$99.99 (6.99, 7.99, 9.99, 10.99, 11.99, 12.99, 14.99, 15.49, 15.99, 17.99, 22.99, 24.99, 29.99, 34.99, 39.99, 49.99, 59.99, 99.99); day U[1,28] | MEASUREMENT | NAMED COMPARATOR (C4): Bango 2025 (5.2 active, $69/mo) — PL's 2.2–4.4 active at ~$27 pool mean ⇒ $60–119/person-month brackets it; CONFORMS per the Pass 2 conditional |
| ATM | 88% users; 1–6/mo; LN($80,.30) floor $20 | MEASUREMENT | Fed Diary |
| Internal transfers | 55% active; 1–3/mo; LN($120,.75) floor $10; 25% round from pool {25, 50×2, 100×3, 150, 200×2, 250, 300, 500×2, 750, 1000×2, 1500, 2000} | CHOICE | — |

**Pass 2 results (L-6):**
* Subscriptions | Citation: pick ONE and name it in the row; the
  survey band is wide: Self Financial 2026 (3.4 active, $35/mo),
  Bango 2025 (5.2 active, $69/mo), Whop/Chargebee-style trackers
  (8.2 active, $219/mo) [Certain that each source says what it
  says]. | Real-world value: PL's 2.2-4.4 active debits at a pool
  mean of ~ $27 imply ~ $60-119/person-month [Derived], inside the
  band and closest to Bango. | Status: CONFORMS conditional on
  naming the comparator source in the row; without a named source
  the row is unfalsifiable.
* Subscriptions COMPARATOR NAMED (C4, 2026-07-18) | Bango 2025 (5.2
  active, $69/mo) is the row's named comparator — of the three
  surveyed bands it is the one PL's derived $60–119/person-month
  brackets. Status: CONFORMS (the Pass 2 conditional is satisfied).
* ATM | Citation: S-DCPC Table 5 (82.6% of consumers used cash in
  the last 30 days, 2024). | Status: PL's "88% ATM users" runs a few
  points above the cash-user share, borderline; the withdrawal
  amount LN($80,.30) remains UNCITED because the Diary tables count
  cash PAYMENTS, not withdrawals. Locate the Diary's cash-withdrawal
  supplement (or Fed cash office data) before setting a status;
  on-person holdings (avg $66.7, conditional median $46, Table 14)
  are consistent with sub-$100 typical withdrawals but do not prove
  the median [Likely].

### L-7. Credit cards

| Parameter | PL value | Class | Suggested source |
|---|---|---|---|
| Ownership / share / limits | per persona (L-1); cardP = strictly CREDIT-card issuance, weighted ≈ .826 (card-behavior-2026-07) | MEASUREMENT | Fed SCF; CFPB CCM |
| Grace period | 25 days | MEASUREMENT | CARD Act ≥21 d; issuer norms |
| Minimum payment | max(2%, $25) | MEASUREMENT | issuer norms |
| Late fee | $32 | MEASUREMENT | CFPB late-fee data (verify current rule status) |
| Autopay | full .40 / minimum .10 / manual .50 per card (`synth/cards/issue.hpp`) — autopay cards BYPASS the manual mixture below. COMPARATOR (C4): share of card ACCOUNTS enrolled in any autopay — issuer/industry surveys run ~.40–.50 [Guessing — owner verifies]; PL's total .50 sits at the band top; the .40/.10 full-vs-minimum composition WITHIN autopay is CHOICE (no published split) | MEASUREMENT-adjacent | issuer autopay adoption surveys |
| Payment mixture (MANUAL payers only) | full .35 / partial .30 / minimum .25 / miss .10; partial fraction Beta(2,5) of statement. EFFECTIVE population shares (autopay + manual, card-behavior-2026-07 code read): **full .575 / minimum .225 / partial .15 / miss .05** — the .575 sits on the S-DCPC ~58% full-payment share: CONFORMS, no code change | MEASUREMENT | CFPB CCM payment-behavior distributions (transactor/revolver) |
| Payment timing | late p .08, 1–20 d late | MEASUREMENT | issuer delinquency curves |
| Disputes | refund p .006/purchase (1–14 d); chargeback p .001 (7–45 d) | CHOICE (re-classed per Pass 2) — e-comm dispute benchmark ~0.6% of txns; PL runs 0.1% blended all-channel by choice (the old "(~0.05–0.1%)" parenthetical misstated published benchmarks). Fraud-driven chargebacks are SEPARATE: reported card compromises reimburse via cc_chargeback rows (F-4 scam-fraud block) | Visa/Mastercard monitoring thresholds (VAMP 1.5% eff. Apr 2026; legacy 0.9%/0.65%) |
| Cycle finalization | 32-day session lag (architecture) | CHOICE | — |

**Pass 1 results (L-7):**
* Grace period | Citation: CARD Act, 15 U.S.C. 1666b; Reg Z, 12 CFR
  1026.5(b)(2)(ii). | Real-world value: statement must be delivered
  at least 21 days before the due date; common issuer grace periods
  21-25 days [Certain on the 21-day floor]. | Status: CONFORMS (25 >=
  21, within issuer norms).
* Late fee | Citation: Reg Z safe harbors, 12 CFR 1026.52(b), 2024
  inflation-adjusted amounts; Chamber of Commerce v. CFPB, N.D. Tex.,
  final judgment Apr 15 2025 vacating the $8 rule (Holland & Knight /
  Goodwin client alerts; CFPB Mar 2024 rule release for the $32
  average). | Real-world value: the $8 cap NEVER took effect and was
  vacated Apr 15 2025 [Certain]; operative safe harbors ~ $32 first
  violation / $43 subsequent [Certain]; CFPB-cited average fee $32
  (2022 data) [Certain]. | Status: CONFORMS as a flat average; note
  the unmodeled $43 repeat-violation tier as a known simplification,
  and keep the row's "verify current rule status" note satisfied as
  of 2026-07-18.
* Payment mixture | Citation: Atlanta Fed 2024 S-DCPC Tables, Table 4
  (Use of Credit Card Debt). | Real-world value: 42.1% of credit card
  adopters carried an unpaid balance last month (Oct 2024) so ~ 58%
  paid in full; 45.4% carried a balance at some point in 12 months;
  mean unpaid balance $3,078 across all adopters, $6,794 per revolver,
  median $2,600 per revolver [Certain, read from Table 4]. | Status:
  NONCONFORMING -> ADJUST or owner re-class to CHOICE: PL's full .35
  is ~ 23 points below the ~ .58 full-payment share. If the intent is
  a delinquency-rich corpus for label density, say so and re-class;
  otherwise ADJUST the mixture toward full ~ .55 / partial ~ .20 /
  minimum ~ .17 / miss ~ .08 and recalibrate against CFPB CCM
  account-level revolver shares next pass (units differ: consumers
  vs. accounts).
  *(REVERSED by the card-behavior-2026-07 code read below: the .35
  is the MANUAL-payer mixture; the population share was already
  ~.575. This verdict compared a conditional to a marginal.)*
* Ownership / share / limits | Citation: same tables, Table 3. |
  Real-world value: credit card adoption 82.3% of consumers, debit
  90.3% (2024) [Certain]. | Status: partial CONFORMS: PL's weighted
  cardP ~ .85 sits between debit and credit adoption; acceptable if
  cardP means "has any payment card", slightly high if strictly
  credit. Limits and balances remain UNCITED (SCF 2022, queued).
  *(C0 code read settled the definition: STRICTLY credit — see the
  L-1 block. The "slightly high" branch applies. RESOLVED by
  card-behavior-2026-07: trimmed to weighted ≈ .826.)*
* Disputes | Status: UNCITED, AT RISK both directions: the suggested
  source text "network chargeback rates (~0.05-0.1%)" needs a real
  citation; public benchmarks cluster nearer 0.1-0.2% of transactions
  with network excessive-chargeback thresholds at 0.65-0.9%
  [Likely]. Verify against Visa/Mastercard program documentation

**Pass 2 results (L-7):**
* Disputes / chargebacks | Citation: network monitoring thresholds
  and industry benchmarks (Visa legacy threshold 0.9% with 0.65%
  early warning; VAMP combined-dispute threshold 1.5% effective Apr
  2026; Mastercard ECM 1.5%/100 disputes; all-industry e-commerce
  average ~ 0.6-0.65% of transactions) [Certain]. | Status: the
  row's parenthetical "(~0.05-0.1%)" misstates published benchmarks
  and must be rewritten. PL's chargeback p .001 (0.1%) blended
  across ALL channels including in-person is BELOW the e-commerce
  average, which is directionally right (card-present disputes are
  rare) but has no direct published comparator. Set
  DEVIATES-BY-CHOICE with the corrected note: "e-comm benchmark
  ~0.6%; PL runs 0.1% blended all-channel by choice." Refund p .006
  stays UNCITED (merchandise return rates ~ 15%+ of e-comm orders
  are a different concept; define what a refund row represents
  before citing anything).
  *(C1 doc ride: row rewritten exactly so; refund defined below.)*
  next pass.

**C0 code read (L-7 refund, 2026-07-18):**
* A "refund" row is a post-purchase MERCHANT CREDIT back to the
  cardholder (`channels/credit_cards/dispute/sampler.cpp`): per
  purchase, p .006 → channel `Credit::refund` with a 1–14 day lag;
  independently p .001 → `Credit::chargeback` with a 7–45 day lag.
  The right comparator is merchandise-RETURN incidence per purchase
  across ALL channels (not e-commerce return rates, which run ~15%+
  and measure a different, order-level concept). Row stays UNCITED
  with this definition in place.

**C3 card-behavior-2026-07 (L-7) — the PAYMENT-MIXTURE AXIS FINDING
(verdict reversed, no code change):**
* Code read (`session.cpp Session::draftPayment` +
  `synth/cards/issue.hpp sampleAutopay`) | The mixture is drawn ONLY
  for `Autopay::manual` cards. At issuance each card samples autopay:
  full .40 / minimum .10 / manual .50. Autopay-full cards ALWAYS pay
  the statement in full (on time, 12 h lag); autopay-minimum cards
  always pay the minimum. The EFFECTIVE population composition is
  therefore: full = .40 + .50×.35 = **.575**; minimum = .10 + .50×.25
  = **.225**; partial = .50×.30 = **.15**; miss = .50×.10 = **.05**.
* Verdict | The Pass 1 NONCONFORMING compared the MANUAL-payer
  mixture (.35 full) to the POPULATION full-payment share (~.58,
  S-DCPC Table 4) — a conditional-vs-marginal axis misread, the same
  class as the C2 employment finding. On the correct axis PL's .575
  sits on the measured ~.58: **CONFORMS, no ADJUST**. The row above
  is rewritten with the decomposition; `payment.hpp` now carries the
  same warning at the constants. Residuals: the autopay split
  {.40/.10/.50} itself is UNCITED (issuer autopay-enrollment surveys
  run ~40–50% of accounts on autopay [Guessing — verify]); the
  effective miss share .05 vs card delinquency benchmarks (~3–4% of
  accounts 30+ dpd) is borderline-high but defensible as
  per-statement rather than per-account — both logged in REMAINING
  OPEN ITEMS.

**C4 definitions (L-7, 2026-07-18):**
* Autopay split | The citable comparator is the share of card
  accounts enrolled in ANY autopay (~.40–.50 in issuer/industry
  surveys [Guessing — owner verifies]); PL's .50 total sits at the
  top of that band, and the 80/20 full-vs-minimum composition within
  autopay is CHOICE. The row stays UNCITED but is now falsifiable.
* Miss-share AXIS MAPPING | PL's effective .05 is a PER-STATEMENT
  miss FLOW — P(no payment this cycle), all cards; the delinquency
  benchmark ~3–4% is a POINT-IN-TIME STOCK (share of accounts 30+
  dpd). With approximately one-cycle cure the flow and the stock
  coincide numerically, so PL ≈ 5% vs ~3–4%: borderline-high, same
  axis and order — CONFORMS-adjacent as a band. Any future verdict
  must compare stock to stock (map PL's per-statement misses through
  cure duration first); never compare the manual-only .10 to either.

### L-8. Credit & obligation products (adoption by persona: student/retiree/freelancer/smallBiz/HNW/salaried)

**Mortgage** — adoption .02/.55/.30/.55/.65/.55; payment LN($1,750,
.55); delinquency: late 4% (1–7 d), miss .5%, partial 1% (30–80%),
cure 30%, cluster ×1.6, ≤6 cure cycles.
**Auto loan** — adoption .10/.20/.40/.45/.45/.45; 35% new; payment new
LN($715,.30) / used LN($525,.35) floor $100; term new 68±6 / used
67±8 mo, clamp 24–84; delinquency: late 5% (1–10 d), miss 1%, partial
1.5%, cure 30%, cluster ×1.7, ≤4 cycles.
**Student loan** — adoption .85/.05/.20/.20/.10/.30; plans standard .65
(120 mo) / extended .20 (240 mo) / IDR .15 (55% 240 else 300 mo);
grace 6 mo (65% of students deferred); payment LN($295,.55) floor $50;
delinquency: late 6% (1–14 d), miss 1.5%, partial 2%, cure 25%,
cluster ×1.8, ≤4 cycles.
**Insurance** — adoption auto .30/.85/.85/.90/.95/.92, home
.05/.55/.30/.55/.70/.55, life .10/.55/.30/.45/.55/.55; mortgage⇒home
.99, auto-loan⇒auto .997; monthly premiums LN: auto $225/.30 (floor
25), home $200/.30 (25) (household-econ-2026-07 — was $163), life
$28/.40 (5); claims: auto 4.2%/y → payout LN($4,700,.80) floor $500;
home 5.5%/y → LN($12,500,.80) floor $1,000 (household-econ-2026-07 —
was LN($15,750,.90)).
**Tax** — adoption .05/.20/.65/.85/.50/.10; quarterly LN($1,250,.65)
floor $100; filing: refund 65% LN($2,500,.55) (household-econ-2026-07
— was LN($1,850,.55)) / balance due 20% LN($1,100,.65).
Class MEASUREMENT. Sources: Freddie Mac PMMS/median P&I; Experian State
of the Automotive Finance Market; College Board / FSA; MBA delinquency
survey; NY Fed CCP; III/NAIC (premiums, claim frequency & severity);
IRS SOI (refund shares & averages).

**Pass 1 results (L-8):**
* Auto loan | Citation: Experian State of the Automotive Finance
  Market, Q4 2025 and Q1 2026 (experian.com/blogs; LendingTree
  compilation of the same). | Real-world value: average monthly
  payment new $767 (Q4 2025) / $770 (Q1 2026), used $537 / $531;
  average terms new 69.5 mo, used 67.7 mo; average amounts financed
  new $43,925, used $27,070 (Q1 2026) [Certain]. | Status: CONFORMS:
  PL's lognormal implied means are ~ $748 new (715 x exp(.30^2/2))
  and ~ $558 used, each within ~ 4% of Experian; terms 68 / 67 match.
  Two notes: real new-loan terms drifted to 69.5 and 73-84 mo loans
  are ~ 30% of new originations with 85+ mo at ~ 2%, so the 84-mo
  clamp truncates a real tail (acceptable, document it).
* Mortgage, Student loan, Insurance, Tax | Status: UNCITED, queued
  (Freddie Mac PMMS / MBA or ICE median P&I; FSA or College Board;
  III/NAIC premium and claim-frequency tables; IRS filing-season
  statistics for refund share and average).

**Pass 2 results (L-8):**
* Mortgage payment | Citation: Census ACS 2024 1-year (median
  monthly mortgage payment $1,521 all mortgaged owners; $2,225 for
  2024 movers); MBA applications median ~ $2,067-2,127 (2025)
  [Certain]. | Status: CONFORMS as a band: PL median $1,750 / implied
  mean ~ $2,036 sits between the all-stock and new-origination
  medians. Write the comparator (mixed stock) into the row.
* Student loan payment | Citation: Fed SHED (typical payment
  $200-299; 60% of payers at or under $299; 45% of borrowers owed no
  payment in the survey month, 2025); secondary averages $336-503
  [Certain]. | Status: CONFORMS: PL median $295 / mean ~ $343.
  Deferral: PL's 65%-of-students deferred is directionally
  consistent with the 45%-of-all-borrowers figure given students
  skew deferred. One flag: PL IDR share .15 vs FSA portfolio IDR
  enrollment around a third of borrowers in repayment [Likely];
  verify against the FSA portfolio summary and expect an ADJUST or a
  CHOICE note.
* Insurance premiums | Citation: 2026 market averages: auto full
  coverage $190-244/mo across Experian, ValuePenguin, Insurance.com;
  home $1,824-2,543/yr sample averages with Philadelphia Fed at
  $2,530 for 2023 trending ~ $3,000 by 2026; term life ~ $26-30/mo
  [Certain for auto/home ranges, Likely for life]. | Status: auto
  CONFORMS (PL implied mean ~ $235/mo); life CONFORMS; home
  DEVIATES-LOW: PL implied ~ $2,047/yr sits at the very bottom of
  the band and ~ 20% under the 2023 national average, ADJUST median
  toward $190-210/mo or cite the low-end sample explicitly.
  *(RESOLVED by household-econ-2026-07: home premium median $163 →
  $200, σ .30 — implied mean ≈ $209/mo ≈ $2,510/yr on the ~$2,530
  anchor.)*
* Insurance claims | Citation: III/ISO: 5.3% of insured homes filed
  a claim (2023); average home claim severity $18,311 (2022), >$17k
  recent five-year [Certain]. | Status: home frequency CONFORMS (PL
  5.5%); home severity DEVIATES-HIGH: PL lognormal implies mean ~
  $23.6k vs measured ~ $17-18k averages, trim sigma or median. Auto
  frequency 4.2%/yr and payout mean ~ $6.5k are consistent with
  collision claim frequency ~ 5-6 per 100 car-years and severity ~
  $5.7-6.6k [Likely]; verify against ISS/III auto tables before
  flipping to CONFORMS.
  *(RESOLVED by household-econ-2026-07: home severity LN($15,750,
  .90) → LN($12,500, .80) — implied mean ≈ $17.2k inside the cited
  $17-18k band.)*
* Tax | Citation: IRS filing season statistics; Tax Foundation
  tracker. | Real-world value: 64.1% of 2024 returns and ~ 63% of
  2025 returns received refunds; average refund $3,167-3,170 (2025
  season) and $3,462-3,521 (2026 season, inflated one year by OBBB
  withholding lag) [Certain]. | Status: refund share CONFORMS (PL
  .65). Refund amount DEVIATES-LOW: PL implied mean ~ $2,152 is ~
  32% under the 2025 average; no official median exists, so either
  ADJUST the median toward ~ $2,300-2,600 or document that PL
  targets a median below the published mean by construction.
  *(RESOLVED by household-econ-2026-07: refund median $1,850 →
  $2,500, σ .55 — implied mean ≈ $2,908, ~8% under the 2025-season
  average with the median-below-mean construction documented in the
  code.)*
* Mortgage/auto/student delinquency ladders | UNCITED still; the
  right comparators are MBA NDS (mortgage 30+ ~ 4% band), NY Fed CCP
  transition rates, and FSA delinquency stats; queued as a
  unit-mapping exercise (PL's per-payment lateness vs 30/60/90-day
  buckets do not map one-to-one).

### L-9. Family transfers

| Flow | PL value | Suggested source |
|---|---|---|
| Spousal | 60% separate accounts — DEFINITION (C4, code: `family/spouse.cpp separateAccountsP`): the share of couples that ACTIVELY ROUTE inter-spouse transfers between individually-owned accounts (PL models no joint accounts; every account has one owner), i.e. "at least some money kept separate" — NOT fully-separate finances; 2–6 txns/mo; breadwinner-directional 65%; LN($85, .90) | Fed SHED — Bankrate 2026: 62% keep at least some money separate ⇒ CONFORMS under the written definition |
| Parental support | 35% of eligible; Pareto(xm=$25, α=2.4)/txn | Fed SHED; AARP |
| Allowances | weekly 70% (else monthly); Pareto($8, 1.8) (household-econ-2026-07 — was Pareto($35, 2.2)) | T. Rowe Price kids-and-money surveys |
| Sibling transfers | 15% pairs active; 18%/mo; LN($120, .90) | — |
| Grandparent gifts | 8%; LN($150, .70) | — |
| Parent gifts | 12%; Pareto($75, 1.6) | — |
| Tuition | 65% of students; 4–5 installments; LN(e^8.95 ≈ $7,712, .35) each — DEFINITION (C4): the stream funds the student's FULL annual cost of attendance at PUBLISHED prices (COA: tuition + fees + living), paid parent→STUDENT account (`family/tuition.cpp` pays the student's own account, not a university merchant); ≈$31–39k/yr vs public in-state COA $30,990 ⇒ CONFORMS as a family-funded COA stream | College Board Trends in Pricing |
| Inheritance | event p .0015; LN($25,000, 1.0) | estate-transfer literature; Fed SCF |
| External recipients | 18% of family transfers leave the bank | CHOICE |

**Pass 2 results (L-9):**
* Spousal 60% separate accounts | Citation: Bankrate couples survey
  (press release Feb 2026): 62% of coupled adults keep at least some
  money separate (36% hybrid + 26% fully separate); Census SIPP
  2023: 23% of married couples hold NO joint account, 77% hold at
  least one jointly, all-joint share down to 40% [Certain]. |
  Status: CONFORMS if and only if the row means "at least one
  separate account exists to route inter-spouse transfers" (62% real
  vs 60% PL). If the code means fully separate finances, real is
  23-26% and the row is NONCONFORMING. Write the definition into the
  row; the number is fine under the first reading.
* Spousal DEFINITION WRITTEN (C4, 2026-07-18) | Code read
  (`family/spouse.cpp`): `separateAccountsP` .60 is a per-couple
  gate on whether inter-spouse transfers flow at all, between the
  spouses' individually-owned accounts — PL has no joint-account
  concept, so the row means "at least one separate account exists
  to route transfers": the FIRST reading above. Comparator:
  Bankrate 62% (at least some money separate). Status: CONFORMS.
* Allowances | Citation: Greenlight platform data (avg weekly
  allowance $14.72 in 2023, $13.15 in 2025, ages 5-19); Till
  Financial 2025-26 (avg $17/wk, median $10/wk); AICPA 2019 survey
  (~$30/wk self-reported, teen-heavy) [Certain]. | Real-world value:
  transaction-data averages $13-17/wk, median ~ $10/wk. PL
  Pareto($35, 2.2) has minimum $35/wk and mean ~ $64/wk [Derived]. |
  Status: NONCONFORMING -> ADJUST: PL's floor exceeds every measured
  average including the generous self-report. Proposal: Pareto(xm
  $8, alpha 1.8) (mean ~ $18) or LN(median $12, sigma .7); re-pin
  goldens per template.
  *(RESOLVED by household-econ-2026-07: Pareto($35, 2.2) →
  Pareto($8, 1.8), mean ≈ $18/wk — the first proposed form, keeping
  the distribution family and draw pattern.)*
* Tuition | Citation: College Board, Trends in College Pricing 2025.
  | Real-world value (2025-26): published tuition+fees public 4yr
  in-state $11,950 / out-of-state $31,880 / private nonprofit
  $45,000; NET tuition after aid public $2,300 / private $16,910;
  total cost of attendance public in-state $30,990 / private $65,470
  [Certain]. | Status: DEVIATES-BY-CHOICE pending definition: PL's
  4-5 x $7,712 = $31-39k/yr matches public COA or a public/private
  published-price mix, but is far above NET tuition actually paid.
  Write into the row what the transfer funds (COA vs tuition, sticker
  vs net); as a family-funded COA stream it CONFORMS to the public
  COA anchor.
* Tuition DEFINITION WRITTEN (C4, 2026-07-18) | The transfer funds
  the student's full annual COST OF ATTENDANCE at published
  (sticker) prices, not net-after-aid tuition; it lands in the
  STUDENT'S account (`tuition.cpp pickPayer` draws a parent, the
  payee is the student), so it is family COA funding, not a
  university payment. 4–5 × $7,712 ≈ $31–39k/yr vs public in-state
  COA $30,990: CONFORMS under the written definition (the
  private-COA tail lives inside the lognormal spread).
* Parental support, sibling, grandparent, parent gifts, inheritance,
  external recipients | UNCITED; see REMAINING OPEN ITEMS. Fed SHED
  has qualitative family-support incidence; no clean per-transfer
  distributions exist, so expect these to end as CHOICE rows with
  incidence-only citations.

### L-10. Business / freelancer revenue (per-persona monthly profiles)

**Freelancer** — clients: active .88, 2–5 counterparties, 1–4
payments/mo, LN($1,400,.70); platforms: .42, 1–2, 1–4, LN($425,.60);
owner draw: .70, 1–2, LN($1,800,.75); cash takings: .25, 1–4/mo,
LN($450,.60) $10-rounded, floor $100; quiet month p .12 (activity
×.40 via skip .60).
**Small business** — clients: .55, 2–6, 0–3, LN($2,600,.75); platforms:
.22, 1–2, 0–3, LN($950,.70); card settlements: .74, 4–12/mo,
LN($680,.55); owner draw: .86, 1–2, LN($3,400,.70); cash takings:
.40, 4–10/mo, LN($2,800,.72) $10-rounded, floor $100; quiet month .06.
**High net worth** — owner draw .55, 1–2, LN($6,000,.65); investment
inflows .72, 1–3, LN($12,000, 1.0); quiet month .02.
**Retiree** — draw-like income .33, 1, LN($1,100,.50); investment .50,
1–2, LN($400,.65); quiet month .05.
**Salaried** — cash tips only: .03 active, 2–4/mo, LN($200,.55)
$10-rounded, floor $100 (cash-split-2026-07).
**Student** — cash tips only: .16 active, 1–3/mo, LN($140,.55)
$10-rounded, floor $100 (cash-split-2026-07; share recomputed by
household-econ-2026-07 — see the split table).
Class MEASUREMENT-adjacent. Sources: freelance-platform earning
studies; SBA/Intuit small-business cash-flow data.

**THE CASH-HANDLING SPLIT (cash-split-2026-07) — research-anchored;
every figure below is a NAMED-SOURCE recalled value with a confidence
tag, awaiting the owner's retrieval pass per the standing protocol:**

| Persona (share of pop) | Cash-active | Cadence | Amount | Basis |
|---|---|---|---|---|
| smallBusiness (.06) | **.40** | 4–10/mo | LN($2,800,.72) | cash-intensive establishment tier (below) |
| freelancer (.10) | **.25** | 1–4/mo | LN($450,.60) | offline informal work paid in cash (below) |
| salaried (.60) | **.03** | 2–4/mo | LN($200,.55) | tipped workers ≈ 2.5% of employment (below) |
| student (.12) | **.16** | 1–3/mo | LN($140,.55) | employment .40 × ~.4 tipped-job share [Derived — recomputed by household-econ-2026-07] |
| retiree (.10) | 0 | — | — | CHOICE: net cash SPENDERS, not depositors |
| highNetWorth (.02) | 0 | — | — | CHOICE: no takings/tips channel |

* ECONOMY-WIDE CASH CONTEXT | Source: Federal Reserve, Findings from
  the Diary of Consumer Payment Choice (2024 and 2025 editions,
  frbservices.org / Atlanta Fed S-DCPC tables). | Recalled values:
  cash ≈ 14–16% of payment COUNT; ~6–7 cash payments/person-month;
  82.6% of consumers used cash in the last 30 days (Table 5, already
  cited at L-6) [Certain on the ballpark, verify exact year figures].
  This is the demand side that makes business cash takings real.
* SMALL BUSINESS .40 | Sources: (a) IRS "Cash Intensive Businesses
  Audit Techniques Guide" (irs.gov) — the canonical sector list:
  restaurants/bars, convenience & grocery stores, salons/barbers,
  laundromats, car washes, taxis, vending, parking, scrap [Certain
  the guide exists and names these sectors]. (b) Census SUSB/CBP
  establishment mix: food service ~8%, small-format retail ~13%,
  personal services ~7%, gas/convenience ~2% of employer
  establishments ⇒ cash-heavy core ≈ 25–30% [Derived from public
  aggregates, Likely]. (c) Square "Making Change" seller reports:
  cash share of in-person transactions ~37% (2015) → ~30% (2019) →
  high-teens/low-20s post-2020, still declining [Likely on exact
  vintages]. | PL sets .40 — above the 25–30% establishment core —
  because PL's smallBusiness archetype is a Main-Street storefront
  (74% card-settlement active), which over-represents cash-accepting
  sectors relative to all establishments [Derived, documented].
* FREELANCER .25 | Sources: Fed SHED gig-work section (share of
  adults doing gig/informal work ~16%/month in recent vintages) and
  the Fed Board's Enterprising and Informal Work Activities (EIWA)
  survey (2015): cash is the dominant payment mode for OFFLINE
  informal work (trades, markets, personal services) [Likely on the
  cash-mode share — pull the exact figure at the verify pass]. PL's
  freelancer persona blends professional and informal freelancing;
  .25 cash-active is the offline-informal fraction [Derived].
* SALARIED .03 | Source: Yale Budget Lab, "Who Are Tipped Workers?"
  (June 2024): ~4.0M tipped workers ≈ **2.5% of US employment**,
  concentrated in food service and personal care, a third under 25
  [Certain on ~4M/2.5%]. PL rounds to .03 for the salaried persona
  (the general workforce). Amounts modest (LN($200,.55), 2–4
  deposits/mo ≈ $500–900 cash tips/mo) because card tipping now
  carries most tip volume [Likely].
* STUDENT .16 | [Derived]: PL student employment .40 (L-4,
  household-econ-2026-07) × ~.4 of student jobs in tipped
  food-service/hospitality (BLS student employment industry mix
  [Likely]) ≈ .16. This is the recompute the cash-split round queued
  (the original .05 was derived from the base-table .12 that the
  C2 employment finding showed was never the effective rate).
* RETIREE/HNW 0 | CHOICE, documented: Fed Diary age tables show 65+
  are the heaviest cash USERS for payments — they withdraw and spend
  cash, they do not deposit takings [Likely]. HNW have no
  takings/tips channel by construction.
* CTR CALIBRATION [Derived]: smallBusiness 600/10k × .40 = 240
  depositors × ~7/mo; P(> $10,000 | LN($2,800,.72)) ≈ 3.9%;
  P($9,000–$10,000) ≈ 1.4%. At the pinned config (10k, 60 d):
  ≈ 3,360 business deposits ⇒ ≈ 129 CTRs pre-attrition, ≈ 121 after
  the measured quiet-month/weekend haircut (re-pin #9 measured the
  .55/$2,500 config at 117 vs its ≈125 estimate — attrition ≈ .94).
  ANCHOR: FinCEN FY2024 20.5M CTRs/yr ÷ ~262M US adults ≈ 0.078
  CTRs/adult-yr ⇒ ≈ 128 expected at this config [Derived — Certain
  on the FinCEN numerator]. Salaried/student/freelancer deposits
  stay far below threshold and add ~3,000 small rows of realism
  (student tier tripled by the C2 recompute). The CTR:SAR ratio runs
  far above the national ~4.4 because SARs are ring-driven and
  deliberately sparse at pinned seeds; the F-6 funnel block treats
  that ratio as a sanity band, not a target.

### L-11. Population scaffolding

Accounts per person: 1 + Binomial(2, .25) — mean 1.5, max 3
(MEASUREMENT — Fed SCF accounts-per-household; SCOPE per C4:
checking-like transaction accounts ONLY — PL models no
savings/account-type distinction (`synth/accounts/counts.hpp`,
homogeneous one-owner deposit accounts), so the checking-only
comparator applies and the row CONFORMS per the Pass 2 conditional). Merchants: core
120/10k + tail 400/10k. Landlords: 12/10k. Counterparties per 10k
(floor): platforms 2 (2), processors 1 (2), owner businesses 200 (25),
brokerages 40 (5), plus employers 25 (floor 5, 4% internal-bank) and
clients 250 (floor 25, 2% internal-bank) —
`synth/counterparties/make.hpp:64–73` (labels closed by C0).
Government cohort: synthetic birth-day = content-hash 1–28
(mechanical, no claim). Merchant/landlord/counterparty densities:
re-classed CHOICE (2026-07-18, per the Pass 2 recommendation below —
no public per-10k-customer source exists).

**Pass 2 results (L-11):**
* Accounts per person 1.5 | Citation: S-DCPC Table 1 (2024: bank
  account 95.4%, checking 94.7%, savings 77.2% of consumers); SCF
  2022 transaction-account tables. | Real-world value: no official
  accounts-per-person count is published; ownership rates imply the
  average adult holds at least ~ 1.7 checking+savings accounts
  [Derived]. | Status: PL's 1 + Binomial(2,.25), mean 1.5 max 3, is
  consistent-to-slightly-low if savings accounts are in scope, fine
  if PL models checking only. Define scope in the row; then CONFORMS.
* Accounts-per-person SCOPE WRITTEN (C4, 2026-07-18) |
  Checking-only: every PL deposit account is an undifferentiated
  one-owner transaction account (spend/deposit-capable; no savings
  type exists in `entity::account`). Status: CONFORMS.
* Merchant/landlord/counterparty densities | UNCITED, and likely
  permanently CHOICE: no public per-10k-customer density source
  exists. Recommend re-classing these rows CHOICE now rather than
  leaving them as implied measurements.
  *(C1 doc ride: re-classed CHOICE in the header above.)*

## EXTRACTION QUEUE — CLOSED (2026-07-18: nothing remains)

The final cosmetic items were resolved by the C0 code reads: the two
ScaledCounts at `counterparties/make.hpp:64–73` are **employers**
{25/10k, floor 5, 4% internal-bank} and **clients** {250/10k, floor
25, 2% internal-bank}; `kRentTimestampJitter` = day offset ≤ 6, hours
07–22 (`activity/income/timestamps.hpp`); the day shock behind
`shockShape = 1.3` is Gamma(shape 1.3, scale 1/shape) — unit mean
(`activity/spending/actors/day.cpp`). No value in this document was
guessed. (Cash-deposit and scam-fraud values are tabulated in their
L-10/F-4 blocks with sources and tags.)

## Change history (model versions)

### fraud-audit-2026-07 (SHIPPED 2026-07-18)

F1 structuring ε max 400→1500 (realized 0.375 below alert band; 7/8
below the old $9,600 floor — liveness proven; row-count neutral, L
untouched). F2 shellScore constant→derived (`labels::ShellStats`, one
shared per-row accumulator; probe spread {0.00×5,.04,.10,.21}). F3 SAR
filing = content-keyed 70% AND ≥$5,000 inside `generateSars`;
consumers subset-safe. Trial scoring VACUOUS (standard-only goldens
were fraud-blind; pinned corpus has zero structuring rows; an earlier
attribution of baseline drift to F1 is RETRACTED — drift predates the
batch, uncommitted arc). Fix: FRAUD-VISIBLE PIN — `test_table_golden`
"fraud" section (aml-txn-edges 10k/60d/seed 7;
`tests/golden_tables_aml.md5`; HARD-REQUIRES ShellAccount/SAR/Alert/
CTR/InvestigationCase, stems verbatim; ≈39 s) — ENFORCING; its first
intentional-change round is the table golden's promotion trial; CSV
step 5 gated behind it.

Probe pitfalls: ledger CSV rows end CRLF (awk: `sub(/\r$/,"",$10)`);
ledger columns amount=$3, channel=$10; fraud-dense probe pop 10000/
seed 7 (5 rings, 53 structuring rows, shells, 2 SARs).

### conformance-statutory (SHIPPED 2026-07-18, re-pin #8)

The C1 batch of the conformance program; the table golden's REAL
promotion trial (fraud-audit-2026-07's was vacuous). CTR trigger
brought to the cited 31 CFR 1010.311 text: fires on STRICTLY MORE
THAN $10,000 (was >=) AND only on currency channels — new
`channels::isCurrency` predicate in
`taxonomies/channels/predicates.hpp`, applied in
`derived.cpp TxnSweep::observe` (was channel-blind: salary/ACH/card/
wire rows filed CTRs; the salary tail alone put ~7% of salary credits
over $10k). Sev-2 band extended to [$9,000, $10,000] INCLUSIVE so
exactly-$10,000.00 alerts at sev 2 and files nothing; band stays
all-channel by CHOICE. Read-back decode contract extended: the
transactions-table scan decodes `channel` losslessly
(`channels::parse` inverts `channels::name`); `test_pg_readback` pins
the round-trip, `test_derived_readback` pins corpus/readback parity
plus the boundary and scope negatives. Same-day aggregation
(1010.313(b)) stays unmodeled — known simplification. Golden effect
(re-pinned #8): only `tests/golden_tables_aml.md5` moved; CTR count
collapsed 340 → ~0, exposing the realism gap the next version fixes.

### cash-deposits-2026-07 (SHIPPED 2026-07-18, re-pin #9 — MEASURED)

CTR LIVENESS: legitimate business cash-takings deposits — the
real-world source of routine CTR filings — added as a sixth revenue
source in the L-10 system. New `Legit::cashDeposit` channel
("cash_deposit"): currency (`channels::isCurrency` now {atm_withdrawal,
cash_deposit, fraud_structuring}), payday-inbound like its five
sibling revenue channels (clearing/cure/splitters/credit
classification all inherited). Source = the branch/ATM cash hub
(`RevenueCounterparties::cashHubAccount`, wired in `passes.cpp` from
`hubAccounts.front()`); destination = business-else-personal account;
weekday branch hours; amounts LN snapped to $10 bills, floor $100
(`flow::detail::kCashTakings`, `Rule::roundTo`). Exporters: aml
purpose table gains "cash_deposit"; credit/debit classification via
isPaydayInbound. Tests: `test_channels` pins
name/payday/currency/byte-layout; `test_derived_readback` CTR pins
made STRUCTURAL. **MEASURED at re-pin #9 (pop 10k/60d/seed 7): 117
CTR rows (anchor ≈128); alerts 24,231; SARs 2; corpus 881,368 (was
910,176, −3.2% — liquidity knock-on: fewer overdraft fees and
terminal rejections). All invariance gates green.**

### cash-split-2026-07 (re-pin #10 — research-backed persona split)

Owner-requested research round replacing the provisional cash shares
with the L-10 CASH-HANDLING SPLIT (named sources + confidence tags;
retrieval verification is the owner's — the assistant's browser
permission was unavailable, so figures are recalled named-source
values per the standing protocol). Changes: smallBusiness cashTakings
activeP .55 → **.40** (cash-intensive establishment tier: IRS ATG
sector list + Census establishment-mix derivation + Square cash-share
trajectory) with median $2,500 → **$2,800** (keeps the FinCEN CTR
anchor: expected ≈121 measured vs anchor ≈128); freelancer .25
unchanged, now cited (SHED/EIWA offline informal cash); NEW salaried
tipped-worker tier **.03** (Yale Budget Lab: ~4M tipped workers ≈
2.5% of employment), 2–4/mo LN($200,.55); NEW student tier **.05**
[Derived: employment .12 × ~.4 tipped-job share — RECOMPUTE at the C2
student-employment ADJUST]; retiree/HNW excluded by documented
CHOICE. Implementation: `catalog.hpp` only — salaried/student
personas gain cash-only revenue profiles. GOLDEN EFFECT: ALL THREE
baselines re-pin (#10). Record measured CTR count in F-7.

### scam-fraud-2026-07 (re-pin #11 — victim scams + fraud reporting)

Owner-requested: model reported transaction fraud and gift-card
scams. (1) **Gift-card scam rail** in the unauthorized-fraud family
(`Rail::giftCardScam` in `typologies/unauthorized.hpp`): rail mix
card .60 / scam .12 / ATO .28 (`injector.cpp buildCompromisePlans`);
2–6 cards per case (targetEvents U{2..6}) in ONE 1–4 h coached burst;
amounts `amounts::giftCardScamAmount` — 75% {$100,$200,$500×3} else
$50–$500 on the $10 lattice, mean ≈ $339 (test-pinned in
`test_fraud_amounts`); rows ride the LEGITIMATE card_purchase channel
at retail merchants; NEW fraud_type label `scam_gift_card`
(`taxonomies/fraud/types.hpp`; the fraud_type ledger column renders
it everywhere). (2) **Reporting/reimbursement layer**: card-rail
compromises reported per-case p .85 → every fraudulent SPEND is made
whole by a merchant chargeback credit (cc_chargeback, flag-0, typed
none, lag 1–10 d, no attacker session); sub-$5 test charges never
reimbursed; gift-card scams NEVER reimbursed (authorized payments —
the modelable scandal contrast); ATO/Reg E remediation logged as a
known gap. BUDGET MECHANICS: reimbursements are flag-0 remediation
outside F = pL/(1−p) — `unauthorized::generate` bounds only flag-1
rows (`fraudEmitted` counter), so exact fraud denominators are
unchanged in kind. Tests: `test_unauthorized_keyed` extended (Rail
enum, a scam plan, label/no-reimbursement/chargeback pins; batch- and
history-invariance still hold); `test_fraud_amounts` pins the new
sampler. GOLDEN EFFECT: fraud rows re-roll (rail split + new draws)
and flag-0 chargeback rows enter the corpus ⇒ ALL THREE baselines
re-pin (#11 — commit #10 FIRST if it has not landed, one named commit
per model version). Sources & tags in the F-4 scam-fraud block.

### household-econ-2026-07 (SHIPPED, re-pin #12 — C2 household economics)

The C2 conformance batch: eleven rows shipped in one version, plus
two code findings and one ledger-coverage fix. Value-only changes —
no draw pattern, stream identity, or ordering was touched, so the
invariance gates must stay green while ALL THREE goldens re-pin.
* Rent amount: `math::amounts::kRent` Γ(2,400)+$50 → **Γ(2,700)+
  $100** (mean $850 → $1,500; ACS ~$1,487 anchor; L-3).
* RENTER-SHARE FINDING + ADJUST: no homeowner exclusion is wired and
  the .80 fit target made ~80% of people full-rent payers (~2.3× the
  ACS household renter share) — `rent::Rules::paidFraction` .80 →
  **.35**; effective persona shares in the L-3 C2 block; aggregate
  rent per capita reconciles to ~$525 vs real ~$520/person-month.
* Lease tenure 1–3y → **3–8y** (mean 5.5y ⇒ ~18%/yr turnover; L-5).
* EFFECTIVE-EMPLOYMENT FINDING + ADJUST: the salary selector's .95
  fit target scale-clamped every persona except retirees to ~100%
  employment; `salary::Rules::paidFraction` .95 → **.65** (= the
  persona table's weighted mean, fitted scale ≈ 1.0) and student
  base .12 → **.40** (NCES/BLS) — the table now IS the effective
  rate (L-4 C2 block).
* Student cash-tips share .05 → **.16** (`catalog.hpp`; the L-10
  recompute queued by cash-split-2026-07).
* Allowances Pareto($35, 2.2) → **Pareto($8, 1.8)** (mean ≈ $18/wk;
  L-9).
* Fuel ticket LN($45,.35) → **LN($32,.35)** (mean ≈ $34.0 vs Diary
  $32.8; L-2/L-3).
* Seasonality tails damped: Jan .88→.94, Feb .94→.96, Mar 1.04→1.02,
  Apr 1.02→1.01, Jun .98→.99, Jul .97→.98, Aug 1.05→1.03,
  Sep 1.02→1.01, Oct .99→1.00, Nov 1.16→**1.05**, Dec 1.22→**1.15**
  (Dec/Jan ratio 1.39 → 1.22 on the Census NSA anchor; consteval
  unit-mean normalization preserves ratios; L-2).
* P2P amount LN($45,.80) → **LN($55,.80)** (mean ≈ $75.7 vs the
  Diary mobile-app $71.9; the COUNT share stays a documented CHOICE;
  L-2/L-3).
* Home insurance premium LN($163,.30) → **LN($200,.30)** (≈
  $2,510/yr vs ~$2,530 anchor; L-8).
* Home claim severity LN($15,750,.90) → **LN($12,500,.80)** (mean ≈
  $17.2k vs III/ISO $17–18k; L-8).
* Tax refund LN($1,850,.55) → **LN($2,500,.55)** (mean ≈ $2,908 vs
  the ~$3,167 2025-season average; median-below-mean documented;
  L-8).
* COVERAGE FIX: new **L-4b** government-benefits table (SSA
  retirement .87/LN($2,071,.30) floor $900; SSDI .04/LN($1,630,.25)
  floor $500) — streams existed in code, absent from the ledger.
Files: `math/amounts.hpp`, `math/seasonal.hpp`,
`activity/recurring/lease.hpp`, `activity/income/salary.hpp`,
`activity/income/rent.hpp`, `routines/family/allowances.hpp`,
`synth/products/terms/insurance.hpp`,
`transfers/channels/insurance/rates.hpp`,
`synth/products/terms/tax.hpp`, `activity/income/revenue/catalog.hpp`.
GOLDEN EFFECT: ALL THREE baselines re-pin (#12 — commit #11 FIRST if
it has not landed). Measure list in F-7.

### card-behavior-2026-07 (THIS ROUND — C3 card behavior, re-pin #13)

The C3 batch: two code changes, one verdict REVERSAL, one re-class,
one design write-up. Value-only; invariance gates stay green; ALL
THREE goldens re-pin.
* PAYMENT-MIXTURE AXIS FINDING (doc-only, verdict reversed) | The
  mixture {full .35 / partial .30 / min .25 / miss .10} applies only
  to MANUAL payers (~50% of cards); autopay-full (.40) always pays
  in full, autopay-min (.10) always pays the minimum
  (`Session::draftPayment`). EFFECTIVE population shares: full .575 /
  min .225 / partial .15 / miss .05 — the .575 sits on the S-DCPC
  ~.58: **CONFORMS, the Pass 1 NONCONFORMING is withdrawn.** No code
  change; `payment.hpp` now documents the decomposition at the
  constants (L-7 C3 block).
* cardP TRIM (`taxonomies/personas/archetypes.hpp`) | student .65 →
  **.60**, retiree .84 → **.82**, freelancer .88 → **.85**, salaried
  .88 → **.85** (smallBusiness .95 / HNW .98 unchanged) ⇒ weighted
  mean .855 → **.826** on the strictly-credit comparator 82.3%
  (S-DCPC Table 3). Closes the L-1/L-7 near-CONFORMS flag.
* ATO PER-CASE ADJUST (`injector.cpp`) | non-card targetEvents
  U{2..5} → **U{3..8}** (mean 5.5) ⇒ per-case drain ≈ $3.0k vs UK
  remote-banking ~$3.5k/case — CONFORMS as a band. Drain sampler
  untouched (its $180 median is the cited per-victim anchor).
* CARD-RAIL PER-CASE → CHOICE | targetEvents stays U{5..14};
  documented label-density deviation (~8× UK per-case) in F-4.
* REG E DESIGN (written, owner-gated) | Bank-funded provisional
  credits for reported ATO cases (p ≈ .90, lag 2–10 business days,
  12 CFR 1005.6/1005.11); prerequisites: a bank-remediation
  counterparty and a NEW credit channel (cc_chargeback is
  merchant-funded — wrong semantics). Ships as its own version when
  the owner approves the entity + channel.
Files: `taxonomies/personas/archetypes.hpp`,
`src/transfers/fraud/injector.cpp`,
`transfers/channels/credit_cards/payment.hpp` (comment only).
GOLDEN EFFECT: ALL THREE baselines re-pin (#13 — land #12 FIRST if
pending). Measure list in F-7.

## Findings log

| Date | Item | Verdict | Action |
|---|---|---|---|
| 2026-07-17 | CTR ≥ $10,000; layering 3–8 hops | CONFIRMED (statutory / shape) | verify eCFR text / FATF edition in the pass |
| 2026-07-18 | F1 / F2 / F3 | SHIPPED (+F1 MEASURED 0.375) | F3 funnel calibration in the pass |
| 2026-07-18 | golden coverage | FINDING → FIXED | fraud pin enforcing |
| 2026-07-18 | posted structuring mix 15/32/53 | MEASUREMENT (emergent) | none |
| 2026-07-18 | ground-truth ledger opened; Part II extracted; queue CLOSED | — | owner citation pass (both parts) |
| 2026-07-18 | AUTHORITY RULE adopted | owner directive | cited rows govern; nonconforming code is changed via model versions |
| 2026-07-18 | Citation Pass 1 executed (retrieval) | 15 rows cited: 6 CONFORMS, 3 DEVIATES-BY-CHOICE, 4 NONCONFORMING, 2 partial | ADJUST proposals: CTR strict-> boundary + currency scope; rent retarget; CC payment mixture |
| 2026-07-18 | CTR trigger boundary | NONCONFORMING (reg says more than $10,000; PL fires at >=) | ADJUST proposal filed in F-4/F-6 blocks; also verify currency-only scope |
| 2026-07-18 | Rent Gamma mean $850 vs ACS ~$1,487 | NONCONFORMING (confirms row's own LOW flag) | ADJUST proposal in L-3 block; liquidity knock-ons noted |
| 2026-07-18 | CC full-payment share .35 vs ~.58 (S-DCPC T4) | NONCONFORMING or re-class CHOICE | owner decision requested in L-7 block |
| 2026-07-18 | Fraud budget deviation text ("a few bp") | inaccurate: US ~11 bp value (Nilson), 17.6 bp debit (Fed) | rewrite deviation note; count-vs-value axis documented |
| 2026-07-18 | Pass 2 executed: at-risk queue + L-8/L-9/L-11 tail | ~25 additional rows cited or anchored | see Pass 2 blocks per section |
| 2026-07-18 | Rent verdict REVISED | DCPC Table 13 per-txn rent avg $824 sits on PL mean $850; ACS household median ~$1,487 | status CONTESTED; unit definition (code read) decides |
| 2026-07-18 | Student employment .12 vs BLS/NCES ~40-45% | NONCONFORMING | ADJUST or persona redefinition (knock-on: L-10 student cash share) |
| 2026-07-18 | Allowances Pareto($35,2.2) vs $13-17/wk platform data | NONCONFORMING (floor exceeds measured averages) | ADJUST proposal in L-9 block |
| 2026-07-18 | Lease tenure 1-3y vs renter turnover 15-22%/yr | NONCONFORMING (~2.5-3x too fast) | ADJUST or CHOICE re-class; fix stale ~13% parenthetical |
| 2026-07-18 | Card fraud spend vs UK Finance 2025 per-case avg | CONFORMS (PL mean $162 vs ~$167) | none |
| 2026-07-18 | Fuel ticket LN($45,.35) vs Diary gas avg $32.8 | NONCONFORMING (~46% high) | ADJUST fuel median toward $32-38 |
| 2026-07-18 | Seasonality Dec/Jan amplitude 1.39 vs Census NSA ~1.22 | shape ok, amplitude wide | damp tails or CHOICE re-class |
| 2026-07-18 | C0 code reads executed | CTR defects CONFIRMED in code; rent CONTESTED → NONCONFORMING (sole tenant, household axis); cardP = strictly credit; refund = merchant credit; ATO unit = CompromisePlan × targetEvents; cosmetic queue closed | C1 shipped; rent ADJUST queued C2 |
| 2026-07-18 | conformance-statutory (C1) | CTR strict-> + currency scope; band → [$9,000, $10,000]; readback decodes channel | re-pinned #8; CTR volume gap exposed |
| 2026-07-18 | Card fraud spend AXIS | Pass 2 CONFORMS compared per-TXN $162 to UK per-CASE ~$167 — mismatch; PL per-case ≈ $1.2–1.5k (targetEvents U{5..14}) ≈ 8× UK | re-opened; owner call rides with C3 |
| 2026-07-18 | ATO per-case ≈ $1.9k (targetEvents U{2..5}) vs UK ~$3.5k/case | same order, low side (~55%) | owner: CONFORMS-as-band or ADJUST; rides with C3 |
| 2026-07-18 | CTR volume gap (post-C1 table ~empty) | REALISM GAP: no legit large-currency behavior modeled | FIXED by cash-deposits-2026-07 |
| 2026-07-18 | cash-deposits-2026-07 MEASURED (re-pin #9) | 117 CTRs (anchor ≈128, −9%); alerts 24,231; SARs 2; corpus −3.2% (liquidity knock-on: fewer OD fees/rejections) | figures recorded in F-7 |
| 2026-07-18 | cash-split-2026-07 | persona cash split research-anchored: smallBusiness .40 (IRS ATG + Census mix + Square), freelancer .25 (SHED/EIWA), salaried .03 (Yale tipped 2.5%), student .05 [Derived], retiree/HNW 0 (CHOICE) | re-pin #10; owner verifies the five named sources |
| 2026-07-18 | scam-fraud-2026-07 | gift-card scam rail (.12, victim-authorized, scam_gift_card label, never reimbursed) + card-fraud reporting p .85 with flag-0 chargeback reimbursements (Reg Z); ATO/Reg E remediation = known gap | re-pin #11; owner verifies FTC spotlights / §1643 / Security.org |
| 2026-07-18 | EFFECTIVE-EMPLOYMENT finding (C2 code read) | salary fitScale at target .95 clamped every persona except retirees to ~100% employment — the L-4 table described base weights, not behavior; prior verdicts inherited the misread | FIXED by household-econ-2026-07: paidFraction .65 = table weighted mean, scale ≈ 1, table = effective rate |
| 2026-07-18 | RENTER-SHARE finding (C2 code read, closes the queued residual) | rent paidFraction .80 ⇒ ~4 in 5 people paid a full household rent (~2.3× ACS ~.34–.36); no homeowner exclusion wired | FIXED by household-econ-2026-07: .80 → .35; homeowner overlap logged as known simplification |
| 2026-07-18 | GOVERNMENT-BENEFITS coverage gap | SSA retirement (.87, LN($2,071,.30)) and SSDI (.04, LN($1,630,.25)) streams existed in code with NO ledger rows | L-4b added, UNCITED with recalled anchors; owner verifies SSA snapshot |
| 2026-07-18 | household-econ-2026-07 (C2) | rent $1,500 mean + renter share .35 + tenure 3–8y + employment semantics + student .40 (+cash .16) + allowances $8/1.8 + fuel $32 + seasonality damped + P2P $55 + home premium $200 + home severity $12.5k/.80 + tax refund $2,500 | re-pin #12; measure list in F-7; owner verifies ACS/CPS/NCES/Greenlight/III/IRS/SSA anchors |
| 2026-07-18 | PAYMENT-MIXTURE AXIS finding (C3 code read) | the mixture is MANUAL-payers-only; autopay-full .40 bypasses it ⇒ effective full-payment share .575 vs measured ~.58 — the Pass 1 NONCONFORMING compared a conditional to a marginal | verdict REVERSED to CONFORMS; L-7 row rewritten; no code change |
| 2026-07-18 | card-behavior-2026-07 (C3) | cardP weighted .855 → .826 (82.3% comparator); ATO targetEvents U{2..5} → U{3..8} (per-case ≈ $3.0k vs UK ~$3.5k); card-rail per-case re-classed CHOICE (label density); Reg E design written, owner-gated | re-pin #13; owner verifies S-DCPC T3/T4, UK Finance per-case averages, autopay-adoption source |
| 2026-07-18 | c4-definitions-2026-07 (C4, doc-only) | seven definitions written and code-verified: spousal = some-money-separate (CONFORMS, Bankrate 62%); tuition = family-funded sticker COA to the student's account (CONFORMS, $30,990 anchor); subscriptions comparator = Bango 2025 (CONFORMS); repeat victimization = cross-ring reuse, ≤12-mo window; accounts-per-person = checking-only (CONFORMS); autopay comparator = accounts-on-autopay ~.40–.50; miss share = per-statement FLOW vs 30+dpd STOCK (CONFORMS-adjacent band) | no code, no re-pin; conformance write-side COMPLETE |
| 2026-07-20 | card-fraud-split-retire-2026-07 | owner directive: PhantomLedger emits NO ML train/val/test splits, ever — is_train/is_val/is_test dropped from Payment_Transaction (11 -> 8 loaded columns), the U-1 split row retired, the split derivations deleted from derive.hpp, the streaming Config's split-only window member removed; dataset splitting is downstream/in-graph work | golden re-pin: tests/golden_tables_card_fraud.md5 ONLY (cf_Payment_Transaction bytes; corpus stream and every other baseline untouched) |

## REMAINING OPEN ITEMS (after Pass 2 + C0/C1 + cash + scam + C2 + C3 rounds)

Everything below is either a definition task, a thin-tail row with no
strong published comparator, or an owner-gated design. Nothing here
is a known numeric contradiction; all known contradictions have been
resolved by shipped ADJUSTs or documented CHOICEs.

1. Code reads: DONE (renter share closed by C2; payment-mixture
   semantics closed by C3).
2. Definition writes: DONE (c4-definitions-2026-07) — spousal-
   separate, tuition COA, subscription comparator (Bango 2025),
   repeat-victimization window, accounts-per-person scope, autopay
   comparator and the miss-share axis mapping are written into
   their rows; every one is now falsifiable at the owner's pass.
3. Citations to pull verbatim at the verify pass: eCFR section text
   snapshots; FATF Professional ML (2018) page cites; a named Visa
   card-testing advisory; FinCEN structuring guidance and FFIEC
   manual pages; ISS/III auto claim frequency-severity table; FSA
   portfolio IDR shares; BLS Employee Tenure 2024 exact figure; OEWS
   May 2025 refresh; Atlanta Fed tracker current print; Diary
   cash-withdrawal (not payment) statistics for the ATM amount row.
   **Cash-split verify list:** Fed Diary 2024/2025 cash share; IRS
   Cash Intensive Businesses ATG; Census SUSB/CBP establishment
   counts; Square "Making Change"; Yale Budget Lab "Who Are Tipped
   Workers?" (Jun 2024, ~4M/2.5%); Fed SHED gig + EIWA 2015; BLS
   student-employment industry mix. **Scam-fraud verify list:** FTC
   gift-card Data Spotlights (top-reported scam payment method;
   ~$217M 2023; max-denomination coaching; brand mix); FTC CSN
   payment-method report mix (the .60/.12/.28 rail split); retailer
   $500 per-card caps; Reg Z / 15 U.S.C. §1643 + network
   zero-liability; Security.org reimbursement share (the p .85);
   Reg E (the documented ATO-remediation gap). **C2 verify list:**
   ACS B25064 median gross rent (~$1,487) + ACS/HVS renter share of
   households (~.34–.36); CPS renter turnover (15–22%/yr); NCES/BLS
   student employment (~.40–.45); Greenlight/Till allowance data
   ($13–17/wk); Diary Table 13 gas $32.8; Census MARTS NSA Dec/Jan
   ~1.22; Diary Table 8 mobile-app $71.9; home premium ~$2,530/yr
   (Philadelphia Fed); III/ISO home severity $17–18k; IRS 2025
   average refund ~$3,167; SSA Monthly Statistical Snapshot (retired-
   worker average benefit; SSDI average + beneficiary count).
   **C3 verify list:** S-DCPC Table 3 (credit adoption 82.3%) and
   Table 4 (~58% full payment); UK Finance 2026 per-case averages
   (remote purchase ~$167; remote banking ~$3.5k); an issuer
   autopay-enrollment source (the {.40/.10/.50} split is UNCITED);
   12 CFR 1005.6/1005.11 (Reg E liability tiers + provisional
   credit) for the design block.
4. Thin tail, expect CHOICE outcomes: L-9 parental/sibling/
   grandparent/gift/inheritance distributions (SHED gives incidence
   only); L-10 freelancer/small-business revenue profiles (platform
   earning studies are non-comparable); L-11 merchant/landlord/
   counterparty densities (re-classed CHOICE 2026-07-18).
5. Funnel calibration (F3 finding): fit SAR p and alert-to-case
   against the FinCEN FY2024 anchors (4.7M SARs, 20.5M CTRs, fraud
   52%) as sanity bands under deliberate oversampling. CTR liveness
   measured at 117 (re-pin #9); re-measure at each re-pin.
6. Owner-gated designs: ATO Reg E remediation (design in the F-4 C3
   block — needs a bank-remediation counterparty + a new credit
   channel); homeowner/renter overlap (wire `isHomeowner` into
   `RentRoll`); student part-time wage tier. (Monolithic-engine
   retirement SHIPPED 2026-07-18: one engine in the binary; the
   retained-corpus reference survives as the library test oracle.)
7. C3 residuals: CLOSED by C4 — the autopay comparator and the
   miss-share axis mapping are written into L-7; both rows stay
   UNCITED pending the owner's pass, but are now falsifiable.


### U-1. card-fraud use-case view derivations (card-fraud-2026-07)

View definitions and exporter-side presentation derivations for the
`card-fraud` use case (TigerGraph TF_GNN_v3 target;
exporter/card_fraud/). Card-view membership, identifier rendering,
category fallback, `use_chip`, and `error` do not alter the settled
corpus. The current `use_chip` and `error` fields are legacy
content-keyed presentation values; they do NOT report persisted
transaction mode, authentication, or authorization-attempt state.
World-modeled merchant geography is governed separately by U-2.

| Item | PL value | Class | Suggested source |
|---|---|---|---|
| Card view | channels {card_purchase, merchant}; merchant-channel (account-paid POS) rows interpreted as DEBIT-card transactions; ATO (p2p rail) excluded | CHOICE | IBM TabFormer mixes credit/debit cards |
| Card attribution | source Key in card registry -> that credit card (<=1 credit card/person); any other source -> the account's derived debit card; Card.is_fraud = card ever carried a flag-1 view row | CHOICE (label definition) | — |
| Identifier scheme | C/D/M = prefixed role.bank.number of the entity Key; P<person>; T<row_seq> (Payment_Transaction ids cross-reference the transactions table 1:1) | CHOICE | — |
| use_chip presentation mix | Swipe .63 / Chip .26 / Online .11, content-keyed per settled card-view row. These are PL presentation proportions over the TabFormer-shaped value set, NOT a measured in-person/remote purchase-mode share and NOT authentication state; Transaction currently persists neither axis | CHOICE (presentation compatibility; explicitly not a mode measurement) | IBM TabFormer (credit_card_transactions "Use Chip" field supplies the presentation vocabulary; no empirical proportion is claimed here) |
| error model | incidence 2.0% of view rows; mix Insufficient Balance .40 / Bad PIN .20 / Technical Glitch .20 / Bad Card Number .08 / Bad Expiration .05 / Bad CVV .05 / Bad Zipcode .02; error-free rows carry the empty string [Likely on incidence and mix — verify against the TabFormer "Errors?" column] | CHOICE | IBM TabFormer (credit_card_transactions "Errors?" column) |
| Category fallback | non-catalog view destinations (the unauthorized rail draws biller accounts) become Merchant vertices with a content-keyed uniform category over the 10-category taxonomy; keyed by destination so mer_cat and Merchant_Assigned agree by construction | CHOICE | — |
| mer_cat granularity | the 10-category merchant taxonomy stands in for TabFormer's MCC codes | DEVIATES-BY-CHOICE (an MCC taxonomy would be its own model round) | — |


### U-2. card-fraud finisher derivations (card-fraud-2026-07, continued)

Vertex/edge-side reporting added with the T3 finisher
(exporter/card_fraud/export.cpp). Identifier-only presentation fields may
remain content-keyed, but merchant geography and City.population are NOT
exporter derivations: the finisher reports causal world state already set
before spending. Reporting remains deterministic and has zero effect on
the settled corpus stream or any other use case's bytes.

| Item | PL value | Class | Suggested source |
|---|---|---|---|
| Merchant geography | the exporter resolves each observed catalog merchant's world-modeled `Record.location` through the build-fixed geography catalogue. A physical record with a valid area emits one internally consistent Has_City/Has_State/Has_Zip plus Assigned_To/Located_In chain; `online` records and non-catalog destinations remain geography-free. City id = "<city>_<state>". No exporter geo hash or PII zip-table draw remains | CHOICE (world-state reporting contract; merchant placement calibration remains provisional) | current input: `data/geo/us_cities.csv` placeholder; target provenance: Census Gazetteer/ACS |
| City.population | copied from the resolved world's `GeoArea.population`, so every merchant geo edge and City row reports the same catalogue record. The current build-fixed file is only a runnable 71-US-city-core + 15-international-place placeholder; it is not Census-complete, row order still defines `GeoAreaId`, and land area is not loaded. No exporter-side synthetic population hash remains | CHOICE (placeholder input; provenance/data expansion pending) | Census Gazetteer + ACS population/land-area target; current `data/geo/us_cities.csv` is not yet that artifact |
| Party.gender | content-keyed even F/M split per person — gender is NOT modeled anywhere in the world (names are pool indices without a gender attribute) | CHOICE | — |
| Party.is_fraud | fraud ACTOR label: person carries the fraud, soloFraud or mule roster flag; victims stay 0 | CHOICE (label definition) | — |
| Party.created_at | the standard exporter's Membership model (joinTs), identical to the public-schema customer table | CONFORMS (reuses the existing modeled value) | — |
| Party identifiers | Party ids are the CANONICAL customer ids (common::renderCustomerId); Merchant ids the canonical counterparty rendering; Device/IP ids the canonical renderings from the transactions table — card-fraud tables JOIN against every other use case's tables and the raw ledger | CHOICE (identifier reuse) | — |
| Is_Merchant | UNPOPULATED (header-only): the world has no modeled merchant-owning-party link (business owners own accounts, not catalog merchants); populating it is a model round of its own | DEVIATES-BY-CHOICE (documented gap) | — |
| Device/IP is_blocked | the MODELED flags (devices.flagged / ips.blacklisted), not placeholders | CONFORMS (reuses existing modeled values) | — |
| PII layer population | Address/Phone/Email/ID(ssn)/Full_Name/DOB vertices deduplicated over the person roster; edges per person; empty fields skipped — TF_GNN_v3 marks this layer DEMO ONLY and it is empty on real TabFormer; PhantomLedger fills it from its PII synthesis | CHOICE (the use case's differentiator) | — |


### U-3. card-fraud Payment_Transaction fraud rate at TabFormer scale (card-fraud-2026-07, measurement)

Recorded 2026-07-19 by the T4 measurement merge script, LIVE from
card_fraud."cf_Payment_Transaction" after the owner's TabFormer-scale
smoke (`--usecase card-fraud --population 20000 --days 730`, default
seed/start). THE AXIS: share of CARD-VIEW rows (channels card_purchase
+ merchant) carrying fraud flag 1 — NOT the corpus-wide illicit ratio
(targetIllicitP applies to ALL rows across every rail) and NOT an
event count.

| Item | PL value | Class | Suggested source |
|---|---|---|---|
| Payment_Transaction fraud rate | measured 0.1347% (12997 of 9645706 view rows at pop 20000 / 730d / default seed) vs the TabFormer anchor ~0.1% | MEASUREMENT | IBM TabFormer (credit_card_transactions "Is Fraud?" share over its ~24M rows) [Likely on the exact anchor figure — verify at citation time] |

If the measured rate deviates materially from the cited anchor,
closing the gap is a FRAUD-BUDGET change (targetEvents, the
unauthorized rail mix, and every fraud denominator are
model-versioned): owner-gated ADJUST round with golden re-pins, never
a silent edit.
### U-4. macro-history-v1 H0 era reference series (macro-history-2026-07)

Recorded 2026-07-24 by the H0 merge. The 1990-2020 era series live in
pinned data files (`data/econ/us_macro_annual.csv`,
`data/econ/us_mortality.csv`) at build-fixed paths (`PL_ECON_DATA`,
`PL_ECON_MORTALITY`) and are loaded + validated by
`synth::econ::macroSeries()` / `mortality()` and pinned by
`test_econ_catalog`. **H0 CONTRACT: the series are UNREAD by
generation.** No model constant changes in this round; every H1+
consumer lands in its own named model-moving round citing these rows.
Column encodings, precision, and per-column verification status:
`data/econ/README.md`. THE AXIS: unemployment is the ANNUAL AVERAGE
(not the monthly peak); `recession_months` counts months strictly
after the NBER peak month through the trough month; the canonical run
window `[1991-01-01, 2020-01-01)` ends BEFORE the COVID recession, so
the 2020 row is data, not required behavior.

| Item | PL value | Class | Suggested source |
|---|---|---|---|
| CPI-U annual averages 1990-2020 (`cpi_u_e3`) | 1990 130.7 -> 2019 255.657 -> 2020 258.811; 2019/1991 ~ 1.877; 2009 is the era's only annual deflation (-0.4%) | MEASUREMENT | BLS CPI-U, series CUUR0000SA0 [Likely - transcribed from the published series; verify at citation time] |
| SSA national Average Wage Index 1990-2020 (`awi_cents`) | 1991 $21,811.60 -> 2019 $54,099.99 (~2.48x); 2009 dips below 2008 | MEASUREMENT | SSA AWI series, ssa.gov/oact/cola/AWI.html [Likely - transcribed; verify] |
| Nominal per-capita PCE 1990-2020 (`pce_per_capita_dollars`) | ~$15,300 (1990) -> ~$44,000 (2019) (~2.9x), rounded to $100 | MEASUREMENT | BEA NIPA (FRED A794RC0A052NBEA) [PROVISIONAL - DERIVED from NIPA PCE / mid-year population, not transcribed from BEA's per-capita table; owner MUST verify before H1 wiring] |
| U-3 unemployment annual averages (`unemployment_rate_bp`) | 5.6% (1990), 7.5% (1992), 9.3%/9.6% (2009/2010), 3.7% (2019), 8.1% (2020); monthly peaks are HIGHER (7.8% 1992-06, 6.3% 2003-06, 10.0% 2009-10, 14.7% 2020-04) and need a monthly series in the H4 round | MEASUREMENT | BLS series LNS14000000 [Likely - transcribed; verify] |
| NBER recession months per year (`recession_months`) | 1990:5, 1991:3, 2001:8, 2008:12, 2009:6, 2020:2 (sums = published durations 8/8/18/2) | MEASUREMENT | NBER business-cycle dating [Certain on the dates; the per-year counting convention is documented in data/econ/README.md] |
| US resident population (`population_thousands`) | 249.6M (1990) -> 328.2M (2019) -> 331.5M (2020), rounded to 100k; strictly increasing every year | MEASUREMENT | Census intercensal/postcensal estimates [Likely - transcribed + rounded; verify vintage] |
| Mortality qx pivot table (`us_mortality.csv`, 24 pivot ages 0-110, qx_e6 male/female, log-linear interpolation) | e.g. male qx ~ .0019 (30), .0049 (50), .0163 (65), .0609 (80); implies survival 22->51 > 90% and 65->94 < 20% | CHOICE (PROVISIONAL) | SSA 2019 PERIOD life table (Actuarial Table 4C6) - the pivots APPROXIMATE it at 2-3 significant figures and were NOT transcribed from the primary source; the H3 authority round MUST replace them with exact single-age Table 4C6 values (one named table year) before any mortality behavior is wired |
| Funeral cost anchors (NOT yet in a data file) | NFDA median adult funeral (viewing+burial): 1991 ~ $3,742 -> 2019 $7,640 (2021 $7,848; cremation $6,970); H3 wires reference-year median x price index, payer = surviving kin | MEASUREMENT | NFDA General Price List surveys [Likely - verify at H3 citation time] |
| Conditional inheritance size/incidence | current `InheritanceEvent{eventP=.0015, LN median $25k, sigma 1.0}` is an UNANCHORED CHOICE and an UNCAUSED hazard; H3 retires the hazard in favor of death-caused estate transfer and re-anchors size/incidence | CHOICE (open) | Fed Survey of Consumer Finances intergenerational-transfer tables |
| Reference year for nominal dollar constants | PROPOSED 2019 (most audit anchors are 2015-2022 measurements); H1 scales event-year draws by series ratio from this base | CHOICE (PROPOSED - OWNER APPROVAL REQUIRED before H1 wiring) | - |
| H0 wiring status | data loaded + validated + meaning-gated by test_econ_catalog; UNREAD by generation; zero golden movement | MEASUREMENT (code fact) | this repository |

Deviation rule for this section: H1+ rounds that wire any of these
series into behavior are MODEL-MOVING (named round, meaning gates, one
internal re-pin); replacing the PROVISIONAL mortality pivots or the
DERIVED PCE column with exact primary-source values while the series
remain UNREAD is a data/doc correction, not a model change, and moves
zero goldens.
**U-4 VERIFICATION AMENDMENT (macro-history-2026-07b, primary-source
audit 2026-07-24):** the owner granted read-only in-app browser access
and the H0 series were audited against live primary sources (no
downloads; FRED's fredgraph.csv download endpoint is blocked by
policy, so the HTML /data views were used). Results, superseding the
corresponding rows/statuses above:

* **Mortality table — PROVISIONAL CLOSED, re-classed MEASUREMENT.**
  `data/econ/us_mortality.csv` is now an EXACT full transcription of
  the SSA PERIOD LIFE TABLE FOR 2023, as used in the 2026 Trustees
  Report (Actuarial Life Table 4C6, ssa.gov/oact/STATS/table4c6.html,
  read 2026-07-24): single ages 0-119, male/female qx to six decimals
  = exact qx_e6 integers. The named-table-year requirement is
  satisfied (2023 / 2026 TR); the 24-pivot approximation is RETIRED.
  Male and female qx are equal from age 109 up (source values). The
  arc's cohort claims cross-check against the source lives column:
  survival 65->94 = 8,320/79,084 ~ 10.5%; survival 22->51 =
  90,659/98,458 ~ 92.1%.
* **AWI — VERIFIED EXACT.** All 31 values 1990-2020 in `awi_cents`
  matched ssa.gov/oact/cola/awiseries.html (read 2026-07-24),
  including the 2009 dip (-1.51%).
* **Per-capita PCE — PROVISIONAL-DERIVED CLOSED, re-classed
  MEASUREMENT (exact).** `pce_per_capita_dollars` now carries the
  exact BEA values from FRED series A794RC0A052NBEA (HTML data view,
  vintage "Last Updated 2026-04-09"): $15,225 (1990) -> $43,682
  (2019) -> $42,886 (2020); 2019/1990 ~ 2.87.
* **Population — SOURCE CHANGED to exact.** `population_thousands` now
  carries the exact BEA NIPA MIDPERIOD population from FRED series
  B230RC0A052NBEA (vintage "Last Updated 2026-02-20"): 250,181 (1990)
  -> 330,513 (2019) -> 331,840 (2020). AXIS: this is the per-capita
  PCE denominator, NOT the Census July-1 resident estimate (<0.3%
  apart); the U-4 row's Census framing is superseded.
* **CPI-U and unemployment — UNCHANGED, still transcription
  [Likely].** bls.gov timed out and no alternate origin was approved
  during the audit session; owner-verify at citation time or approve
  an origin for a follow-up read. Values remain the standard published
  BLS annual averages.

The series remain UNREAD by generation, so every correction above
moved ZERO goldens; test_econ_catalog re-ran green on the exact
values (its bands were chosen as direction bands and required no
edits). Remaining open items in this section: owner approval of the
PROPOSED 2019 reference year (blocking H1), the NFDA/SCF anchors
(H3-time), and the monthly unemployment path (H4-time).
### U-5. macro-history-v1 H1 opening: calibration year + coverage extension (macro-history-2026-07c)

Recorded 2026-07-24 by the H1 opening merge. STORAGE NOTE superseding
U-4's file paths: the era data now lives EMBEDDED in
`include/phantomledger/synth/econ/era_data.hpp` (constexpr tables; the
`data/econ/` files and their build-fixed paths are retired per the
owner's minimize-repo-data-files directive #4); the provenance/refresh
contract moved to `docs/era_data_provenance.md`. The series remain
UNREAD by generation — every change in this round moved ZERO goldens.

| Item | PL value | Class | Suggested source |
|---|---|---|---|
| CALIBRATION YEAR for nominal dollar constants | 2019 (`kCalibrationYear`, era_data.hpp; `MacroSeries::calibrationYear()`, builder-validated inside coverage; exact denominators pinned: CPI 255.657, AWI $54,099.99). The U-4 "reference year PROPOSED 2019" row is RESOLVED: OWNER-APPROVED 2026-07-24 under the owner's durability criterion — the year is a PROVENANCE FACT of the calibration data (constants measured ~2015-2024 declared 2019-denominated: the last full canonical-window year and last pre-COVID year), never "the present"/coverage-tail/wall-clock/run-start. It changes only with the constants it denominates. | CHOICE (OWNER-APPROVED) | Rejected alternatives recorded in docs/era_data_provenance.md; per-constant measurement vintages = registered upgrade path |
| Macro coverage extension 2021-2024 | `kMacroAnnual` now 1990-2024 (35 rows), every cell fully MEASURED: CPI 270.970/292.655/304.702/313.689; AWI $60,575.07/$63,795.13/$66,621.80/$69,846.57; per-capita PCE $48,480/$52,909/$55,870/$58,501; U-3 5.3/3.6/3.6/4.0; recession months 0; population 332,503/334,350/337,087/340,095 (thousands). 2025 is IMPOSSIBLE to pin as of 2026-07: AWI 2025 publishes ~2026-10, and the October 2025 CPI release and CPS survey were cancelled (federal shutdown) — no official 2025 annual averages exist. | MEASUREMENT | ssa.gov AWI page; FRED CPIAUCNS / A794RC0A052NBEA / UNRATENSA / B230RC0A052NBEA HTML data views, all read 2026-07-24 |
| CPI-U precision correction 1990-2006 | tenths-rounded transcriptions replaced by EXACT annual averages (e.g. 1990 130.7 -> 130.658; 1991 136.2 -> 136.192; 2006 201.6 -> 201.592); 2007-2020 already exact. Method: the official BLS annual average IS the mean of the 12 NSA monthly indexes; recomputed from the FRED CPIAUCNS data view for every covered year and matched to the third decimal. U-4's CPI status [Likely - transcribed] is SUPERSEDED: VERIFIED EXACT. | MEASUREMENT | FRED CPIAUCNS (BLS CUUR0000SA0 mirror), read 2026-07-24 |
| Unemployment verification narrowing | official BLS annual averages retained; FRED UNRATENSA monthly means reproduce every covered value within 0.1pp (the official statistic is a ratio of annual averages, not a mean of monthly rates — last-digit differences in e.g. 2011, 2021). Status: transcribed + cross-checked. | MEASUREMENT | FRED UNRATENSA, read 2026-07-24; BLS LNS14000000 remains the canonical citation |
| H0.6 lock consequence | the card-fraud era lock reads coverage bounds, so the runnable window widened to 1990-2024 with NO engine change; the default 2025-01-01 start REMAINS rejected (2025 unmeasured) — a DELIBERATE TRIPWIRE assertion in test_app_options flips when the 2025 row lands. Windows crossing 2020-04 now get correct nominal LEVELS; COVID/EIP BEHAVIOR stays P2 (the same unmodeled-modulation status every pre-2020 recession has until H4). | MEASUREMENT (code fact) | this repository |
| H1 wiring status | NOT YET WIRED: generation still reads nothing from the series. The wiring round (H1 step 2) is MODEL-MOVING: AWI drives incomes (replacing the flat .025 compounding), CPI drives price-level amounts, both as index(year)/index(2019); beyond-coverage years FREEZE at the last measured scale with a printed declared notice (adopted policy: freeze-and-declare, never silent extrapolation, never a new CLI arg); one named re-pin. | MEASUREMENT (code fact) | this repository |

Deviation rule for this section: appending fully published years is a
data+authority refresh (zero goldens until H1 wiring lands, MODEL-
MOVING after); changing `kCalibrationYear` is a re-calibration round
touching every dollar constant's denomination — never bundle it with a
coverage refresh.

### U-6. macro-history-v1 H1 step 2b: nominal-scale wiring classes (macro-history-2026-07d)

Recorded 2026-07-25 by the H1 step-2b wiring merge. This section is
the authority for the MODEL-MOVING wiring of `synth/econ/nominal.hpp`
(priceScale/wageScale, level-anchored at kCalibrationYear 2019,
freeze-and-declare outside 1990-2024 coverage) into every
dollar-realization surface, per the contract
docs/h1_nominal_scale_wiring.md. Wiring shape everywhere:
draw -> x scale -> (denomination re-snap if any) -> roundMoney ->
spool/emit. RNG streams, lanes and entity ordering are byte-identical
to the pre-wiring engine; ONLY amounts (and amount-derived screens)
move. ONE named re-pin of all four goldens lands with this round.
U-5's "H1 wiring status: NOT YET WIRED" row is SUPERSEDED: wired.

| Item | PL value | Class | Suggested source |
|---|---|---|---|
| Class W (wage-indexed) | salaries (SalaryCalculator at pay date), freelancer/business revenue (revenue Cycle at month), SSA retirement + disability benefit deposits (at deposit date) scale by wageScale(realization year) = AWI(year)/AWI(2019). Deviation stated: real SSA wage-indexes at award then CPI-COLAs per cohort; ONE index era-wide is the H1 simplification until personas retire in-model (H2/H3). | CHOICE | SSA AWI series (ssa.gov); docs/h1_nominal_scale_wiring.md |
| Class P (price-indexed) | rent (calculateRent at pay date), spending-session tickets (payment router at event day), subscriptions (kPricePool price at DEBIT date — subscription prices track the era level; per-contract frozen pricing REJECTED: would hold 1991 prices for decades), insurance premiums (at billing date) + claims (at claim date), family routines incl. interim inheritance (at event ts), ATM and internal self-transfers (see denomination row), card late fee + minimum-payment dollar floor (at cycle date) scale by priceScale(realization year) = CPI-U(year)/CPI-U(2019). | CHOICE | FRED CPIAUCNS; docs/h1_nominal_scale_wiring.md |
| Class P stocks (window-start anchor) | account opening balances, overdraft fees, protection buffers, LOC limits (OpeningBalanceSeeder) and card credit limits (synth::cards::issue) scale ONCE by priceScale(window-start year); they then evolve through scaled flows. Persona initialBalance/baselineCash references scale identically so liquidity/utilization RATIOS stay coherent. DECLARED APPROXIMATION: the stock anchor is fixed at window start while flow scale drifts across a decades window — nominal balance levels lag late-window flows until H4 macro coupling; ratios remain order-correct. | CHOICE | docs/h1_nominal_scale_wiring.md |
| Class D (origination-anchored debt) | mortgage/auto/student monthly payments scale by priceScale(ORIGINATION year) at issue and stay FIXED NOMINAL after (real loans are nominal contracts). Backdated originations before 1990 clamp to 1990 (freeze-and-declare = scaleYear's coverage clamp — declared, deterministic). Tax quarterlies/filings scale by priceScale(due year) instead: IRL brackets index annually. | CHOICE | docs/h1_nominal_scale_wiring.md |
| Class S (statutory fixed-nominal) | BSA/CTR $10,000 threshold + the structuring typology's threshold band (<= $9,950) + amounts UNCHANGED; ATM $20-note and cash-deposit $10-bill lattices UNCHANGED as lattices (amounts scale then RE-SNAP: a 1991 withdrawal is fewer $20s, not scaled $20s); $0.01 billable-interest de-minimis and $1 de-minimis amount floors UNCHANGED. | MEASUREMENT | 31 CFR 1010.311 — the CTR threshold has been UNINDEXED since the 1970s; holding it fixed while everything else scales is HISTORICALLY CORRECT (in 1991 it bit at roughly 2x today's real value) |
| Class F (fraud) | continuous fraud amount draws scale by priceScale(event year): kFraud ($900 median) / kFraudCycle ($600) rails (classic, cycle, mule, invoice, layering, scatter-gather, bipartite, funnel), cardFraudSpend ($79 median, clamps [$1,$5k] x scale), atoDrainAmount ($180 median, clamps [$10,$85k] x scale). Structuring EXCLUDED (class S). The 0.11675% calibration target is a COUNT rate — unaffected by amount scaling; funnel dollar floors scale with their amounts so funnel geometry is scale-invariant. | CHOICE | Security.org card-fraud + ATO reports (F-4 lineage); docs/h1_nominal_scale_wiring.md |
| Denomination-lattice fraud amounts | cardTestCharge anchors ($0.50/$1/$2/$5) and giftCardScamAmount rack denominations ($100/$200/$500 + $10-step rack range) stay FIXED-NOMINAL (owner-approved 2026-07-25): the round-amount signature IS the typology, and denominations are physical/rack artifacts like the $20 note. Deviation from the literal "every typology except structuring scales" contract row is DECLARED here. Era-availability of these rails (gift cards, card-testing) is the H5 instrument-history debt, unchanged by this round. | CHOICE (OWNER-APPROVED) | FTC gift-card Data Spotlight family; Chargeflow card-testing (F-4 lineage) |
| Camouflage mimicry index | fraud camouflage traffic scales with the index of the flow it MIMICS: bill/p2p cover draws x priceScale(event year); the salary-mimic (kSalary x 12) x wageScale. A camouflage row that scaled differently from its cover class would be a detectable artifact. | CHOICE | docs/h1_nominal_scale_wiring.md |
| Dollar-literal screens (invariant-4 sweep) | BEHAVIORAL screens scale with the index of the amounts they screen: paycheck $50 minimum (wageScale), revenue rule floors $20-$250 (wageScale), ATM affordability reserve clamp $40-$120 (priceScale), liquidity kCashRefFloor $75 (priceScale at event day), card minPaymentDollars $25 + lateFee $32 (priceScale at cycle), funnel floors $50/$5 and fraud clamps (priceScale). STATUTORY/DE-MINIMIS screens stay fixed: CTR $10k, structuring band, $0.01 interest, $1 amount floors. | CHOICE | docs/h1_nominal_scale_wiring.md invariant 4 |
| Retirement of the .025 annualInflation constants | employment.hpp SalaryGrowthRules and lease.hpp RentGrowthRules no longer carry `annualInflation = 0.025`; the field is REMOVED from growth::CompoundRules (the AWI/CPI index IS the economy-wide nominal path — start-anchored geometric 2.5% was the stationary placeholder). The seeded `salary_real_raise` / `rent_real_raise` idiosyncratic lanes SURVIVE as career/lease progression ON TOP of the index: population mean drifts (1+mu)^tenure above the pure index (mu = .015 salary / .020 rent) — the aggregate acceptance bands allow this declared drift. | CHOICE | docs/h1_nominal_scale_wiring.md; L-3 amount catalog rows unchanged (draws stay calibration-year dollars) |
| Frozen-era declared notice | runs whose window touches years outside 1990-2024 coverage print ONE stderr notice (app layer, options.hpp helper + main.cpp; pinned by test_app_options) declaring the freeze at the nearest covered year's level. Never extrapolated, never wall-clock, no new CLI. | MEASUREMENT (code fact) | this repository |
| Acceptance gates | serverless test_econ_wiring (GateWorld legs): 2019/1991 salary ratio inside an AWI band (~2.48x +/- idiosyncratic-drift allowance); ticket ratio inside a CPI band (~1.88x); calibration-year magnitudes ~= catalog; structuring band intact at 1991 while classic-fraud amounts scale; deflated per-active spend ~flat across a multi-year leg (macro modulation is H4). | MEASUREMENT (code fact) | this repository |

Deviation rule for this section: every future era-data refresh is
MODEL-MOVING (the series are now READ by generation); re-indexing any
class or scaling a class-S constant is a re-calibration decision, not
a refresh. The mortgage counterparty defect (mortgage.cpp ->
studentServicer) was EXPLICITLY NOT bundled into this round
(owner decision 2026-07-25): the step-2b diff moves amounts only.

### U-7. macro-history-v1 H2 step 1: persona timeline classes (macro-history-2026-07e)

Recorded 2026-07-25 by the H2 step-1 merge. This section is the
authority for the persona TIMELINE: the pure primitive
`synth/personas/timeline.hpp` (persona-AT-DATE derived once per person
on the isolated {"persona-era", personId} lane from the seed
assignment + birth date; contract docs/h2_persona_timeline.md). STEP 1
lands the primitive UNWIRED — zero golden movement, meaning gates in
tests/test_persona_timeline.cpp. STEP 2 (the wiring: salary spans,
SSA onset, the age carrier, the AML customer export, the retirement
spending step) is the next MODEL-MOVING round with ONE named re-pin.
Owner decisions 2026-07-25: staged delivery; single age axis via an
isolated {"dob", personId} lane + compact carrier at the wiring round;
AML Customer reports the END-OF-WINDOW persona; the wiring includes
the retirement spending step.

| Item | PL value | Class | Suggested source |
|---|---|---|---|
| Full retirement age schedule | timeline::fraMonths(birthYear): 65y through 1937 cohorts; +2 months per birth year 1938-1942; 66y for 1943-1954; +2 months per birth year 1955-1959; 67y from 1960. Cohort-varying by statute; pinned test-exact. SSA's "born January 1 counts as the previous year" quirk is a declared simplification away. | MEASUREMENT | Social Security Amendments of 1983; ssa.gov retirement-age chart |
| Claiming-age mixture | .30 mass at exactly 62; .10 uniform over [63y, FRA); .45 at FRA; .05 uniform over (FRA, 70y); .10 at exactly 70; plus 0-60 day birthday-relative jitter. ONE distribution era-wide — era variation enters through the statutory FRA. Deviation stated: claiming at 62 was substantially more common in the early 1990s than in recent cohorts; per-cohort claiming shares are a REGISTERED UPGRADE (same pattern as the single mortality-table year, U-4). | CHOICE | SSA Annual Statistical Supplement, OASI claiming-age tables (ssa.gov/policy/docs/statcomps/supplement/) |
| Student work-start | age drawn over 19-28 with mass at 22-26 (weights .05/.05/.08/.20/.20/.15/.10/.07/.05/.05 from age 19), anchored to the BIRTH date; destination salaried .85 / freelancer .15. Non-completers enter early; graduate tail to 28. | CHOICE | NCES completion-age statistics; BLS CPS student employment (the L-4 student .40 employment row is the DURING-study probability; the timeline sets when study ends) |
| Small-business churn | residual business lifetime = memoryless exponential with median 5 years, clamped [30 days, 40 years] (constant hazard: backdating-invariant); destination after close: salaried .70 / freelancer .30; retirement DOMINATES (a business surviving to the claiming date closes there). One business per seed owner at H2 (repeat founders = registered upgrade). | TYPOLOGY (hazard shape) on a MEASUREMENT anchor (~50% five-year establishment survival) | BLS Business Employment Dynamics, establishment age/survival (bls.gov/bdm) |
| Seed-consistency clamps | personaAt(timeline, simStart) == seed type is a PINNED invariant. Drawn dates already in the past SETTLE forward: seed students past the drawn work-start age finish in U[90,540] days; seed workers past the drawn claim work another U[180,1825] days; seed retirees' claims clamp to sim start. In-window drawn dates stand exactly as drawn (clamps bind ONLY on past dates). | CHOICE | docs/h2_persona_timeline.md (the seed assignment is by definition the state at sim start) |
| highNetWorth exemption | NO timeline transitions at H2 — a retired-HNW spending profile is CEX/H3 work; forcing the plain retiree archetype onto HNW would distort more than it fixes. Declared, revisited at the CEX age-profile round. | CHOICE | docs/h2_persona_timeline.md |
| Single age axis (wiring step) | the DOB draw moves from the shared sequential PII stream to the isolated {"dob", personId} lane feeding a compact carrier; PII renders FROM the carrier; the SSA deposit cohort derives from the REAL birth day-of-month (1-10 / 11-20 / 21-31 -> cohorts 0/1/2 per the SSA payment schedule), RETIRING cohort::syntheticBirthDay (the existing DOB/cohort inconsistency is a declared defect fixed at the wiring). PII bytes move; the step-2 re-pin absorbs them. | MEASUREMENT (the SSA payment-day scheme) + CHOICE (single axis) | ssa.gov payment schedule; docs/h2_persona_timeline.md |
| AML Customer persona semantics (wiring step) | the Customer vertex reports personaAt(window END) — KYC/CDD current-state extraction semantics (a dataset extracted today carries today's customer type over full-history transactions). customerType / networthCode / incomeCode / occupation follow the end-of-window persona. | CHOICE (OWNER-APPROVED 2026-07-25) | docs/h2_persona_timeline.md |
| Retirement spending step (wiring step) | at the claiming date, consumption applies a declared level factor ~-12% (band 10-15%) through the H1 day-frame seam; payday sensitivity re-anchors to the SSA deposit days. The full CEX age-profile re-anchor (rate/amount archetype swap) is explicitly NOT at H2. | CHOICE anchored to MEASUREMENT (retirement consumption drop) | Aguiar-Hurst (JPE 2005) retirement-consumption literature; BLS CEX age profiles |
| Salary/benefit selection becomes timeline-consequent (wiring step) | the L-4 per-persona employment probabilities apply to the persona-AT-DATE; Paymaster employment spans truncate at retirement; seed-student spans start at workStart; SSA recipients = retired-at-date with deposits from the claim date. The paidFraction .65 fit target is re-derived at the wiring read (the weighted mean moves with the era mix) — declared there, gated as a band. | CHOICE | salary.hpp L-4 lineage; docs/h2_persona_timeline.md |

Deviation rule for this section: the step-1 primitive is UNWIRED and
zero-golden; the step-2 wiring is MODEL-MOVING (ONE named re-pin).
Changing any distribution above (claim mixture, work-start weights,
business hazard, spending step) after the wiring is a re-calibration
decision on this lineage, never a silent edit.
#### U-7 ADDENDUM (macro-history-2026-07f: H2 arc close)

Code-facts and declarations recorded at the H2 arc close (steps 2b/2c
executed; contract docs/h2_persona_timeline.md).

| PL value | Class | Suggested source |
|---|---|---|
| Payroll era-axis defect FIX: `paydatesForProfile` weekly/biweekly lattices are era-agnostic — weekly pays on its weekday in every week; biweekly aligns the window's first on-weekday date to the 2025-01-01 anchor's FORTNIGHT PARITY (the anchor is a lattice PHASE, not a start bound). Pre-fix, every pre-2025 window emitted ZERO weekly/biweekly paydates (75% of the cadence mix silent; the 1991 smokes carried roughly half the 2025-start income at equal population). Windows at/after the anchor emit unchanged dates. Cadence mix and anchor constants UNCHANGED. | MEASUREMENT (correctness repair on the U-7 lineage; no model parameter moved) | code-fact, tests/test_persona_wiring.cpp |
| Retirement spending step exemption: the ~-12% step (kRetiredSpendScale .88, from the claiming day, through the day-frame seam) applies ONLY to working-seed archetypes transitioning in-window; SEED RETIREES and highNetWorth carry no step — the retiree archetype already encodes retired-calibrated spending (rate x0.6 / amount x0.9), so stacking would double-count | CHOICE anchored to MEASUREMENT (Aguiar-Hurst JPE 2005; the U-7 step row) | BLS CEX age profiles; the audited persona table |
| Payday re-anchor to SSA days: `buildPaydaysByPerson` screens the inbound stream by `isPaydayInbound`, which admits gov_social_security / gov_pension / gov_disability — a mid-window retiree's liquidity relief/stress cycle re-anchors from salary days to SSA deposit days AUTOMATICALLY once the 2b income switch moves their inbound stream (zero new code; pinned in test_persona_wiring) | MEASUREMENT (code-fact) | channel taxonomy, tests/test_channels.cpp |
| AML Customer persona = END-OF-WINDOW state: both streaming sinks resolve `personaAt(lastTs)` in takeArtifacts(), where lastTs is the corpus maximum timestamp (derived from the replay-sorted stream exactly as simStart is from its first row); empty streams / carrier-less packs leave the seed assignment | CHOICE (owner decision 2026-07-25; U-7 row executed) | exporter/aml/vertices.hpp |
### U-8. macro-history-v1 H3: mortality, estates, replenishment (macro-history-2026-07g)

Steps verified through part 3b-i (contract docs/h3_mortality_estate.md;
gates test_lifespan, test_persona_wiring H3 sections, test_estates).

| PL value | Class | Suggested source |
|---|---|---|
| Death dates: annual hazard walk over the EMBEDDED SSA 2023 period life table (4.C6, sex-specific qx, log-linear age interpolation), inverted at one uniform per person on the isolated {"mortality", personId} lane; deaths anchor to BIRTH dates; residual mass beyond age 120 dies at the cap | MEASUREMENT (table) + CHOICE (inversion mechanics) | SSA Period Life Table 2023, table 4.C6 |
| ALIVE-AT-START invariant: the hazard walk begins at the person's current fractional age — death is conditional on survival to sim start and lands strictly after it (the mortality analog of personaAt(simStart)==seed) | INVARIANT | model consistency |
| Latent sex: Bernoulli 50/50 drawn on the mortality lane (the world models no sex; the table's ~2.7-year male/female gap is retained as real signal); surfacing sex to PII and a measured population ratio are REGISTERED UPGRADES | CHOICE | Census sex ratio (registered) |
| One period table era-wide (2023); NO persona/SES-differential mortality; deaths uniform within the death year | CHOICE (three declared simplifications; historical-period tables and SES gradients registered) | SSA historical life tables (registered) |
| Death stops — income: salary interval ends at min(retirement, death); SSA/disability deposits end at death (benefits die with the beneficiary; survivor benefits REGISTERED); revenue months stop at death including the otherwise-perpetual retiree/HNW plans | CHOICE anchored to program rules | SSA payment-after-death rules |
| Death stops — behavior: the spending session skips a dead spender's person-days; ATM withdrawals and internal self-transfers stop (emission-side filters — rng streams byte-identical); rent stops (the lease dies with the tenant; a declared shared-stream shift); family gifts drop when either party is dead (external XF members' deaths unmodeled — declared) | CHOICE (mechanism); split deposits stop as a CODE-FACT (they consume the payday-inbound stream, which death-clipped income ends) | — |
| THE BEHAVIORAL/CONTRACTUAL LINE: contractual flows (subscriptions, insurance premiums, loan/tax obligations, card cycles) keep posting against the estate until ACCOUNT CLOSURE (part 3c-ii membership close) — estates really do keep getting billed | CHOICE (declared, realism-defensible interim) | estate-administration practice |
| DEATH-CAUSED ESTATES replace the uncaused hazard (0.15% of retirees per 180-day sweep — RETIRED): every in-window death with heirs distributes an estate at death+30-90 days (probate settle, declared); estate size keeps the interim lognormal($25,000, sigma 1.0) — an SCF-anchored re-derivation is REGISTERED; heirless estates unhandled until closure | TYPOLOGY (causation) + CHOICE (interim size) | SCF net-worth distributions (registered) |
| FUNERAL: one bill-channel payment from the decedent's account at death+3-10 days to the external service-merchant hub (dedicated funeral counterparty/channel REGISTERED); lognormal median $6,300 CALIBRATION dollars = NFDA 2019 GPL blend (funeral with viewing+burial $7,640; cremation with viewing $5,150; ~55% 2019 cremation rate), sigma .40, floor $1,000, CPI-realized at the death year | MEASUREMENT (NFDA blend — OWNER SPOT-CHECK) + CHOICE (sigma/floor) | NFDA 2019 General Price List survey; NFDA/CANA cremation rate |
| Fixed per-decedent draw order (funeral amount/day/hour/minute, estate total/day/hour/minute) on the isolated {"family","inheritance"} lane — emit decisions cannot move later decedents; the lane's stream changed wholesale with the hazard retirement (declared) | INVARIANT | draw discipline |
| GATE AMENDMENT: test_econ_wiring's ring-rail fraud mean band widened (1.4, 2.4) -> (1.4, 2.6) — the sparse heavy-tailed mean recomposes whenever the candidate count L moves (F = pL/(1-p)); the band is DIRECTIONAL; the axis law stays pinned by test_econ_scale, test_fraud_amounts, and the class-S sub-gate | MEASUREMENT (statistical tolerance) | observed 2.418 post-H3-part-1 vs CPI 1.877 |
#### U-8 ADDENDUM. membership, replenishment, closure (macro-history-2026-07h)

Code-facts and declarations recorded at the H3 arc close (part 3c-ii
executed; contract docs/h3_mortality_estate.md; gates
tests/test_membership.cpp, serverless 44).

| PL value | Class | Suggested source |
|---|---|---|
| MEMBERSHIP INTERVAL [joinTs, closeTs): joinTs = window start for the seed roster and a drawn join day for the join cohort; closeTs = death + 120-day settlement (`pii::kSettlementDays` — sized to strictly contain the funeral (death+3-11d) and the estate distribution (death+30-90d), so every estate row is corpus-visible before the accounts close); the STANDARD exporter's visible corpus filters every row on BOTH endpoint owners' intervals (the pre-existing join filter gained the close bound); the aml / aml_txn_edges / mule_ml / card_fraud corpora remain FULL-WORLD exports (declared) and reflect lifecycle through values: the AML Customer status cell flips active -> closed when the corpus end reaches closeTs, and standard customer.csv gains a closed_at column (empty while the account is still open at export) | CHOICE (owner directive: the population must both persist AND die) | estate-administration practice; deposit-account closure norms |
| JOIN-COHORT REPLENISHMENT sizing: joinerCount = population x SUM over window days of r(year(day)) / 365.2425 (linear day-weighted, declared), where r(y) = pop(y+1)/pop(y) - 1 from the EMBEDDED BEA NIPA population series (era_data.hpp, U-4/U-5 lineage); RATE-CLAMPED at coverage edges — a frozen year reads the LAST MEASURED year-over-year rate, the rate-axis analog of the H1 level freeze (never extrapolated, never wall-clock); joiners are the LAST K person ids, so the seed roster's dob/timeline/mortality draws are byte-identical; the flat 2%/yr `pii::Growth` model is RETIRED | MEASUREMENT (series) + CHOICE (the bank's customer base tracks resident-population growth; per-bank customer-acquisition series would be a registered upgrade) | BEA NIPA population (embedded era series); U.S. Census population estimates |
| JOIN-DAY distribution: exactly ONE draw per joiner on the isolated {"join-cohort", personId} lane — inverse-CDF over per-day weights proportional to r(year(day)), so high-growth years recruit proportionally more; entity N cannot move entity N+1; a second draw later is an explicit model change | INVARIANT (draw discipline) + CHOICE (growth-proportional placement) | — |
| JOINER AGE AXIS ERROR closed: a joiner's dob, persona timeline, and lifespan now anchor at the JOIN DATE through the single-age carrier (a 2015 joiner draws 2015-appropriate ages; the seed-consistency clamps and the alive invariant bind at join — death lands strictly after joining); previously every joiner aged as of sim start | MEASUREMENT (axis repair on the U-7/U-8 lineage; no model parameter moved) | model consistency |
| ACCOUNT CLOSURE ends the contractual line: subscription debits, insurance premiums, and loan/tax obligation events stop at closeTs via emission-side filters placed AFTER the sites' existing draws burn (shared rng streams byte-identical — only rows and their screen postings disappear); CARD servicing stops at the last statement close at least 50 days before closeTs (`kCardSettleTailDays` — covers grace 25d + late-payment tail 20d + the fee morning, so no session row posts at/after closure; per-card isolated rng lanes keep every other card byte-identical); insurance CLAIMS stop at DEATH, not closure (claim filing is behavioral) via the same post-draw filter | CHOICE (mechanism + declared guards) | card ToS billing-cycle norms; estate-administration practice |
| H1 WIRING DEFECT FIX (subscriptions never scaled in production): the routines DebitEmitter — the ONLY production subscription path (passes::addSubscriptions) — drafted and screened the raw calibration-dollar sub.amount, while the U-6 CPI wiring had landed in the parallel channels emitter that production never calls; test_econ_wiring's calibration gate could not see it (it pins 2019 rows, where scale == 1.0 makes scaled and unscaled identical). FIXED: screen and draft both realize sub.amount x priceScale(debit month) — the U-6 row's stated behavior; pinned by the deflated pair-ratio gate in test_membership | MEASUREMENT (correctness repair on the U-6 lineage; no model parameter moved) | code-fact; tests/test_membership.cpp |
| FRAUD-SCHEDULING INTERVALS (rings never recruit the dead): each ring plan carries the MINIMUM death epoch over its fraud + mule participants; the ring's typology bursts AND its camouflage window clamp to that alive horizon minus a 22-day schedule guard (`kRingScheduleGuardDays` — the invoice typology's weekly lattice can emit up to 21 days past its base range; every other typology's tail padding already contains its bursts); VICTIM accounts are exempt from the minimum (fraud against deceased persons' accounts is a real, documented typology) and the solo/unauthorized rail is exempt under the same declaration | TYPOLOGY (deceased-victim fraud) + CHOICE (guard) | ring-operation windows in AML case literature; deceased-identity fraud advisories |
| AML onboarding date vs joinTs: the aml / aml_txn_edges Customer onboarding stays the synthetic backdated derivation (identity::onboardingDate) while standard customer.csv and the card_fraud Party table export the membership joinTs — a declared inconsistency; aligning AML onboarding to the membership axis is REGISTERED | MEASUREMENT (code-fact, declared) | — |
### U-9. macro-history-v1 H4: macro modulation — the real consumption level (macro-history-2026-07i)

Model choices adopted 2026-07-26 (contract docs/h4_macro_modulation.md;
owner decisions 1-4). Recorded at H4 step 1 (the primitive lands,
pinned by test_econ_scale, UNREAD by generation); the step-2 wiring
round adds the consumers, the wiring gates, and the four-golden
re-pin.

| PL value | Class | Suggested source |
|---|---|---|
| THE REAL CONSUMPTION LEVEL: `pceScale(year)` = per-capita nominal PCE index over the calibration year; `realPceLevel(year)` = pceScale/priceScale — the measured REAL per-capita consumption path (~0.67 at 1991, exactly 1.0 at 2019), carrying the measured level dips (1991, 2008-09, the 2020 collapse + 2021 rebound; the 2001 recession slowed growth without a per-capita consumption dip — the series says so and the model inherits it) at annual resolution; level-anchored and freeze-and-declare exactly like the H1 scales (frozen years hold the last measured level; never extrapolated, never wall-clock) | MEASUREMENT (the embedded series) + CHOICE (level definition) | BEA A794RC per-capita PCE (embedded era series, U-4/U-5 lineage) |
| THE CHANNEL (owner decision 1): real consumption modulates the discretionary session's transaction COUNT axis only; ticket AMOUNTS stay exactly as U-6 wired them (calibration draw x priceScale). The quantity axis carries the real growth (per-capita payment counts grew on this order over the era — Fed Payments Study lineage); the fraud budget F = pL/(1-p) rides the candidate count L, so fraud DENSITY stays proportional across eras with no fraud-side wiring; the CPI ticket band in test_econ_wiring stays correct as written and becomes the channel-separation pin. A mixed count/amount split is REGISTERED (no anchor for early-era ticket sizes exists in the catalog) | CHOICE (owner-adopted 2026-07-26) | Fed Payments Study per-capita noncash counts; U-6 denomination law |
| BUDGET SEMANTICS: the session's window budget (targetTotalTxns) is denominated at the CALIBRATION LEVEL; realized per-year volume = target x realPceLevel(year) — a 2019 window reproduces today's volumes exactly, a 1991 window runs at ~0.67x. The factor is ONE pure lookup per day frame (the dayPriceScale_ pattern; both engines share the code, oracle parity automatic); NO draws, NO lanes, NO CLI | INVARIANT (level anchoring) + CHOICE | model consistency with the H1 scales |
| SCOPE (owner decision 2): the discretionary spending session ONLY. Wages/revenue/benefits already ride AWI (the labor axis is measured once); rent/subscriptions/premiums/obligations/card terms are CONTRACTUAL (their era axis is the price level they already carry). ATM withdrawal cadence and the cash-vs-card mix are DECLARED era-flat (real cash usage declined over the era — a cash-share era model is REGISTERED); family gift cadences likewise declared era-flat | CHOICE (declared simplifications) | — |
| UNEMPLOYMENT (owner decision 3): DECLARED, not modeled — the demand side already carries the downturns through the PCE series at the same annual resolution; a labor-market separation-spell model (recession job loss interrupting payroll) is REGISTERED, as is within-year NBER recession shading (the 8 peak/trough dates would be a small constexpr table with NBER provenance). The U-3 series stays embedded + exported | CHOICE (declared) | BLS U-3 (embedded); NBER business-cycle dating (registered) |
| COVID/EIP (owner decision 4): the 2020 collapse and 2021 rebound ship FREE through realPceLevel (the COVID axis facts are pinned in test_econ_catalog); the three Economic Impact Payments (CARES Apr 2020 $1,200/adult; Dec 2020-Jan 2021 $600; ARPA Mar 2021 $1,400) are REGISTERED as a future class-S statutory table — the canonical card-fraud window ends 2020-01-01, so no current probe config reaches the EIP dates | CHOICE (owner-adopted; statutory amounts fixed-nominal when wired) | CARES Act / CAA 2021 / ARPA statutes (registered) |
| HARNESS DRAIN (analysis): the ~27% deflated year-over-year drain in 300-person second-year gate legs is a SMALL-WORLD BUDGET ARTIFACT (income under-provision -> declining balances -> liquidity suppression), predating every macro-history round; production populations do not share the geometry. H4 gates are cross-era ratios of same-position years so the drain cancels; a gate-world income/spending budget calibration round is REGISTERED | MEASUREMENT (harness fact) | test_econ_wiring drift diagnostics |
| GATE AMENDMENT (declared for the step-2 wiring): test_econ_wiring's DRIFT PARITY gate divides out realPceLevel (parity in calibration-level units) — under H4 the era-equal-real-growth assumption it encoded is intentionally false (1991->1992 real growth is positive, 2019->2020 negative). New wiring gates: volume band (1991/2019 per-capita session counts ~0.67 +-15%), ticket band UNCHANGED (channel separation), calibration identity, recession direction (2009 below 2008 in calibration-level units), fraud proportionality (fraud count ratio tracks L) | MEASUREMENT (statistical tolerance) + INVARIANT | — |

#### U-9 ADDENDUM. H4 step 2 — the wiring as BUILT and MEASURED (macro-history-2026-07j)

Recorded after the step-2 wiring round landed and the owner re-pinned
all four goldens. Two rows amend the step-1 U-9 plan; the rest are the
verification record.

| PL value | Class | Suggested source |
|---|---|---|
| THE SEAM AS BUILT: `SpenderEmissionLoop::RateSampler` gains `dayRealLevel_` = realPceLevel(year of the day frame's start), computed ONCE per day frame in the constructor initializer list beside `dayPriceScale_`, and multiplied into `combinedMultiplierFor()` alongside `frame_.seasonalMult`. Two files (loop.hpp, loop.cpp); both engines share the sampler, so windowed==monolith oracle parity is automatic and needed no separate gate. NO draws moved, NO lanes, NO CLI. The calibration year is an IEEE x1.0 no-op, so 2019-start worlds sample BIT-IDENTICALLY to the unmodulated loop | MEASUREMENT (code fact) | this repository |
| AMENDS the step-1 plan — RECESSION-DIRECTION GATE SUBSUMED, NOT BUILT: the planned "2009 below 2008 in calibration-level units" leg was dropped. At annual resolution the 2008-09 per-capita real consumption dip is 1-3%, while the harness's own second-year liquidity drain is ~27% at N=300 — the gate could not have distinguished signal from harness. The dip DIRECTION stays pinned at the primitive (test_econ_scale: realPceLevel(2009) < realPceLevel(2008)) and the count-axis TRANSMISSION is pinned by the 33% volume gate; their composition IS the recession behavior. Follows the standing lesson: do not gate what the harness cannot resolve | CHOICE (declared deviation) + MEASUREMENT (harness resolution) | test_econ_wiring drift diagnostics |
| AMENDS the step-1 plan — VOLUME GATE FORM: realized as an ANCHOR-YEAR comparison (year 1 of each leg — 1991 vs 2019 legit session ticket COUNTS) rather than a window-integrated per-capita ratio. Same population and the same harness position on both sides, so the drain cancels cleanly. Band +-15% around realPceLevel(1991)/realPceLevel(2019) = 0.668. OBSERVED 0.7049: the realized ratio sits ~5% ABOVE the pure level because a quieter 1991 session drains its balances more slowly and the liquidity multiplier feeds back — a model property of the coupled system, not a wiring error, and the reason the band is sized at +-15% | MEASUREMENT (statistical tolerance) + INVARIANT | test_econ_wiring |
| SUPERSEDES the U-6 acceptance row "deflated per-active spend ~flat across a multi-year leg (macro modulation is H4)": under H4 that flatness assumption is intentionally false, since era year-pairs have different measured real growth (1991->1992 positive, 2019->2020 negative through COVID). The gate now compares y/y spend in CALIBRATION-LEVEL units (nominal total / (priceScale x realPceLevel)) across eras. OBSERVED parity 0.927 inside the unchanged 0.80-1.25 band; the harness drain still cancels in the cross-era division | MEASUREMENT (code fact) | test_econ_wiring |
| VERIFICATION RECORD (300-person 730-day legs at 1991 and 2019, all gates green): session volume ratio 0.7049 (vs R 0.668); ticket mean ratio 1.845 (vs CPI 1.877) and salary mean ratio 2.395 (vs AWI 2.480) — BOTH UNMOVED by the count modulation, which is the channel-separation evidence; fraud-rides-L parity 0.9504 (flagged-row count ratio over legit-row count ratio), confirming the injector's budgets chain off realizedBaseCount and not off population or window constants; calibration-level drift parity 0.927; ring-rail fraud mean ratio 2.012, which moved AWAY from the 2.6 band edge that H3 had approached at 2.418 | MEASUREMENT (observed) | test_econ_wiring |
| FREEZE DIRECTION (declared, and it points UP): outside coverage the activity level freezes with the price scales, and realPceLevel(2024) = 1.0915 — a default 2025-start window runs ~9% ABOVE calibration volume, not below. Measured corpus effect: the standard golden (2025 start, 60d, pop 2000) grew 184,988 -> 197,245 rows (+6.63%), which +9.15% applied to the session-routed share of the stream reproduces. The app's existing frozen-era stderr notice already covers these windows | MEASUREMENT (observed) + INVARIANT (freeze-and-declare) | BEA A794RC / CPI-U embedded series |
| RUN-PLANNING COROLLARY: any window STARTING before the calibration year now emits FEWER session rows than pre-H4 (the canonical 1991-start card-fraud run integrates realPceLevel ~0.67 -> 1.0 across its years; a short 1991 leg is ~33% lighter on the session rail). The fraud COUNT RATE is preserved by F = pL/(1-p) riding realized L. Restoring the IBM 24.4M-row comparison anchor exactly is therefore a RUN-TIME population/window decision, never a code change | MEASUREMENT (model consequence) | docs/h4_macro_modulation.md |
