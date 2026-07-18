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
model version **conformance-statutory** — see the F-6 C1 block and the
change history.

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
| Repeat victimization | p 0.10 | MEASUREMENT | FTC Sentinel; revictimization literature |

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
| Cycle / boost-cycle amounts | LN($600, .25) / LN($500, .20) | CHOICE | — |
| Card-test charge | U[$0.50,$5.00], ~40% anchors {.50,1,2,5} (test-pinned) | MEASUREMENT | Nilson; issuer advisories |
| Card fraud spend | median ≈ $79, mean ≈ $162, clamp [$1,$5k] (test-pinned; PER TRANSACTION — see the C0 axis flag below) | MEASUREMENT | Fed Payments Study; UK Finance |
| ATO drain | median ≈ $180, mean ≈ $554, clamp [$10,$85k], ~0.4% ≥ $10k (test-pinned; PER DRAIN TRANSACTION — case unit written by C0 below) | MEASUREMENT | FTC Sentinel; IC3 |

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
  per-case.)*
* ATO drain | Citation: same report. | Real-world value: remote
  banking fraud GBP 104.4M across 37,646 cases = ~GBP 2,773 (~$3.5k)
  average per CASE [Certain, Derived]. | Status: UNIT CHECK REQUIRED
  before a verdict: PL's mean $554 is per drain transaction; if a
  modeled ATO case executes ~5-7 drains the case totals reconcile
  with the UK average. Read the code, write the per-case expectation
  into the row, then set CONFORMS or ADJUST. Do not compare per-txn
  to per-case again.
* Card-test charge | Status: UNCITED, low risk: sub-$5 authorization
  testing is qualitatively described in issuer and network advisories
  [Likely]; pull one named advisory (Visa card-testing bulletin) for
  the citation slot. The pattern, not the exact bounds, is the claim.

**C0 code reads (F-4, 2026-07-18):**
* ATO drain UNIT (closes the Pass 2 unit check) | One ATO case = one
  `CompromisePlan` executing `plan.targetEvents` drain transactions
  (`transfers/fraud/typologies/unauthorized.cpp`). The sampler
  (`injector.cpp` budget-split loop): rail is card at p .72;
  NON-CARD (bank-drain) plans draw targetEvents ~ U{2..5}, mean 3.5,
  each event an atoDrainAmount draw (mean $554) ⇒ implied per-case
  mean ≈ 3.5 × $554 ≈ **$1.9k vs the UK remote-banking ~$3.5k per
  case** [Derived] — same order of magnitude, low side (~55%).
  Owner call (rides with C3): CONFORMS-as-band, or ADJUST (raise
  non-card targetEvents toward ~4–7 or the drain median).
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
* CTR trigger | Both statutory defects were confirmed in code
  (`derived.cpp TxnSweep::observe`: `>= 10000.0`, no channel filter)
  and FIXED by C1 — see the F-6 C1 block.

### F-5. Camouflage

Small P2P p .03/day; monthly bill p .35; salary inbound p .12 — CHOICE
(+ planned separability measurement).

### F-6. Detection & label layer

| Parameter | PL value | Class | Suggested source |
|---|---|---|---|
| Below-CTR alert band | [$9,000, $10,000] → sev 2, ALL channels (upper edge inclusive since C1, so exactly-$10,000 lands here) | CHOICE | FFIEC; TM vendor catalogs |
| CTR record | STRICTLY > $10,000 AND currency channel (`channels::isCurrency`: atm_withdrawal, fraud_structuring) → sev 3 + CTR row (C1) | MEASUREMENT (statutory) | 31 CFR §1010.311 |
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
SHIPPED:**
* Both statutory defects are FIXED to the cited 1010.311 text: the
  CTR (alert + row) fires on **strictly more than $10,000** and
  **only on currency channels** — `channels::isCurrency` in
  `taxonomies/channels/predicates.hpp` = {`Legit::atm`
  ("atm_withdrawal", cash-out) and `Fraud::structuring` (structured
  cash deposits; structuring is definitionally a currency offense,
  31 USC 5324)}. No other tag in the taxonomy models physical cash
  (everything else is ACH/card/wire/check-like); widening the set is
  a model-version decision recorded here first.
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
  the two statutory pins: an exactly-$10,000.00 currency row files
  nothing; a $15,000.77 merchant row files nothing; exactly the two
  handcrafted currency rows above $10k file).
