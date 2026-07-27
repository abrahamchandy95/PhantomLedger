# PhantomLedger — MASTER CITATION DOCUMENT (Model Ground Truth)

THE ONE DOCUMENT. Every research-sensitive number in PhantomLedger is a
row here: the value the code implements, the claim it makes about the
real world, the citation, and the current status. (Filename is
historical; scope is the whole model.)

**CONSOLIDATED 2026-07-27.** This document was rebuilt from a 2,318-line
predecessor that had accreted four genres: citation tables, per-round
citation-pass logs, a model-version change history, and round-by-round
engineering records (the "U-sections") with their addenda. Only the
citation genre survives. Every row now carries its FINAL verdict rather
than the sequence of verdicts that produced it; the intermediate
archaeology is gone except where a reversal is load-bearing, and those
are collected once in SUPERSEDED CLAIMS at the end. Round-by-round
engineering narrative lives in `docs/card_fraud_v2_roadmap.md`,
`docs/h1_nominal_scale_wiring.md`, `docs/h2_persona_timeline.md`,
`docs/h3_mortality_estate.md`, `docs/h4_macro_modulation.md`,
`docs/card_fraud_victimization.md` and `docs/era_data_provenance.md`.

## THE AUTHORITY RULE (owner directive, 2026-07-18)

This document is NORMATIVE, in two phases per row:

* **UNCITED row:** the value mirrors the code and is provisional. If doc
  and code disagree in this phase, the DOC is wrong — fix the doc.
* **CITED row** (owner has verified a real source): **THE DOCUMENT
  GOVERNS.** If the cited real-world value contradicts the model value
  the row is NONCONFORMING and PhantomLedger is CHANGED TO FIT THIS
  DOCUMENT — always through the model-version pipeline (owner-approved
  ADJUST, one named commit, re-pin of every golden baseline touched),
  never a silent edit.
* **CHOICE rows** are normative too, but their normative content is the
  DOCUMENTED DEVIATION: cite the real-world value, state the deviation
  and its reason. A CHOICE row conforms when the deviation is explicit
  and justified — not when the raw number matches the world.

Classification: **MEASUREMENT** (value should match real data) /
**TYPOLOGY** (shape/structure should match documented patterns) /
**CHOICE** (deliberate deviation, documented) / **INVARIANT** (a
model-consistency law, no external claim). Never conflate prevalence
axes; labels (`is_fraud`, SARs, alerts/CTRs, chain/shell) are calibrated
separately.

Statuses: **CONFORMS** · **DEVIATES-BY-CHOICE** · **NONCONFORMING →
ADJUST** · **UNCITED**. Confidence tags on cited values: [Certain] read
directly from the source; [Likely] strong secondary inference; [Derived]
arithmetic on cited values; [Guessing] recalled, unverified.

**Standing axis discipline.** Three of this document's four reversals
were axis misreads (conditional vs marginal, per-transaction vs
per-case, flow vs stock). Before any verdict, state the axis of the PL
value and the axis of the comparator and check they match.

═══════════════════════════════════════════════════════════════════════
# PART I — FRAUD / AML MODEL
═══════════════════════════════════════════════════════════════════════

### F-1. Prevalence & fraud budget

| Parameter | PL value | Class | Real-world anchor & source | Status |
|---|---|---|---|---|
| Fraud rings per 10k customers | mean 6.0, lognormal σ 0.4 | CHOICE | Real organized-group density per customer base is far lower; PL oversamples for label density. Europol EMMA cycles; UK Finance Annual Fraud Report | DEVIATES-BY-CHOICE |
| Solo fraudsters per 10k | 4.0 | CHOICE | Lone-actor density, oversampled. FTC Consumer Sentinel; FBI IC3 | DEVIATES-BY-CHOICE |
| Max fraud participation / illicit persons | 6% / ≤0.5% of population | CHOICE | Internal caps, no external claim | — |
| Fraud budget p | 0.0012 of TRANSACTIONS (F = pL/(1−p), exact L) | CHOICE | Nilson (US-issued cards $14.32B on $13.007T in 2023 = ~11.0 bp of VALUE [Certain, Derived]; worldwide 6.58¢/$100 in 2023, $33.41B in 2024 [Certain]); Fed Reg II (covered-issuer debit fraud 17.6 bp of value, 2023 [Certain]). **AXIS: PL's 12 bp is a share of transaction COUNT; every published benchmark is value-based. Count-based incidence benchmarks are not published in these sources, so the oversampling factor vs count is UNKNOWN** | DEVIATES-BY-CHOICE |

### F-2. Ring topology

| Parameter | PL value | Class | Real-world anchor & source | Status |
|---|---|---|---|---|
| Ring size | lognormal μ2.0 σ0.7, clamp [3,150], mean ≈ 9.4 | TYPOLOGY | Europol EMMA 7/8/9 publish network-level totals (18,351 / 8,755 / 10,759 mules; 324 / 222 / 474 recruiters), NOT per-ring size distributions; mule-to-recruiter ratios ~23–57 [Certain on totals, Derived on ratios]. The lognormal is a modeling convenience anchored only to qualitative typology | UNVERIFIABLE against published data — DEVIATES-BY-CHOICE |
| Mule fraction of ring | Beta(2,4) → [0.10,0.70], mean ≈ 0.30 | TYPOLOGY | Europol EMMA | as above |
| Mule multi-ring reuse | p 0.06 | TYPOLOGY | Europol EMMA (recurring mules) | as above |
| Victims per ring | lognormal μ3.0 σ0.8, clamp [3,500], mean ≈ 27.7 | TYPOLOGY | IC3 / FTC | UNCITED |
| Repeat victimization | p 0.10. DEFINITION: per victim slot of each ring, p .10 that the slot is filled by a victim of an EARLIER ring (cross-ring reuse); window = the whole simulated period, ≤12 months at standard configs (`synth/people/fraud.hpp Victims::repeatP`, applied in `people/make.hpp`) | MEASUREMENT | Literature reports 10–45% depending on window, fraud type and definition. PL's .10 lands inside almost any band | UNCITED (definition closed; number is low-risk) |

### F-3. Playbook mix (17 playbooks, weights sum 1.00)

classic .12, pureMule .12, placementToIntegration .12 (structuring
.25→layering .55→invoice .20), rapidFunnelMule .10 (.20/.65/.15),
smurfThenLayer .08 (.40/.60), shellLaundering .06 (.65/.35),
pureScatterGather .05, pureLayering / pureFunnel / pureStructuring /
pureCycle / pureBipartite / classicWithLayering /
scatterGatherWithLayering .04 each, bipartiteWeb .03, pureInvoice /
mixingService .02 each. Structuring-phase mass 0.36 (⇒ P(no structuring
per ring) ≈ 0.64).

**Status:** the placement→layering→integration SKELETON is the canonical
FATF three-stage model (fatf-gafi.org; FATF *Professional Money
Laundering*, 2018) [Likely on edition, Certain on framework] —
**CONFORMS**. The 17-weight vector is CHOICE with no external
comparator — **DEVIATES-BY-CHOICE by construction**.

### F-4. Typology structure & fraud amounts

