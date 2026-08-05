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
| Compromised card instrument | unauthorized `Rail::card` rows currently use the victim's primary account and export as derived debit. A Round 7 attempt to substitute an issued credit-card liability was reverted after tracing lifecycle order: fraud is planned after `CardCycleDriver` has already closed statements and generated payments/interest, so the swap created unserviced debt | KNOWN GAP + correctness guard | existing-card misuse spans credit and debit | DEVIATES: `test_card_prevalence` requires zero late-injected credit-liability sources until fraud planning is integrated into card lifecycle servicing |
| ATO drain | median ≈ $180, mean ≈ $554, clamp [$10,$85k]×priceScale, ~0.4% ≥ $10k (PER DRAIN TRANSACTION). Per-CASE: targetEvents U{3..8} ⇒ ≈ $3.0k | MEASUREMENT | UK Finance 2026: remote-banking fraud £104.4M over 37,646 cases = ~£2,773 (~$3.5k) per CASE [Certain, Derived]. PL ≈ 87% of that | CONFORMS as a band |
| Unauthorized rail mix | card compromise .48 / gift-card scam .12 / impostor push .12 / ATO .28 | MEASUREMENT-adjacent | FTC CSN payment-method report mix. The two authorized rails are weighted EQUALLY: CSN names gift cards the most-REPORTED scam payment method of the era and bank transfers the largest by reported LOSS | UNCITED (verify list) |
| Gift-card scam (victim-AUTHORIZED) | 2–6 cards/case in ONE 1–4 h coached burst; denominations 75% {$100,$200,$500 triple-weighted} else $50–$500 in $10 steps (mean ≈ $339/card ⇒ ≈ $700–2,000/case, test-pinned); retail merchants; channel `card_purchase`; label `scam_gift_card`; NEVER reimbursed; UNGRADED by victim age in both denomination and count | MEASUREMENT-adjacent | FTC gift-card Data Spotlights: most-reported scam payment method for several years; ~$217M reported losses 2023; victims coached to buy multiple max-denomination cards; retailer per-card caps commonly $500; median per-scam losses $500–$1,000 [Likely on vintages] | UNCITED (verify list) |
| Impostor push (victim-AUTHORIZED) | `FraudType::scamImpostor`; 50/50 over `externalUnknown` (wire-shaped) and `p2p`; `scamWireAmount` LN($900, σ1.3) clamp [$50,$50k] × priceScale(era) × age severity; NEVER reimbursed | CHOICE (magnitudes) + MEASUREMENT (order of magnitude) | UK Finance APP-fraud reporting puts per-case losses one to two orders above card-rail fraud [Certain on the ordering]; FTC CSN medians. Crypto DECLINED — the era lock ends the window in 2020 | UNCITED (order of magnitude anchored) |
| Victim susceptibility, two OPPOSITE gradients | incidence FALLS with age (bands 1.35/1.30/1.15/0.95/0.75/0.60/0.50 by decade from the 20s); severity RISES (0.70/0.80/0.90/1.00/1.30/1.70/2.20, ~3× span). Persona factors carry NON-AGE structure only: student 1.10, freelancer 1.15, smallBusiness 1.25, salaried/highNetWorth/**retiree all exactly 1.00** (anti-double-count — persona and age are strongly correlated). Tilt share 0.65, clamp [0.25, 3.00]× the eligible mean | MEASUREMENT (directions) + CHOICE (magnitudes) | FTC CSN Data Books (reports peak in the 20s–30s, decline after 60); FTC "Protecting Older Consumers" reports to Congress (median reported loss climbs monotonically, oldest band ~3× the youngest) [Certain on both directions] | directions CONFORM; magnitudes DEVIATE-BY-CHOICE |
| Card-fraud reporting | per-case reported p .85 → every fraudulent SPEND made whole by a merchant chargeback credit (flag-0, `cc_chargeback`, lag 1–10 d, OUTSIDE the fraud budget); sub-$5 test charges never reimbursed | MEASUREMENT-adjacent | Reg Z / 15 U.S.C. §1643 caps unauthorized-use liability at $50 and network zero-liability waives it [Certain on the statute]; Security.org: the large majority of card-fraud victims are made whole [Likely on the share] | UNCITED (statute Certain) |
| NO reimbursement on either AUTHORIZED rail | gift-card and impostor-push rows are never made whole | MEASUREMENT (regulatory) | Reg E (15 U.S.C. 1693) covers UNAUTHORIZED transfers only; the UK reimbursement code postdates the corpus window. The asymmetry against the mostly-reimbursed card rail is a MODELED FACT, not an omission | CONFORMS |
| Membership across a case | every rail requires `[joinTs, closeTs)`, where `closeTs = death + 120-day settlement`. Authorized scams additionally require ALIVE. Planning resolves the earliest relevant exclusive horizon (victim close, victim death for authorized rails, owned payee close) and accepts a case only when its full sampled span fits; the keyed generator defensively rejects malformed plans and suppresses post-horizon chargebacks | MEASUREMENT (defect repair) + CHOICE (declared scope) | Deceased-identity fraud advisories; estate settlement | CONFORMS; card/ATO may occur after death only inside the declared estate tail |
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
| Known-biller preference / exploration / commerce evolution | .55, retry limit 6, pick attempts 250; exploration base .02/txn, propensity Beta(1.6, 9.5), bursts .487/yr for 3–9 d; merchant add .35 / drop .10 **per MONTH** (max **30**, was 40), contacts add .08 / drop .03 (max 20) | CHOICE | — | — (two corrections in this row: the cadence is monthly, not daily — the evolver's only hook is a month boundary; and `maxFavorites` moved 40 → 30, see L-2b) |

### L-2b. Merchant selection: REACH vs VOLUME (`merchant-selection-2026-08`)

**Why this section exists.** `Record.weight` is a VOLUME weight and was being
used unchanged as the sampling law for favourite-set MEMBERSHIP, which is a
graph EDGE. With `favK ~ U[8,30]` draws that turns a weight `w` into
`P(card → merchant) = 1 − (1 − w)^favK` — a favK-fold amplification of a volume
weight into an edge probability. Measured at the owner's 8,000-person /
20-year run: the top merchant's ~5% volume weight became a **51% share of all
68,618 cards**, the monthly evolver's ratchet took that to **85%**, and
`P(two random cards share a merchant)` was **1.000**. The merchant COUNT was
never wrong (570 base + 563 churn births = 1,133 records ≈ 712 per 10,000
people, inside both anchors in the first row below).

| Parameter | PL value | Class | Real-world anchor & source | Status |
|---|---|---|---|---|
| Merchants per 10,000 population | 250 floor + 120/10k core + 400/10k tail ⇒ 712/10k at pop 8,000, 520/10k at pop ≥ 20,875 | MEASUREMENT | **Census CBP 2022** `cbp22us.txt`: 8,298,562 employer establishments / 334,017,321 = **248.4 per 10k** (CBP 2023 gives 248.25, stable to 0.1%); consumer-facing (NAICS 44-45 + 72 + 81 + 71) 2,778,542 = **83.2 per 10k**. **Nilson Report YE2024**: 34M US in-store + online card-accepting locations / 340,110,988 = **999.7 per 10k** (SECONDARY — press release; the report is paywalled and undecomposed) [Certain for CBP] | CONFORMS as a band. **The count was never the defect** — it sits between the CBP floor and the Nilson ceiling at every population |
| Top-1 merchant CARD REACH | **0.12 target, 0.25 hard ceiling** (`kTargetTop1Reach`, `kMaxTop1Reach`) | CHOICE | **No published source reports per-card merchant reach.** Krumme et al. give the card side only. Numerator's household ladder (Great Value 86% of US households in 12mo to 6/30/24, McDonald's 87%, Amazon 83%) is **BRAND** granularity and is the wrong anchor: `place.hpp` gives every non-online record one GeoArea and the exporter writes one `cf_Merchant_Location` centroid per record, so a Record is an ACCEPTANCE LOCATION (`entities/counterparties/merchants.hpp` states this outright). Band anchored on the consequence: R-GCN's fixed `1/|N_i^r|` normalisation is "particularly problematic for nodes of high degree" — Schlichtkrull et al., ESWC 2018 §5.1 | **CLASS S UNCITED at level.** Measured 0.830 → **0.129** (pop 300) and 0.356 → **0.107** (pop 2,000). Gated by `test_card_merchant_graph` sub-gates A/B, both disarms red |
| Membership flattening exponent γ | solved by fixed-iteration bisection so `max π = target`; π = 1 − (1 − q)^k̄, q ∝ w^γ | DERIVED | — | Draw-free and stateless (batch/windowed lockstep). PRINTED by sub-gate E, which reds if it pins at a bound — a solved constant that saturates silently is the failure `merchant-churn` rule 6 records twice |
| Within-card visit rank law | Zipf, α = **0.80** (`kVisitZipfAlpha`); pseudo-rank is a draw-free hash of (person, merchant) | MEASUREMENT | **Krumme, Llorente, Cebrian, Pentland, Moro, "The predictability of consumer visitation patterns", Scientific Reports 3:1645 (2013)**, Results + Fig. 1: P(r) ∼ r^−α with α = **0.80** (North American issuer, >50M accounts) and 1.13 (European, 4M); top merchant takes **~13%** (NA) / ~22% (EU) of that cardholder's visits; law holds independent of set size [Certain] (accessed 2026-08-04) | CONFORMS. Was **UNIFORM** (α = 0), scoring 1/F = 5.3% at F=19. Measured after: **0.183** absolute, ratio **3.65** against the same-cards baseline vs **2.12–2.18** disarmed. Sub-gate F evaluates the cited arithmetic (Σ r^−0.80 over r=1..64 = 7.067 ⇒ 0.1415 vs published 0.13) rather than restating it |
| Favourite-set size | seeded U[8,30]; cap `maxFavorites` 40 → **30** | CHOICE at level | **Alessandretti, Sapiezynski, Sekara, Lehmann, Baronchelli, "Evidence for a conserved quantity in human mobility", Nature Human Behaviour 2:485-491 (2018)**: ~25 familiar locations, **size CONSERVED while membership turns over**, ~40,000 individuals over multi-year traces [Certain] (accessed 2026-08-04) | CITED for the conservation property. The old cap **exceeded what the seed could produce** (40 > 30), and with add 0.35/mo against a UNIFORM drop 0.10/mo every set grew monotonically past its own ceiling: measured **19.1 → 37.8** over 240 monthly steps |
| Home→favourite distance (physical) | **1,206 mi mean, 4.96% within 50 mi** | **3.8 mi mean, 97.3% within 50 mi** (pop 500,000) | Membership is now home-conditioned through the distance-decay pool; a `Record` is an acceptance LOCATION with one centroid, so a nationwide holder base is impossible | **CLOSED.** Also closes the inverted distance shortcut: fraud card-present sits 0–11 mi from home, so a 1,206-mi legit mean gave `within 50 mi ⇒ fraud` ~7x lift at a 0.31% base rate. Gated by `test_card_merchant_graph` sub-gate H at BOTH scales; disarm (ignore the resident's home) reds at 1,184 mi / 0.158 |
| Card-not-present share of card payments, BY NUMBER | derived from catalogue mass, **0.118–0.138**, era-FLAT | **dated series: 0.010 (1991) → 0.271 (2019) → 0.362 (2022) → 0.416 (2026)** | **Federal Reserve Payments Study, National Payment Volumes Detailed Data (CY 2021 and 2022): "In 2022, in-person payments were 63.8 percent of total GP card payments by number"** ⇒ remote 36.2% [Certain] (accessed 2026-08-05). SHAPE from Census Quarterly Retail E-Commerce Sales (e-commerce share of retail sales, published from 1999), scaled by **2.46** so 2022 lands on the Fed anchor | **CITED for the 2022 level and the shape.** The 2.46x multiplier and all pre-1999 points are CLASS S. Note against the common intuition: in-person remains the MAJORITY by count, nearly 2:1; CNP exceeds e-commerce's 16.9% of retail sales because it also carries phone/mail order, recurring billing and in-app. Rebuilt at every month boundary so it walks the calendar |
| Distance-decay pool memory | dense `areas × physicalMerchants` doubles, held twice: **26.75 MB** at pop 500,000, O(A·M) | **0.481 MB**, O(M + A·k) — **56x** | Centroid geometry: the within-area factor is home-independent, so the dense matrix stored one vector 71 times | Exact refactor for the within-area half; the inter-area cutoff discards a measured **2.7e-08** of reachable mass, asserted by sub-gate H. Unblocks extending `geo_data.hpp` past ~300 areas, which was a hard blocker at 197 MB / 1.24 GB |
| National top-1 VOLUME share | **emergent, printed, not imposed** — measured 0.64% at n=570, 0.48% at n=26,000 | MEASUREMENT | NRF 2025 Top-100 Walmart $568.70B / Census MARTS Dec-2024 $8,544,433M = **6.66%** of US retail+food; ~4–5% of the Fed Payments Study's $11.50T card value [Certain] | **DEVIATES BY CONSTRUCTION, and the deviation is correct at this granularity.** 6.66% is a BRAND number reachable only at brand reach (0.86 household penetration × ~8% within-household share ≈ 6.9%). National share ≈ reach × within-card share, so an outlet capped at 0.12 reach has a ceiling near 2%. Pinning both reach and national volume is OVER-DETERMINED — an earlier design that did so pushed the within-card top-1 share to **31%**, outside Krumme's cited band, i.e. it broke a cited quantity to hit an unreachable one |

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
| Payment timing | due cutoff 17:00 on the resolved due date; autopay at 12:00 that calendar day; manual late p .08, 1–20 d after the cutoff; late fee at 10:00 the following day. The old code added 12 h to an already-timed due value and therefore made autopay systematically late; `test_card_payment_timing` closes that defect, including weekend resolution | MEASUREMENT-adjacent mechanics | issuer delinquency curves | timing ordering CONFORMS; late probability UNCITED |
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
| MEMBERSHIP INTERVAL | [joinTs, closeTs): joinTs = window start for the seed roster, a drawn join day for the join cohort; closeTs = death + **120-day settlement**, sized to strictly contain the funeral (death+3–11d) and the estate (death+30–90d) so every estate row is corpus-visible before closure. The STANDARD exporter filters both owned endpoints. ROUND 7 also applies the interval in fraud victim selection and in the streamed card feature graph: authorized scams additionally require ALIVE, while card/ATO may use only the post-death settlement tail. The shared raw ledger remains the full generated corpus | CHOICE (owner directive: the population must both persist AND die) | deposit-account closure norms |
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
| Card attribution | source Key in the card registry → that credit card (≤1 per person); any other source → the account's derived debit card. Unauthorized positives currently use the latter path only; parsing the `C`/`D` identifier tag as a feature is prohibited | CHOICE (label definition) + KNOWN GAP | credit-fraud servicing must move into `CardCycleDriver`; expiry, replacement, reissue, and multiple-card histories also remain unmodeled |
| Identifier scheme | C/D/M = prefixed role.bank.number of the entity Key; P&lt;person&gt;; T&lt;row_seq&gt;. Party ids are the CANONICAL customer ids and Merchant/IP use canonical renderings. Device IDs are stable opaque pseudonyms in one fixed-width `D` namespace; owner role is not exposed through prefix, width, or numeric range | CHOICE (identifier reuse/pseudonymization) | device IDs are categorical, not ordinals and not a security boundary |
| **Withheld entity labels** | `Card.is_fraud`, `Party.is_fraud`, `Device.is_blocked`, `IP.is_blocked` are FULL-WINDOW verdicts and render as **0**. Columns are RETAINED because TF_GNN_v3 loading jobs map POSITIONALLY. The verdicts live in dedicated `card_fraud."cf_Ground_Truth_Label"` (entity_type, entity_id, label) — positives only, pointed at by no edge, loaded by no job. The current export contains **37 tables** | CHOICE (owner ruling) | `Payment_Transaction.is_fraud` is the supervised target, never an input feature; production-like evaluation still needs delayed label availability |
| use_chip / error | **`use_chip` is CAUSAL since ROUND 8 (use-chip-causal-2026-07):** "Online Transaction" ⟺ the destination is a geography-free acceptance endpoint (catalog `Footprint::online` — the exact population both legitimate selection and the fraud rails draw card-not-present picks from — or a non-catalog remote biller); physically-located outlets split Chip/Swipe by the dated US EMV terminal mix (`chipShareBasisPoints`: 0 before 2012, .10 at the 2015 liability shift, .65 in 2019, frozen .90 outside coverage), with the per-row draw content-keyed on the retained `kUseChipLane`. `error` (2.0% incidence, mix Insufficient Balance .40 / Bad PIN .20 / Technical Glitch .20 / Bad Card Number .08 / Bad Expiration .05 / Bad CVV .05 / Bad Zipcode .02) REMAINS a content-keyed FNV hash — point-in-time SAFE but MECHANISM-FREE (authorization attempts are unmodeled) | MEASUREMENT-adjacent (entry mode; EMV curve values [Likely] — owner verifies vs the EMVCo US chip-share series) + CHOICE (`error` compatibility; non-catalog→Online) | IBM TabFormer supplies the vocabulary. Gate: `test_card_use_chip` (coherence, pre-EMV zero-chip, 2019 band). A modeled authorization-outcome mechanism for `error` is REGISTERED; see the use-chip-causal amendment below |
| Transaction-time sessions | `Transaction_Uses_Device(txn_id, device_id, edge_unix_time)` and `Transaction_Uses_IP(txn_id, ip_id, edge_unix_time)` carry the exact assigned row session. They are append-only stream-prefix edges; score a transaction from prior state before appending its current edges. `Has_Device` / `Has_IP` are header-only loader-compatibility tables | INVARIANT (causal export contract) | one edge per assigned endpoint, test-pinned against the payment stream |
| Device/IP vertex and topology closure | `cf_Device` and `cf_IP` are the union of synthesized roster infrastructure and endpoints actually observed in card-view rows; roster flag/blacklist facts remain withheld in vertices and quarantined in the overlay. Static Party ownership is withheld for all endpoints because exogenous attackers have no truthful Party owner | INVARIANT (referential integrity + anti-shortcut) | closes both missing-vertex and missing-Party-edge shortcuts without inventing ownership |
| Legitimate credit-card sessions | the access-router owner map merges card-registry ownership with deposit-account ownership, without inserting liabilities into the deposit-account slices. Legitimate credit-card purchases therefore receive the owner's normal device/IP session | MECHANISM REPAIR | closes the “credit-card row has no session” modality shortcut |
| mer_cat granularity | the 10-category merchant taxonomy stands in for TabFormer's MCC codes | DEVIATES-BY-CHOICE | an MCC taxonomy would be its own round |
| Merchant geography | the exporter resolves each catalog merchant's world-modeled `Record.location` through the build-fixed geography catalogue; a physical record with a valid area emits one internally consistent Has_City/Has_State/Has_Zip plus Assigned_To/Located_In chain; `online` records and non-catalog destinations remain geography-free. City.population is copied from the world's `GeoArea.population`. No exporter geo hash or PII zip draw remains. **AMENDED merchant-coordinates-2026-07: the same catalogue row's `latitudeE6`/`longitudeE6` now also ship, as decimal degrees on `Merchant_Location` (per merchant), `Zipcode` and `City` — AREA CENTROIDS, so co-located merchants share a point** | CHOICE (world-state reporting) | **Current input is a 71-US-city + 15-international placeholder, not Census-complete; row order defines `GeoAreaId` and land area is NOT loaded, so true DENSITY is not computable.** Target provenance: Census Gazetteer + ACS |
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
`randomIpv4`, not TEST-NET-2. Device identities use one opaque role-neutral
namespace, and every observed session endpoint is materialized as a
vertex before an edge can reference it. Static `Has_Device`/`Has_IP`
tables are header-only, so Party adjacency cannot expose endpoint role.
Since ROUND 8 the exported `use_chip` reads the same footprint axis the
selection uses, so the flat feature AGREES with the graph structure
instead of contradicting it — a real, modeled CNP-majority correlation,
not a new shortcut. Measured: fraud-only-merchant row share 1.0 →
**0.0000**; merchant-ID-only recall@precision≥0.90 **0.0000** against a
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
design; the EMVCo US chip-transaction-share series and the networks'
October 2015 liability-shift milestones (the ROUND 8 `use_chip` EMV
curve values are [Likely]). *Macro:* NFDA GPL surveys; SCF
intergenerational transfers; SSA OASI claiming-age tables.

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
part-time wage tier. The victim-session item formerly listed here is
CLOSED by the victim-session amendment below and needs no new carrier.

**5. Registered model upgrades.** Per-cohort SSA claiming shares;
historical-period mortality tables and SES gradients; survivor benefits;
an SCF-anchored estate-size re-derivation; a dedicated funeral
counterparty/channel; repeat founders; surfacing latent sex to PII with a
measured population ratio; a cash-share era model; labor-market
separation spells and within-year NBER recession shading; the EIP
statutory table; a monthly unemployment path; an MCC taxonomy; the
Census Gazetteer geography round (blocking any true density model);
compromise-incidence scaling by home-area population (Bettencourt
β ≈ 1.15 — must land AFTER a home-area-only baseline, or the tilt has
no gate); elder-specific scam sub-typologies (FinCEN / CFPB SAR
calibration); a per-era scam payment-method mix. **Card depth still
open:** effective expiry, renewal, compromise replacement/reissue,
product migration, multiple instruments over a lifetime, and dated
card-adoption/authentication/CNP mechanisms. Integrating unauthorized
credit-card events into statement/payment/interest servicing is a
separate P0 prerequisite. Transaction-time device/IP edges are CLOSED
by ROUND 7. Exporting the real card-present modality into `use_chip` is
CLOSED by ROUND 8 (use-chip-causal amendment below); still registered
from that design: a MODELED authorization-outcome mechanism to replace
the `error` hash, a time-varying legitimate CNP share, and the EMV-curve
citation pull in item 1.

**6. Gaps that block a benchmark CLAIM.** LEVEL calibration of
card-fraud and scam prevalence against a named issuer-side series
(Nilson / FTC), including the CNP share; credit-card fraud integrated
before lifecycle servicing rather than relabeled afterward; era/concept
drift in compromise incidence, payment method, authentication and
attacker infrastructure; delayed report/chargeback/verdict availability
rather than an instantaneous target; and an executable GSQL temporal
feature query, training pipeline, temporal splits, baselines and
evaluation harness.
The current arc measures separability and stability of an export, not
an end-to-end production detector. The README and online-GNN contract
state this without overclaiming.

═══════════════════════════════════════════════════════════════════════
# AMENDMENT — victim-session-2026-07
═══════════════════════════════════════════════════════════════════════

**Supersedes the victim-own-device clause formerly carried in OPEN
ITEMS #4**, which is CLOSED. Its parenthetical rationale ("routing the
victim's device through `infra::Router` would perturb legitimate
routing") was FALSE when written.

| The claim | What falsified it | The law it produced |
|---|---|---|
| Attaching the victim's own device to an authorized-push row requires a new carrier, because routing from the fraud planner would advance that person's sticky index and diverge the two engines | Every unauthorized row already has `ringId = -1`, `source = victimAccount` and a customer-session channel, so `transactions::Factory::make` ALREADY called `routeDeviceFor`/`routeIpFor` for the victim on the plan's rng lane and wrote the result — which `unauthorized.cpp` then overwrote. The sticky advance was already being paid; the fix removes two lines and adds no draws | **A deferral rationale is a claim and rots like any other: re-read the path and check the cost is not ALREADY BEING PAID before building the carrier it asks for.** And: before adding a carrier, check whether the value is already on the row |
| A non-advancing `devicesByPerson[person].front()` read is the safe way to attach the victim's device | It pins authorized rows to pool slot 0 while the SAME victim's legitimate rows follow the sticky index, so "device != this person's current device" becomes the replacement label | **The more explicit fix can be the one that opens the shortcut.** Systematic difference from a person's own legitimate rows IS a label |

**Session semantics, now normative.** `card`/`ato` = attacker device+IP
(third party with stolen credentials — the exogenous session is the
model). `giftCardScam`/`scamImpostor` = the VICTIM's routed session (the
victim is the operator). Gated in `tests/test_unauthorized_keyed.cpp`;
the card/ato half is a TRIPWIRE. CLASS: TYPOLOGY. Status: CONFORMS.

**7. HISTORICAL ROUND 6 finding: the `FD` device render was a
DETERMINISTIC label.** The old `OwnerType::ring` branch wrote literal
`FD…` while person and legitimate-shared devices used distinguishable
layouts. That was strictly stronger than the TEST-NET-2 shortcut
(deterministic, not 1-in-14M). ROUND 6 correctly retained attacker
session semantics on card/ato and declared the exporter-side leak.
**SUPERSEDED BY ROUND 7:** every assigned identity now renders through a
stable opaque digest in one fixed-width `D` namespace. The gate rejects
role prefixes and width/range differences. `device_id` is feature-safe
only as a categorical identifier; parsing or ordering it is prohibited.

═══════════════════════════════════════════════════════════════════════
# AMENDMENT — card-session-lifecycle-2026-07
═══════════════════════════════════════════════════════════════════════

ROUND 7 records the structural repairs and the lifecycle blocker found
while tracing the card graph end to end:

| Finding | Repair | Residual scope |
|---|---|---|
| Join-only victim selection allowed cases after account closure, and a valid case start could expand across death/closure | All rails require `[joinTs, death + 120d settlement)`; authorized scams also require alive. The complete sampled case span must fit before the earliest victim/payee horizon, and post-horizon chargebacks are suppressed. The streamed card view independently applies Membership to both owned endpoints | the raw ledger retains valid full-world rows, while neither generator nor card graph carries an out-of-interval owned endpoint |
| Every card-compromise positive uses a deposit account and therefore exports as a derived debit card | An attempted issued-card key swap was reverted: fraud is planned after card cycles are serviced, so it bypassed statements, payments, interest, and screening. Keep debit-backed truth and test-reject the false swap | OPEN: move fraud planning into `CardCycleDriver`, then model credit/debit mix plus expiry/reissue/replacement |
| Credit-card keys were absent from the access-router owner map, leaving legitimate credit-card rows without ordinary owner sessions | Merge card-registry ownership into the router map without treating liabilities as deposit accounts | device adoption/replacement dynamics remain era-flat |
| `FD` / person / legitimate-shared layouts exposed device role | Stable opaque field digest plus one fixed-width `D` layout for every assigned identity | pseudonym, not cryptographic boundary; categorical use only |
| Card graph had only whole-window Party→Device/IP associations and omitted exogenous attacker endpoints from the vertex roster | Add timestamped `Transaction_Uses_Device` / `Transaction_Uses_IP` stream edges; make Device/IP vertices the union of roster and observed endpoints; emit `Has_Device` / `Has_IP` header-only so missing Party adjacency cannot label exogenous endpoints | online scoring uses event-time edges; no false Party ownership is invented |
| The due timestamp already carried an hour, but autopay added another 12 h and therefore landed late | One 17:00 resolved cutoff; autopay noon same day; manual samples ordered around the same cutoff; late fee 10:00 next day | late-rate calibration remains UNCITED |

The card-fraud schema is now **37 tables**. These repairs support a
causal point-in-time feature construction under the written contract;
they do not supply the remaining benchmark claim: dated fraud concept
drift, delayed labels, real modality calibration, or the external
GSQL/training/evaluation implementation.

═══════════════════════════════════════════════════════════════════════
# AMENDMENT — use-chip-causal-2026-07
═══════════════════════════════════════════════════════════════════════

ROUND 8 (exporter-only) closes the entry-mode half of the PART IV
`use_chip / error` row and supersedes its former "the real card-present
modality … is NOT exported" clause.

| Finding | Repair | Residual scope |
|---|---|---|
| `use_chip` was a content-keyed FNV hash (Swipe .63 / Chip .26 / Online .11) for fraud and legitimate rows alike: point-in-time safe but mechanism-free, and INCOHERENT with the graph — a physical outlet could render "Online Transaction", a 1994 row could render "Chip Transaction", and the flat feature could contradict the merchant-geography structure a model also sees | Entry mode now reads the destination's ACCEPTANCE ENVIRONMENT — the `Footprint` axis both legitimate selection (`payments.cpp pickMerchantIndex`) and the fraud rails (`unauthorized.cpp pickMerchantDestination`) already partition destinations on — so the export reads BACK the modality decision generation made. Online ⟺ catalog `Footprint::online` or a non-catalog remote biller (DECLARED CHOICE); physical outlets split Chip/Swipe by a dated US EMV terminal-mix table (0 before 2012, .10 at the Oct 2015 liability shift, .65 in 2019, frozen .90 outside coverage — the era-freeze convention). Per-row draw stays content-keyed on `kUseChipLane`: no generation randomness, no carrier, no row-schema change — the exporter-side instance of ROUND 6's "the value is already on the row" law | `error` stays a mechanism-free hash (authorization attempts unmodeled) — the open half of online-GNN gate 4. The chip/swipe split is a presentation-layer terminal-technology mix, not per-card/terminal adoption state (card-lifecycle gate). The legitimate CNP share (`kCardPresentShare` .89) is era-flat — its dated version belongs to the fraud-process era-drift gate. EMV curve values are [Likely], owner verifies (OPEN ITEMS #1) |

**Anti-shortcut disposition.** The causal `use_chip` correlates with the
label (fraud is CNP-majority by F-4's rail design; legitimate spend is
CNP-minority) — REAL signal, the same class as b-2's distance decay, and
NOT a new structural shortcut: the modality was already visible to a
graph model through merchant geography, so the round makes the flat
feature agree with the structure instead of contradicting it. The
merchant-ID baseline gate keeps the ceiling on destination-derived
separability.

Gate: `tests/test_card_use_chip.cpp` — coherence pin (Online ⟺
geography-free destination), pre-EMV zero-chip pin (1991), 2019
chip-share band behind a power precondition that FAILS as under-powered,
compile-time pins on the EMV table's fixed points; fraud-vs-legit CNP
shares PRINTED, not gated (instrument first). The feature contract
reclassifies `use_chip` USE WITH CARE → FEATURE-SAFE. GOLDEN IMPACT:
only `golden_tables_card_fraud.md5` re-pins; the corpus stream and the
standard/aml table goldens must NOT move. CLASS: exporter presentation,
MEASUREMENT-adjacent. Status: CONFORMS as entry mode; `error` remains
the documented gap.

# AMENDMENT — econ-wiring-power-2026-07
══════════════════════════════════════════════════════════════════

T3 disposition (owner ruling, 2026-07-27): the fraud-rides-L sub-gate of
`test_econ_wiring` is re-specified from a single-seed statistic to a
pinned seed-pair panel. HARNESS-ONLY; zero golden movement; the model
was measured unmoved before the gate was touched.

| Finding | Repair | What did NOT change |
|---|---|---|
| The single-seed parity carries sd ~0.076–0.086 (12 paired seeds, measured twice: campaign mean 0.9072 / panel mean 0.9095), while the defect the sub-gate exists for — a fraud budget pinned to population or window constants, parity collapsing to 1/legit-ratio ≈ 0.79 — sits ~1.6σ below the healthy mean. One seed cannot separate healthy from defective: the shipped seed drew 0.7795 (below the defect value itself) on a model 12 paired seeds put at p ≈ 0.16 no-effect, and the panel shows ~10% of re-rolls land below the old 0.80 edge. | The estimator is the MEAN over 12 pinned seed-pairs (same seed both eras, paired by construction; pair 0 reuses the shipped-seed legs). Each pair asserts its own preconditions (join cohort present, flagged rows > 30) and the panel must be COMPLETE. A derived power check FAILS the gate if its own sampling band (mean − t₀.₉₉₅,₁₁·se) cannot exclude the realized defect parity — under-powered is red, never vacuous green. Observed: mean 0.9095, se 0.0248, defect 0.7932 vs exclusion edge 0.8325. | The owner's 0.80/1.25 band edges — NOT widened, now judging a √12-tighter estimator with ~4.4 se lower-edge headroom. No band was derived to fit an observation; the edges predate the panel. |

Status: CONFORMS. `test_econ_wiring` green at the shipped configuration;
the suite's only red is cleared without touching model or goldens.

# AMENDMENT — email-minhash-2026-07
══════════════════════════════════════════════════════════════════

Owner-requested additive round: the card-fraud export gains an email
LSH similarity layer. Supersedes the "now 37 tables" clause of the
use-chip amendment by name: **the card-fraud schema is now 39 tables.**

| Addition | Construction | Safety basis |
|---|---|---|
| `cf_Email_Minhash` (bucket vertices) + `cf_Has_Email_Minhash` (Email → bucket edges, 10 per email) | The shared `exporter/common/minhash` LSH stack already serving the AML name/address buckets, via a new `emailMinhashIds` wrapper: normalize (trim + lowercase) → 3-gram shingles → 10-permutation signature → band buckets, prefix `EMH`, b=10 × r=1. Derived at export time from the distinct-email set already written to `cf_Email`; deterministic and draw-free, so the corpus stream cannot move. | No new information channel — a pure function of an already-exported string. No time axis, no label content. Card-view emails are customer-party PII only (the export loop and `cf_Party` share the same roster bound; ring/attacker identities carry no email), so bucket membership cannot encode role. Feature contract: FEATURE-SAFE as graph structure, bucket id opaque-categorical. |

Count sweep: all nine 37-table assertions updated (schema/export/
streaming headers, `test_pipeline_e2e` list + comment, `test_table_golden`
floor assert 37 → 39, tests/CMakeLists note, acceptance manifest + three
count checks). GOLDEN IMPACT: `golden_tables_card_fraud.md5` only — it is
currently unpinned (T2) and pins fresh with the new tables on the owner's
next `test_table_golden` run; corpus stream and standard/aml table goldens
must NOT move (verified: full non-PG suite 56/56 green, `test_run_golden`
digest unmoved). CLASS: exporter-only, additive. Status: CONFORMS.

# AMENDMENT — attacker-infra-2026-07
══════════════════════════════════════════════════════════════════

MODEL round. Closes the largest open defect in the card-fraud use case: **attacker session endpoints were minted one per compromise, so cross-victim endpoint reuse was ZERO BY CONSTRUCTION.** Supersedes by name, and inverts, the two INVARIANT rows above — "Static Party ownership is withheld for all endpoints because exogenous attackers have no truthful owner" (Device/IP vertex and topology closure) and the "`Has_Device` / `Has_IP` are header-only loader-compatibility tables" clause of Transaction-time sessions — together with the ANTI-SHORTCUT paragraph's sentence "Static `Has_Device`/`Has_IP` tables are header-only, so Party adjacency cannot expose endpoint role." All three were correct descriptions of a generator that no longer exists.

**THE FINDING, AND HOW IT SURVIVED FOUR GREEN GATES.** `buildCompromisePlans` wrote `Identity{ring, 0xACE00000 + seq, 0}` and a fresh `network::randomIpv4` into every accepted plan. Unique owner id per case plus an independent draw from a ~3.5e9 address space means no attacker device or IP was ever seen by two victims. "One endpoint touching many cards" is the reason to model card fraud as a graph rather than a table, and it was absent. **Four gates, the acceptance script and two normative documents all asserted things about these endpoints, and not one of them measured DEGREE:** they checked that every payment carried an endpoint, that the endpoint was present in the vertex table, that the identifier revealed no role, and that the ownership table was empty. A count of endpoints is not a measurement of the graph.

| Change | Construction | Safety basis |
|---|---|---|
| Attacker infrastructure is a WORLD entity with a lifetime | `infra::AttackerInfra` (entities layer) built by `synth::infra::attackers` on the isolated `{"infra","attackers"}` lane off the run seed. An operator is a CAMPAIGN, not a person: lognormal length (median 110d, σ 0.95) clipped to the window, holding 1–3 concurrent device lines and 1–3 IP lines, each line a `timeline::sampleChain` replacement chain that TILES the campaign. Case load is Pareto(α 1.35, cap 80) so a few operators work many cards. Operators are deliberately NOT roster Parties — the attacker population is exogenous and far larger than any plausible in-corpus fraud cohort, and making them customers would have traded a missing signal for a false one. | Cross-victim reuse MEASURED at 74–82% of attacker devices seen by >1 victim, mean 5.1–8.8 victims, max 37–40; IPs 59–71% shared, mean 3.2–5.9. Gated by `tests/test_card_endpoint_graph.cpp` sub-gate A on mean AND tail — a flat degree distribution passes a mean-only gate while carrying none of the structure an alert fires on. Non-vacuity CONFIRMED by disarming reuse: fan-out collapses to mean 1.03 / max 3 and 12 checks go red. |
| Endpoint resolution is DRAW-FREE and whole-case-span | `operatorAt(u, ts)` / `deviceAt(op, ts, endTsExcl, salt)` / `ipAt(...)` are point queries with no sticky state; the planner spends exactly FOUR uniforms per plan — the same four `randomIpv4` used to spend — drawn unconditionally so no branch can change the count. An endpoint must cover the WHOLE half-open case interval, not merely be live at the case date. | Draw-count preservation VERIFIED against its own prediction: `golden_run.b2sum` rows **189,035 → 189,035** with only the digest moving, so every rail, event count, amount and timestamp is bit-identical and the corpus delta is confined to `device_id`/`ip_address`. Statelessness is what keeps both engines in lockstep — `test_arch_equivalence` and `test_spool_equivalence` green. Whole-span coverage makes sub-gate D a HARD ZERO (device 0, ip 0 outside tenure), not a band. |
| Ownership topology RESTORED, asymmetry removed in the GENERATOR | `infra::enrollment` models the institution's endpoint registry as INCOMPLETE — a draw-free hash of (party, endpoint), coverage 0.72 device / 0.61 address — and `Usage::enrolled` carries it. `Has_Device`/`Has_IP` export the on-file associations from `world.infra.*.usages`, never from the stream. The other direction comes from the planner: 18% of unauthorized cases are operated from the VICTIM'S OWN endpoint (remote-access / household compromise) and 30% of operator sessions exit through a RESIDENTIAL PROXY — some other customer's address. | Residual "endpoint not on file ⇒ fraud" precision **0.027 (device) / 0.016 (ip)** at **2.9x / 1.8x** lift, against 1.0 for the attacker half before the round. Sub-gate C bands the precision ceiling AND requires the lift to stay above 1.0, because replacing a shortcut with pure noise is the opposite error — an un-enrolled endpoint IS riskier in production. Both tables stay WORLD-derived, so `test_card_point_in_time`'s full-vs-prefix byte identity still holds. |
| Entity-level endpoint ground truth is now truthful | `export.cpp` used to `try_emplace(identity, false)` for every stream-observed endpoint, so the only `device/flagged` positives in the overlay were the 5 AML ring-shared devices — which reach the card view ONLY through a ring member's LEGITIMATE purchase. Entity device/IP labels were both vanishing (5/66,964) and ANTI-CORRELATED with the transaction label. Attacker infrastructure is a world-known set, so the verdict is now derived from membership in it. A residential-proxy address is deliberately NOT marked: it belongs to a customer and is not attacker inventory. | Verdicts remain quarantined in `cf_Ground_Truth_Label` with `is_blocked` still written as 0 — the withheld-label INVARIANT above is untouched, only the overlay's content becomes non-degenerate. |

**A SIZING DEFECT THE NEW GATE CAUGHT ON ITS FIRST RUN, worth recording because the disposition is the standing law.** Sub-gate B′ measures mean CONCURRENT campaigns against the rule's declared floor. It read 4.92 and 4.32 against a nominal 6.0 and went red. The cause was a real construction defect, not a mis-set band: campaign starts were drawn over [0, W), so the first ~L days of every run were covered only by campaigns beginning inside them and early-window compromises systematically failed to attribute to any operator. Fixed in the generator — starts now range over [−L, W) and coverage is uniform. It then read low a SECOND time, because the count was derived from the length distribution's ANALYTIC mean (169d) while the realized clipped span implies ~137–144d; the constant is now declared MEASURED (`effectiveCampaignDays`) with the gate printing the realized span beside the realized concurrency. Neither band was widened. Realized after both fixes: 6.62 / 7.07 concurrent, and the unattributable residual in the victim-endpoint share fell from ~8% to ~2%.

**INVERTED IN THE SAME ROUND, as the four hard-fail points the old emptiness was enforced by:** `tests/test_pipeline_e2e.cpp` (header-only → non-empty plus four-way referential integrity against Party/Device/IP), `tests/test_table_golden.cpp` (STATIC ENDPOINT LEAK → UNREACHABLE ENDPOINT LAYER), `docs/card_fraud_postgres_acceptance.sql` (`RAISE EXCEPTION` on any row → on an empty table), and `tests/golden_tables_card_fraud.md5` (pinned `0` rows + empty md5 → re-pins with the tables populated). The withholding was never free: TF_GNN_v3 reaches Device and IP ONLY through `Party_Has_Device`/`Party_Has_IP` and has no transaction→endpoint edge type at all, so empty ownership tables left every endpoint vertex isolated and the entire endpoint layer inert regardless of what the session edges carried.

**FOLLOW-ON FINDING, SAME ROUND — `giftcard-channel-2026-07`. The coached gift-card purchase was hardcoded card-PRESENT.** `unauthorized.cpp` read `cardPresent = plan.rail == Rail::giftCardScam ? true : …`, so **100% of coached gift-card purchases in the corpus were in-store swipes** and the DIGITAL branch did not exist at all. A coached victim is also routinely walked through buying an Apple / Google Play / Amazon e-gift code online and reading the number out over the phone — in which case the purchase is card-not-present, the destination is an online acceptance endpoint, and **the session address on the row is genuinely the victim's own home IP**, because the victim really is at their own machine under instruction. That is the branch where an issuer has a live session to score and something to INTERRUPT before the codes are read out, which is the intervention this rail exists to support; collapsing every case to a swipe deleted it. Replaced with a dated digital share (`digitalGiftCardShareBasisPoints`, CLASS S UNCITED, mirroring `derive::chipShareBasisPoints` in construction and in honesty): 0 before 2005, rising to 3,500bp by 2019, **physical remaining the majority throughout** — the FTC-documented pattern of the period is a victim sent to a drugstore or big-box store. Both branches now spend exactly ONE coin on the per-plan `{"fraud","unauth","merchant",seq}` lane where the gift-card rail previously spent none, so the extra draw moves gift-card DESTINATIONS only and cannot reach another plan, rail, amount or timestamp. Measured by sub-gate F′: online share **0.2354** (2012-16 leg) and **0.2630** (2016-18 leg) — both branches present, physical majority pinned, and the era ordering visible across the two legs. Row count re-verified UNMOVED at 189,035.

**AND THE AUDIT THAT FINDING PROMPTED, recorded because a null result is worth the same as a positive one.** Every constant probability and branch in the unauthorized rails was swept for the same failure mode — a mechanism ABSENT rather than mis-weighted. `grep` for hardcoded-true/false modality across `src/transfers/fraud/` and `include/phantomledger/transfers/fraud/` returns **the gift-card ternary and nothing else**; it was unique. Everything remaining (`kCardNotPresentShare` 0.70, `isTest` 0.7, `reported` 0.85, `wireRail` 0.5, the rail mix .48/.12/.12/.28) is a DRAWN split with both branches realized. Several of those are era-FLAT and that is a genuine calibration debt — post-EMV CNP migration after 2015, and app-push transfers barely existing before ~2017 against a flat 0.5 wire/app coin — but it is the already-registered "per-era scam payment-method mix" item, not a missing mechanism. **A mis-weighted split degrades realism; an absent branch deletes a detection opportunity. Only the second class is a defect of this severity.**

**SECOND FOLLOW-ON — `merchant-ownership-2026-07`, raised by a DOWNSTREAM ABORT rather than by a gate.** `tf_gnn_loader_v2` refuses the entire push at `sql/postgres/001_validate_sources.sql` with `cf_Is_Merchant is empty; Party_Is_Merchant edges would not load`. Supersedes the `Is_Merchant` INVARIANT row above ("UNPOPULATED (header-only): the world has no merchant-owning-party link — DEVIATES-BY-CHOICE (documented gap)") by name. **The gap was real and correctly described — `synth::accounts::assignBusinessOwners` mints `Role::business` keys owned by roster people, `synth::merchants::makeCatalog` mints `Role::merchant` keys with no owner, and the two are disjoint by Role and serial space — but "documented gap" understated it: an empty table is a HARD STOP in another repository, not a missing feature.** The graph schema declares `Party_Is_Merchant(FROM Party, TO Merchant)` with `REVERSE_EDGE="Merchant_Owned_By_Party"`, and the loader's `party_first_seen` takes the MIN over a party's linked cards AND linked merchants, so the intended semantics is ownership.

| Change | Construction | Safety basis |
|---|---|---|
| `entity::merchant::Record::owner` — the institution's beneficial-owner record | Filled in the entity stage by `entity::merchant::ownership::ownerFor`, immediately after `synthesizeBusinessOwners`, from the EXISTING business-owner cohort read back out of the account registry (sorted + deduplicated, because the pick is positional). Coverage 0.45, CLASS S UNCITED. **DRAW-FREE — the function takes no Rng at all**, so it appends nothing to the shared entity stream. | `golden_run.b2sum` re-verified **UNMOVED** (`22db0e33…`, 189,035 rows) after the change: this is a world-attribute + exporter round, so by the standing law only table goldens may move, and only the card-fraud one does. |
| Register membership is a hash of the merchant KEY ALONE | `ownership::onFile` mixes role, bank and serial and reads NO other attribute — not `footprint`, not `weight`, not `location`, not category. | **THIS IS THE LEAK-CONTAINMENT DECISION AND IT COST REALISM ON PURPOSE.** The tempting rule — small local outlets have proprietors, national services and online merchants do not — would have been more realistic AND a shortcut: the card rail is ~70% card-not-present and draws ONLY from `Footprint::online` while card-present draws only from physical outlets, so any eligibility rule reading footprint or weight inherits the modality split and with it the label. Merchants are where card fraud LANDS; an ownership register that correlated with the label would be a shortcut into the destination side of every fraud row. |
| Keyed on ownership, NOT on acquiring relationship | `Bank::internal` on a merchant key already means "settles through this bank" and is `internalP = 0.02` of CORE merchants — **five merchants at population 10,000.** | Keying on it would have produced a ~5-row table: non-empty, enough to silence the loader's abort, and carrying no graph structure at all. Refused explicitly as the same "a count is not a measurement" trap the attacker-endpoint round exists to close. |
| Restricted to merchants OBSERVED IN THE VIEW | `cf_Merchant` is built from `artifacts.merchants` and is a growing stream-derived vertex set. | Emitting the world's whole register would dangle edges at merchant vertices a prefix export has not written — which the loader validates. `test_card_point_in_time` accordingly classifies `Is_Merchant` as a growing LINE SUBSET, beside `Merchant`, not as world-derived-identical. |

**OWNER RULING 2026-07-28: the edge asserts IDENTITY, NOT MONEY FLOW, and that is the design rather than a gap.** A card purchase settles to the merchant's `Role::merchant` counterparty key, which is a sink; no funds move to the proprietor's business account. `Party_Is_Merchant` carries `REVERSE_EDGE="Merchant_Owned_By_Party"` and the loader derives only `party_first_seen` from it, so nothing downstream asks it for a settlement path. A merchant remittance leg is therefore NOT owed — building one would be a clearing-layer change moving every row count in the corpus to satisfy a claim this edge never makes. This was briefly recorded here as a registered limitation; that framing is WITHDRAWN. The guidance that survives is for consumers: use the edge for graph STRUCTURE, never as evidence that funds moved.

Measured by sub-gate G: **119 of 286 and 135 of 322 catalogue merchants owned (41.6% / 41.9% against 45% nominal); fraud lift on "destination has an owner edge" 1.122x and 0.950x.** It STRADDLES 1.0, and that sign flip across two independent seeds is the evidence that no construction correlation exists — a systematic leak keeps its sign. The residual either way is a finite-catalogue realization effect: at a few hundred merchants a 45% hash lands on a subset whose online/physical composition differs from its complement's by a few points, and the card rail's CNP share turns that into a small lift of either sign. The band is `(0.80, 1.25)` for that reason and is NOT zero-width.

GOLDEN IMPACT: declared model re-pin. `golden_run.b2sum` re-pinned to `22db0e33…` at **189,035 rows (unmoved)**, verified against each failing run's own printed prediction before the baseline was deleted. The merchant-ownership change is draw-free and moves `golden_tables_card_fraud.md5` on `cf_Is_Merchant` ALONE. The three table goldens are deleted for the owner's PostgreSQL-gated re-pin; all three digest `public.transactions`, whose device/IP cells moved, and the card-fraud file additionally pins the two ownership tables and the overlay. CLASS: model, named re-pin. Status: CONFORMS. Verified: full non-PG suite **57/57 accounted (56 pass, `test_scale_soak` skipped, zero failures)**.

══════════════════════════════════════════════════════════════════
# AMENDMENT — merchant-coordinates-2026-07
══════════════════════════════════════════════════════════════════

EXPORTER-ONLY round, owner-requested. **The world has carried merchant
coordinates since `geo-causal-v1` and used them to drive selection, and no
exporter ever wrote one.** Amends the `Merchant geography` INVARIANT row
above — which described the Has_City/Has_State/Has_Zip chain accurately but
recorded nothing about the coordinate pair the same catalogue row carries.

**THE FINDING.** `entity::geography::GeoArea` stores `latitudeE6` /
`longitudeE6` as integer microdegrees, and `haversineMiles` over them is a
live causal input: legitimate merchant selection scores
`popularity(weight) * exp(-distanceMiles / scaleMiles(homeArea))`
(`geo_pools.hpp`), and the fraud rail's geographic axis reads the same
distance (`unauthorized.cpp`). The exporter resolved the area and used
exactly three of its fields — `stateCode`, `city`, `postalAreaCode` —
dropping the coordinates on the floor. **Downstream had already noticed and
worked around it:** TF_GNN_v3 declares `lat DOUBLE, lon DOUBLE,
has_coordinates BOOL DEFAULT "false"` on `Merchant_Location`, `City`,
`Zipcode` and `Street_Address`, and `tf_gnn_loader_v2` fills all three from
defaults at `gsql/loading_jobs.gsql:196` with the note *"the source has no
coordinates, and 0,0 is a real place. has_coordinates=false is the mask."*
The generator held the value the consumer had given up on.

| Change | Construction | Safety basis |
|---|---|---|
| `cf_Merchant_Location(merchant_id, lat, lon)` — NEW table, 40th | Written in the existing merchant loop from the area already resolved for the Has_* chain, converted by `derive::degreesFromE6`. **Row presence IS the `has_coordinates` mask:** only the physical-outlet branch reaches the writer, so online merchants and non-catalog billers are ABSENT rather than carrying a 0,0 that reads as the Gulf of Guinea. | DRAW-FREE — no Rng is touched and no branch is added to any generation path. `golden_run.b2sum` re-verified **UNMOVED at `22db0e33…`, 189,035 rows**, so by the standing law only table goldens may move. `test_card_point_in_time` classifies it as a KEYED-STABLE growing table, not a line subset: `merchant_id` is unique within it, so the stronger check applies and a merchant's coordinate must be prefix-invariant. |
| `lat`/`lon` APPENDED to `cf_City` and `cf_Zipcode` | `kCityCols` 3 → 5, `kZipcodeCols` 1 → 3. Appended, never inserted, so the loader's positional mapping for `id`/`city`/`population` is unmoved — and the resulting order matches TF_GNN_v3's own attribute order for both vertices. | A city or zip is FIRST-WRITER-WINS here exactly as `population` already was, so all attributes of a `City` row come from ONE area — never a population from one area and a centroid from another. Moot on the current catalogue (city+state and postal code are both unique across its 86 rows) and correct if it ever grows. |

**THE HONESTY CONSTRAINT, AND IT IS THE POINT OF THE ROUND.** These are
**AREA CENTROIDS, so co-located merchants share a point.**
`Record.location` is a `GeoAreaId`; the world does not model street
addresses, and emitting a jittered per-outlet point would have INVENTED
resolution the generator does not have. Measured on the e2e window: **149
merchants across 48 distinct centroids.** Recorded in
`card_fraud_feature_contract.md` as a prohibition, not a footnote — a
nearest-neighbour-merchant or intra-ZIP-clustering feature is reading
precision that is not there.

**THE GATE, AND WHY IT HAS FOUR CHECKS INSTEAD OF ONE.** A count of
coordinates is not a measurement of geography: a table of constant `0,0`
would satisfy presence, referential integrity and agreement-with-Zipcode
simultaneously. The coordinate block in `tests/test_pipeline_e2e.cpp`
therefore pins (a) coverage EQUAL to `cf_Has_Zip` in both directions, (b)
the US bounding box, since `placeGeography`'s `domesticAreas()` filters on
`Country::us`, (c) byte-identity against the merchant's OWN `cf_Zipcode`
row, and (d) more than one distinct point. **Non-vacuity CONFIRMED by two
disarms, each caught by a different subset:** a lat/lon swap reds 149/149
on bounds AND on agreement; a constant point reds bounds and the
distinct-point floor while PASSING agreement, because `cf_Zipcode` goes
constant with it. Neither check is redundant.

**WHAT THIS ROUND DELIBERATELY DID NOT DO.** Distance-from-home is still
not computable downstream, because **party geography is unexported**:
`cf_Address` is a bare street string, `pii::Address::geoArea` is never
written, and `Party_Has_Std_City` / `_Std_Postcode` / `_Std_State` sit in
`tf_gnn_loader_v2`'s `_UNLOADED_EDGE_TYPES` with `verify.py` asserting they
count **ZERO**. Emitting party geography would FAIL the downstream verifier
until the loader changes in lockstep — the same trap as `Tax_Id_Number`,
which is declared in the schema and likewise asserted empty. **The
asymmetry is worth carrying: an absent table can be a hard abort
(`cf_Is_Merchant`) or a hard assertion of emptiness, and the two are
indistinguishable from inside this repository.** Merchant coordinates were
chosen precisely because they are additive on a LOADED vertex whose
attribute slots already exist, so nothing downstream breaks by shipping
them early.

GOLDEN IMPACT: exporter-only, so `golden_run.b2sum` is UNMOVED and only
`golden_tables_card_fraud.md5` moves — on `cf_City`, `cf_Zipcode` and the
new `cf_Merchant_Location`, three tables of forty. CLASS: exporter,
table-golden re-pin only. Status: CONFORMS. Verified: full non-PG suite
**62/62 accounted (56 pass, 5 PostgreSQL-gated skips, `test_scale_soak`
skipped, zero failures)**.

══════════════════════════════════════════════════════════════════
# AMENDMENT — party-geography-2026-07
══════════════════════════════════════════════════════════════════

EXPORTER round on this side, plus a LOCKSTEP change in `tf_gnn_loader_v2`.
**Makes cardholder-to-merchant DISTANCE computable downstream for the first
time.** Directly supersedes the closing paragraph of the
`merchant-coordinates-2026-07` amendment above, which registered distance as
NOT computable and named the two-repo constraint as the reason.

**THE FINDING.** Merchant coordinates alone were half a feature. Distance
needs two endpoints and the cardholder end did not exist downstream:
`cf_Address` is a bare street string, and `pii::Address::geoArea` — the
party's canonical home area, assigned at PII synthesis on the isolated
`{"home-geo", <household>}` lane so coresidents share it — had no exported
representation at all. The generator has scored merchant selection as
`popularity * exp(-distanceMiles / scaleMiles(homeArea))` since
`geo-causal-v1`, and the fraud rails read the same distance
(`unauthorized.cpp`), so **the single most-cited card-fraud feature was one
the model was built around and no consumer could reconstruct.**

**WHY IT COULD NOT SHIP UNILATERALLY, which is the transferable part.**
TF_GNN_v3 declares `Party_Has_Std_City` / `_Std_Postcode` / `_Std_State`,
and `tf_gnn_loader_v2` listed all three under DECLARED BUT NOT LOADABLE
with the reason "cf_Address is a bare street string; the source has no
party-level geography" — **and `verify.py` ASSERTED THEY COUNT ZERO.** So
populating them from this repo alone would have turned a green load into a
failed one. That is the exact inverse of `cf_Is_Merchant`, where an EMPTY
table hard-aborted the push. **An absent table can be a hard abort or a
hard assertion of emptiness, and from inside this repository the two are
indistinguishable — which is why the loader change is part of this round
rather than a follow-up.**

| Change | Construction | Safety basis |
|---|---|---|
| `cf_Has_Std_City` / `cf_Has_Std_Postcode` / `cf_Has_Std_State` — three NEW tables (40 → 43) | Written in a pass placed BEFORE the City/State/Zipcode writers, over `p = 1..roster.count` matching `cf_Party`'s own loop bound exactly. Reads `pii.records[p-1].address.geoArea` and resolves it through the pinned catalogue. Guarded on `contains(geoArea)`, so absence is the mask exactly as it is for merchants. | DRAW-FREE. `golden_run.b2sum` re-verified **UNMOVED** at `22db0e33…`, 189,035 rows. `test_card_point_in_time` classifies all three as WORLD-DERIVED IDENTICAL — the strictest of its three classes — because home area is assigned once and the roster is fixed, so unlike the merchant side there is no growing vertex set to excuse any drift. |
| Party areas UNIONED into the City / State / Zipcode vertex tables | The ordering of the new pass is load-bearing: a home area no merchant occupies must become a vertex or its edge dangles. `Assigned_To` and `Located_In` consequently span both populations. | The vertex tables stay correctly classified despite the union — their party half is prefix-invariant while their merchant half still grows — so `City` remains keyed-stable and `Zipcode`/`State` remain line subsets. Referential integrity is gated in BOTH directions, which is the one way this round could have dangled an edge. |
| FOREIGN-domiciled parties emitted, not dropped | ~4% of the roster under `LocaleMix::usBankDefault`. The 15 foreign catalogue areas carry real subdivision codes (LND, ON, CMX, MH, SH, SEO, …) that collide with no US state code, so the shared State vertex stays unambiguous. | Dropping them was the tempting simplification and it is a DOUBLE error: it hides the population an issuer most wants to reason about, and it silently makes "has a Std_City edge" a US-RESIDENCY FLAG — a new shortcut introduced while closing a gap, which standing law forbids. |

**THE HARNESS DIVERGENCE THIS ROUND UNCOVERED AND FIXED, and it is the most
useful thing in it.** The first disarm — drop foreign parties — **PASSED.**
Not because the gate was weak, but because **every gate harness in this
repository ran `LocaleMix::usOnly()` while production runs
`usBankDefault()`, so no test had ever exported a foreign-domiciled
party.** Harmless while party geography did not exist; not harmless the
moment an edge began resolving a home area, because the foreign areas are
precisely the ones whose city ids, subdivision codes and postal formats
differ from every US row. `test_pipeline_e2e` now builds all 16 locale
pools (minority pools sized 512) and runs the production mix, and the gate
**asserts at least one home centroid outside the US bounding box** so the
coverage cannot be lost again silently. With that in place the same disarm
reds twice: coverage 94/100 and foreign centroids 0. **A gate that cannot
see the population production generates is not a gate; and the way this
was found is that a disarm passed, which is why disarming is mandatory
rather than decorative.**

**MEASURED:** 100 parties → 29 distinct home centroids, **3 foreign**, zero
unreachable on the end-to-end `Party → Zipcode → coordinate` walk. Merchant
side 140 merchants / 50 centroids on the same window.

**LOCKSTEP LOADER CHANGES (`tf_gnn_loader_v2`, not under version control —
flagged to the owner).** `verify.py`: the three edges PROMOTED from
`_UNLOADED_EDGE_TYPES` to `_LOADED_EDGE_TYPES` with `_EXPECTED_OBJECTS`
entries, so they are now count-matched against the manifest instead of
asserted zero. `060_create_geography_views.sql`: three new `loaded_party_*`
views; `merchant_locations` gains lat/lon with `has_coordinates` derived
from ROW ABSENCE in `cf_Merchant_Location` rather than from the values,
because 0,0 is a real place; **and the City/Zipcode/State registries widened
from "referenced by a merchant location" to the UNION with party-referenced
places** — the change without which every party edge would dangle.
`080_create_load_views.sql`: three `load_party_*` views plus coordinates on
the city, zipcode and merchant-location load views.
`gsql/loading_jobs.gsql`: three edge jobs, and `_` → `$N` for
lat/lon/has_coordinates on three vertex jobs. `export.py`: datasets 40–42.
`001_validate_sources.sql`: the four new tables required to exist, their
columns required present, and **non-empty aborts on both ends of the
distance pair** — either one empty reduces distance to unevaluable while
every other check still passes.

GOLDEN IMPACT: exporter-only on this side, so `golden_run.b2sum` is UNMOVED
and only `golden_tables_card_fraud.md5` moves — on the three new tables plus
`cf_City`, `cf_Zipcode`, `cf_State`, `cf_Assigned_To` and `cf_Located_In`,
which grow to cover party areas. CLASS: exporter, table-golden re-pin only.
Status: CONFORMS. Verified: full non-PG suite **62/62 accounted (56 pass, 5
PostgreSQL-gated skips, `test_scale_soak` skipped, zero failures)**.

══════════════════════════════════════════════════════════════════
# AMENDMENT — merchant-churn-2026-07
══════════════════════════════════════════════════════════════════

MODEL round, owner-raised. **The merchant universe was static: over the
owner's 20-year target window not one merchant ever opened and not one ever
closed, and every person's favourite merchant set was FROZEN for all 7,305
days.** Supersedes the `Merchant geography` INVARIANT row's implicit
assumption that a catalogue entry is live for the whole run.

**THE TWO DEFECTS, AND THE SECOND WAS HIDING IN A COMMENT.**

1. `makeCatalog` took no window and no date; `merchant::Record` carried no
   time field of any kind. At the target config that is 490 acceptance
   endpoints, fixed, for two decades — against BLS Business Employment
   Dynamics retail survival of ~84% at 1 year, ~58% at 5, and the March-1994
   birth cohort down to ~14% by March 2025.
2. `math::evolution::evolveFavorites` was **DEAD CODE**. Fully written, its
   `merchantAddP` / `merchantDropP` / `maxFavorites` config validated at
   startup, and never called from anywhere: `evolveAll` evolved only the P2P
   contact graph. A `TODO(structural)` named the obstacle accurately —
   `primitives::utils::Csr` had fixed-length rows — and badly understated
   the consequence. **The config strings were strong evidence that
   preference drift was modelled. It was not.**

| Change | Construction | Safety basis |
|---|---|---|
| Merchant operating interval | `[firstEpoch, lastEpochExcl)` half-open on `Record`, matching `infra::Tenure`. Three-band annual death hazard, each band reproducing ONE published BLS retail survival point: 0.158 (<1yr), 0.1145 (1–5yr), 0.0540 (mature). NBER recession modulation off `MacroYear::recessionMonths`, which the authority already classes MEASURED. | Default is ALWAYS-LIVE, so every existing unit harness saw unchanged behaviour until lifecycle was assigned — that is what kept the suite green through the change. Incumbent survival MEASURED at **0.4343 over 15 years against a derived 0.4349**, and 0.7448 vs 0.7577 over 5. |
| Incumbents draw the MATURE band, not the birth-cohort curve | The BLS curve describes a birth cohort; the merchants a window opens with are survivors of every earlier cohort and so are survivorship-biased toward maturity. | Applying the birth curve to incumbents would have over-killed them. 20-year forward survival is ~33%, not ~25%, and the difference is a modelling fact rather than a tolerance. |
| Churn REPLACEMENTS on an isolated lane | `appendChurnReplacements` sizes births as expected deaths (`base * h * years`) and draws off `churnSeed` only. Replacements inherit a donor incumbent's economic shape, so merchant age cannot proxy for size. | **THIS REPLACED A WRONG FIRST DESIGN, and the failure is the lesson.** Sizing the catalogue by the window inside `makeCatalog` looked harmless and was not: that function spends one shared-entity-stream `lognormal` per core record, so changing the count shifted every downstream entity value. Measured fallout: **51,079 account-closure violations in `test_membership`**, `test_econ_wiring` outside its band, and the owner register at max 7 outlets per proprietor — three gates with nothing to do with merchants. |
| Liveness reaches SELECTION in all three sites | National CDF and biller CDF rebuilt monthly in the existing evolver; `GeographicMerchantPools::rebuildLive` masks CACHED distance-decay weights (merchant location never moves, so ~10M haversines per run were avoidable); the fraud rail filters per case on the CASE'S OWN timestamp, which is stricter than a month boundary and draw-count neutral. | Out-of-tenure transactions fell **49% → 10.2% → 5.0% → 0.27%** (15y leg) and **24% → 0.21%** (5y leg). |
| `Csr` gains capacity + a live count; `evolveFavorites` WIRED | `pushBack` writes at `count++`, `swapRemove` moves the last live entry over the hole — both O(1), no offset shifting, spans stay valid. An instance built without counts reports full spans, so `Billers` and every fixed-length caller stayed bit-identical while the type changed underneath them. | Closes the TODO with a mechanism rather than deleting it. A FORCED-DROP pass precedes the voluntary add/drop: a favourite whose merchant closed must go regardless of the coin, and a forced drop is world state rather than a draw, so it spends nothing. |
| Biller churn uses PAIRED REPLACEMENT | A favourite that closes is dropped; a utility that closes is REPLACED. | The distinction is load-bearing: dropping without replacing would have quietly reduced every long-run household's recurring-debit count — a realism regression dressed as a bug fix. |
| Behaviour draws moved to their own lane | `exploreProp`, `burstStart`, `burstLen` now ride `{"payee-behavior", id}`. | `WeightedPicker::pick` RETRIES on duplicates, so its draw count is DATA-DEPENDENT: a larger merchant CDF collides less, finishes in fewer tries, and shifts everything drawn after it on the same lane. That coupling made a merchant-pool change move every person's exploration propensity and burst window — +4,138 rows of movement that had nothing to do with merchants. **Any draw whose count depends on data must be last on its own lane.** |

**THE RESIDUAL IS STRUCTURAL AND IS NOT A WIDENED BAND.** 0.18–0.49% of
merchant rows still post out of tenure, because liveness is enforced at
MONTH BOUNDARIES — the evolver's only hook — so a merchant closing on the
5th serves its existing favourites until the next one. The bill channel runs
highest because a monthly debit gets exactly one chance to fall in that gap.
The fraud rail is exempt. The gate's ceiling sits just above the observed
floor: the frozen-favourites state measured 49%, biller staleness added 5%,
and the pre-round state is 100% of rows on any closed merchant, so **no
configuration between "monthly granularity" and "no liveness" lands inside
the band.**

**TWO OF THIS ROUND'S OWN FINDINGS WERE INSTRUMENT DEFECTS, NOT MODEL
DEFECTS, and both were found by disbelieving a number.** (1) `external_unknown`
measured 85% out of tenure; `PaymentRouter::emitExternal` routes the
unmodelled-merchant catch-all to a HARDCODED `makeKey(merchant, external, 1)`
which collides with catalogue serial 1, so the gate's key join was counting a
sentinel as merchant #1. (2) `test_table_golden`'s divergence report
TRUNCATED SILENTLY at ten lines shared across both lists, so the card-fraud
section printed ten `changed-or-new` rows, emitted ZERO `was-in-baseline`
rows, and hid six further moved tables — making `cf_Merchant_Location` look
unmoved while `cf_Has_Zip` had moved, which is impossible since the exporter
writes both in the same branch of the same loop. Each list now has its own
budget and every suppressed line is counted. **A re-pin decision is made
from that output; truncating it silently is the same defect class as a gate
that bounds coverage without saying so.**

**AND ONE ATTRIBUTION THAT WAS SIMPLY WRONG, recorded because the correction
came from the goldens.** The AML alert layer dropped 17% (`ALERT_ON` 18,699
→ 15,559) and this amendment first blamed a camouflage change that skipped
merchant-destined P2P rows. The goldens falsified it: skip and re-pick
produce BYTE-IDENTICAL digests, so that change has no effect at production
ring rates. The real explanation is threshold amplification — `Rule::
velocityBurst` fires at `count >= 5` and the corpus moved −1.3%, while
`fraudMlFlag` can account for at most the 1,003 fraud rows in a 720,053-row
corpus, leaving ≥14,556 alerts threshold-driven. Fraud generation is intact
at 0.139% against a ~0.1% target, and `cf_Ground_Truth_Label` moved by one
row. The camouflage re-pick SURVIVES (it fixes a real wrongness visible at
the gate's inflated fraud profile) but explains none of the delta.

SUPERSEDED by AMENDMENT bls-citation-2026-07: the hazards are now CITED
(accessed 2026-07-30) and the levels below were WRONG — see that amendment.
PRIOR TEXT: CALIBRATION STATUS: the three hazard bands are MEASUREMENT-derived with
levels [Likely] — the survival POINTS are published BLS figures and the
per-band hazards are the arithmetic reproducing them. **The owner must
download `bls.gov/bdm/us_age_naics_44_table7.txt` by hand to promote them to
CITED: BLS blocks automated retrieval by stated policy, so this repository
cannot verify itself.** `kRecessionHazardLift` (0.60) is CLASS S UNCITED —
direction BLS-anchored, magnitude declared.

GOLDEN IMPACT: declared model re-pin, ALL FOUR baselines.
`golden_run.b2sum` re-pinned `22db0e33…` → `9d0a9399…` at **189,035 →
188,478 rows**, verified against the failing run's own printed prediction
before deletion. Bisected: merchant churn −859, behaviour-lane separation
+302. Table sections: 7 of 38 standard, 23 of 59 fraud, 16 of 40 card_fraud,
every one a matched pair with no table appearing or disappearing.
`cf_Merchant_Location` 540 → 546 EQUAL to `cf_Has_Zip`, and
`cf_Has_Std_City`/`_Postcode`/`_State` unmoved at 10,000 with `cf_City`/
`cf_Zipcode` at 86 — merchant churn did not leak into party geography.
CLASS: model, named re-pin. Status: CONFORMS. Verified: non-PG suite
**62/63 accounted, zero failures**.

# AMENDMENT — card-churn-2026-07 + burst-rate-2026-07
══════════════════════════════════════════════════════════════════

MODEL round (card churn) plus one corpus-neutral repair (burst rate), both
owner-raised. **The exported card number was `'C'/'D' + renderAccountKey` — a
pure function of the account — so across the owner's 20-year target window not
one cardholder ever received a replacement card.** Supersedes nothing: no
prior row asserted card-identity stability, which is precisely why it went
four rounds unexamined.

SOURCE, and it is a DECOMPOSITION not a single rate. Auriemma Consulting
Group's US cardholder reissuance research reports ~50% of cardholders
experiencing at least one reissuance within a year, split roughly ~33%
scheduled expiry, ~26% the EMV migration wave, ~14% lost/stolen/damaged, and
~27% fraud-driven. CLASS: [Likely] — the shares are the published
decomposition; `kDispersionSigma` (0.80) and the 36–60-month validity span are
CLASS S UNCITED, the latter anchored on Mercator 2019 U.S. PaymentsInsights
reporting three-year terms as common against a traditional ~5-year cycle.

MECHANISM. `entities/holdings/card_reissue.hpp` tiles
`[windowStart, windowEndExcl)` with contiguous generations, DRAW-FREE: every
date is a hash of the card key and the generation index across four
INDEPENDENT FNV domains (validity / proneness / event / EMV), so no term can
alias another. Unscheduled replacement rides a mean-1 lognormal proneness via
Box-Muller — the repository's existing over-dispersion idiom — keyed on the
CARD rather than the person, because Auriemma's repeat-victim finding is
per-instrument. The exporter emits one `cf_Card` vertex and one
`cf_Party_Has_Card` edge per OBSERVED generation; a scheduled generation with
no transaction in the view is not a vertex.

MEASURED: mean **6.672** generations/card over 20 years against a pre-round
state of exactly **1.000**; proneness mean 0.982 with p99/median **6.20x**;
60-day churn **1.2%**; EMV wave 2081 of 4000 cards, all with an in-window
boundary. Sub-gate H fraud lift **1.012x / 0.990x**.

REGISTERED LIMITATION — FRAUD-DRIVEN REISSUE IS DELIBERATELY OMITTED, and it
is ~27% of the published decomposition. Reissue after compromise is causally
DOWNSTREAM of the label, and the only fraud signal available at export time is
`seen.fraud` — the full-window verdict that `cf_Card.is_fraud` exists to
withhold. Modelling it would make an exported vertex count an entity label.
CONSEQUENCE, stated plainly: exported card churn is LOWER than reality and
carries NO fraud correlation. Both errors are in the safe direction, and
sub-gate H is the standing proof the second one holds.

THREE FINDINGS FROM BUILDING SUB-GATE H, each a repeat of a lesson this
document already carries.

1. **THE FLAG HAD TO BE RULE-LEVEL, NOT OBSERVED.** Counting generations that
   appear in the view confounds the measurement with EXPOSURE: a card with
   more rows both straddles a boundary more often and, via `exposure.hpp`'s
   activity tilt on unauthorized victim selection, is victimized more often.
   That correlation is real and intended, so an observed flag would sit above
   1.0 for a legitimate reason and the gate would stop measuring construction.
2. **THE BAND IS MEASURED AND THE BINOMIAL WOULD HAVE BEEN WRONG BY 2.4x.**
   Eight readings over four seed pairs give mean 0.991 and SD **0.0366**
   against a naive binomial 0.015, because fraud rows CLUSTER — one compromise
   produces several charges, so the effective sample is cases not rows. Band
   set at 3.5 SD: 0.87–1.13. This is the SAME "derived instead of measured"
   error the attacker concurrency floor made twice.
3. **THE DISARM EXPOSED A SENSITIVITY LIMIT.** Marking every compromised card
   as reissued reds leg-wide at **1.478x** but leg-long at only **1.153x**:
   over a four-year window 66% of view cards already reissue, so the flag
   SATURATES and the leak has little contrast left to show. Sub-gate H's power
   therefore DEGRADES with window length — material, because the owner's
   target run is 20 years. My first band, inherited from sub-gate G at
   0.80–1.25, caught only one leg; that is how this surfaced.

BURST RATE, a separate corpus-neutral repair found while wiring merchant
churn. `buildPersonBursts` applied `burstProbability = 0.08` ONCE PER RUN, so
a person got at most one spending burst regardless of window: 0.49 bursts/year
at 60 days and **0.004/year over twenty years**, leaving 92% of a 20-year
population with none. Now a per-year rate scaled by `segmentSpan / 365.25`,
with the level DERIVED (`burstsPerYear = 0.487`) to reproduce the old coin
exactly at the 60-day golden horizon. MEASURED 0.482 at 365 days and 4.843 at
3652 — the gate runs TWO horizons because a per-run probability and a per-year
rate are indistinguishable at one, which is how this survived every prior
round. CLASS: repair, no citation claimed. Two self-inflicted errors caught by
the same gate: applying the annual rate without its duration (6x prevalence at
60 days), and a first gate version that re-derived the formula instead of
measuring `buildPersonBursts`, which this document forbids.

TECHNICAL DEBT. `day.cpp:15` is CLOSED, completing the owner's 2026-07-29
directive: `time::weekday` is Mon=0..Sun=6 (`calendar.cpp:65`) so `>= 5`
correctly selects Sat/Sun, and the duplicated threshold is collapsed onto
`time::isWeekend`, which is literally `weekday(tp) >= 5` at `calendar.cpp:70`
— provably output-identical, and confirmed so. The sweep now returns zero live
markers and two historical prose references.

GOLDEN IMPACT: EXPORTER-ONLY, and PREDICTED rather than discovered — the
corpus `Transaction` carries account keys and no card identity, and no
standard or AML table carries a card number. `golden_run.b2sum` **UNMOVED** at
`9d0a9399…`/188,478 rows. The `standard` (38 tables) and `fraud` (59 tables)
sections came back **digest-identical**. Movement confined to three of 44
card_fraud tables: `cf_Card` and `cf_Party_Has_Card` **18,199 → 18,350**, and
`cf_Card_Send_Transaction` at an **unchanged 373,733 rows with a new digest**,
which is the expected signature of card numbers changing on rows after a
reissue. `cf_Ground_Truth_Label` did NOT move, and the reason is worth
recording: it emits one row per generation of a fraud-touched card, so its
stillness says no fraud-touched card crossed a boundary in this 60-day window
— consistent with, though not proof of, the independence sub-gate H measures
directly. CLASS: exporter, `golden_tables_card_fraud.md5` only. Status:
CONFORMS. Verified: suite **64/64, zero failures**; sub-gate H disarm reds both
legs and the re-armed gate is green.

# AMENDMENT — relocation-2026-07
══════════════════════════════════════════════════════════════════

MODEL round, owner-approved within the merchant-churn arc and deferred once as
its own round. **`pii::Address::geoArea` was assigned once on the isolated
`{"home-geo", <household>}` lane and fixed for the run, so the owner's 20-year
target window contained ZERO relocation.** AMENDS the `party-geography-2026-07`
amendment's statement that home geography is "static, prefix-invariant, and
observable at any timestamp": prefix-invariant and observable still hold, static
does not.

SOURCE. Census CPS ASEC annual geographic mobility: the mover rate runs 15.9%
in 1998-99 declining to 8.4% in 2021, with composition 53.5% within-county,
24.3% same-state-different-county, 17.3% different-state and 4.9% from abroad.
CLASS: [Likely] — the anchors and the composition are the published series; the
linear interpolation between anchors, `kAreaChangingShare` (0.85) and the
collapse of within-county into one same-state class are CLASS S UNCITED.

MECHANISM. `entities/parties/relocation.hpp` holds a per-person tenure history,
contiguous and ascending, with tenure 0 stamped at the window start and
byte-identical to the `homeAreas` snapshot — that identity is what makes a
zero-move window reproduce the pre-round corpus. Construction rides its OWN
`{"home-relocation", <group>}` lane and spends FOUR uniforms per group-year
UNCONDITIONALLY (move coin, day-in-year, destination position, same-state coin),
drawn before any branch tests them, so a data-dependent draw count cannot
couple a group's later moves to whether its earlier ones landed in-state.

MEASURED (20 years, 400 people): **0.1047 moves/person-year against a
CPS-derived nominal 0.1033**; era decline **0.1212 → 0.0882** across the two
halves; realized same-state share 0.674; cross-country moves **0**; 62 distinct
areas ever occupied against 46 at window start; 60-day leg 0.0175 moves/person.
Coresidence: 20 multi-member groups, 14 of which move, **0 divergent**.

FOUR FINDINGS, and two were defects in my own construction rather than in the
model.

1. **THE RATE CAME IN AT HALF THE SERIES AND THE BAND WAS NOT THE ANSWER.**
   Sampling the full residential pool and dropping a redraw that landed on the
   origin as a no-op delivered 0.0511 moves/person-year against 0.103, and
   dragged the realized same-state share to 0.63 from a nominal 0.818 — because
   the pinned catalogue holds only a handful of areas per state, so in-state
   redraws self-hit far more often than national ones. `sampleExcluding`
   renormalises over the origin's complement: conditional on moving, a
   household moves SOMEWHERE ELSE. ONE construction fix moved BOTH measurements
   toward nominal, which is the signature of a real fix rather than a widened
   band.
2. **THE RESIDUAL 0.674 IS THE CATALOGUE, AND THE GATE PRINTS THE PROOF.** 51 of
   64 states with residential areas have exactly ONE, so an in-state intention
   there falls back to a country-wide draw. The check is a FLOOR for that
   reason, and the shortfall shrinks as the catalogue grows.
3. **THE GROUP KEY IS (household, initial area), NOT the household.**
   `buildRecord` samples `country` PER PERSON off the locale mix and only then
   calls `homeAreaFor`, so two members of one household who drew different
   countries already hold DIFFERENT home areas — reachable under the production
   `usBankDefault` mix. Keying relocation on the household alone would have
   MERGED them into one area.
4. **THE ENGINE DIVERGENCE WAS A HARNESS GAP AND IT COST 5,156 ROWS TO FIND.**
   `test_arch_equivalence` went red at monolith 252,517 vs windowed 257,673. The
   windowed leg is built by `GateWorld`, which constructs its OWN market, so
   wiring the schedule through `windowed_run.cpp` left the harness leg with
   immobile homes while the monolith reference had movers. It read exactly like
   a windowing bug. STANDING LAW REINFORCED: any carrier the fold reads must be
   filled in `gate_world.hpp` too, or the equivalence gate compares two
   different worlds. Second time this gap has been paid for.

EXPORTED FORM. `cf_Has_Std_City` / `_Postcode` / `_State` become ONE ROW PER
OCCUPIED TENURE with `since_unix_time` APPENDED as column 3 (schema.hpp's
standing law: appended, never inserted, so the loader's positional mapping of
`party_id` and the area id is untouched). A party who never moves emits exactly
one row at the window start, so the pre-round shape is the no-move case rather
than a special case. Every tenure's area is unioned into the City/State/Zipcode
vertex tables, for the same dangling-edge reason the merchant pass has.

REGISTERED LIMITATIONS. Household composition is static, so nobody ever moves
OUT of a household — a young adult leaving home is a real and common move this
does not produce. Foreign moves are out of scope: a party's `country` drives
locale, PII format and the whole identity layer, none of which can move
mid-run. **NO AGE OR TENURE TILT**, though mover rates fall steeply with age and
owners move far less than renters: the schedule is keyed on the HOUSEHOLD and a
household has no single age, so picking one member's would be arbitrary and
picking the eldest would encode a householder concept the roster does not carry.
A party who RE-OCCUPIES a previously-held area collapses to one graph edge
stamped at the earliest occupancy, because TigerGraph keys an edge by (from, to)
with no discriminator.

LOADER LOCKSTEP (`tf_gnn_loader_v2`, not a git repository — no revert path).
The three `060` prep views carry `since_unix_time` and group by (party, place)
taking the earliest occupancy; `080` stamps `Party_Has_Std_*.edge_unix_time`
from the TENURE START rather than the party's first transaction, guarded with
`greatest(...)` so an edge cannot predate the party entering the graph;
`party_home_coordinates` selects the LATEST tenure instead of `min(id)`, because
`Street_Address` carries a single point and that point should be the CURRENT
home; `contract.py` REQUIRES the new column so a reverted source fails loudly;
`090`'s multi-area count is reclassified from a warning sign to the expected
shape. `verify.py` needed no change — it compares against the dataset row count
dynamically.

GOLDEN IMPACT: declared model re-pin. `golden_run.b2sum` `9d0a9399…` →
`2afaf188…` at **188,478 → 188,477 rows** — a ONE-ROW move, because only ~1.75%
of people relocate inside the 60-day golden window and a changed merchant
destination only alters a row COUNT when it flips an insufficient-funds outcome.
All three table goldens deleted for the same named commit. CLASS: model, named
re-pin. Status: CONFORMS. Verified: suite **65/65 accounted, zero failures**
with relocation live, including `test_arch_equivalence` byte-identical across
engines.

# AMENDMENT — bls-citation-2026-07 + the acceptance-script table count
══════════════════════════════════════════════════════════════════

CALIBRATION round plus one BLOCKING repair. AMENDS the
`merchant-churn-2026-07` amendment's CALIBRATION STATUS paragraph, which said
the owner must download the BLS table by hand to promote the hazards from
[Likely] to CITED.

PROMOTION: [Likely] -> **CITED (accessed 2026-07-30)**. BLS Business Employment
Dynamics, "Table 7. Survival of private sector establishments by opening year",
NAICS 44 (Retail Trade), `bls.gov/bdm/us_age_naics_44_table7.txt`. The
March-1994 cohort: 80,604 establishments at birth, 82.8% surviving at 1 year,
57.7% at 4 years (46,469), 48.0% at 6 years (38,669), 13.7% at 31 years.
`bls.gov` returns HTTP 403 to direct programmatic fetch, so the figures were
read through a search index and cross-checked by their own internal arithmetic:
46,469/80,604 = 57.65% and 38,669/80,604 = 47.97% both reproduce the published
percentages. That count-to-percentage consistency is what distinguishes a real
table row from a plausible-looking number, and anyone revising these must
reproduce it.

**THE PROMOTION FALSIFIED THE PRIOR CALIBRATION IN TWO INDEPENDENT WAYS.**

1. BOTH CITED FIGURES WERE WRONG FOR NAICS 44. The block claimed retail 1-year
   survival ~84.2% and 5-year ~58.3%; the published values are 82.8% and ~51%.
   58.3% is within a rounding step of the 57.7% FOUR-year figure, which is the
   likely provenance of the error.
2. THE STATED ARITHMETIC WAS ALSO WRONG. The comment asserted
   `0.842 * (1 - 0.1145)^4 = 0.583`; the left side is 0.5177. **A derivation
   written out in a comment is not a derivation until someone evaluates it.**

The two errors partly cancelled, which is why the old constants still landed
within 1.5pp of every published point and no gate objected.

RECALIBRATED, fitted to the 1-, 4- and 31-year points:
`kHazardFirstYear` 0.158 -> **0.1720**, `kHazardYears1To5` 0.1145 -> **0.1134**,
`kHazardMature` 0.0540 -> **0.0494**. Reproduce the three fitted points to
+/-0.01pp.

**THE SIX-YEAR POINT IS THE EVIDENCE, AND IT IS NOT FITTED.** March 2000 (48.0%)
was excluded from the fit and falls out at 48.63% — a 0.63pp error on a point
the calibration never saw. Three constants matching three points proves only
arithmetic; a fourth landing inside two thirds of a percentage point is evidence
about the THREE-BAND STRUCTURE.

GATE SPLIT, because the old check could not have caught any of this.
`test_merchant_churn` check C hardcoded the literal 0.0540 while the constant it
claimed to test lived elsewhere, so a recalibration would have left it asserting
the OLD hazard. It is now C1 (implementation fidelity — derives its expectation
from `kHazardMature`, and passes for ANY value of it, which is stated in the
check) plus **C2, NEW** (calibration — asserts the banded curve against the four
published points, fitted ones at 0.005 tolerance and the unfitted 6-year at
0.02). Confirmed non-vacuous: restoring the old constants reds **6 checks**.

GOLDEN IMPACT: **NONE.** The corpus digest is unmoved at `2afaf188…`/188,477 —
over the 60-day golden window no merchant's liveness flipped, though the
constants demonstrably reach the corpus path (incumbent 15-year survival moved
0.4343 -> 0.4599). It is NOT corpus-neutral at the owner's target config:
incumbent 20-year survival moves **0.3295 -> 0.3630**, so ~3.4pp more of the
opening merchant set survives the window. CLASS: calibration, no re-pin.

---

**THE ACCEPTANCE SCRIPT WOULD HAVE HARD-ABORTED A CORRECT CORPUS.**
`docs/card_fraud_postgres_acceptance.sql` asserted `registered_count <> 39` and
`physical_count <> 39` with `RAISE EXCEPTION`, while the export ships **43**
tables. `merchant-coordinates-2026-07` took the set 39 -> 40 and
`party-geography-2026-07` took it 40 -> 43; both rounds updated this file's table
MANIFEST and the schema header comment, and NEITHER updated the two scalars. The
owner's acceptance run would have failed with "expected 39 registered card_fraud
tables, found 43" on a corpus with nothing wrong with it.

`test_table_golden` could not catch it: its own assertion was `assert(size >=
39)`. **A LOWER BOUND IS NOT A COUNT** — it accepted the four additions silently
and would equally have accepted a table that went MISSING.

FIXED with one source of truth: `kTableCount = 43` in
`exporter/card_fraud/schema.hpp`, `test_table_golden` asserting the live registry
equals it EXACTLY, and both SQL scalars updated with the staleness history
recorded inline. The manifest was diffed against the live 43-table set — no
difference — so only the scalars were stale. CLASS: instrument defect, no re-pin.

**THE PATTERN, since this is the third instance:** a count duplicated into a
place that cannot include the header will go stale, and a gate written as an
inequality will not notice. The two other instances were `test_table_golden`'s
silently-truncating divergence report and the `external_unknown` sentinel
collision. **Disbelieve a number before believing the model defect it implies —
and prefer one constant over four copies.**