* KNOWN SIMPLIFICATION (logged, unmodeled): same-business-day
  currency aggregation, 31 CFR 1010.313(b) — PL files
  single-transaction CTRs only, so a structurer's five same-day
  $2,500 cash deposits do not aggregate into a CTR.
* VOLUME CONSEQUENCE (expected, not a defect): at the pinned fraud
  config the CTR table collapses from 340 rows to ~0. Every prior
  CTR was a NON-currency ≥$10k row (dominated by the salary tail —
  LN($4,500,.55) puts ~7% of salary credits above $10k — plus
  inheritance/invoice/layering tails), i.e., pure manifestations of
  the scope defect. PL currently models NO legitimate large-currency
  behavior (business cash deposits — the source of the real ~20.5M
  CTRs/yr). CTR label liveness is therefore gated on a FUTURE model
  version adding legit cash-deposit behavior, calibrated against the
  FinCEN FY2024 anchors (20.5M CTRs / 4.7M SARs, ratio ~4.4). Until
  then the near-empty CTR table is the statutorily correct output.
* Status after C1: the F-4/F-6 CTR rows conform to the cited
  1010.311 text (boundary + scope); owner confirms CONFORMS at the
  re-pin commit.

### F-7. Measured emergent properties (from generated corpora)

Threshold splits below alert band 0.375 (analytic 0.345); posted
structuring mix 15/32/53 vs sampler 60/25/15 (unfunded victim debits
bounce at clearing — emergent); SAR filing observed 2/9 and 0/2 groups;
fraud rows 1,769 / candidates L=909,116 (0.19%) at pop 10k/60d/seed 7.
(CTR count at this config pre-C1: 340, all non-currency — see the F-6
C1 block; re-measure at the C1 re-pin.)

═══════════════════════════════════════════════════════════════════════
# PART II — LEGITIMATE ECONOMY
═══════════════════════════════════════════════════════════════════════

### L-1. Population & personas

Shares (CHOICE — verify demographic context): salaried .60, student
.12, retiree .10, freelancer .10, smallBusiness .06, highNetWorth .02.

| Persona | rate× | amt× | timing | init bal | cardP | ccShare | limit | weight | paySens |
|---|---|---|---|---|---|---|---|---|---|
| student | 0.7 | 0.7 | consumer | $200 | .65 | .55 | $800 | .18 | .67 |
| retiree | 0.6 | 0.9 | consumerDay | $1,500 | .84 | .55 | $2,500 | .30 | .50 |
| freelancer | 1.1 | 1.1 | consumer | $900 | .88 | .65 | $4,000 | .95 | .33 |
| smallBusiness | 1.2 | 1.4 | business | $8,000 | .95 | .75 | $7,000 | 1.50 | .29 |
| highNetWorth | 1.3 | 2.8 | consumer | $25,000 | .98 | .80 | $15,000 | 2.20 | .11 |
| salaried | 1.0 | 1.0 | consumer | $1,200 | .88 | .70 | $3,000 | 1.00 | .40 |

`cardP` gates CREDIT-card issuance specifically (`synth/cards/
issue.hpp` coins `persona.card.prob`); `ccShare` = credit share of
spend; `limit` = credit limit (C0 code read, 2026-07-18).
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

### L-2. Spending engine