| Parameter | PL value | Class | Real-world anchor & source | Status |
|---|---|---|---|---|
| CTR threshold | $10,000 — files STRICTLY ABOVE, currency-only | MEASUREMENT (statutory) | eCFR 31 CFR §1010.311 (retrieved 2026-07-18): report required for currency transactions of MORE THAN $10,000; same-day aggregation per §1010.313(b) [Certain] | CONFORMS (both defects fixed; see SUPERSEDED CLAIMS) |
| Layering hops | 3–8 | TYPOLOGY | FATF Professional ML (2018) | UNCITED (page cite pending) |
| Structuring ε below threshold | U[$50, $1,500] | TYPOLOGY | FinCEN structuring guidance; FFIEC BSA/AML manual | UNCITED |
| Structuring profile mix | 60% threshold ($8.5k–$9.95k) / 25% medium ($3–7k) / 15% small ($300–1.5k) | CHOICE | FinCEN SAR narratives | — |
| Splits per victim burst | 3–12; burst 3–8 d; sub-burst 1–2 d; 08–22 h; secondary target p .20 | CHOICE | — | — |
| Classic-fraud amount | LN($900, .70) floor $50 | CHOICE | FTC CSN Data Book 2024: overall median individual fraud loss $497 ($500 2021-23); average per loss-report $12,651; 63% of loss reports under $1,000 [Certain]. UK Finance 2026: APP fraud averaged £2,324/case in 2025 [Certain, Derived]. **Target population declared:** money-movement/bank-drain scams (phone-contact median $1,400 in 2022; APP per-case ~$2,950), deliberately above the all-fraud median | DEVIATES-BY-CHOICE (conforms only with that sentence) |
| Cycle amount | LN($600, .25) | CHOICE | — | — |
| Card-test charge | U[$0.50,$5.00], ~40% anchors {.50,1,2,5} (test-pinned) | MEASUREMENT | Sub-$5 authorization testing is qualitatively described in issuer/network advisories [Likely]; the PATTERN, not the bounds, is the claim | UNCITED (low risk; pull a Visa card-testing bulletin) |
| Card fraud spend | median ≈ $79, mean ≈ $162, clamp [$1,$5k]×priceScale (test-pinned; PER TRANSACTION). Per-CASE: targetEvents U{5..14} ⇒ ≈ $1.2–1.5k | MEASUREMENT (per-txn) / CHOICE (per-case) | UK Finance 2026: remote-purchase card fraud £423.5M over 3.2M cases = ~£132 (~$167) per CASE [Certain, Derived]. PL runs ~8× that per case — DELIBERATE: compromise sessions need enough rows for device/IP/burst pattern learning (same label-density rationale as F-1) | per-txn UNCITED; per-case DEVIATES-BY-CHOICE |
| ATO drain | median ≈ $180, mean ≈ $554, clamp [$10,$85k]×priceScale, ~0.4% ≥ $10k (PER DRAIN TRANSACTION). Per-CASE: targetEvents U{3..8} ⇒ ≈ $3.0k | MEASUREMENT | UK Finance 2026: remote-banking fraud £104.4M over 37,646 cases = ~£2,773 (~$3.5k) per CASE [Certain, Derived]. PL ≈ 87% of that | CONFORMS as a band |
| Unauthorized rail mix | card compromise .48 / gift-card scam .12 / impostor push .12 / ATO .28 | MEASUREMENT-adjacent | FTC CSN payment-method report mix. The two authorized rails are weighted EQUALLY: CSN names gift cards the most-REPORTED scam payment method of the era and bank transfers the largest by reported LOSS | UNCITED (verify list) |
| Gift-card scam (victim-AUTHORIZED) | 2–6 cards/case in ONE 1–4 h coached burst; denominations 75% {$100,$200,$500 triple-weighted} else $50–$500 in $10 steps (mean ≈ $339/card ⇒ ≈ $700–2,000/case, test-pinned); retail merchants; channel `card_purchase`; label `scam_gift_card`; NEVER reimbursed; UNGRADED by victim age in both denomination and count | MEASUREMENT-adjacent | FTC gift-card Data Spotlights: most-reported scam payment method for several years; ~$217M reported losses 2023; victims coached to buy multiple max-denomination cards; retailer per-card caps commonly $500; median per-scam losses $500–$1,000 [Likely on vintages] | UNCITED (verify list) |
| Impostor push (victim-AUTHORIZED) | `FraudType::scamImpostor`; 50/50 over `externalUnknown` (wire-shaped) and `p2p`; `scamWireAmount` LN($900, σ1.3) clamp [$50,$50k] × priceScale(era) × age severity; NEVER reimbursed | CHOICE (magnitudes) + MEASUREMENT (order of magnitude) | UK Finance APP-fraud reporting puts per-case losses one to two orders above card-rail fraud [Certain on the ordering]; FTC CSN medians. Crypto DECLINED — the era lock ends the window in 2020 | UNCITED (order of magnitude anchored) |
| Victim susceptibility, two OPPOSITE gradients | incidence FALLS with age (bands 1.35/1.30/1.15/0.95/0.75/0.60/0.50 by decade from the 20s); severity RISES (0.70/0.80/0.90/1.00/1.30/1.70/2.20, ~3× span). Persona factors carry NON-AGE structure only: student 1.10, freelancer 1.15, smallBusiness 1.25, salaried/highNetWorth/**retiree all exactly 1.00** (anti-double-count — persona and age are strongly correlated). Tilt share 0.65, clamp [0.25, 3.00]× the eligible mean | MEASUREMENT (directions) + CHOICE (magnitudes) | FTC CSN Data Books (reports peak in the 20s–30s, decline after 60); FTC "Protecting Older Consumers" reports to Congress (median reported loss climbs monotonically, oldest band ~3× the youngest) [Certain on both directions] | directions CONFORM; magnitudes DEVIATE-BY-CHOICE |
| Card-fraud reporting | per-case reported p .85 → every fraudulent SPEND made whole by a merchant chargeback credit (flag-0, `cc_chargeback`, lag 1–10 d, OUTSIDE the fraud budget); sub-$5 test charges never reimbursed | MEASUREMENT-adjacent | Reg Z / 15 U.S.C. §1643 caps unauthorized-use liability at $50 and network zero-liability waives it [Certain on the statute]; Security.org: the large majority of card-fraud victims are made whole [Likely on the share] | UNCITED (statute Certain) |
| NO reimbursement on either AUTHORIZED rail | gift-card and impostor-push rows are never made whole | MEASUREMENT (regulatory) | Reg E (15 U.S.C. 1693) covers UNAUTHORIZED transfers only; the UK reimbursement code postdates the corpus window. The asymmetry against the mostly-reimbursed card rail is a MODELED FACT, not an omission | CONFORMS |
| Membership at the case date | every rail requires the victim to have JOINED. The SCAM rails additionally require ALIVE (a dead person cannot be talked into authorizing a payment); the card and ATO rails do NOT — deceased-account fraud is real and the exemption is preserved explicitly | MEASUREMENT (defect repair) + CHOICE (declared scope) | Deceased-identity fraud advisories | CONFORMS |
| ATO / Reg E remediation | UNMODELED. Design written, owner-gated: per-case reported p ≈ .90, the VICTIM'S BANK posts a credit per drain, lag ~2–10 business days. Two prerequisites — a bank-remediation counterparty account, and a DEDICATED credit channel (reusing `cc_chargeback` would conflate merchant-funded chargebacks with bank-funded Reg E credits) | CHOICE (declared gap) | 12 CFR 1005.6 ($50 if reported ≤2 business days, $500 ≤60 days); 1005.11 (provisional credit within 10 business days) [Certain on the framework] | KNOWN GAP |

**Budget mechanics (engineering note).** Reimbursement credits are
flag-0 remediation rows — like camouflage rows they live OUTSIDE the
exact fraud budget F = pL/(1−p); `unauthorized::generate` bounds only
flag-1 rows. Fraud density on the flag axis is unchanged.

### F-5. Camouflage

Small P2P p .03/day; monthly bill p .35; salary inbound p .12 — CHOICE.
Camouflage amounts scale with the index of the flow they MIMIC (bill/p2p
× priceScale, salary-mimic × wageScale): a camouflage row that scaled
differently from its cover class would be a detectable artifact.

### F-6. Detection & label layer

| Parameter | PL value | Class | Real-world anchor & source | Status |
|---|---|---|---|---|
| Below-CTR alert band | [$9,000, $10,000] → sev 2, ALL channels (upper edge inclusive, so exactly-$10,000 lands here and files nothing) | CHOICE | FFIEC; TM vendor catalogs. Band stays all-channel by CHOICE (generic high-amount monitoring) | — |
| CTR record | STRICTLY > $10,000 AND currency channel (`channels::isCurrency` = {atm_withdrawal, cash_deposit, fraud_structuring}) → sev 3 + CTR row | MEASUREMENT (statutory) | eCFR 31 CFR §1010.311 [Certain] | CONFORMS |
| Velocity alert | ≥5 txns/(account,day) → sev 2 | CHOICE | TM vendor docs | — |
| Alert→case escalation | 1 in 8 (content-hash) | CHOICE — no external claim | The "TM conversion 5–15%" source was uncitable vendor folklore [Guessing]. Regulator speeches quote false-positive rates above 90%, implying under-10% conversion | re-annotated CHOICE |
| SAR filing probability | 0.70 per group (content-keyed) | CHOICE | FinCEN SAR Stats. Calibration anchors: FinCEN FY2024 4.7M SARs and 20.5M CTRs (12,870 and 56,160/day); ratio ~4.4 CTRs per SAR; fraud-typed SARs ~52% [Certain] — a SANITY BAND, not a target, since PL oversamples fraud | UNCITED |
| SAR monetary floor | ≥ $5,000 group total | MEASUREMENT (statutory) | eCFR 31 CFR §1020.320(a)(2): required when a transaction "involves or aggregates at least $5,000" with suspicion criteria met [Certain]. Nuances NOT modeled: insider abuse reportable at ANY amount; $25,000+ tier with no suspect identified | CONFORMS (simplifications logged) |
| SAR filing lag | activity end + 30 days | MEASUREMENT → re-classed | eCFR 31 CFR §1020.320(b)(3): no later than 30 CALENDAR DAYS after INITIAL DETECTION; +30 (60 total) if no suspect identified [Certain]. PL keys the lag to activity end because a detection date is not modeled | DEVIATES-BY-CHOICE (proxy documented) |
| shell_score | round2(passThrough × (1 − organicShare)) | CHOICE | FATF shell typologies | — |

**KNOWN SIMPLIFICATION (logged, unmodeled):** same-business-day currency
aggregation, 31 CFR §1010.313(b) — PL files single-transaction CTRs
only, so a structurer's five same-day $2,500 cash deposits do not
aggregate into a CTR.

### F-7. Measured emergent properties

Probe config pop 10k / 60d / seed 7 unless noted.

| Measurement | Value |
|---|---|
| CTR rows | **117** vs the FinCEN per-adult anchor ≈128 [Derived] (20.5M CTRs ÷ ~262M US adults ≈ 0.078/adult-yr). Analytic pre-attrition ≈129; quiet months, weekend rolls and window edges account for the haircut |
| Alerts / SARs | 24,231 / 2 |
| Threshold splits below alert band | 0.375 (analytic 0.345) |
| Posted structuring mix | 15/32/53 vs sampler 60/25/15 — unfunded victim debits bounce at clearing (EMERGENT, not a defect) |
| Card-view fraud rate | 0.1347% (12,997 of 9,645,706 view rows at pop 20k / 730d) vs the IBM TabFormer anchor ~0.1% / observed 0.11675% [Likely on the anchor]. **AXIS:** share of CARD-VIEW rows (channels card_purchase + merchant) carrying flag 1 — NOT the corpus-wide illicit ratio and NOT an event count |
| Liquidity coupling | adding legitimate cash inflows moved the corpus −3.2% (fewer overdraft-fee and retry rows). Deterministic and internally consistent; all invariance gates green |

Closing a material gap between the measured card-view rate and the
TabFormer anchor is a FRAUD-BUDGET change (targetEvents, rail mix, every
fraud denominator) — owner-gated ADJUST with golden re-pins, never a
silent edit.

═══════════════════════════════════════════════════════════════════════
# PART II — LEGITIMATE ECONOMY
═══════════════════════════════════════════════════════════════════════

### L-1. Population & personas

Shares (CHOICE): salaried .60, student .12, retiree .10, freelancer .10,
smallBusiness .06, highNetWorth .02.

| Persona | rate× | amt× | timing | init bal | cardP | ccShare | limit | weight | paySens |
|---|---|---|---|---|---|---|---|---|---|
| student | 0.7 | 0.7 | consumer | $200 | .60 | .55 | $800 | .18 | .67 |
| retiree | 0.6 | 0.9 | consumerDay | $1,500 | .82 | .55 | $2,500 | .30 | .50 |
| freelancer | 1.1 | 1.1 | consumer | $900 | .85 | .65 | $4,000 | .95 | .33 |
| smallBusiness | 1.2 | 1.4 | business | $8,000 | .95 | .75 | $7,000 | 1.50 | .29 |
| highNetWorth | 1.3 | 2.8 | consumer | $25,000 | .98 | .80 | $15,000 | 2.20 | .11 |
| salaried | 1.0 | 1.0 | consumer | $1,200 | .85 | .70 | $3,000 | 1.00 | .40 |

`cardP` gates CREDIT-card issuance specifically (`synth/cards/issue.hpp`);
`ccShare` = credit share of spend; `limit` = credit limit.
Paycheck-sensitivity Beta(α,β): student (4,2), retiree (3,3), freelancer
(2,4), smallBusiness (2,5), HNW (1,8), salaried (2,3). Heterogeneity:
medians jittered LN σ.15; probabilities Normal σ.08 clamp [.01,.99].

| Row | Real-world anchor & source | Status |
|---|---|---|
| cardP weighted mean **.826** | S-DCPC Table 3: credit adoption 82.3% of consumers, debit 90.3% (2024) [Certain]. Comparator is strictly CREDIT | CONFORMS |
| Initial balances (weighted ~$1,960/person) | Fed SCF 2022: median household transaction account $8,000 (mean $62,410); median checking-only $2,800 (mean $16,891); under-35 median $5,400; top income decile $111,600 [Certain] | DEVIATES-BY-CHOICE as day-zero initial conditions, not steady state. **Two flags stand:** retiree $1,500 is LOW (65–74 medians are multiples of it) and HNW $25,000 is low against $111,600 unless HNW wealth is held off-ledger by design |

### L-2. Spending engine

| Parameter | PL value | Class | Real-world anchor & source | Status |
|---|---|---|---|---|
| Transaction load | 40 txns/person/month (engine only) | MEASUREMENT | Fed Diary 2026 (Oct 2025): 47 payments/consumer/month, 6 cash. Atlanta Fed 2024 S-DCPC Table 6: 48.2 total, cash 6.7, check 1.2, debit 14.3, credit 16.6; average payment $142 [Certain]. **Full accounting:** PL bank rows = 40 engine + ~2.5 subscriptions + ~3 ATM + ~1 internal + ~2–5 loan/insurance/card ≈ **48–52/person-month**; Diary comparator = 41.5 non-cash + 3–5 ATM ≈ **45–46**. PL runs ~5–15% above [Derived] | CONFORMS as a band (this derivation is the row) |
| Daily counts | gamma-Poisson k=1.5; weekend ×0.8; day shock Gamma(1.3, 1/1.3) — unit mean | TYPOLOGY | payment-count dispersion literature | UNCITED |
| Slot mix | merchant .82 / bills .10 / p2p .08 around external .05 ⇒ effective 77.9/9.5/7.6/5.0 | MEASUREMENT | S-DCPC Tables 9a/11/13: bills 10.2 of 48.2 payments = 21.2% of count (62% of value, avg $418/bill); purchases incl. P2P 78.8%; "A person" 1.8/mo = 3.7% of count, avg $181 [Certain]. **Bills RECONCILE on a mapped basis** — PL's .10 engine slot plus out-of-engine recurring debits ≈ 20–25% of PL rows vs 21.2% | bills CONFORM; **P2P count share DEVIATES-BY-CHOICE** (PL ~2× the Diary's 3.7% — P2P density feeds the fraud typologies) |
| Seasonality (unit mean) | Jan .94, Feb .96, Mar 1.02, Apr 1.01, May 1.00, Jun .99, Jul .98, Aug 1.03, Sep 1.01, Oct 1.00, Nov 1.05, Dec 1.15 | MEASUREMENT | Census MARTS NSA: Dec-to-Jan ratio ~1.22 (Dec 2025 $817B, Jan 2025 $668B). PL's Dec/Jan = 1.22 after damping | CONFORMS (shape and amplitude) |
| Momentum | AR(1) φ .45, σ .15, clamp [.20, 3.00] | CHOICE | — | — |
| Dormancy | enter .0012/day; 7–45 d at ×.05; wake 2–5 d | CHOICE | — | — |
| Paycheck boost | ≤ +10% × sensitivity, 4-day decay | TYPOLOGY | payday-response literature (JPMC Institute) | UNCITED |
| Liquidity throttle | relief ≤2 d post-payday (+.04+.06·sens); stress from day 7 over 7 d (−.10−.15·sens); cash factor .85+.15·(avail/max($75,baseline)); burden max(.88, 1−.08·ratio); clamp [.70, 1.10]; count factor (.5+.5·liq)²; amount factor 1→.85 across liq .95→.70 | CHOICE (mechanism) | consumption-smoothing literature | — |
| Known-biller preference / exploration / commerce evolution | .55, retry limit 6, pick attempts 250; exploration base .02/txn, propensity Beta(1.6, 9.5), burst p .08 for 3–9 d; merchant add .35 / drop .10 per day (max 40), contacts add .08 / drop .03 (max 20) | CHOICE | — | — |

### L-3. Amount catalog (LN = lognormal(median, σ); Γ(shape, scale)+add)

All draws are CALIBRATION-YEAR (2019) dollars; realization applies the
PART III scale classes.

| Channel | PL model | Class | Real-world anchor & source | Status |
|---|---|---|---|---|
| Salary (monthly) | LN($4,500, .55) floor $50, ×12 annual | MEASUREMENT | BLS OEWS May 2024: median annual wage all occupations $49,500 = $4,125/mo [Certain]; CPS full-time median ~$1,192/wk = ~$5,165/mo [Likely]. PL sits between the all-worker and full-time medians — coherent for a salaried persona that excludes gig/student income | CONFORMS (comparator declared) |
| Rent | Γ(2, 700)+$100 (mean $1,500) | MEASUREMENT | Census ACS B25064 median gross rent ~$1,406 (2023), ~$1,487 (2024, +5.8% per CBPP) [Derived]. **UNIT: each PL renter is a SOLE TENANT paying one full household rent — no roommate split exists — so the HOUSEHOLD axis governs** and the DCPC per-consumer transaction average ($824) does not apply | CONFORMS |
| P2P | LN($55, .80), mean ≈ $75.7 | MEASUREMENT | Diary Table 8 mobile-app payment average $71.9/txn; "A person" average $181/txn [Certain]. App-like reading is the declared comparator | CONFORMS |
| Bill | Γ(2, 55)+$15 (mean $125) | MEASUREMENT | Fed Diary bills | UNCITED |
| ATM | LN($80, .30) floor $20 | MEASUREMENT | Diary tables count cash PAYMENTS, not withdrawals; on-person holdings (avg $66.7, conditional median $46, Table 14) are consistent with sub-$100 withdrawals but do not prove the median [Likely] | UNCITED (need the cash-withdrawal supplement) |
| Subscription (fallback) | LN($15, .40) floor $5 | MEASUREMENT | see L-6 | CONFORMS |
| Client ACH credit | LN($1,500, .75) floor $50 | MEASUREMENT | freelance invoice data | UNCITED |
| External unknown / Self transfer / Card settlement / Platform payout / Owner draw / Investment inflow | LN($120,.95) f$5 / LN($250,.80) f$10 / LN($650,.60) f$20 / LN($400,.65) f$10 / LN($2,500,.80) f$100 / LN($5,000,1.0) f$100 | CHOICE | — | — |
| Cash deposit (takings/tips) | per-persona split — see L-10 (LN, $10-rounded, floor $100) | MEASUREMENT-adjacent | FinCEN CTR volume; Fed Diary; Yale Budget Lab; IRS ATG | see L-10 |

**Merchant tickets** LN(median, σ): grocery 50/.55, fuel 32/.35,
restaurant 28/.60, pharmacy 25/.65, ecommerce 85/.70, retailOther 45/.75,
utilities 120/.40, telecom 75/.30, insurance 150/.35, education 200/.60;
default 45/.70.

Verified against S-DCPC Table 13 per-transaction averages [Certain
inputs, Derived comparison]: utilities $132.4 vs PL $130 · communications
$78.7 vs $78.5 · education $250 vs $239 · grocery cluster $52.2 vs $58.2
· restaurant+fast-food blended $27.8 vs $33.5 · stores $82.0 vs PL
ecommerce/retailOther blend · **gas $32.8 vs PL fuel $34.0** — table
**CONFORMS**.

**Rent-LEVEL mechanics** (correct, not part of any ADJUST): the monthly
debit is CONSTANT within each lease year and steps up once per lease
anniversary by 1 + 2.5% inflation + real raise N(2.0%, 1.5%) floor −1%
(one content-keyed draw per lease-year) ≈ 4.5%/yr nominal — an annual
renewal escalation, not month-over-month compounding. Moving re-draws
the base rent; leases are BACKDATED at world creation so some tenants
start mid-tenancy. Comparator for the escalation rate: CPI rent of
primary residence (UNCITED).

### L-4. Income & employment

| Parameter | PL value | Class | Real-world anchor & source | Status |
|---|---|---|---|---|
| Employment probability | EFFECTIVE per-persona: salaried .98, student .40, retiree .02, freelancer .08, smallBiz .04, HNW .12. Fit target `paidFraction` = **.65** = the table's weighted mean under the L-1 shares (Σ share × p = .6508), so the fitted scale ≈ 1.0, nothing clamps, and **the table IS the effective rate** | MEASUREMENT | BLS employment-population ratios; NCES Condition of Education (40% of full-time undergraduates employed); BLS USDL-25-0563 (student LFP 44.6%, Oct 2024) [Certain]. Retiree .02 is deliberate: the persona is FULLY retired and its income is the L-4b SSA stream, so the BLS 65+ ratio (~19%) lives implicitly in the salaried persona | CONFORMS |
| Pay cadences | weekly .20 / biweekly .55 / semimonthly .15 / monthly .10 | CHOICE | BLS CES Feb 2023 ESTABLISHMENT shares: biweekly 43.0%, weekly 27.0%, semimonthly 19.8%, monthly ~10%; 72.9% of 1,000+ employee establishments pay biweekly [Certain]. **AXIS: PL needs WORKER-weighted shares and workers concentrate in large biweekly employers** | DEVIATES-BY-CHOICE (cannot claim conformance to a published number) |
| Payday mechanics | Friday default (25% Thu↔Fri); semimonthly {15,31} (35% {1,15}); monthly ∈ {28,30,31}; roll to previous business day; posting lag 0–1 d; salary posts 06:00–12:00 same day. Weekly/biweekly lattices are ERA-AGNOSTIC — biweekly aligns to the anchor's FORTNIGHT PARITY (the anchor is a lattice PHASE, not a start bound) | MEASUREMENT | payroll-industry conventions | CONFORMS |
| Job tenure | 1.5–4.0 y/job | MEASUREMENT | BLS median employee tenure 3.9 years (2024) [Likely]. PL's per-job range tops out at the national median, so PL workers churn faster | DEVIATES-BY-CHOICE (short sim windows need job-change events) |
| Wage growth | real raise N(1.5%, 2.0%) floor −2% ON TOP of the AWI index; switch bump N(+8%, 6%) floor −5% | MEASUREMENT | Atlanta Fed Wage Growth Tracker ~4–4.5% nominal median recently [Likely]. The flat 2.5% inflation constant is RETIRED — the AWI index IS the economy-wide nominal path (PART III) | base CONFORMS; switch bump UNCITED |
| Floors/jitter | ≥$50/paycheck (wage-scaled); initial salary jitter LN σ.03 | CHOICE | — | — |

**RESIDUAL:** employed students draw the same LN($4,500,.55) salary model
as everyone else — a part-time wage tier is a registered upgrade.

### L-4b. Government benefits

| Parameter | PL value | Class | Real-world anchor & source | Status |
|---|---|---|---|---|
| SSA retirement | retirees only; eligibleP .87; LN($2,071, .30) floor $900/mo; paid on the 3 SSA Wednesday cohorts by REAL birth day-of-month (1–10 / 11–20 / 21–31 → cohorts 0/1/2) | MEASUREMENT | ~90% of 65+ receive Social Security (PL .87 of retirees CONFORMS-adjacent); average retired-worker benefit ≈ $1,907/mo (Dec 2024), ≈ $1,976 after the Jan 2025 COLA — PL's implied mean ≈ $2,166 is a few percent high [Likely]. SSA payment schedule [Certain on the scheme] | UNCITED (verify SSA Monthly Statistical Snapshot) |
| SSDI disability | non-retiree/non-student personas; eligibleP .04; LN($1,630, .25) floor $500/mo | MEASUREMENT | average disabled-worker benefit ≈ $1,540/mo — PL implied mean ≈ $1,682, ~9% high; beneficiaries ≈ 7.2–7.4M ≈ 3–4% of working-age population (PL .04 CONFORMS-adjacent) [Likely] | UNCITED |

Benefits DIE with the beneficiary; survivor benefits are a registered
upgrade.

### L-5. Housing

Lease tenure **3–8 y** (mean 5.5y ⇒ ~18%/yr turnover); on move: new
landlord, fresh base rent (jitter LN σ.05); growth per L-3 mechanics.

**Anchor:** Census CPS ASEC renter mover rate 21.7% (2017, a then-historic
low); BLS continuing-tenant work (new-tenant share ~15% recently) ⇒
renters turn over at ~15–22%/yr, implying mean stays of ~4.5–7 years
[Certain on rates, Derived on the implication]. **CONFORMS.**

**Renter SHARE:** `rent::Rules::paidFraction` = **.35** against the ACS
renter share of households ~.34–.36 [Likely]. Effective persona shares:
student ≈ .33, retiree ≈ .12, freelancer ≈ .38, smallBusiness ≈ .23, HNW
≈ .07, salaried ≈ .41. **Aggregate reconciliation** [Derived]: PL
per-capita rent outflow ≈ .35 × $1,500 = $525/person-month vs real ≈ .35
× $1,487 ≈ $520 — the amount and share calibrations reconcile an
aggregate that either alone would have broken.

**KNOWN SIMPLIFICATION (logged):** homeowner/renter overlap — the
`RentRoll.isHomeowner` hook exists but is unwired, so a mortgage payer
can also be selected as a renter (expected overlap ≈ .35 × mortgage
adoption ≈ 16% of people).

### L-6. Recurring debits

| Routine | PL value | Class | Real-world anchor & source | Status |
|---|---|---|---|---|
| Subscriptions | 4–8 candidates/person, 55% become debits; 18-point pool $6.99–$99.99; day U[1,28] | MEASUREMENT | NAMED COMPARATOR: Bango 2025 (5.2 active, $69/mo). PL's 2.2–4.4 active at ~$27 pool mean ⇒ $60–119/person-month brackets it [Derived]. The survey band is wide (Self Financial 2026: 3.4/$35; Whop-style trackers: 8.2/$219) — naming the comparator is what makes the row falsifiable | CONFORMS |
| ATM | 88% users; 1–6/mo; LN($80,.30) floor $20 | MEASUREMENT | S-DCPC Table 5: 82.6% of consumers used cash in the last 30 days (2024). PL runs a few points above the cash-user share | borderline; amount UNCITED |
| Internal transfers | 55% active; 1–3/mo; LN($120,.75) floor $10; 25% round from a pool {25…2000} | CHOICE | — | — |

### L-7. Credit cards

| Parameter | PL value | Class | Real-world anchor & source | Status |
|---|---|---|---|---|
| Ownership / share / limits | per persona (L-1); cardP weighted ≈ .826 | MEASUREMENT | S-DCPC Table 3 credit adoption 82.3% [Certain] | CONFORMS (limits/balances UNCITED) |
| Grace period | 25 days | MEASUREMENT | CARD Act 15 U.S.C. 1666b; Reg Z 12 CFR 1026.5(b)(2)(ii): statement ≥21 days before due; issuer norms 21–25 [Certain] | CONFORMS |
| Minimum payment | max(2%, $25) — the $25 floor is price-scaled | MEASUREMENT | issuer norms | UNCITED |
| Late fee | $32 (price-scaled) | MEASUREMENT | Reg Z safe harbors 12 CFR 1026.52(b): operative ~$32 first / $43 subsequent; the $8 rule NEVER took effect and was vacated Apr 15 2025 (Chamber of Commerce v. CFPB, N.D. Tex.); CFPB-cited average $32 (2022) [Certain] | CONFORMS as a flat average ($43 repeat tier unmodeled) |
| Autopay | full .40 / minimum .10 / manual .50 per card. **Autopay cards BYPASS the manual mixture** | MEASUREMENT-adjacent | COMPARATOR: share of card ACCOUNTS on any autopay ~.40–.50 in issuer/industry surveys [Guessing]; PL's .50 total sits at the band top. The .40/.10 composition WITHIN autopay is CHOICE (no published split) | UNCITED but falsifiable |
| Payment mixture (MANUAL payers only) | full .35 / partial .30 / minimum .25 / miss .10; partial fraction Beta(2,5) of statement. **EFFECTIVE population shares: full .575 / minimum .225 / partial .15 / miss .05** | MEASUREMENT | S-DCPC Table 4: 42.1% of adopters carried an unpaid balance last month ⇒ ~58% paid in full; 45.4% carried a balance at some point in 12 months; mean unpaid balance $3,078 across adopters, $6,794 per revolver [Certain]. PL's .575 sits on the measured ~.58 | CONFORMS, no code change |
| Miss share | effective .05 | MEASUREMENT | **AXIS MAPPING:** PL's .05 is a PER-STATEMENT miss FLOW; the delinquency benchmark ~3–4% is a POINT-IN-TIME STOCK (accounts 30+ dpd). With ~one-cycle cure the flow and stock coincide numerically. Any future verdict must compare stock to stock, and must never compare the manual-only .10 to either | CONFORMS-adjacent as a band |
| Payment timing | late p .08, 1–20 d late | MEASUREMENT | issuer delinquency curves | UNCITED |
| Disputes | refund p .006/purchase (1–14 d); chargeback p .001 (7–45 d) | CHOICE | Network thresholds and benchmarks: Visa legacy 0.9% with 0.65% early warning; VAMP combined-dispute 1.5% eff. Apr 2026; Mastercard ECM 1.5%; all-industry e-commerce average ~0.6–0.65% of transactions [Certain]. PL's 0.1% blended across ALL channels including in-person is BELOW the e-comm average — directionally right (card-present disputes are rare) but with no direct published comparator. **DEFINITION:** a "refund" is a post-purchase MERCHANT CREDIT; the comparator is merchandise-RETURN incidence per purchase across all channels, NOT e-commerce order-level return rates (~15%+, a different concept) | DEVIATES-BY-CHOICE; refund UNCITED with definition in place |
| Cycle finalization | 32-day session lag | CHOICE (architecture) | — | — |

### L-8. Credit & obligation products

Adoption by persona in order student / retiree / freelancer / smallBiz /
HNW / salaried.

**Mortgage** — adoption .02/.55/.30/.55/.65/.55; payment LN($1,750, .55);
delinquency late 4% (1–7 d), miss .5%, partial 1% (30–80%), cure 30%,
cluster ×1.6, ≤6 cure cycles.
**Auto loan** — .10/.20/.40/.45/.45/.45; 35% new; payment new LN($715,.30)
/ used LN($525,.35) floor $100; term new 68±6 / used 67±8 mo, clamp
24–84; late 5% (1–10 d), miss 1%, partial 1.5%, cure 30%, cluster ×1.7,
≤4 cycles.
**Student loan** — .85/.05/.20/.20/.10/.30; plans standard .65 (120 mo) /
extended .20 (240 mo) / IDR .15 (55% 240 else 300 mo); grace 6 mo (65% of
students deferred); payment LN($295,.55) floor $50; late 6% (1–14 d),
miss 1.5%, partial 2%, cure 25%, cluster ×1.8, ≤4 cycles.
**Insurance** — auto .30/.85/.85/.90/.95/.92, home .05/.55/.30/.55/.70/.55,
life .10/.55/.30/.45/.55/.55; mortgage⇒home .99, auto-loan⇒auto .997;
monthly premiums LN auto $225/.30 (floor 25), home $200/.30 (25), life
$28/.40 (5); claims auto 4.2%/y → LN($4,700,.80) floor $500; home 5.5%/y
→ LN($12,500,.80) floor $1,000.
**Tax** — .05/.20/.65/.85/.50/.10; quarterly LN($1,250,.65) floor $100;
filing refund 65% LN($2,500,.55) / balance due 20% LN($1,100,.65).

Monthly payments are ORIGINATION-ANCHORED and fixed-nominal thereafter
(PART III class D); tax scales at the DUE year (brackets index annually).

| Row | Real-world anchor & source | Status |
|---|---|---|
| Auto loan | Experian Q4 2025/Q1 2026: average monthly payment new $767/$770, used $537/$531; terms new 69.5 mo, used 67.7 mo; amounts financed new $43,925, used $27,070 [Certain]. PL implied means ~$748 new / ~$558 used, within ~4% | CONFORMS (the 84-mo clamp truncates a real tail — 73–84 mo is ~30% of new originations, 85+ ~2%; documented) |
| Mortgage payment | Census ACS 2024: median monthly mortgage payment $1,521 all mortgaged owners, $2,225 for 2024 movers; MBA applications median ~$2,067–2,127 (2025) [Certain]. PL median $1,750 / implied mean ~$2,036 sits between the all-stock and new-origination medians | CONFORMS as a band (mixed-stock comparator) |
| Student loan payment | Fed SHED: typical payment $200–299; 60% of payers at or under $299; 45% of borrowers owed no payment in the survey month (2025); secondary averages $336–503 [Certain]. PL median $295 / mean ~$343 | CONFORMS. **FLAG:** PL IDR share .15 vs FSA portfolio IDR enrollment ~a third of borrowers in repayment [Likely] — expect an ADJUST or a CHOICE note |
| Insurance premiums | 2026 market averages: auto full coverage $190–244/mo; home $1,824–2,543/yr with Philadelphia Fed at $2,530 (2023) trending ~$3,000 by 2026; term life ~$26–30/mo [Certain auto/home, Likely life]. PL auto implied ~$235/mo, home ~$2,510/yr, life $28 | all three CONFORM |
| Insurance claims | III/ISO: 5.3% of insured homes filed a claim (2023); average home claim severity $18,311 (2022), >$17k five-year [Certain]. PL home frequency 5.5%, implied severity mean ≈ $17.2k | CONFORMS. Auto frequency 4.2%/yr and payout mean ~$6.5k are consistent with collision frequency ~5–6 per 100 car-years and severity ~$5.7–6.6k [Likely] — verify before flipping to CONFORMS |
| Tax | IRS: 64.1% of 2024 and ~63% of 2025 returns received refunds; average refund $3,167–3,170 (2025 season) [Certain]. PL refund share .65; implied mean ≈ $2,908, ~8% under, with the median-below-mean construction documented | CONFORMS |
| Delinquency ladders | MBA NDS (mortgage 30+ ~4% band), NY Fed CCP transition rates, FSA delinquency stats. PL's per-payment lateness does not map one-to-one onto 30/60/90-day buckets | UNCITED — a unit-mapping exercise |

### L-9. Family transfers

| Flow | PL value | Real-world anchor & source | Status |
|---|---|---|---|
| Spousal | 60% separate accounts. **DEFINITION:** the share of couples that ACTIVELY ROUTE inter-spouse transfers between individually-owned accounts — PL models no joint accounts, so this means "at least some money kept separate", NOT fully-separate finances (`family/spouse.cpp separateAccountsP`); 2–6 txns/mo; breadwinner-directional 65%; LN($85, .90) | Bankrate 2026: 62% of coupled adults keep at least some money separate (36% hybrid + 26% fully separate); Census SIPP 2023: 23% hold NO joint account [Certain]. Under the written definition the comparator is 62% | CONFORMS (would be NONCONFORMING under the fully-separate reading — the definition is load-bearing) |
| Allowances | weekly 70% (else monthly); Pareto($8, 1.8), mean ≈ $18/wk | Greenlight platform data (avg weekly $14.72 in 2023, $13.15 in 2025); Till Financial 2025-26 (avg $17/wk, median $10/wk); AICPA 2019 (~$30/wk self-reported, teen-heavy) [Certain] ⇒ transaction-data averages $13–17/wk | CONFORMS |
| Tuition | 65% of students; 4–5 installments; LN($7,712, .35) each. **DEFINITION:** funds the student's FULL annual COST OF ATTENDANCE at PUBLISHED (sticker) prices, paid parent→STUDENT account (`family/tuition.cpp`), not a university payment | College Board 2025-26: published tuition+fees public 4yr in-state $11,950 / out-of-state $31,880 / private nonprofit $45,000; NET tuition after aid public $2,300 / private $16,910; total COA public in-state $30,990 / private $65,470 [Certain]. PL's $31–39k/yr sits on public COA; the private-COA tail lives in the lognormal spread | CONFORMS under the written definition |
| Parental support | 35% of eligible; Pareto(xm=$25, α=2.4)/txn | Fed SHED; AARP — incidence only, no per-transfer distributions exist | UNCITED (expect CHOICE) |
| Sibling / grandparent / parent gifts | 15% pairs active, 18%/mo, LN($120,.90) · 8%, LN($150,.70) · 12%, Pareto($75,1.6) | — | UNCITED (expect CHOICE) |
| Inheritance | DEATH-CAUSED estates only (see PART III); size LN($25,000, σ1.0) interim | Fed SCF intergenerational-transfer / net-worth tables | UNCITED — an SCF-anchored re-derivation is REGISTERED |
| External recipients | 18% of family transfers leave the bank | CHOICE | — |

Family gifts drop when either party is dead; external XF members' deaths
are unmodeled (declared).

### L-10. Business / freelancer revenue

**Freelancer** — clients: active .88, 2–5 counterparties, 1–4 payments/mo,
LN($1,400,.70); platforms .42, 1–2, 1–4, LN($425,.60); owner draw .70,
1–2, LN($1,800,.75); cash takings .25, 1–4/mo, LN($450,.60) $10-rounded
floor $100; quiet month p .12.
**Small business** — clients .55, 2–6, 0–3, LN($2,600,.75); platforms .22,
1–2, 0–3, LN($950,.70); card settlements .74, 4–12/mo, LN($680,.55);
owner draw .86, 1–2, LN($3,400,.70); cash takings .40, 4–10/mo,
LN($2,800,.72) $10-rounded floor $100; quiet month .06.
**High net worth** — owner draw .55, 1–2, LN($6,000,.65); investment
inflows .72, 1–3, LN($12,000, 1.0); quiet month .02.
**Retiree** — draw-like income .33, 1, LN($1,100,.50); investment .50, 1–2,
LN($400,.65); quiet month .05.
**Salaried** — cash tips only: .03 active, 2–4/mo, LN($200,.55).
**Student** — cash tips only: .16 active, 1–3/mo, LN($140,.55).

Revenue months stop at death, including the otherwise-perpetual
retiree/HNW plans.

**THE CASH-HANDLING SPLIT.** Every figure below is a NAMED-SOURCE
recalled value with a confidence tag, awaiting the owner's retrieval
pass.

| Persona (pop share) | Cash-active | Basis |
|---|---|---|
| smallBusiness (.06) | **.40** | IRS *Cash Intensive Businesses ATG* names the canonical sectors (restaurants/bars, convenience & grocery, salons, laundromats, car washes, taxis, vending, parking, scrap) [Certain the guide exists]; Census SUSB/CBP establishment mix ⇒ cash-heavy core ≈ 25–30% [Derived, Likely]; Square "Making Change" cash share of in-person transactions ~37% (2015) → ~30% (2019) → high-teens/low-20s post-2020 [Likely]. PL sets .40 ABOVE the establishment core because its smallBusiness archetype is a Main-Street storefront (74% card-settlement active), over-representing cash-accepting sectors [Derived, documented] |
| freelancer (.10) | **.25** | Fed SHED gig section (~16%/month doing gig/informal work); Fed EIWA 2015 — cash is the dominant mode for OFFLINE informal work [Likely]. .25 is the offline-informal fraction [Derived] |
| salaried (.60) | **.03** | Yale Budget Lab, "Who Are Tipped Workers?" (Jun 2024): ~4.0M tipped workers ≈ **2.5% of US employment** [Certain]. Amounts modest because card tipping now carries most tip volume [Likely] |
| student (.12) | **.16** | [Derived] student employment .40 (L-4) × ~.4 of student jobs in tipped food-service/hospitality [Likely] |
| retiree (.10) / HNW (.02) | 0 | CHOICE, documented: Diary age tables show 65+ are the heaviest cash USERS for payments — they withdraw and spend cash, they do not deposit takings [Likely]. HNW have no takings/tips channel by construction |

**Economy-wide context:** cash ≈ 14–16% of payment COUNT; ~6–7 cash
payments/person-month; 82.6% used cash in the last 30 days [Certain on
the ballpark]. **CTR calibration** [Derived]: smallBusiness 600/10k × .40
= 240 depositors × ~7/mo; P(> $10,000 | LN($2,800,.72)) ≈ 3.9% ⇒ ≈129
CTRs pre-attrition, 117 measured (attrition ≈ .94) against the FinCEN
per-adult anchor ≈128. The CTR:SAR ratio runs far above the national 4.4
because SARs are ring-driven and deliberately sparse.

### L-11. Population scaffolding

Accounts per person: 1 + Binomial(2, .25) — mean 1.5, max 3.
**SCOPE:** checking-like transaction accounts ONLY — every PL deposit
account is an undifferentiated one-owner transaction account; no savings
type exists in `entity::account`. Anchor: S-DCPC Table 1 (bank account
95.4%, checking 94.7%, savings 77.2%); no official accounts-per-person
count is published. **CONFORMS** under the checking-only scope.

Merchants core 120/10k + tail 400/10k; landlords 12/10k; per 10k (floor):
platforms 2 (2), processors 1 (2), owner businesses 200 (25), brokerages
40 (5), employers 25 (floor 5, 4% internal-bank), clients 250 (floor 25,
2% internal-bank). **Densities re-classed CHOICE** — no public
per-10k-customer source exists and likely never will.

Government cohort: SSA payment cohort derives from the REAL birth
day-of-month (1–10 / 11–20 / 21–31 → 0/1/2), per the SSA payment
schedule.

═══════════════════════════════════════════════════════════════════════
# PART III — MACRO / ERA MODEL
═══════════════════════════════════════════════════════════════════════

The era series are EMBEDDED constexpr tables in
`synth/econ/era_data.hpp` (the `data/econ/` files are retired per the
minimize-repo-data-files directive); provenance and refresh contract in
`docs/era_data_provenance.md`. **The series are READ by generation**, so
every refresh is now MODEL-MOVING.

### M-1. The embedded series (1990–2024)

| Series | Values & axis | Class | Source & verification | Status |
|---|---|---|---|---|
| CPI-U annual averages | 1990 130.658 → 2019 255.657 → 2020 258.811 → 2024 313.689; 2019/1991 ≈ 1.877; 2009 is the era's only annual deflation (−0.4%) | MEASUREMENT | FRED CPIAUCNS (BLS CUUR0000SA0 mirror), read 2026-07-24. The official BLS annual average IS the mean of the 12 NSA monthly indexes; recomputed and matched to the third decimal for every covered year | **VERIFIED EXACT** |
| SSA national Average Wage Index | 1991 $21,811.60 → 2019 $54,099.99 (≈2.48×) → 2024 $69,846.57; 2009 dips below 2008 (−1.51%) | MEASUREMENT | ssa.gov/oact/cola/awiseries.html, read 2026-07-24 — all 31 values 1990–2020 matched | **VERIFIED EXACT** |
| Nominal per-capita PCE | $15,225 (1990) → $43,682 (2019) → $42,886 (2020) → $58,501 (2024); 2019/1990 ≈ 2.87 | MEASUREMENT | FRED A794RC0A052NBEA (BEA NIPA), HTML data view, vintage 2026-04-09 | **VERIFIED EXACT** |
| Population | BEA NIPA MIDPERIOD population: 250,181k (1990) → 330,513k (2019) → 331,840k (2020) → 340,095k (2024); strictly increasing | MEASUREMENT | FRED B230RC0A052NBEA, vintage 2026-02-20. **AXIS: this is the per-capita PCE denominator, NOT the Census July-1 resident estimate** (<0.3% apart) | **VERIFIED EXACT** |
| U-3 unemployment, annual average | 5.6% (1990), 7.5% (1992), 9.3%/9.6% (2009/2010), 3.7% (2019), 8.1% (2020), 4.0% (2024). **AXIS: annual average, not the monthly peak** (peaks were 7.8% 1992-06, 10.0% 2009-10, 14.7% 2020-04) | MEASUREMENT | BLS LNS14000000 is the canonical citation; FRED UNRATENSA monthly means reproduce every covered value within 0.1pp (the official statistic is a ratio of annual averages, not a mean of monthly rates) | transcribed + cross-checked |
| NBER recession months per year | 1990:5, 1991:3, 2001:8, 2008:12, 2009:6, 2020:2 (sums = published durations 8/8/18/2). **AXIS: months strictly after the NBER peak month through the trough** | MEASUREMENT | NBER business-cycle dating [Certain on the dates; the counting convention is PL's, documented] | CONFORMS |
| Mortality qx | EXACT full transcription of the SSA PERIOD LIFE TABLE FOR 2023 (Actuarial Table 4C6, as used in the 2026 Trustees Report): single ages 0–119, male/female qx to six decimals. Male and female qx are equal from age 109 up (source values) | MEASUREMENT | ssa.gov/oact/STATS/table4c6.html, read 2026-07-24. Cohort cross-checks against the source lives column: survival 65→94 = 8,320/79,084 ≈ 10.5%; 22→51 = 90,659/98,458 ≈ 92.1% | **VERIFIED EXACT** (the 24-pivot approximation is RETIRED) |
| Funeral cost anchors | NFDA median adult funeral: 1991 ~$3,742 → 2019 $7,640 (2021 $7,848; cremation $6,970) | MEASUREMENT | NFDA General Price List surveys [Likely] | UNCITED (owner spot-check) |

**2025 is IMPOSSIBLE to pin as of 2026-07:** AWI 2025 publishes ~2026-10,
and the October 2025 CPI release and CPS survey were cancelled (federal
shutdown) — no official 2025 annual averages exist. A deliberate
TRIPWIRE assertion in `test_app_options` flips when the 2025 row lands.

### M-2. Calibration year and the level primitives

| Item | PL value | Class | Source | Status |
|---|---|---|---|---|
| CALIBRATION YEAR | **2019** (`kCalibrationYear`). Exact denominators pinned: CPI 255.657, AWI $54,099.99 | CHOICE (OWNER-APPROVED 2026-07-24) | Durability criterion: the year is a PROVENANCE FACT of the calibration data (constants measured ~2015–2024, declared 2019-denominated — the last full canonical-window year and last pre-COVID year), never "the present", never the coverage tail, never wall-clock. It changes only with the constants it denominates. Rejected alternatives in `docs/era_data_provenance.md` | ADOPTED |
| priceScale(y) / wageScale(y) | CPI-U(y)/CPI-U(2019) and AWI(y)/AWI(2019); exactly 1.0 at the calibration year | MEASUREMENT | derived from M-1 | — |
| pceScale(y) / realPceLevel(y) | pceScale = per-capita nominal PCE over the calibration year; **realPceLevel = pceScale/priceScale** — the measured REAL per-capita consumption path (≈0.67 at 1991, exactly 1.0 at 2019), carrying the measured dips (1991, 2008–09, the 2020 collapse and 2021 rebound). The 2001 recession slowed growth without a per-capita consumption dip — the series says so and the model inherits it | MEASUREMENT + CHOICE (level definition) | BEA A794RC (M-1) | — |
| FREEZE-AND-DECLARE | outside 1990–2024 coverage every scale HOLDS at the nearest covered year's level and the run prints ONE stderr notice. Never extrapolated, never wall-clock, no new CLI | MEASUREMENT (code fact) | pinned by `test_app_options` | — |

### M-3. Nominal-scale wiring classes

Wiring shape everywhere: draw → × scale → (denomination re-snap if any)
→ roundMoney → emit. RNG streams, lanes and entity ordering are
byte-identical to the pre-wiring engine; ONLY amounts move.

| Class | Scope | Index | Notes |
|---|---|---|---|
| **W** wage-indexed | salaries at pay date; freelancer/business revenue at month; SSA retirement + disability at deposit date | wageScale(realization year) | DEVIATION: real SSA wage-indexes at award then CPI-COLAs per cohort; ONE index era-wide is the declared simplification |
| **P** price-indexed | rent at pay date; session tickets at event day; subscriptions at DEBIT date; insurance premiums at billing and claims at claim date; family routines; ATM and internal transfers; card late fee and minimum-payment floor at cycle date | priceScale(realization year) | Per-contract frozen subscription pricing was REJECTED — it would hold 1991 prices for decades |
| **P-stock** window-start anchor | opening balances, overdraft fees, protection buffers, LOC limits, card credit limits; persona initialBalance/baselineCash references | priceScale(window-start year), ONCE | DECLARED APPROXIMATION: the stock anchor is fixed at window start while flow scale drifts across a decades-long window; nominal balance levels lag late-window flows, but liquidity/utilization RATIOS stay coherent |
| **D** origination-anchored debt | mortgage/auto/student monthly payments | priceScale(ORIGINATION year) at issue, FIXED NOMINAL after | Real loans are nominal contracts. Backdated originations before 1990 clamp to 1990. Tax quarterlies/filings use priceScale(DUE year) instead — brackets index annually |
| **S** statutory fixed-nominal | the BSA/CTR $10,000 threshold and the structuring band (≤$9,950); ATM $20-note and cash-deposit $10-bill lattices (amounts scale then RE-SNAP: a 1991 withdrawal is fewer $20s, not scaled $20s); $0.01 interest and $1 amount de-minimis floors | none | **HISTORICALLY CORRECT:** 31 CFR 1010.311's threshold has been UNINDEXED since the 1970s — in 1991 it bit at roughly 2× today's real value |
| **F** fraud (continuous) | kFraud ($900) / kFraudCycle ($600) rails; cardFraudSpend ($79 median, clamps [$1,$5k]×scale); atoDrainAmount ($180, clamps [$10,$85k]×scale); scamWireAmount | priceScale(event year) | Structuring EXCLUDED (class S). The prevalence target is a COUNT rate, unaffected by amount scaling; funnel dollar floors scale with their amounts so funnel geometry is scale-invariant |
| **F-lattice** exception | cardTestCharge anchors ($0.50/$1/$2/$5) and giftCardScamAmount rack denominations ($100/$200/$500 + the $10-step range) stay **FIXED-NOMINAL** | none | OWNER-APPROVED 2026-07-25. The round-amount signature IS the typology, and denominations are physical rack artifacts like the $20 note. **This exception is load-bearing for gate design — see SUPERSEDED CLAIMS** |
| Screens | BEHAVIORAL dollar screens scale with the index of what they screen (paycheck $50 minimum → wageScale; revenue floors $20–250 → wageScale; ATM reserve clamp $40–120, liquidity $75 reference, card $25/$32 → priceScale). STATUTORY/de-minimis screens stay fixed | — | — |

The flat `.025 annualInflation` constants are RETIRED from
SalaryGrowthRules and RentGrowthRules — the AWI/CPI index IS the
economy-wide nominal path. The seeded `salary_real_raise` /
`rent_real_raise` idiosyncratic lanes SURVIVE as career/lease
progression ON TOP of the index (μ = .015 salary / .020 rent); the
aggregate acceptance bands allow that declared drift.

### M-4. Macro modulation — the real consumption level

| Item | PL value | Class | Source |
|---|---|---|---|
| THE CHANNEL | real consumption modulates the discretionary session's transaction **COUNT** axis only; ticket AMOUNTS stay exactly as class P wired them. The quantity axis carries the real growth; **the fraud budget F = pL/(1−p) rides the candidate count L, so fraud DENSITY stays proportional across eras with no fraud-side wiring** | CHOICE (owner-adopted 2026-07-26) | Fed Payments Study per-capita noncash counts |
| BUDGET SEMANTICS | the session's window budget is denominated at the CALIBRATION LEVEL; realized per-year volume = target × realPceLevel(year). A 2019 window reproduces today's volumes exactly; a 1991 window runs at ~0.67×. ONE pure lookup per day frame — no draws, no lanes, no CLI | INVARIANT + CHOICE | model consistency with the M-3 scales |
| SCOPE | the discretionary spending session ONLY. Wages/revenue/benefits already ride AWI; rent/subscriptions/premiums/obligations/card terms are CONTRACTUAL (their era axis is the price level they carry). ATM cadence, the cash-vs-card mix and family gift cadences are DECLARED era-flat | CHOICE (declared simplifications) | a cash-share era model is REGISTERED |
| UNEMPLOYMENT | DECLARED, not modeled — the demand side already carries the downturns through the PCE series at the same annual resolution. A labor-market separation-spell model and within-year NBER recession shading are REGISTERED | CHOICE | BLS U-3 (embedded); NBER dating |
| COVID / EIP | the 2020 collapse and 2021 rebound ship FREE through realPceLevel. The three Economic Impact Payments (CARES Apr 2020 $1,200/adult; Dec 2020–Jan 2021 $600; ARPA Mar 2021 $1,400) are REGISTERED as a future class-S statutory table; the canonical card-fraud window ends 2020-01-01 so no current probe reaches them | CHOICE (statutory amounts fixed-nominal when wired) | CARES / CAA 2021 / ARPA |
| HARNESS DRAIN (analysis, not a model fact) | the ~27% deflated year-over-year drain in 300-person second-year gate legs is a SMALL-WORLD BUDGET ARTIFACT (income under-provision → declining balances → liquidity suppression) that PREDATES every macro round. Production populations do not share the geometry. Gates are cross-era ratios of same-position years so the drain cancels | MEASUREMENT (harness fact) | `test_econ_wiring` drift diagnostics |

### M-5. Persona timeline, mortality and membership

| Item | PL value | Class | Source & anchor |
|---|---|---|---|
| Full retirement age | `fraMonths(birthYear)`: 65y through 1937; +2 months/birth year 1938–1942; 66y for 1943–1954; +2 months/birth year 1955–1959; 67y from 1960. Cohort-varying BY STATUTE, pinned test-exact. SSA's "born January 1 counts as the previous year" quirk is a declared simplification away | MEASUREMENT | Social Security Amendments of 1983; ssa.gov retirement-age chart |
| Claiming-age mixture | .30 at exactly 62; .10 uniform over [63y, FRA); .45 at FRA; .05 uniform over (FRA, 70y); .10 at exactly 70; plus 0–60 day jitter. ONE distribution era-wide — era variation enters through the statutory FRA | CHOICE | SSA Annual Statistical Supplement, OASI claiming-age tables. DEVIATION: claiming at 62 was substantially more common in the early 1990s; per-cohort shares are a REGISTERED upgrade |
| Student work-start | age over 19–28 with mass at 22–26 (weights .05/.05/.08/.20/.20/.15/.10/.07/.05/.05 from 19), anchored to the BIRTH date; destination salaried .85 / freelancer .15 | CHOICE | NCES completion-age statistics; the L-4 student .40 is the DURING-study probability, this sets when study ends |
| Small-business churn | residual lifetime = memoryless exponential, median 5 years, clamp [30 d, 40 y] (constant hazard ⇒ backdating-invariant); after close salaried .70 / freelancer .30; retirement DOMINATES | TYPOLOGY on a MEASUREMENT anchor (~50% five-year establishment survival) | BLS Business Employment Dynamics |
| Seed-consistency clamps | `personaAt(simStart) == seed type` is a PINNED invariant. Drawn dates already past SETTLE FORWARD; in-window dates stand exactly as drawn (clamps bind ONLY on past dates) | INVARIANT | the seed assignment IS the state at sim start |
| highNetWorth exemption | NO timeline transitions — a retired-HNW spending profile is CEX work; forcing the retiree archetype onto HNW would distort more than it fixes | CHOICE (declared) | revisited at the CEX age-profile round |
| Retirement spending step | ~−12% consumption level factor (`kRetiredSpendScale` .88) from the claiming day; payday sensitivity re-anchors to SSA deposit days automatically. **EXEMPTION:** applies ONLY to working-seed archetypes transitioning in-window — SEED RETIREES and HNW carry no step, because the retiree archetype already encodes retired-calibrated spending (rate ×0.6 / amount ×0.9) and stacking would double-count | CHOICE anchored to MEASUREMENT | Aguiar–Hurst (JPE 2005); BLS CEX age profiles |
| Death dates | annual hazard walk over the embedded SSA 2023 table (sex-specific qx, log-linear age interpolation), inverted at ONE uniform per person on the isolated `{"mortality", personId}` lane; anchored to BIRTH dates; residual mass beyond 120 dies at the cap. Latent sex Bernoulli 50/50 (the world models no sex; the table's ~2.7-year gap is retained as real signal) | MEASUREMENT (table) + CHOICE (mechanics) | M-1. Three declared simplifications: one period table era-wide, no persona/SES-differential mortality, deaths uniform within the death year |
| ALIVE-AT-START invariant | the hazard walk begins at the person's current fractional age — death is conditional on survival to sim start and lands strictly after it | INVARIANT | the mortality analog of `personaAt(simStart)==seed` |
| Death stops | INCOME: salary ends at min(retirement, death); SSA/disability end at death; revenue months stop. BEHAVIOR: the session skips dead person-days; ATM and internal transfers stop; rent stops; family gifts drop. **THE BEHAVIORAL/CONTRACTUAL LINE:** contractual flows (subscriptions, premiums, loan/tax obligations, card cycles) keep posting against the estate until ACCOUNT CLOSURE — estates really do keep getting billed | CHOICE (declared) | estate-administration practice |
| Estates and funerals | every in-window death with heirs distributes an estate at death+30–90 days. FUNERAL: one bill-channel payment from the decedent's account at death+3–10 days, LN median **$6,300 calibration dollars** = the NFDA 2019 GPL blend (viewing+burial $7,640; cremation with viewing $5,150; ~55% 2019 cremation rate), σ .40, floor $1,000, CPI-realized at the death year | MEASUREMENT (NFDA blend) + CHOICE (σ/floor) | NFDA 2019 GPL survey; NFDA/CANA cremation rate — OWNER SPOT-CHECK |
| MEMBERSHIP INTERVAL | [joinTs, closeTs): joinTs = window start for the seed roster, a drawn join day for the join cohort; closeTs = death + **120-day settlement**, sized to strictly contain the funeral (death+3–11d) and the estate (death+30–90d) so every estate row is corpus-visible before closure. The STANDARD exporter filters every row on BOTH endpoint owners' intervals; the aml / aml_txn_edges / mule_ml / card_fraud corpora are FULL-WORLD exports (declared) and reflect lifecycle through values | CHOICE (owner directive: the population must both persist AND die) | deposit-account closure norms |
| JOIN-COHORT sizing | joinerCount = population × Σ over window days of r(year(day)) / 365.2425, where r(y) = pop(y+1)/pop(y) − 1 from the embedded BEA population series. RATE-CLAMPED at coverage edges (a frozen year reads the LAST MEASURED year-over-year rate — the rate-axis analog of the level freeze). Joiners are the LAST K person ids, so the seed roster's draws stay byte-identical. Exactly ONE draw per joiner on the isolated `{"join-cohort", personId}` lane, inverse-CDF over per-day weights ∝ r(year(day)). **A joiner's dob, persona timeline and lifespan anchor at the JOIN DATE** | MEASUREMENT (series) + CHOICE | the bank's customer base tracks resident-population growth; per-bank customer-acquisition series would be a registered upgrade. The flat 2%/yr growth model is RETIRED |
| ACCOUNT CLOSURE | subscription debits, premiums and loan/tax obligations stop at closeTs via emission-side filters placed AFTER the sites' existing draws burn (shared rng streams byte-identical). CARD servicing stops at the last statement close ≥50 days before closeTs (grace 25d + late tail 20d + the fee morning). Insurance CLAIMS stop at DEATH, not closure — claim filing is behavioral | CHOICE (mechanism + declared guards) | card ToS billing-cycle norms |
| Rings never recruit the dead | each ring plan carries the MINIMUM death epoch over its fraud + mule participants; typology bursts AND the camouflage window clamp to that horizon minus a 22-day schedule guard. **VICTIM accounts are EXEMPT** — fraud against deceased persons' accounts is a real, documented typology — and the solo/unauthorized rail is exempt under the same declaration | TYPOLOGY + CHOICE (guard) | deceased-identity fraud advisories |

**DECLARED INCONSISTENCY:** the aml / aml_txn_edges Customer onboarding
date stays the synthetic backdated derivation while standard
`customer.csv` and the card_fraud Party table export the membership
joinTs. Aligning AML onboarding to the membership axis is REGISTERED.

═══════════════════════════════════════════════════════════════════════
# PART IV — CARD-FRAUD USE CASE (exporter contract)
═══════════════════════════════════════════════════════════════════════

Exporter-side presentation for the TigerGraph TF_GNN_v3 target. None of
this alters the settled corpus. Feature-safety classes are governed by
`docs/card_fraud_feature_contract.md`; the arc record is
`docs/card_fraud_v2_roadmap.md`.

| Item | PL value | Class | Notes |
|---|---|---|---|
| Card view | channels {card_purchase, merchant}; merchant-channel (account-paid POS) rows interpreted as DEBIT-card transactions; the impostor-push rail is excluded (a wire scam is not a card transaction) | CHOICE | IBM TabFormer mixes credit/debit |
| Card attribution | source Key in the card registry → that credit card (≤1 per person); any other source → the account's derived debit card | CHOICE (label definition) | — |
| Identifier scheme | C/D/M = prefixed role.bank.number of the entity Key; P&lt;person&gt;; T&lt;row_seq&gt;. Party ids are the CANONICAL customer ids, Merchant/Device/IP the canonical renderings — card-fraud tables JOIN against every other use case and the raw ledger | CHOICE (identifier reuse) | — |
| **Withheld entity labels** | `Card.is_fraud`, `Party.is_fraud`, `Device.is_blocked`, `IP.is_blocked` are FULL-WINDOW verdicts and render as **0**. Columns are RETAINED because TF_GNN_v3 loading jobs map POSITIONALLY. The verdicts live in a 35th table, `card_fraud."cf_Ground_Truth_Label"` (entity_type, entity_id, label) — positives only, pointed at by no edge, loaded by no job | CHOICE (owner ruling) | **The one supervised target in the graph is `Payment_Transaction.is_fraud`, observable at its own row's timestamp** |
| use_chip / error | `use_chip` Swipe .63 / Chip .26 / Online .11 and `error` (2.0% incidence, mix Insufficient Balance .40 / Bad PIN .20 / Technical Glitch .20 / Bad Card Number .08 / Bad Expiration .05 / Bad CVV .05 / Bad Zipcode .02) are content-keyed FNV hashes of the row. They are point-in-time SAFE but MECHANISM-FREE, and are NOT a measured purchase-mode or authentication share. The real card-present modality drives destination selection and is NOT exported | CHOICE (presentation compatibility; explicitly not a mode measurement) | IBM TabFormer supplies the vocabulary; no empirical proportion is claimed. Exporting the real modality is REGISTERED |
| mer_cat granularity | the 10-category merchant taxonomy stands in for TabFormer's MCC codes | DEVIATES-BY-CHOICE | an MCC taxonomy would be its own round |
| Merchant geography | the exporter resolves each catalog merchant's world-modeled `Record.location` through the build-fixed geography catalogue; a physical record with a valid area emits one internally consistent Has_City/Has_State/Has_Zip plus Assigned_To/Located_In chain; `online` records and non-catalog destinations remain geography-free. City.population is copied from the world's `GeoArea.population`. No exporter geo hash or PII zip draw remains | CHOICE (world-state reporting) | **Current input is a 71-US-city + 15-international placeholder, not Census-complete; row order defines `GeoAreaId` and land area is NOT loaded, so true DENSITY is not computable.** Target provenance: Census Gazetteer + ACS |
| Party.gender | content-keyed even F/M split — gender is NOT modeled anywhere in the world | CHOICE | mechanism-free |
| Party.created_at | the Membership joinTs, identical to the public-schema customer table | CONFORMS | prohibition formally LIFTED |
| Is_Merchant | UNPOPULATED (header-only): the world has no merchant-owning-party link | DEVIATES-BY-CHOICE (documented gap) | — |
| PII layer | Address/Phone/Email/ID(ssn)/Full_Name/DOB vertices deduplicated over the roster; TF_GNN_v3 marks this layer DEMO ONLY and it is empty on real TabFormer | CHOICE (the use case's differentiator) | — |

**Anti-shortcut gates (all measured at 0.0000 or better).** Fraud draws
its card-rail destinations from the same merchant acceptance population
legitimate sessions use — card-present from the victim's distance-decayed
pool using the SHARED decay kernel, card-not-present from the online
footprint by popularity. A flat national draw would have traded the
merchant shortcut for a DISTANCE shortcut. Attacker IPs come from
`randomIpv4`, not TEST-NET-2. Measured: fraud-only-merchant row share 1.0
→ **0.0000**; merchant-ID-only recall@precision≥0.90 **0.0000** against a
gate of <0.25.

═══════════════════════════════════════════════════════════════════════
# SUPERSEDED CLAIMS
═══════════════════════════════════════════════════════════════════════

Every claim this document once asserted and later measured false. Kept
because each one produced a law, and because a reader with only the
corrected row would re-propose the error.

| The claim | What falsified it | The law it produced |
|---|---|---|
| CTR fires at ≥ $10,000, any channel | eCFR 31 CFR 1010.311: strictly MORE THAN $10,000, currency only. Both defects were confirmed in code (`>= 10000.0`, no channel filter) | Verify statutory boundaries against the primary text, not recollection |
| Fraud budget deviation is "a few bp" vs real card fraud | US card fraud is ~11 bp of VALUE (Nilson), 17.6 bp debit (Fed) — and PL's 12 bp is a share of COUNT | **Never conflate prevalence axes.** The count-vs-value oversampling factor remains UNKNOWN, not "~100×" |
| Credit-card full-payment share .35 vs measured ~.58 → NONCONFORMING | The .35 is the MANUAL-payer mixture; autopay-full (.40) bypasses it. Effective population share is .575, sitting on the measured ~.58 | **Conditional vs marginal.** Verdict reversed with no code change |
| Student employment .12 is a third of the measured ~40% | The salary selector's fit target scale-clamped every persona except retirees to ~100%. The printed table was BASE WEIGHTS, not behavior — effective student employment was 2.5× the measured rate, not a third of it | **Read the selection function before characterising a distribution.** Every prior verdict on the row had inherited the misread |
| Card fraud spend CONFORMS (PL mean $162 vs UK ~$167) | Compared PL's PER-TRANSACTION mean to a PER-CASE average. On the per-case axis PL runs ~8× | **Per-txn vs per-case.** Re-classed CHOICE (label density), which is defensible; the near-match was coincidence |
| Miss share .05 vs delinquency ~3–4% | PL's is a per-statement FLOW; the benchmark is a point-in-time STOCK (accounts 30+ dpd) | **Flow vs stock.** They coincide only because cure is ~one cycle; map through cure duration before comparing |
| Rent mean $850 CONFORMS (DCPC per-transaction $824) | Each PL renter is a SOLE TENANT paying one full household rent; no roommate split exists. The household axis governs, so ACS ~$1,487 is the comparator | A unit definition is a CODE-READING decision, not a literature decision |
| Severity buys MORE gift cards for older victims | Grading the card COUNT put an 80-year-old at 13 × $500 = $6,500 in four hours out of a retail checking account — mostly unfundable rows the ledger discards, i.e. a decline burst no FTC spotlight describes | **The PLAN was wrong, not just the code.** Severity applies to the impostor AMOUNT alone; the gift-card rail is ungraded in both denomination and count |
| The gate harness measured the shipped population | `GateWorld` defaulted `withJoinCohort = false` → no join cohort, while production sets it unconditionally. Every behavioural band in the card arc had been calibrated against a world the generator never emits; `test_arch_equivalence` reported it as a settlement-side "SEMANTIC divergence" and the diagnostics blamed the wrong layer for a round | **An equivalence gate must PIN THE WORLD SHAPE it assumes.** And: when a harness default exists to freeze existing gates, every gate comparing against PRODUCTION must opt out of it in the round the default is introduced |
| The world-shape witness should be re-derived from `joinerCount()` | Both legs would then evaluate the same formula on the same inputs and could never disagree — the pin would go vacuous against the exact bug it exists for | **A precondition that exists to catch a CONSTRUCTION failure must MEASURE the construction, never re-derive it** |
| Per-year deflated card fraud amounts flat < 2.5× is "the only gate proving class F reaches the card rail" | The card view mixes two FIXED-NOMINAL lattices (M-3 class F-lattice) with one CPI-scaled sampler, so deflating the combined mean asserts the OPPOSITE of U-6 — and the ring-rail gate already excluded that rail for that reason, so the two gates contradicted each other. Independently under-powered: 42–92 lognormal(σ1.2) draws give CV 19–28% on a per-year mean. Purging the resolvable lattice made the spread WORSE (2.69× → 3.11×), the signature of noise | **A flatness gate over an aggregate mixing era-scaled and fixed-nominal families measures the MIXTURE WEIGHTS, not the scaling.** Withdrawn and replaced by a cross-era deflated-QUANTILE gate that sizes its own band from realized n and fails as UNDER-POWERED unless that band excludes the fixed-nominal null. **Prefer an effect you can see over a null you must resolve** |
| Population 900 exercises both solo and ring card spends | The gate's own first run printed `ring 0` in both legs. `buildCompromisePlans` excludes ring participants and victims, so the unauthorized card rail is ring-free BY DESIGN at every population | **Audit the justification you wrote against the gate's own printed output** before calling a round done. The ring counter is retained as a TRIPWIRE, and is documented as one |
| The TEST-NET attacker-IP claim is stale (grep found nothing) | The defect was written in INTEGER OCTET form: `Ipv4::pack(198, 51, 100, …)` | **Grep the constructor, not the rendered literal**, before calling an audit claim stale |

═══════════════════════════════════════════════════════════════════════
# OPEN ITEMS
═══════════════════════════════════════════════════════════════════════

Nothing below is a known numeric contradiction; all known contradictions
have been resolved by shipped ADJUSTs or documented CHOICEs.

**1. Citations to pull verbatim at the owner's verify pass.** eCFR
section-text snapshots; FATF *Professional ML* (2018) page cites; a named
Visa card-testing advisory; FinCEN structuring guidance and FFIEC manual
pages; ISS/III auto claim frequency-severity; FSA portfolio IDR shares;
BLS Employee Tenure 2024; OEWS May 2025 refresh; Atlanta Fed tracker
current print; the Diary cash-WITHDRAWAL (not payment) supplement for the
ATM amount row; BLS CPI-U and U-3 direct reads (bls.gov timed out during
the audit session — values remain standard published annual averages).
*Cash split:* IRS Cash Intensive Businesses ATG; Census SUSB/CBP; Square
"Making Change"; Yale Budget Lab (Jun 2024); Fed SHED gig + EIWA 2015;
BLS student-employment industry mix. *Scam/fraud:* FTC gift-card Data
Spotlights; FTC CSN payment-method mix (the rail split); retailer $500
per-card caps; Reg Z / §1643 + network zero-liability; Security.org
reimbursement share (the p .85); UK Finance 2026 per-case averages;
FTC CSN age-band incidence and loss tables (both victimization
gradients). *Household:* ACS B25064 + renter share; CPS renter turnover;
NCES/BLS student employment; Greenlight/Till allowance data; Diary Table
13 gas and Table 8 mobile-app; home premium ~$2,530/yr; III/ISO home
severity; IRS 2025 average refund; SSA Monthly Statistical Snapshot.
*Card:* S-DCPC Tables 3 and 4; an issuer autopay-enrollment source (the
{.40/.10/.50} split is UNCITED); 12 CFR 1005.6/1005.11 for the Reg E
design. *Macro:* NFDA GPL surveys; SCF intergenerational transfers;
SSA OASI claiming-age tables.

**2. Thin tail — expect CHOICE outcomes.** L-9 parental/sibling/
grandparent/gift/inheritance distributions (SHED gives incidence only);
L-10 freelancer and small-business revenue profiles (platform earning
studies are non-comparable); L-11 merchant/landlord/counterparty
densities (already re-classed CHOICE).

**3. Funnel calibration.** Fit SAR p and alert-to-case against the FinCEN
FY2024 anchors (4.7M SARs, 20.5M CTRs, fraud-typed ~52%) as SANITY BANDS
under deliberate oversampling. Re-measure CTR liveness at each re-pin.

**4. Owner-gated designs.** ATO Reg E remediation (needs a
bank-remediation counterparty plus a NEW credit channel — reusing
`cc_chargeback` would corrupt the F-4 reporting row's semantics);
homeowner/renter overlap (wire `isHomeowner` into `RentRoll`); a student
part-time wage tier; the victim-own-device carrier for authorized-push
rows (attaching the attacker session is wrong, and routing the victim's
device through `infra::Router` would perturb legitimate routing).

**5. Registered model upgrades.** Per-cohort SSA claiming shares;
historical-period mortality tables and SES gradients; survivor benefits;
an SCF-anchored estate-size re-derivation; a dedicated funeral
counterparty/channel; repeat founders; surfacing latent sex to PII with a
measured population ratio; a cash-share era model; labor-market
separation spells and within-year NBER recession shading; the EIP
statutory table; a monthly unemployment path; exporting the real
card-present modality into `use_chip`; transaction-time device/IP edges;
an MCC taxonomy; the Census Gazetteer geography round (blocking any true
density model); compromise-incidence scaling by home-area population
(Bettencourt β ≈ 1.15 — must land AFTER a home-area-only baseline, or
the tilt has no gate); elder-specific scam sub-typologies (FinCEN /
CFPB SAR calibration); a per-era scam payment-method mix.

**6. The one gap that blocks a benchmark CLAIM.** LEVEL calibration of
card-fraud and scam prevalence against a named issuer-side series
(Nilson / FTC), including the CNP share. This arc measured separability
and stability, not level. The README states this without overclaiming.