| Parameter | PL value | Class | Suggested source |
|---|---|---|---|
| Transaction load | 40 txns/person/month | MEASUREMENT | Fed Diary of Consumer Payment Choice |
| Daily counts | gamma-Poisson k=1.5; weekend ×0.8; day shock Gamma(shape 1.3, scale 1/1.3) — unit mean, fatter tail at lower shape (`actors/day.cpp`, C0) | TYPOLOGY | payment-count dispersion literature |
| Slot mix | merchant .82 / bills .10 / p2p .08 around external .05 ⇒ effective 77.9/9.5/7.6/5.0 (matches run-log attempt shares) | MEASUREMENT | Fed Diary payment purposes |
| Known-biller preference | .55; merchant retry limit 6; pick attempts 250 | CHOICE | — |
| Exploration | base .02/txn; per-person propensity Beta(1.6, 9.5); burst p .08 for 3–9 d | CHOICE | — |
| Seasonality (unit mean) | Jan .88, Feb .94, Mar 1.04, Apr 1.02, May 1.00, Jun .98, Jul .97, Aug 1.05, Sep 1.02, Oct .99, Nov 1.16, Dec 1.22 | MEASUREMENT | Census monthly retail sales |
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
  the row, not the Pass 1 one.
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
* P2P amount | Real-world value: Diary "A person" average $181/txn;
  mobile-app payment average $71.9/txn (Table 8) [Certain]. PL
  LN($45,.80) has mean $62, low against both. | Status: DEVIATES,
  owner call: raise median toward $55-70 (app-like) or document the
  small-social-payment choice.
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

### L-3. Amount catalog (LN = lognormal(median, σ); Γ(shape, scale)+add)

| Channel | PL model | Class | Suggested source |
|---|---|---|---|
| Salary (monthly) | LN($4,500, .55) floor $50, ×12 annual | MEASUREMENT | BLS OEWS median wages |
| Rent | Γ(2, 400)+$50 (mean $850) | MEASUREMENT | Census/HUD median gross rent (model likely LOW — the pass decides) |
| P2P | LN($45, .80) | MEASUREMENT | Fed Diary P2P |
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

Merchant tickets LN(median, σ): grocery 50/.55, fuel 45/.35, restaurant
28/.60, pharmacy 25/.65, ecommerce 85/.70, retailOther 45/.75,
utilities 120/.40, telecom 75/.30, insurance 150/.35, education
200/.60; default 45/.70. MEASUREMENT — Fed Diary / BLS CE per-category
average tickets.

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

### L-4. Income & employment

| Parameter | PL value | Class | Suggested source |
|---|---|---|---|
| Employment probability | salaried .98, student .12, retiree .02, freelancer .08, smallBiz .04, HNW .12; overall fit .95 of eligible | MEASUREMENT | BLS employment-population ratios |
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

### L-5. Housing

Lease tenure 1–3 y; on move: new landlord, fresh base rent (jitter LN
σ.05); growth 2.5% inflation + real N(2.0%, 1.5%) floor −1%/y (applied
once per lease anniversary ≈ 4.5%/yr nominal — mechanics in the L-3 C0
block). MEASUREMENT — CPS renter turnover ~15–22%/y (the overall CPS
mover rate was 11.0% in 2017 and has run single-digit %/y in the
2020s; the stale "~13%/y" parenthetical is removed per Pass 2), CPI
rent index.

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

### L-6. Recurring debits

| Routine | PL value | Class | Suggested source |
|---|---|---|---|
| Subscriptions | 4–8 candidates/person, 55% become debits; 18-point pool $6.99–$99.99 (6.99, 7.99, 9.99, 10.99, 11.99, 12.99, 14.99, 15.49, 15.99, 17.99, 22.99, 24.99, 29.99, 34.99, 39.99, 49.99, 59.99, 99.99); day U[1,28] | MEASUREMENT | consumer subscription surveys |
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
| Ownership / share / limits | per persona (L-1); cardP = strictly CREDIT-card issuance (C0) | MEASUREMENT | Fed SCF; CFPB CCM |
| Grace period | 25 days | MEASUREMENT | CARD Act ≥21 d; issuer norms |
| Minimum payment | max(2%, $25) | MEASUREMENT | issuer norms |
| Late fee | $32 | MEASUREMENT | CFPB late-fee data (verify current rule status) |
| Payment mixture | full .35 / partial .30 / minimum .25 / miss .10; partial fraction Beta(2,5) of statement | MEASUREMENT | CFPB CCM payment-behavior distributions (transactor/revolver) |
| Payment timing | late p .08, 1–20 d late | MEASUREMENT | issuer delinquency curves |
| Disputes | refund p .006/purchase (1–14 d); chargeback p .001 (7–45 d) | CHOICE (re-classed per Pass 2) — e-comm dispute benchmark ~0.6% of txns; PL runs 0.1% blended all-channel by choice (the old "(~0.05–0.1%)" parenthetical misstated published benchmarks) | Visa/Mastercard monitoring thresholds (VAMP 1.5% eff. Apr 2026; legacy 0.9%/0.65%) |
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
* Ownership / share / limits | Citation: same tables, Table 3. |
  Real-world value: credit card adoption 82.3% of consumers, debit
  90.3% (2024) [Certain]. | Status: partial CONFORMS: PL's weighted
  cardP ~ .85 sits between debit and credit adoption; acceptable if
  cardP means "has any payment card", slightly high if strictly
  credit. Limits and balances remain UNCITED (SCF 2022, queued).
  *(C0 code read settled the definition: STRICTLY credit — see the
  L-1 block. The "slightly high" branch applies.)*
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
25), home $163/.30 (25), life $28/.40 (5); claims: auto 4.2%/y →
payout LN($4,700,.80) floor $500; home 5.5%/y → LN($15,750,.90) floor
$1,000.
**Tax** — adoption .05/.20/.65/.85/.50/.10; quarterly LN($1,250,.65)
floor $100; filing: refund 65% LN($1,850,.55) / balance due 20%
LN($1,100,.65).
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
* Insurance claims | Citation: III/ISO: 5.3% of insured homes filed
  a claim (2023); average home claim severity $18,311 (2022), >$17k
  recent five-year [Certain]. | Status: home frequency CONFORMS (PL
  5.5%); home severity DEVIATES-HIGH: PL lognormal implies mean ~
  $23.6k vs measured ~ $17-18k averages, trim sigma or median. Auto
  frequency 4.2%/yr and payout mean ~ $6.5k are consistent with
  collision claim frequency ~ 5-6 per 100 car-years and severity ~
  $5.7-6.6k [Likely]; verify against ISS/III auto tables before
  flipping to CONFORMS.
* Tax | Citation: IRS filing season statistics; Tax Foundation
  tracker. | Real-world value: 64.1% of 2024 returns and ~ 63% of
  2025 returns received refunds; average refund $3,167-3,170 (2025
  season) and $3,462-3,521 (2026 season, inflated one year by OBBB
  withholding lag) [Certain]. | Status: refund share CONFORMS (PL
  .65). Refund amount DEVIATES-LOW: PL implied mean ~ $2,152 is ~
  32% under the 2025 average; no official median exists, so either
  ADJUST the median toward ~ $2,300-2,600 or document that PL
  targets a median below the published mean by construction.
* Mortgage/auto/student delinquency ladders | UNCITED still; the
  right comparators are MBA NDS (mortgage 30+ ~ 4% band), NY Fed CCP
  transition rates, and FSA delinquency stats; queued as a
  unit-mapping exercise (PL's per-payment lateness vs 30/60/90-day
  buckets do not map one-to-one).

### L-9. Family transfers

| Flow | PL value | Suggested source |
|---|---|---|
| Spousal | 60% separate accounts; 2–6 txns/mo; breadwinner-directional 65%; LN($85, .90) | Fed SHED |
| Parental support | 35% of eligible; Pareto(xm=$25, α=2.4)/txn | Fed SHED; AARP |
| Allowances | weekly 70% (else monthly); Pareto($35, 2.2) | T. Rowe Price kids-and-money surveys |
| Sibling transfers | 15% pairs active; 18%/mo; LN($120, .90) | — |
| Grandparent gifts | 8%; LN($150, .70) | — |
| Parent gifts | 12%; Pareto($75, 1.6) | — |
| Tuition | 65% of students; 4–5 installments; LN(e^8.95 ≈ $7,712, .35) each | College Board Trends in Pricing |
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
* Parental support, sibling, grandparent, parent gifts, inheritance,
  external recipients | UNCITED; see REMAINING OPEN ITEMS. Fed SHED
  has qualitative family-support incidence; no clean per-transfer
  distributions exist, so expect these to end as CHOICE rows with
  incidence-only citations.

### L-10. Business / freelancer revenue (per-persona monthly profiles)

**Freelancer** — clients: active .88, 2–5 counterparties, 1–4
payments/mo, LN($1,400,.70); platforms: .42, 1–2, 1–4, LN($425,.60);
owner draw: .70, 1–2, LN($1,800,.75); quiet month p .12 (activity ×.40
via skip .60).
**Small business** — clients: .55, 2–6, 0–3, LN($2,600,.75); platforms:
.22, 1–2, 0–3, LN($950,.70); card settlements: .74, 4–12/mo,
LN($680,.55); owner draw: .86, 1–2, LN($3,400,.70); quiet month .06.
**High net worth** — owner draw .55, 1–2, LN($6,000,.65); investment
inflows .72, 1–3, LN($12,000, 1.0); quiet month .02.
**Retiree** — draw-like income .33, 1, LN($1,100,.50); investment .50,
1–2, LN($400,.65); quiet month .05.
Class MEASUREMENT-adjacent. Sources: freelance-platform earning
studies; SBA/Intuit small-business cash-flow data.

### L-11. Population scaffolding

Accounts per person: 1 + Binomial(2, .25) — mean 1.5, max 3
(MEASUREMENT — Fed SCF accounts-per-household). Merchants: core
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
guessed.

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

### conformance-statutory (SHIPPED 2026-07-18)

The C1 batch of the conformance program; the table golden's REAL
promotion trial (fraud-audit-2026-07's was vacuous). CTR trigger
brought to the cited 31 CFR 1010.311 text: fires on STRICTLY MORE
THAN $10,000 (was >=) AND only on currency channels — new
`channels::isCurrency` predicate {atm_withdrawal, fraud_structuring}
in `taxonomies/channels/predicates.hpp`, applied in
`derived.cpp TxnSweep::observe` (was channel-blind: salary/ACH/card/
wire rows filed CTRs; the salary tail alone put ~7% of salary credits
over $10k). Sev-2 band extended to [$9,000, $10,000] INCLUSIVE so
exactly-$10,000.00 alerts at sev 2 and files nothing; band stays
all-channel by CHOICE. Read-back decode contract extended: the
transactions-table scan decodes `channel` losslessly
(`channels::parse` inverts `channels::name`); `test_pg_readback` pins
the round-trip, `test_derived_readback` pins corpus/readback parity
plus the boundary and scope negatives. Same-day aggregation
(1010.313(b)) stays unmodeled — known simplification. Expected golden
effect: ONLY `tests/golden_tables_aml.md5` (fraud section) re-pins —
corpus digest unchanged; Alert/CTR/Disposition vertices and
ALERT_ON/DISPOSITIONED_AS/FILED_CTR/CONTAINS_ALERT/ESCALATED_TO edges
diff; CTR row count collapses 340 → ~0 (statutorily correct — legit
large-cash behavior is unmodeled; future model version, FinCEN
FY2024 anchors). Standard-section golden and the CSV golden must NOT
move.

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
| 2026-07-18 | Student employment .12 vs BLS/NCES ~40-45% | NONCONFORMING | ADJUST or persona redefinition |
| 2026-07-18 | Allowances Pareto($35,2.2) vs $13-17/wk platform data | NONCONFORMING (floor exceeds measured averages) | ADJUST proposal in L-9 block |
| 2026-07-18 | Lease tenure 1-3y vs renter turnover 15-22%/yr | NONCONFORMING (~2.5-3x too fast) | ADJUST or CHOICE re-class; fix stale ~13% parenthetical |
| 2026-07-18 | Card fraud spend vs UK Finance 2025 per-case avg | CONFORMS (PL mean $162 vs ~$167) | none |
| 2026-07-18 | Fuel ticket LN($45,.35) vs Diary gas avg $32.8 | NONCONFORMING (~46% high) | ADJUST fuel median toward $32-38 |
| 2026-07-18 | Seasonality Dec/Jan amplitude 1.39 vs Census NSA ~1.22 | shape ok, amplitude wide | damp tails or CHOICE re-class |
| 2026-07-18 | C0 code reads executed | CTR defects CONFIRMED in code; rent CONTESTED → NONCONFORMING (sole tenant, household axis); cardP = strictly credit; refund = merchant credit; ATO unit = CompromisePlan × targetEvents; cosmetic queue closed | C1 shipped; rent ADJUST queued C2 |
| 2026-07-18 | conformance-statutory (C1) | CTR strict-> + currency scope {atm_withdrawal, fraud_structuring}; band → [$9,000, $10,000]; readback decodes channel | fraud golden re-pin; CTR volume note in F-6 C1 block |
| 2026-07-18 | Card fraud spend AXIS | Pass 2 CONFORMS compared per-TXN $162 to UK per-CASE ~$167 — mismatch; PL per-case ≈ $1.2–1.5k (targetEvents U{5..14}) ≈ 8× UK | re-opened; owner call rides with C3 |
| 2026-07-18 | ATO per-case ≈ $1.9k (targetEvents U{2..5}) vs UK ~$3.5k/case | same order, low side (~55%) | owner: CONFORMS-as-band or ADJUST; rides with C3 |

## REMAINING OPEN ITEMS (after Pass 2 + C0/C1, 2026-07-18)

Everything below is either a code-reading task, a definition task, or
a thin-tail row with no strong published comparator. Nothing here is
a known numeric contradiction; all known contradictions are already
logged as NONCONFORMING above.

1. Code reads: DONE except one — the renter-selection share (who
   rents at all), queued with the C2 rent ADJUST. Resolved by C0:
   CTR currency-only scope (shipped in C1); ATO drain unit +
   targetEvents sampler (F-4 C0 block); rent debits per tenancy (L-3
   C0 block — decided the CONTESTED verdict); cardP strictly-credit
   (L-1); refund-row semantics (L-7 C0 block).
2. Definition writes still open: spousal-separate definition; tuition
   COA-vs-net definition; subscription comparator source (name one —
   PL sits closest to Bango 2025); repeat-victimization window (usual
   definition: same fraud type within 12 months); accounts-per-person
   scope (checking-only vs checking+savings).
3. Citations to pull verbatim at ADJUST time: eCFR section text
   snapshots; FATF Professional ML (2018) page cites; a named Visa
   card-testing advisory; FinCEN structuring guidance and FFIEC
   manual pages; ISS/III auto claim frequency-severity table; FSA
   portfolio IDR shares; BLS Employee Tenure 2024 exact figure; OEWS
   May 2025 refresh; Atlanta Fed tracker current print; Diary
   cash-withdrawal (not payment) statistics for the ATM amount row.
4. Thin tail, expect CHOICE outcomes: L-9 parental/sibling/
   grandparent/gift/inheritance distributions (SHED gives incidence
   only); L-10 freelancer/small-business revenue profiles (platform
   earning studies are non-comparable); L-11 merchant/landlord/
   counterparty densities (re-classed CHOICE 2026-07-18).
5. Funnel calibration (F3 finding): fit SAR p and alert-to-case
   against the FinCEN FY2024 anchors (4.7M SARs, 20.5M CTRs, fraud
   52%) as sanity bands under deliberate oversampling. Note the C1
   volume consequence: CTR count is ~0 until a legitimate
   cash-deposit behavior ships (its own future model version).
6. Owner calls riding with C3 (card behavior): payment mixture (L-7);
   cardP slightly-high (L-1); card-rail per-case spend axis and ATO
   per-case level (F-4 C0 block).
