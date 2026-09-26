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
Small P2P pays a uniformly drawn customer deposit account, the only
destination legitimate P2P pays (AMENDMENT bank-gl-2026-09, "The camouflage
pool, restricted at review").
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
| Card-view fraud rate | 0.1347% (12,997 of 9,645,706 view rows at pop 20k / 730d). **THE EXTERNAL ANCHOR WAS REMOVED 2026-08** — it was a released third-party artifact's observed 0.11675%, and that lineage is out. The rate is currently **UNCALIBRATED**: the measurement stands, the comparison does not. A replacement must be an issuer-side rate BY NUMBER of transactions, never value-loss basis points. **AXIS:** share of CARD-VIEW rows (channels card_purchase + merchant) carrying flag 1 — NOT the corpus-wide illicit ratio and NOT an event count |
| Liquidity coupling | adding legitimate cash inflows moved the corpus −3.2% (fewer overdraft-fee and retry rows). Deterministic and internally consistent; all invariance gates green |

Closing a material gap between the measured card-view rate and whatever
external anchor replaces the removed one is a FRAUD-BUDGET change
(targetEvents, rail mix, every fraud denominator) — owner-gated ADJUST
with golden re-pins, never a silent edit. **That rule is unchanged by the
anchor's removal: do not tune the rate toward a new anchor without going
through it.**

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
| Pay cadences | weekly .20 / biweekly .55 / semimonthly .15 / monthly .10, one draw per EMPLOYER (so the worker-weighted mix rests on the size law's largest payers, and at small populations on few draws) | CHOICE | BLS CES Feb 2023 ESTABLISHMENT shares: biweekly 43.0%, weekly 27.0%, semimonthly 19.8%, monthly ~10%; 72.9% of 1,000+ employee establishments pay biweekly [Certain]. **AXIS: PL needs WORKER-weighted shares and workers concentrate in large biweekly employers** | DEVIATES-BY-CHOICE (cannot claim conformance to a published number) |
| Employer roster and pick | `sizes::employerLaw`: 17 size classes (13 SUSB 2022 enterprise rows, the 20,000+ row as a rank-size tail, federal, state and local government), N_c = clamp(round(P x 0.74 x m_c), 1, F_c); every job and job switch picks through the class law (`growth::pickSized`, `pickSizedDifferent`) | MEASUREMENT (tables) + CHOICE (thinning) | Census SUSB 2022 [Certain]; BLS QCEW 2022 [Certain]; Census of Governments 2022 [Likely]. Authority rows: AMENDMENT counterparty-sizes-2026-09 | CONFORMS on the tables; the thinning to a national sample is a declared CHOICE |
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

**Landlord roster (counterparty-sizes-2026-09).** `sizes::landlordLaw`: the
seven RHFS 2021 property-size columns (CRS R47332 Tables 1 and 3 [Certain])
plus the NMHC 2024 Top-50 owners as a rank-size row carved out of the 150+
column [Likely], N_c = clamp(round(P x 0.35 x m_c), 1, F_c). Each landlord's
type is drawn from ITS CLASS's unit mix: individual = individual investor +
trustee + tenant in common; small LLC = LLC/LP/LLP + general partnership
below 25 units; corporate = every other reported form. The renter-weighted
mix is **.433 / .138 / .429** (reported-only aggregate .447 / .141 / .412);
the retired population-wide .38 / .15 / .47 mix is gone. Authority rows and
the verification corrections: AMENDMENT counterparty-sizes-2026-09.

**Renter SHARE:** `rent::Rules::paidFraction` = **.35** against the ACS
renter share of households ~.34–.36 [Likely]. **AXIS MISMATCH, REGISTERED
(counterparty-sizes-2026-09):** the PL value is a share of PEOPLE, each on
their own lease, and the anchor is a share of HOUSEHOLDS. The round research
puts a 200,000-person region at about 25,000 to 30,000 renter households
(.125 to .15 per person), so PL emits about 2.3 to 2.8 times as many rent
payers as there are renter households. Changing it moves every rent row and
balance, so it is an owner decision; the landlord roster is sized against the
payers PL actually draws. Effective persona shares:
student ≈ .33, retiree ≈ .12, freelancer ≈ .38, smallBusiness ≈ .23, HNW
≈ .07, salaried ≈ .41. **Aggregate reconciliation** [Derived]: PL
per-capita rent outflow ≈ .35 × $1,500 = $525/person-month vs real ≈ .35
× $1,487 ≈ $520. **SUPERSEDED (counterparty-sizes-2026-09): the real side
multiplies a HOUSEHOLD share by a per-household rent and calls it per
person.** At .125 to .15 renter households per person the real figure is
about $186 to $223 per person-month, so PL runs about 2.4 to 2.8 times high
on this aggregate (the axis mismatch above).

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

**Counterparties.** Each loan and policy pays a provider drawn per contract
from a national market-share table (mortgage, auto-loan and student-loan
servicers; auto, home and life insurers). Tax pays the one IRS account. Rows
and limitations: the institutional-providers-2026-09 amendment below.

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

Merchants core 120/10k + tail 400/10k; per 10k (floor): platforms 2 (2),
processors 1 (2), owner businesses 200 (25), brokerages 40 (5), clients 250
(floor 25, 2% internal-bank). **Densities re-classed CHOICE**: no public
per-10k-customer source exists and likely never will.

**Employers and landlords are no longer densities (counterparty-sizes-2026-09).**
The retired 25 per 10k employers (floor 5, 4% internal-bank) and 12 per 10k
landlords (floor 3) are replaced by the thinning law N_c = clamp(round(P x s x
m_c), 1, F_c) over cited size tables: SUSB 2022 plus government for
employers (all external), RHFS 2021 plus the NMHC Top-50 for landlords. At
200,000 people that is 89,231 employers and 66,658 landlords (was 500 and
240); at 500,000, 200,556 and 155,581. Authority rows: AMENDMENT
counterparty-sizes-2026-09.

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
| Card view | channels {card_purchase, merchant}; merchant-channel (account-paid POS) rows interpreted as DEBIT-card transactions; the impostor-push rail is excluded (a wire scam is not a card transaction) | CHOICE | mixing credit and debit in one card view is standard for transaction-fraud graphs |
| Card attribution | source Key in the card registry → that credit card (≤1 per person); any other source → the account's derived debit card. Unauthorized positives currently use the latter path only; parsing the `C`/`D` identifier tag as a feature is prohibited | CHOICE (label definition) + KNOWN GAP | credit-fraud servicing must move into `CardCycleDriver`; expiry, replacement, reissue, and multiple-card histories also remain unmodeled |
| Identifier scheme | C/D/M = prefixed role.bank.number of the entity Key; P&lt;person&gt;; T&lt;row_seq&gt;. Party ids are the CANONICAL customer ids and Merchant/IP use canonical renderings. Device IDs are stable opaque pseudonyms in one fixed-width `D` namespace; owner role is not exposed through prefix, width, or numeric range | CHOICE (identifier reuse/pseudonymization) | device IDs are categorical, not ordinals and not a security boundary |
| **Withheld entity labels** | `Card.is_fraud`, `Party.is_fraud`, `Device.is_blocked`, `IP.is_blocked` are FULL-WINDOW verdicts and render as **0**. Columns are RETAINED because TF_GNN_v3 loading jobs map POSITIONALLY. The verdicts live in dedicated `card_fraud."cf_Ground_Truth_Label"` (entity_type, entity_id, label) — positives only, pointed at by no edge, loaded by no job. The current export contains **37 tables** | CHOICE (owner ruling) | `Payment_Transaction.is_fraud` is the supervised target, never an input feature; production-like evaluation still needs delayed label availability |
| use_chip / error | **`use_chip` is CAUSAL since ROUND 8 (use-chip-causal-2026-07):** "Online Transaction" ⟺ the destination is a geography-free acceptance endpoint (catalog `Footprint::online` — the exact population both legitimate selection and the fraud rails draw card-not-present picks from — or a non-catalog remote biller); physically-located outlets split Chip/Swipe by the dated US EMV terminal mix (`chipShareBasisPoints`: 0 before 2012, .10 at the 2015 liability shift, .65 in 2019, frozen .90 outside coverage), with the per-row draw content-keyed on the retained `kUseChipLane`. `error` (2.0% incidence, mix Insufficient Balance .40 / Bad PIN .20 / Technical Glitch .20 / Bad Card Number .08 / Bad Expiration .05 / Bad CVV .05 / Bad Zipcode .02) REMAINS a content-keyed FNV hash — point-in-time SAFE but MECHANISM-FREE (authorization attempts are unmodeled) | MEASUREMENT-adjacent (entry mode; EMV curve values [Likely] — owner verifies vs the EMVCo US chip-share series) + CHOICE (`error` compatibility; non-catalog→Online) | **The `use_chip`/`error` VALUE SETS are inherited from a third-party corpus's column vocabulary. The name is removed; the STRINGS remain, because the exporter emits them and TF_GNN_v3 loads them — renaming a value is a schema change, not a docs edit.** Gate: `test_card_use_chip` (coherence, pre-EMV zero-chip, 2019 band). A modeled authorization-outcome mechanism for `error` is REGISTERED; see the use-chip-causal amendment below |
| Transaction-time sessions | `Transaction_Uses_Device(txn_id, device_id, edge_unix_time)` and `Transaction_Uses_IP(txn_id, ip_id, edge_unix_time)` carry the exact assigned row session. They are append-only stream-prefix edges; score a transaction from prior state before appending its current edges. `Has_Device` / `Has_IP` are header-only loader-compatibility tables | INVARIANT (causal export contract) | one edge per assigned endpoint, test-pinned against the payment stream |
| Device/IP vertex and topology closure | `cf_Device` and `cf_IP` are the union of synthesized roster infrastructure and endpoints actually observed in card-view rows; roster flag/blacklist facts remain withheld in vertices and quarantined in the overlay. Static Party ownership is withheld for all endpoints because exogenous attackers have no truthful Party owner | INVARIANT (referential integrity + anti-shortcut) | closes both missing-vertex and missing-Party-edge shortcuts without inventing ownership |
| Legitimate credit-card sessions | the access-router owner map merges card-registry ownership with deposit-account ownership, without inserting liabilities into the deposit-account slices. Legitimate credit-card purchases therefore receive the owner's normal device/IP session | MECHANISM REPAIR | closes the “credit-card row has no session” modality shortcut |
| mer_cat granularity | the 10-category merchant taxonomy stands in for real MCC codes | DEVIATES-BY-CHOICE | an MCC taxonomy would be its own round. For a TGN this is a coarse categorical edge feature — do not read it as merchant identity |
| Merchant geography | the exporter resolves each catalog merchant's world-modeled `Record.location` through the build-fixed geography catalogue; a physical record with a valid area emits one internally consistent Has_City/Has_State/Has_Zip plus Assigned_To/Located_In chain; `online` records and non-catalog destinations remain geography-free. City.population is copied from the world's `GeoArea.population`. No exporter geo hash or PII zip draw remains. **AMENDED merchant-coordinates-2026-07: the same catalogue row's `latitudeE6`/`longitudeE6` now also ship, as decimal degrees on `Merchant_Location` (per merchant), `Zipcode` and `City` — AREA CENTROIDS, so co-located merchants share a point** | CHOICE (world-state reporting) | **Current input is a 71-US-city + 15-international placeholder, not Census-complete; row order defines `GeoAreaId` and land area is NOT loaded, so true DENSITY is not computable.** Target provenance: Census Gazetteer + ACS |
| Party.gender | content-keyed even F/M split — gender is NOT modeled anywhere in the world | CHOICE | mechanism-free |
| Party.created_at | the Membership joinTs, identical to the public-schema customer table | CONFORMS | prohibition formally LIFTED |
| Is_Merchant | UNPOPULATED (header-only): the world has no merchant-owning-party link | DEVIATES-BY-CHOICE (documented gap) | — |
| PII layer | Address/Phone/Email/ID(ssn)/Full_Name/DOB vertices deduplicated over the roster; TF_GNN_v3 marks this layer DEMO ONLY | CHOICE (the use case's differentiator) | — |

**Anti-shortcut gates (measured; see the corrected levels below).** Fraud draws
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
not a new shortcut.

**CORRECTED 2026-08 — the 0.0000 pair recorded here was STALE, and the
world moved under it twice** (the join-cohort flip in
`harness-world-shape-2026-07`, then `merchant-selection-2026-08`). The gate
PRINTS its levels on every run and they disagreed with this paragraph. Read
off `test_card_baselines` today:

| leg | fraud-only-merchant share | recall@P≥0.90 | band |
|---|---|---|---|
| pop 300 × 730d (the gate leg) | **0.0079** (1 of 41 touched) | **0.0079** | <0.10 / <0.25 |
| pop 900 × 1461d | **0.0661** (6 of 153) | 0.0726 | <0.10 / <0.25 |

Both bands still pass and neither was widened. Two things change, though.
**Best precision at any threshold is 1.0000 (lift 295.65x), not the 0.1111
this document implied** — a merchant whose only rows are fraud scores 1.0
trivially and at these leg sizes one exists, so the bound is held by RECALL
being tiny, not by precision being unreachable. And **the larger leg carries
only 0.034 of headroom**, so any future round touching fraud destinations is
working with a quarter of the room the 0.0000 suggested. `AUTHORITY: measured
by execution, 2026-08; supersedes the 0.0000 pair.`

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
| Fee and interest postings pay external business counterparties (the card issuer, fee-collection and OD LOC keys, `Role::business` on `Bank::external`; cash-hub-defect-2026-08 lists the card issuer among the distinct external keys) | Every documented core types the contra as a bank-owned internal income GL (FLEXCUBE internal leaf GL of category Income, Temenos ledger categories with no customer, Fiserv DNA GL majors with the customer number blank), and the card accounts the issuer key charged were themselves `Bank::internal` (bank-gl-2026-09) | **Internal is not the same as customer.** The bank's own ledgers need a role of their own, or every exporter types them as a deposit account or an external party |

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
*Providers (institutional-providers-2026-09):* the FSA Data Center
"Portfolio by Loan Servicer" table (the EdFinancial and CRI split); NAIC
individual-life ranks 11-125; auto-lender ranks 6-25; mortgage servicer
shares by loan count over all 1-4 family loans. *Cash split:* IRS Cash
Intensive Businesses ATG; Census SUSB/CBP; Square
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
an SCF-anchored estate-size re-derivation; a dedicated funeral channel
(the funeral-home counterparty shipped in the unknown-counterparty-2026-09
amendment); repeat founders; surfacing latent sex to PII with a
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
sentinel as merchant #1. (The collision itself is closed by the
institutional-providers-2026-09 amendment: the catch-all now has a reserved
key. The catch-all itself is retired by the unknown-counterparty-2026-09
amendment.) (2) `test_table_golden`'s divergence report
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

══════════════════════════════════════════════════════════════════
# AMENDMENT — device-sharing-evidence-2026-08
══════════════════════════════════════════════════════════════════

RESEARCH + INSTRUMENT round. **Owner question: "is there research available
online to support our logic that high device sharing is actually correlated
with high transaction fraud?"** The answer is a qualified YES for the
MECHANISM and a flat NO for the MAGNITUDE, and the first authority row this
document has ever carried for device fan-out is below. It exists because the
prior record lived only in `CLAUDE.md`, which is git-ignored (`.gitignore:94`)
and therefore does not survive a clone: the Group-IB / fraud.net vendor
glossary that `fanout-bimodality-2026-08` rested on appears **nowhere in the
tracked repository.**

## F-8. Device / endpoint fan-out vs the label

| Parameter | PL value | Class | Real-world anchor & source | Status |
|---|---|---|---|---|
| Device fan-out is a fraud signal (DIRECTION) | Encoded: attacker devices reach 23–32 victims, enumeration probes 47–92 cards | MEASUREMENT (direction) | **Visa, "Anti-Enumeration and Account Testing Best Practices for Merchants" V1.2, April 2023 (Visa Public)** — names the trigger in as many words: multiple transactions with different payment accounts sharing one email address and one device ID may be a trigger for fraud classification or review. **Mastercard US 10552836 B2** (filed 2016-10-11, granted 2020) claims fraud risk scoring from the number of transaction accounts that have used a device. Both accessed 2026-08-11 | CONFORMS (direction only) |
| P(fraud \| N cards per device) — THE LEVEL | Not declared; emergent from case-load division | **UNCITABLE** | **No published series exists for cards-per-device in any domain.** OpenAlex body-text search returns ZERO works for "cards per device" and 11 equivalent phrasings; forward-citation traversal over 968 citing works of Ianus, GEM, InfDetect, TitAnt, Cash-Out, Financial Defaulter and SynchroTrap returns ZERO. Structural cause, which is worth more than the null queries: the real-card-data literature has no endpoint layer at all — **TitAnt** (Ant Financial's own payment-fraud system) explicitly defers device to future work; **xFraud** (eBay, 1.1B nodes) has no device node type; **APATE** (Van Vlasselaer et al., *Decision Support Systems* 75, 2015, real European issuer data) contains zero occurrences of device, fingerprint, IP or terminal | UNCITED — and the absence is now EVIDENCED, not merely unsearched |
| The nearest published series | — | MEASUREMENT (wrong population) | **Ianus** (Yuan, Miao, Gong, Yang, Li, Song, Wang, Liang; ACM CCS '19), Figure 3(b): P(Sybil \| device registered N accounts) = 44.6 / 57.7 / 73.7 / 81.8 / 88.0 / 91.8 / 97.6 / 91.8 / 94.7 / 98.9 % for N = 1…≥10, base rate 45.7% (647k Sybil / 1,417k registrations), max lift **2.16x**. Verified by pixel measurement at 600 dpi; the paper publishes no numeric table. **NOT card fraud** — WeChat account REGISTRATION on hard mobile identifiers, labels from WeChat's own detector. Not monotone (dips at N=8). The same paper's IP analogue is non-monotone with four of six buckets BELOW base rate, max lift 1.46x | DEVIATES — population is account abuse, not payments |
| **The measured card anchor (this round's contribution)** | See sub-gate K | MEASUREMENT | **IEEE-CIS Fraud Detection (Vesta Corporation, 2019)** — the only PUBLIC card-not-present dataset with device columns. 118,666 rows bearing `DeviceInfo`, base rate 0.07253, fingerprint = `DeviceInfo\|DeviceType\|id_30\|id_31\|id_32\|id_33\|id_13\|id_17\|id_19\|id_20`, 61,050 groups. Recomputed from the raw Kaggle CSVs and cross-checked against four independently published figures for the same files. **Whole-window lift by cards {1, 2, 3-5, 6-10, 11-25, 26+} = 0.485 / 1.080 / 1.756 / 1.888 / 1.719 / 1.567. AP ratio from degree alone 1.455; from plain ROW COUNT 1.546.** Accessed 2026-08-11 | see the four deviations below |
| Approved-probe → later fraud on that card | **ABSENT** — every probe this corpus emits is DECLINED (`Do Not Honor`, `kProbesPerCard = 1`) | KNOWN GAP | **Visa VAAI Score datasheet, 2025, VisaNet data**: "Globally, enumerated accounts have 22x higher fraud rates than regular accounts", and of enumerated accounts that saw fraud, **33% saw first fraud within 5 days of the enumeration transaction being APPROVED**. Denominator is the authorization network itself, so this is a census and NOT a reports database — it does not inherit the `giftcard-ratio-2026-08` rule 1 under-reporting factor. Accessed 2026-08-11 | KNOWN GAP — the corpus cannot express the strongest documented card-testing signal |
| Fingerprint identity is not a device | Coordinates one device chain per person, serving all their cards | DEVIATES-BY-CHOICE | **Gómez-Boix, Laperdrix & Baudry (WWW 2018)**: 2,067,942 real fingerprints, only **33.6% unique**. **Berke et al. (PoPETs 2025)**: ~60% unique in a representative US panel. **Vastel et al. (IEEE S&P 2018, FP-Stalker)**: fingerprints churn within days. So a measured "device" node is partly collision and partly fragmentation — high degree can be an artifact with no attacker behind it, and one physical device presents as many fingerprints | DEVIATES — see gap (1) |

### The four measured deviations, with sub-gate K's armed readings

| # | Quantity | This corpus | IEEE-CIS | Disposition |
|---|---|---|---|---|
| 1 | Share of rows on a **single-card** fingerprint | **6.5%** | **50.5%** | **The root cause of every other row.** Real fingerprints fragment; this corpus gives one person one device chain serving all their cards, so degree 1 is a minority bucket here and the MAJORITY bucket in reality. REGISTERED, not fixed — the fix is fingerprint fragmentation in legitimate device synthesis, cited to Vastel et al., and it is draw-spending: it moves every golden. |
| 2 | Degree-1 lift | 1.96 – 2.46 | **0.485** | Consequence of (1): a small bucket is dominated by victim-endpoint compromise, where a large one is dominated by ordinary first-time traffic. `kMaxDegreeOneLift = 2.95` is left ALONE and its "the bimodal curve is real" justification is WITHDRAWN in place. Tightening the band to 0.485 would pin a target no current construction can reach. |
| 3 | Curve **shape** | U-shaped (hot at 1 and at 26+, trough at 2–5) | **Unimodal** hump at 6–10, gentle decay | PRINTED per leg beside the anchor, not banded. Banding a shape the construction cannot produce is `merchant-selection-2026-08` rule 5. |
| 4 | AP(degree) ÷ AP(row count) | **1.75 – 2.20x** | **0.941x** | Corpus per-endpoint ROW COUNT is ANTI-predictive (AP ratio 0.656–0.710 — a busy endpoint is SAFER than average) where IEEE-CIS measures it PREDICTIVE at 1.546. So this corpus concentrates discriminating power in the graph axis and reality spreads it across both. **A GNN trained here will lean on structure harder than a production model should.** Bounded by K.3 at 2.65; closing it is a change to the endpoint row-mass mix (POS terminals carry thousands of legitimate rows against a compromise case's 5–14). |

### What SHIPPED

**Sub-gate K in `tests/test_card_endpoint_graph.cpp`** — the gate
`fanout-bimodality-2026-08` rule 4 asked for by name ("PREFER AN AP/AUC-PR
BOUND OVER A LIFT BOUND when the question is 'will a model learn this'").
Test-only: **no golden moves and `kTableCount` does not move.**

| Check | Band | Armed (4 legs) |
|---|---|---|
| K.1 ceiling, AP ratio from degree alone | ≤ **1.75** (mean + 3.5 SD of 1.318 / 1.440 / 1.151 / 1.218) | 1.151 – 1.440 |
| K.2 floor, same quantity | ≥ **1.05** | 1.151 – 1.440 |
| K.3 ceiling, AP(degree) ÷ AP(row count) | ≤ **2.65** (mean + 3.5 SD of 1.855 / 2.195 / 1.747 / 1.814) | 1.747 – 2.195 |
| K.4 non-vacuity | rows > 0, fraud > 0, base rate > 0, **and** support spans both the degree-1 and 26+ buckets | — |
| K.5 | the six-bucket corpus curve PRINTED beside the IEEE-CIS anchor | — |

**NON-VACUITY PROVEN IN ALL THREE DIRECTIONS, and one honest miss recorded:**

| Disarm | K.1 | K.2 | K.3 |
|---|---|---|---|
| `kDisarmInstrumentCeiling` (pre-round world) | RED 24.39 / 36.91 | — | RED 40.09 / 60.89 |
| `kDisarmFraudOffLowDegree` (all fraud on shared infra) | RED 11.48 / 9.65 / 8.10 | — | **GREEN 1.15 / 1.21 / 2.10** |
| `kDisarmDegreeApNoise` (NEW — label independent of degree) | — | RED 1.0002 / 0.9998 / 1.0005 / 1.0013 | — |

**K.3 misses the second leak and that is stated rather than hidden**
(`merchant-selection-2026-08` rule 6, fifth instance): piling fraud onto the
single widest endpoint raises BOTH rankers together — row count reaches 9.99x
— so the RATIO barely moves while each half moves enormously. **A ratio is
blind to a leak common to its numerator and denominator.** K.1 catches it.

**TWO INSTRUMENT DEFECTS FOUND WHILE BUILDING IT.**

1. **K.2's floor cannot be the analytic 1.0.** A noise ranker's AP ratio is
   1.0 in expectation, so a floor at 1.0 is a coin flip on float error — the
   disarm reproduced pure noise and **three of four legs stayed green.** Banded
   at 1.05 on the measurement. `merchant-selection-2026-08` rule 6 again.
2. **The first noise disarm built a stronger leak than it removed.** Rounding
   each endpoint's fraud share independently truncates every endpoint under
   `1/baseRate` ≈ 107 rows to ZERO fraud and dumps the label onto the large
   ones, which made ROW COUNT **25x** predictive. Fixed with cumulative
   apportionment. **A disarm is a construction and needs checking like one.**

### Registered, NOT closed

- **Gap (1), fingerprint fragmentation** — the highest-value realism change
  available on this axis, cited (Vastel et al. IEEE S&P 2018; Gómez-Boix et
  al. WWW 2018), and draw-spending: it moves every golden and needs owner
  sign-off. Predicted effect: moves legitimate mass into degree 1, which
  simultaneously repairs deviations (2) and (3).
- **Gap (4), volume anti-predictiveness** — a change to the endpoint row-mass
  mix, not a dial.
- **The approved-probe population** — adding one requires a new band on
  `test_card_enumeration`'s straddle-1.0 requirement BEFORE it lands, because
  that requirement is only correct for the declined tail.
- **`cards per device` has no published analogue in any domain.** Every
  positive finding located measures ACCOUNTS or USERS per device (Ianus, GEM,
  InfDetect, iovation, ThreatMetrix). ThreatMetrix's five-card rule is cards
  per ACCOUNT. This corpus's quantity is cards per DEVICE.

### Counter-evidence located, recorded so the direction is not over-read

Two Ant Financial production systems **DELETE** devices seen with only one
account as low-risk (InfDetect measures the cost at <0.1% performance loss).
**Rappi/UC Berkeley/UCSD (KDD-MLF '21)**, a real payment population with a
user–device–card graph: pure graph structure scores AUC **0.5626–0.6538**, and
device relation importance collapses **4.0970 → 0.1048 (~39x)** once
behavioural features are added. **GEM** (Alipay, CIKM 2018): ranking accounts
by device-linked component size scores AUC 0.665–0.694 against 0.916–0.936 for
the full model. **PCI SSC / NCFTA (2020)**: attackers deliberately parallelise
across sites (5 attempts × 12 sites; ~200 sites for CVV discovery) to keep
per-site counts low, and Visa (2024) observed enumeration "distributed across
hundreds of merchants" — i.e. the high-fan-out-on-one-endpoint assumption is
itself what attackers evade. **Amazon FDB**: on the only public CNP set with a
raw `device_id`, four of five AutoML frameworks cannot beat random (AUC
0.515–0.636).

CLASS: research + instrument. Status: CONFORMS (direction cited, level
registered as uncitable, four deviations measured and registered). GOLDEN
IMPACT: **NONE — no golden moves, `kTableCount` unmoved at 43.**

## STEP 2 — THE CONSUMER-VISIBLE VIEW, AND THE DILUTION RUNS THE WRONG WAY

**Registered item #2 of this amendment is now MEASURED, and it did not resolve
the way the design predicted.** Sub-gate K.6 in
`tests/test_card_endpoint_graph.cpp`. Test-only; no golden moves.

### The blind spot was real, and worse than "the gate reads the wrong vector"

The first attempt merged `posted.declined` into the degree rollup and measured
**ZERO probes on all four legs** against 4,421–5,225 declined rows in view.
Cause: enumeration probes are synthesized at EXPORT time by
`streaming.hpp`'s `writeEnumerationProbe`, not decided by the ledger replay.
**So the dilution exists in the exported CSV and in NO in-memory structure at
all** — `posted.declined` carries only the funding and non-funding declines.
Every degree ceiling in that file (I.1's all-fraud bucket, I.2's best
precision, K.1's AP) has been blind to it since `device-fanout-2026-08`
shipped, and no harness in the repository measured degree over the view a
consumer actually reads.

K.6 therefore reproduces the exporter's own synthesis — one probe per card on
its FIRST view row, with the `backdatedRowIsObservable` window and membership
guards. **That is only sound because `probeFor` is draw-free and stateless**;
the property `enumeration.hpp` maintains for golden containment is exactly what
lets a second caller reproduce the same probes.

### Measured, four legs

| leg | probes in view | AP ratio settled → merged | best precision | degree-1 lift |
|---|---|---|---|---|
| leg-long | 138 | 1.318 → **1.402** | 0.0383 → 0.0433 | 2.330 → 2.269 |
| leg-wide | 272 | 1.440 → **1.597** | 0.0563 → 0.0559 | 1.960 → 1.838 |
| leg-sizeA | 182 | 1.151 → **1.231** | 0.0254 → 0.0299 | 2.463 → 2.404 |
| leg-sizeB | 220 | 1.218 → **1.331** | 0.0446 → 0.0441 | 2.337 → 2.192 |

**THE DILUTION MAKES DEGREE MORE PREDICTIVE, NOT LESS — on every leg, by
+6.4% / +10.9% / +7.0% / +9.3%.** And leg-wide's merged **1.597 EXCEEDS the
IEEE-CIS anchor of 1.455**, so the consumer-visible corpus is, on that leg,
more degree-separable than real card data. A ceiling sized on the anchor would
red there.

The one thing it does help is the low tail: degree-1 lift falls 2.7–6.2% on
every leg, because probes attach to cards whose settled rows are elsewhere.

### Two standing claims corrected

1. **`device-fanout-2026-08`'s "the top-degree endpoint in the corpus is no
   longer a fraud endpoint" does not hold in this view.** Max degree is
   UNMOVED by the merge (322→322, 324→325, 355→355, 418→418) because the top
   endpoint is a public terminal at 322–418 cards, far above both the quoted
   probe range (47–92) and the quoted compromise range (37–40). Those two
   numbers were measured in `test_card_enumeration`'s own harness and do not
   describe the endpoint-graph view.
2. **"Probes dilute the high-degree tail" is not what the merge shows.** Probes
   add only 7–16 new endpoints per leg at ~8.6 cards each — MID-degree,
   zero-fraud — while `kProbedBasisPoints = 400` was sized so that "the top
   enumeration device's degree land[s] in the same order as a busy attacker's".
   At these legs it does not reach the top of the distribution at all.

### Disposition

**PRINTED, NOT BANDED, and only one assertion ships:** that the merged view
CONTAINS probes at all (`probeViewRows > 0`), which is the non-vacuity check
whose failure exposed the blind spot in the first place. The merged AP is not
banded because every existing band in sub-gates I and K was measured on the
settled view, and re-pointing them at a wider view would be
`merchant-selection-2026-08` rule 13 — a band measured against a superseded
construction is not a measurement.

**OPEN, and this is now the highest-value question on this axis:** whether
`kProbedBasisPoints` should rise so probe endpoints actually reach the top of
the degree distribution, or whether the dilution premise should be withdrawn.
The level is CLASS S UNCITED and explicitly "sized for fan-out, not for
prevalence", so raising it needs no incidence estimate — but it must be sized
against the MERGED view, which nothing has ever measured until now.

## STEP 3 — THE PROBE SHARE WAS SWEPT, AND RAISING IT AMPLIFIES THE LEAK

**Owner selected `kProbedBasisPoints` as the next lever. Measurement says do
not pull it.** Sub-gate K.7. `kProbedBasisPoints` is UNCHANGED at 400.

### The sweep

Swept 400 / 1000 / 2000 / 3000 / 4000 / 6000 bp over the CONSUMER-VISIBLE view
on all four legs. `probeFor` gained a defaulted `probedBasisPoints` parameter so
the sweep uses the real resolver rather than a duplicated predicate.

| bp | leg-wide AP ratio | leg-long best precision | leg-long top probe degree |
|---|---|---|---|
| 400 (shipped) | 1.598 | 0.0434 | 69 |
| 1000 | 1.754 | 0.0526 | 151 |
| 2000 | 1.839 | 0.0638 | 262 |
| 3000 | 1.860 | **0.1667** | 390 |
| 4000 | **1.881** | 0.1301 | 517 |
| 6000 | 1.828 | 0.0899 | 778 |

**Both quantities RISE with the share.** At 2000bp and above the AP ratio
exceeds `kMaxDegreeApRatio = 1.75`, so raising this constant would RED the gate
that bounds the quantity it was meant to protect.

Incidentally this corrects the STEP 2 diagnosis: probe endpoints DO out-degree
compromise devices already at 400bp — top probe degree 58–98 against attacker
max 23–32. STEP 2's "~8.6 cards per probe endpoint" was the MEAN, not the max.
The design intent on fan-out is met; the intent on dilution is not.

### The cause, and it is logically forced rather than incidental

`probeFor` resolves its endpoint from the **same `AttackerInfra` inventory the
compromise planner draws from** — a different salt over the same operator
lines. Measured, of probe rows landing on a device already present in the
settled view:

| leg | probe rows on a settled device | of those, on a FRAUD-carrying device |
|---|---|---|
| leg-long | 71 of 138 | **71 (100%)** |
| leg-wide | 229 of 272 | **229 (100%)** |
| leg-sizeA | 138 of 182 | **138 (100%)** |
| leg-sizeB | 191 of 220 | **191 (100%)** |

**No exceptions, and it cannot be otherwise.** Attacker inventory carries no
legitimate traffic, so an attacker device is visible in the settled view only
because a compromise case used it. Every probe stapled to one of those raises a
fraud-carrying device's degree and pushes it UP the ranking with its fraud rows
intact. **The diluent shares its device pool with the thing it is diluting.**
7–16 of each leg's 16–24 probe devices already carry settled fraud.

### Disposition

**`kProbedBasisPoints` STAYS AT 400**, with the sweep and the cause recorded at
the constant so the next reader does not repeat the attempt. Raising it is now
a documented anti-fix.

**WHAT WOULD ACTUALLY WORK, and it is the registered next step:** a probe
endpoint pool DISJOINT from the compromise device lines, so a probe device
carries probe rows only and its absent label is real. Containment is the cheap
class — exporter-side synthesis, so `golden_run.b2sum` stays unmoved and only
`golden_tables_card_fraud.md5` moves — but it is a resolver change to
`probeFor` and `AttackerInfra`, and it has NOT been made here. It also needs
`test_card_enumeration`'s bands re-measured, since probe endpoint identity
moves.

**THE GENERAL LESSON, and it is the round's most reusable:** an anti-shortcut
population must be DISJOINT from the population it masks. A diluent drawn from
the same pool as the signal does not dilute — it concentrates. This is the
third construction in this repository to share a pool with the thing it was
built to obscure, after the merchant-ownership register (kept keyed on the
merchant key alone for exactly this reason) and the residential-proxy address
(deliberately not marked as attacker inventory).

═══════════════════════════════════════════════════════════════════════
# AMENDMENT — cash-hub-defect-2026-08
═══════════════════════════════════════════════════════════════════════

**✅ CLOSED IN THE CUSTOMER-LEDGER PROJECTION.** The full pre-fix diagnosis,
measurements, design alternatives, implemented disposition, and executable
gates live in `docs/cash_hub_defect.md`. The historical measurements below are
retained so this repair cannot silently regress.

**The former defect in one sentence.** Every ATM withdrawal was credited to a
single randomly chosen roster person's primary checking account; the account
reported infinite liquidity and exported as `inf`. The implementation now uses
multiple ownerless, area-local external cash endpoints and one-sided boundary
posting, with no credited endpoint balance.

## The authority rows

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| ATM withdrawal destination = a registered `Bank::external` processor endpoint among the customer's own nearest four at the event-time area, ties at the four-point cut broken by a per-(person, area, rail) hash window (`cash_points.hpp`, `plans.cpp`, `atm.cpp`; amended by atm-spread-2026-09, which replaced the per-area pool-index tie-break) | A real on-us ATM withdrawal reduces the bank's currency-and-coin asset; an off-us withdrawal also creates network settlement. Neither is a depositor's account | MEASUREMENT (accounting) | Fed FR 2900 reporting instructions (vault cash includes currency in a bank's own ATMs); Oracle FLEXCUBE Core Banking ATM User Manual, screens ATM01/ATM02 (accessed 2026-08-18) [Certain on the mechanism] | **CONFORMS AS A DECLARED CUSTOMER-LEDGER PROJECTION**: only the internal customer leg is booked; the full GL is outside scope |
| Centralized vault-cash / network-settlement GL | Real cores centralize or modestly shard these GLs; terminals are transaction context rather than customer accounts | MEASUREMENT (accounting) | Oracle FLEXCUBE ATM User Manual ATM02 (accessed 2026-08-18); Philadelphia Fed, *Clearing and Settlement of Interbank Card Transactions* (Oct 2013) [Certain] | **CONFORMS BY EXPLICIT OMISSION**: the GL is outside the projection and is not emitted as a global graph target |
| Cash withdrawal points, cash-deposit points, card issuer, billers, employers, and landlord fallback use distinct external keys/pools | A real core carries distinct cash, settlement, issuer, and counterparty roles | TYPOLOGY | Oracle FLEXCUBE ATM User Manual ATM01/ATM02 field lists (accessed 2026-08-18) [Certain] | **CLOSED** |
| Combined ATM terminal/acceptor endpoint, without dedicated DE41 and DE42 columns | The graph identity an issuer-side fraud model sees is the ISO 8583 **pair** DE 41 Card Acceptor Terminal Identification + DE 42 Card Acceptor Identification Code, one acceptor to many terminals, unique only as a pair | MEASUREMENT | Elavon Developer Portal field-41 description; Galileo/SoFi issuer data-elements map exposing DE41 as `terminal_id` and DE42 as `merchant_id`; Marqeta `card_acceptor` (all accessed 2026-08-18) [Certain] | **PARTIALLY CLOSED — EXPLICIT CARRIER GAP** |
| Distributed, geographically resolved cash-point endpoints; no customer or system-wide target | ZERO of five published AML/fraud datasets use one monolithic customer cash node. AMLSim shards onto `Branch`; PaySim shards onto merchants; IBM AMLworld makes cash an edge attribute; the Neo4j reference reifies the transaction; Sparkov omits cash | TYPOLOGY | AMLSim `Branch.java`/`CashOutModel.java`; PaySim `Client.java`; NeurIPS 2023 D&B *Realistic Synthetic Financial Transactions for AML* Table 5; Neo4j `fraud-detection.adoc`; Sparkov README (all accessed 2026-08-18) [Certain] | **CUSTOMER-GRAPH DEFECT CLOSED**; downstream consumers must retain external type/channel |
| Interchange fee direction on ATM rows | On a PURCHASE the acquirer pays the issuer; on a CASH WITHDRAWAL the ISSUER pays the acquirer. The sign inverts | INVARIANT (accounting) | Philadelphia Fed, *Clearing and Settlement of Interbank Card Transactions* (Oct 2013) (accessed 2026-08-18) [Certain] | **NOT MODELLED** (PL has no ATM interchange leg; recorded so nobody adds one with the purchase sign) |
| US ATM density, if a terminal layer is ever sized | There is NO current official US ATMs-per-capita statistic. IMF FAS via World Bank reports the US only through 2009 (425,010 ATMs, 172.76 per 100k adults) and is null 2010 onward. Only current count is commercial: 451,500 (2022), down from a 470,000 peak (2019). The WITHDRAWAL count IS official: 3.7 billion in 2021, average value $156 (2018) → $198 (2021) | MEASUREMENT | World Bank `FB.ATM.TOTL.P5`; Euromonitor via Payments Dive 2023-06-23; Federal Reserve Payments Study 2022 triennial (all accessed 2026-08-18) [Certain on the withdrawal count; the 451,500 is trade press, not a statistical release] | **CLASS S UNCITED for density**; the two candidate anchors disagree ~8x. **Amended by atm-spread-2026-09:** 451,500 / 333.3M people = 13.5 per 10,000, which is the declared `atmTerminals` density (`synth/counterparties/make.hpp`); ATMIA's 520,000 to 540,000 (ATM Marketplace, 12 Sep 2023 [S]) gives 15.6 to 16.2. The density CONFORMS on the trade-press count; the ~8x disagreement was the throughput anchor, which that amendment registers against the ATM withdrawal frequency |

## Pre-fix measurements, retained as the regression baseline

On the 23,866,506-row production export (499,409 distinct accounts): **ONE** ATM
destination account, `A0000247513`, receiving **1,823,332 rows = 7.64% of the
corpus** (1,230,944 `atm_withdrawal` + 352,271 `cc_interest` + 239,313
`cc_late_fee`) from **344,576 distinct counterparties = 69.0% of all accounts**.
It is the highest-degree vertex in the graph and the only one in the top eight
that is not `X`-prefixed. Next highest is `XM00000001` at 176,196.

**Money conservation is broken by +$36,248,870.40, or +42.31% of gross applied
value** (pop 900, 731d, seed `0xC0FFEE`, 668,247 rows, gross $85,682,895.37).
The larger contributor is a SEPARATE bug: 30 `fundingHubs` are `createHub`'d but
never covered by `seedHubAccounts`, so they sit at cash 0.00 with a bypassed
funding screen and source **$33,260,807.20**.

**`kHubCash = 1e18` has a $128 ulp**, so every credit at or below $64 vanishes.
That is **7 of the 18 `kAtmAmounts` entries** ($20, $40×3, $60×3) crediting
$0.00 forever, and **89.7% of hub-credit rows ($2,376,059.08 of $3,697,214.85)
disappearing** on the real corpus.

**The `inf` is reproduced, not inferred.** `docs/cash_hub_inf_repro.cpp` drives
the real `clearing::Ledger` and the real `exporter::csv::Writer` and emits
`A0000000012,inf`. Through `aml::exportAll` at pop 400 it is 4 of 1,674
`Account` rows, scaling to **700 rows at the shipped default
`--population 70000`**.

## Why the pre-fix suite could not see it

`grep -rn 'isfinite|isinf|std::isnan' tests/*.cpp` returns **zero hits** in a
68-test suite. `test_pipeline_e2e` renders the `inf` rows today and passes,
because `expectTable` checks only presence. The plain `aml` use case is covered
by **no golden section at all**. And `tests/golden_tables_aml.md5` line 35
digests `aml_txn_edges_vertices_Account`, whose column 7 is `balance`, so **the
pinned md5 already encodes the `inf` and the correct fix will RED a green
golden.**

## Rules this produced

1. **A DIGEST GOLDEN PINS WHATEVER IT WAS GIVEN, INCLUDING AN ABSURDITY.** A
   byte pin answers "has this changed", never "is this sane". Pair every digest
   pin with at least one predicate on the value's DOMAIN; finiteness is the
   cheapest one there is. This is the sibling of `bls-citation-2026-07` rule 4
   ("a lower bound is not a count").
2. **A SYNTHETIC SINK MUST NOT BE DRAWN FROM THE POPULATION IT SERVES.** Fourth
   instance of the disjointness rule, after the merchant-ownership register, the
   residential-proxy address and the enumeration probe pool, and the worst of
   the four: the sink is a CUSTOMER, still eligible for victim and mule
   selection, with 69% of accounts as its 1-hop neighbours.
3. **AN INFINITY IS A SENTINEL AND MUST NOT CROSS AN EXPORT BOUNDARY.** Infinite
   liquidity is a legitimate in-simulation device for "never rejects"; it became
   a defect only when an exporter read the sentinel as a quantity. Same shape as
   `loc-accrual-perf-2026-08` rule 5 (`ts == 0` is a sentinel, not an instant)
   and the card-fraud `device_risk_score = -1` convention. **Give the ledger a
   separate reporting accessor so the sentinel cannot be read as data.**
4. **`std::to_chars` SUCCEEDS ON INFINITY, SO AN `errc` CHECK IS NOT A VALIDITY
   CHECK.** `csv.cpp`'s throw was never going to fire, and its trailing-zero
   fixup probes for `.eEnN`, which matches the `n` in `inf`, so the one piece of
   code that inspected the rendered text waved it through. A formatter's error
   code tells you it COULD format the value, not that the value should exist.
5. **A SEEDING WALK AND A FLAGGING WALK OVER DIFFERENT KEY SETS IS A SILENT
   HOLE.** `createHub` covered `fundingHubs`; `seedHubAccounts` did not, and the
   resulting 30 zero-balance accounts created more money than the documented
   hubs did. When two passes configure the same concept, assert they cover the
   same set.
6. **`1e18` IS NOT A LARGE NUMBER, IT IS A NUMBER WITH A $128 QUANTUM.** A
   saturating balance needs a FLAG, not a magnitude.
7. **CENTRALISED IS RIGHT, CUSTOMER-OWNED IS WRONG, AND THE TWO GOT
   CONFLATED.** Real cores do centralise the ATM cash GL, so sharding per
   machine at the LEDGER level models something no core system does. The key
   that carries the fraud signal is the ISO 8583 terminal/acceptor pair, which
   lives on the TRANSACTION. **Separate the money leg from the context leg
   before choosing a cardinality.**
8. **HUB SELECTION VIOLATES `merchant-churn-2026-07` RULE 2 AS WRITTEN**, and
   that is why the fix is expensive: its draw COUNT depends on `populationCount`
   and it sits FIRST on the SHARED sequential stream, ahead of the opening book,
   every routine and fraud planning. Floyd's sampler runs `j` over `[n-k, n)`
   with `range = j+1`, so changing the hub count changes the value of the FIRST
   draw. **Move hub selection to its own `RngFactory` lane and re-pin ONCE,
   before changing the hub model**. Otherwise the work costs two full re-pins
   and two full band re-measurements.

## Implemented disposition and residual work

1. **Implemented:** removed `createHub`, hub flags, `kHubCash`, all hub seeding,
   customer selection, hub exclusions, and customer-account fallbacks.
2. **Implemented:** all replay/screening paths post by key; external keys are
   boundary markers rather than internal ledger indices, while unknown internal
   keys reject as unbooked. CSV rejects non-finite doubles.
3. **Implemented:** population-scaled ATM and cash-depository pools, home-area
   placement, event-time relocation, nearest-point selection, and stable
   draw-free customer affinity. Billers and the card issuer use separate pools.
4. **Implemented:** typed clearing contracts now bind endpoint kind, direction,
   and channel for ATM withdrawals, cash deposits, settled check deposits, and
   crypto USD ramps. Household cash/check deposits have a dedicated isolated
   routine; crypto ramp-in is capped by prior accepted per-account ramp-out
   inventory. See `docs/customer_ledger_boundaries.md`.
5. **Residual:** dedicated DE41 + DE42 carriers/edges, cash-point geography and
   on-us/off-us export, and an ATM interchange leg with the withdrawal sign.
6. **Residual:** downstream customer-flow queries must filter external boundary
   nodes or use a typed/reified cash-access edge; terminal-density calibration
   remains class S.

## Also found

- **Closed:** the 30 unseeded `fundingHubs` and their funding bypass no longer
  exist.
- **Closed:** `LegitCounterparties::hubAccounts`,
  `CounterpartyAccess::isHub`, and `CounterpartyAccess::firstHub` were removed.
- **Contained in production, still an API hardening item:** the biller pool now
  has a non-empty external fallback, but `unauthorized.cpp` still assumes a
  caller-provided `billerAccounts` span is non-empty.

## Downstream, and it is why the generator fix alone is not enough

The customer-owned `A…` supernode and its `inf` balance are gone. However,
`MulePatternLearner`'s pre-graph aggregator requires only
`src_acct, dst_acct, amount, ts` and **discards the `channel` column the export
already provides**, and `mule_ml`'s `Transfer_Transaction.csv` has no channel
column at all, so an ATM withdrawal is byte-indistinguishable from a P2P
transfer. Its GSQL feature queries carry **no `is_external` guard**, so WCC
merges the population into one component (making `com_size`, a live model
feature, meaningless) and PageRank can still mishandle shared external
endpoints if type/channel is discarded. The former super-node harm is
documented rather than speculative: SALT-GNN (arXiv:2607.10131)
measures degree-stratified AML GNN degradation on these exact datasets and
states that aggregate F1 HIDES it; GCNs are biased TOWARD high-degree nodes
(Tang et al., CIKM 2020, arXiv:2006.15643), so a super-node earns flattering
metrics while distorting everything around it.

═══════════════════════════════════════════════════════════════════════
# AMENDMENT: institutional-providers-2026-09
═══════════════════════════════════════════════════════════════════════

**What changed.** Three defects that made a handful of external accounts
population-wide hubs (measured on the 200,000-person mule-temporal corpus in
`docs/research/counterparty_hubs_2026-09.md`):

1. **Bug A, closed.** Every mortgage paid the STUDENT-LOAN servicer
   (`mortgage.cpp` routed to `Lending::studentServicer`, flagged "SUSPECTED
   DEFECT" since 2026-07-19), so one account took 1.43M payments.
2. **Bug B, closed.** The external-unknown catch-all was
   `makeKey(merchant, external, 1)`, which IS catalogue merchant serial 1
   whenever that merchant banks externally (98% of seeds), so the catch-all
   and a real merchant were one registry record and one exported node. The
   catch-all now has its own reserved key, `XM1000000001`. Only the key moved:
   the flow itself (the 5% unattributed spending share, P2P with no usable
   contact, and funerals) is unchanged here and is retired by the next stage.
3. **Change 3, shipped.** The six population-wide lender and insurer keys are
   replaced by six provider markets (mortgage, auto loan, student loan, auto,
   home and life insurance). Each contract draws its provider once, at
   issuance, from the tables below, with one uniform on its own
   `{"product-provider", market, person}` lane
   (`synth/products/providers.hpp`, `entities/counterparties/providers.hpp`).
   SSA, disability and the IRS stay single accounts.

Bank-originated postings (card interest and fees, overdraft fees, the overdraft
line of credit) are untouched by this amendment.

## The authority rows

Shares are normalized within the pool. [P] = primary document read,
[S] = secondary summary, both as reported by the round's research pass
(summarized in `docs/research/counterparty_hubs_2026-09.md`, section "Lenders
and insurers are many firms, not one") and by the design's parameter pass,
which read the NAIC, FSOC and Big Wheels tables for the per-rank values.

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| Mortgage servicers: ranks 1-20 = .073 .067 .067 .061 .060 .054 .052 .052 .031 .025 .024 .024 .023 .018 .017 .015 .014 .011 .011 .010, then a 1/rank tail to a pool of 300 | Agency servicing is spread: largest 7.3%, top 10 54.1%, top 20 70.9%, HHI about 350. Mr. Cooper served 6.7M customers in Dec 2024 (about 13% of 50.8M mortgages, subservicing included) | MEASUREMENT | FSOC 2024 Nonbank Mortgage Servicing Report, Table 1 (Inside Mortgage Finance, agency UPB, Q4 2023) [P]; Mr. Cooper 2024 [P] | **CONFORMS** on ranks 1-20 (value-weighted: see the value-weighting limitation below); built table HHI 351 |
| Auto insurers: ranks 1-25 = .1864 .1860 .1156 .1015 .0619 .0357 .0281 .0193 .0190 .0153 .0142 .0139 .0137 .0110 .0096 .0083 .0080 .0071 .0062 .0047 .0044 .0043 .0042 .0042 .0041, tail to 100 | State Farm 18.64%, Progressive 18.60%, top 10 76.88%, top 21 87.0%, HHI about 1,000 | MEASUREMENT | NAIC Property and Casualty market share report (2025), Private Passenger Auto Total [P]. The research note's 2024 read (State Farm 18.87%, Progressive 16.73%, top 10 76.15%) is [S] and is superseded by the primary 2025 table | **CONFORMS**; built table HHI 1,012 |
| Home insurers: ranks 1-25 = .1869 .0942 .0702 .0551 .0546 .0515 .0457 .0250 .0219 .0197 .0185 .0177 .0107 .0103 .0101 .0095 .0091 .0089 .0080 .0078 .0070 .0069 .0068 .0066 .0065, tail to 150 | State Farm 18.69%, Allstate 9.42%, USAA 7.02%; top 5 46.11%, top 10 62.48%, HHI about 620 | MEASUREMENT | NAIC Property and Casualty market share report (2025), Homeowners Multiple Peril [P] | **CONFORMS**; built table HHI 631 |
| Life insurers: ranks 1-10 = .0866 .0576 .0552 .0508 .0408 .0373 .0349 .0328 .0318 .0302; anchors top 25 = .7283 and top 125 = .9924; pool of 125 renormalized by .9924 | Northwestern Mutual 8.66%, New York Life 5.76%, MassMutual 5.52%; top 10 45.79%, top 25 72.83%, top 125 99.24%, HHI about 300 | MEASUREMENT | NAIC market share report for life/fraternal groups (2024), individual life [P] | **CONFORMS** on ranks 1-10 and both anchors. Ranks 11-125 are the 1/rank shape between the anchors (the NAIC table lists them; they were not transcribed). Built table HHI 302 |
| Auto lenders: ranks 1-5 = .054 .054 .050 .046 .044 (Toyota Financial, GM Financial, Ally, Chase, Capital One), tail to 200 | Largest about 5.4% of about $1.9T outstanding, top 5 about 25%; the research note gives Toyota Financial about 6% of $1.8T and top 5 about 25% | MEASUREMENT | Auto Finance News, "Big Wheels" 2025 outstandings ranking [S]; research note [S] | **CONFORMS [Likely]**: secondary source only. Ranks 6-200 are the declared tail (see the auto-lender tail limitation below) |
| Student servicers, federal part (92.4% of the pool): Nelnet .31, Aidvantage .19, MOHELA .15, EdFinancial .175, CRI .175, each times .924 | Five federal servicers. Nelnet 14.0M of about 45M borrowers (about 31%); MOHELA 6.7M (about 15%); Aidvantage 8.4M to 9M (about 19%) | MEASUREMENT for Nelnet, MOHELA and Aidvantage; CHOICE for EdFinancial and CRI | Nelnet 10-K 2024 [P]; MOHELA [P]; Aidvantage [S]; all via the research note. The design pass found MOHELA 6.8M accounts in Feb 2025 (consistent with 15%) and a 2021 Aidvantage figure of 5.6M (stale, not adopted) | **PROVISIONAL**. EdFinancial and CRI are uncited and split the federal remainder equally, which also absorbs the Default Resolution Group (see that limitation below). Replace with the FSA Data Center "Portfolio by Loan Servicer" table when it is pulled (it timed out in both research passes) |
| Student lenders, private part (7.6% of the pool): Sallie Mae .63 of it, then nine lenders on a 1/rank tail (pool of 15 in all) | Private loans are 7.6% of student-loan balances; Sallie Mae holds about 63% of private originations | MEASUREMENT | MeasureOne via PR Newswire [S]; Sallie Mae share [S]; both from the design pass, not the research note | **CONFORMS [Likely]**: secondary sources; the nine-lender tail is a CHOICE |
| SSA, disability and the IRS stay one account each | SSA and IRS are single originators in ACH data: Treasury uses fixed descriptors ("SOC SEC", "TAX REF") and the company name "IRS TREAS 310" | MEASUREMENT | Bureau of the Fiscal Service Green Book; Treasury tax-refund direct-deposit FAQ [P] | **CONFORMS** |
| Pool sizes: mortgage 300, auto loan 200, student 15, auto 100, home 150, life 125 | Real markets run to thousands of servicers and carriers: most banks and credit unions service their own loans (the CFPB small-servicer exemption) | CHOICE | Butler Snow summary of the small-servicer rule [S] | **DEVIATES-BY-CHOICE**: the pool is large enough that no tail provider approaches a hub (the smallest pool's last share is 0.2%), and each market is one 99,999-serial key block, so a pool can grow without a layout change |
| The tail past the named ranks is 1/rank, scaled to the residual mass | The research reports each market's HHI independently of the rank tables | CHOICE | HHI figures as stated in the research pass (FSOC and NAIC rows) | **CONFORMS AS A DECLARED SHAPE**: the built tables reproduce the research HHI (auto 1,012 vs about 1,000, home 631 vs 620, life 302 vs 300, mortgage 351 vs 350); `test_product_providers` A0 pins it within 10% |
| One uniform per contract, drawn at issuance on `{"product-provider", market, person}`; the per-person portfolio stream, the shared entity stream and `makeCatalog` are untouched | A person holds at most one contract per market, so the lane is unique per contract | INVARIANT | none needed | **ENFORCED** by three checks in `test_product_providers`. A1 compares production against the draw-free singleton tables at pop 200,000 (0 field diffs over 2,947,290 events, 232,651 loans, 181,290 policy holders). A1 alone proves only that no draw depends on the table, because both of its legs run the same binary and a draw both make passes it. A7 therefore pins a digest of every key-free product value (events without the counterparty key, loan terms, policies without the carrier) at that pop and seed to the pre-round build: `da72a2306ca4622e` on HEAD 843f447 and on this build, paired with a domain predicate (0 events or policies outside their domain). B5 pins the next u64 of the gate world's shared stream after the build at the run-golden config: `498e4bde6c6f83ea` on both builds, which also covers `makeCatalog`'s draw count. DISARMS: a mortgage emitter that spends one portfolio draw on half the providers moves 3.67M event fields in A1; one extra portfolio draw on every issued mortgage passes A1 with 0 diffs and reds A7; one extra shared draw in `buildLandlords` reds B5 |
| Only providers some contract uses are registered, external and ownerless, in (market, ordinal) order after every entity-stage record | A small population must not export providers nothing pays | INVARIANT | none needed | **ENFORCED**: A5 (registered set equals the set re-picked from the loan and policy ledgers; no existing index moves); leg B (682 providers from record 9,246 at pop 2,000) |
| The camouflage P2P pool excludes providers | A ring's cover transfer to a mortgage servicer or insurer is a shape no legitimate customer produces, so it would be a label shortcut | TYPOLOGY | none needed | **ENFORCED**: leg B3 counts 0 camouflage rows on a provider. DISARM: without the filter, 15 of 440 camouflage rows land on one |
| Bug A predicate | Mortgages and student loans never share a servicer | INVARIANT | none needed | **CLOSED**: A2 counts 0 mortgage events on a student-market key and 0 student events on a mortgage key, and every loan event, policy, product row, premium and claim is on its own market (A2, B1) |
| External-unknown catch-all = `makeKey(merchant, external, 1'000'000'001)` (`XM1000000001`) | The catch-all is a bucket, not a merchant, and must never share a catalogue key | INVARIANT | merchant-churn-2026-07 rule 6 | **CLOSED, then SUPERSEDED** by the unknown-counterparty-2026-09 amendment, which retires the catch-all: the key stays reserved and unregistered, and no row may name it. At this amendment `test_merchant_churn` counted 0 external-unknown rows on a catalogue key (the old key scored 156,259 and 86,150 on its two legs), `test_counterparties` built a 500,000-person catalogue with 20 years of churn births (max serial 51,688) and found no collision, and `test_estates` and leg B2 required the funeral and every legit external-unknown row to pay the reserved key; those checks now require the opposite |
| Institutional offsets 1, 2, 3, 5, 6 and 7 are retired and never reused | An export written before this round still carries the old singleton keys | INVARIANT | none needed | **ENFORCED**: `test_counterparties`; leg B finds no row touching a retired key |

## Registered limitations

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| Shares are value-weighted (servicing UPB, written premium, outstanding balances) but assigned per contract | Count shares differ from value shares; banks' portfolio mortgages make the real tail by loan count fatter than agency UPB suggests | CHOICE | FSOC 2024 Table 1 is UPB-weighted [P] | **REGISTERED**: needs mortgage shares over all 1-4 family loans by count |
| The markets are era-flat: the 2023-2025 structure applies to every window year | Market structure moves: Rocket closed its acquisition of Mr. Cooper in October 2025 (about 1 in 6 mortgages), and Navient left federal servicing in 2021 | CHOICE | Rocket Companies press release on closing the Mr. Cooper acquisition; Maximus notice on the completed novation of Navient's federal servicing contract [S] | **REGISTERED** (the same era-flat declaration as the attacker-technology mix) |
| No servicing transfers and no carrier switching during a loan or policy | Servicing rights are sold and policyholders switch carriers | CHOICE | none pulled | **REGISTERED** |
| Subservicing is not modelled | The customer-facing servicer can be a subservicer of the owner of the servicing rights (Mr. Cooper's 6.7M includes subservicing) | CHOICE | Mr. Cooper 2024 [P] | **REGISTERED** |
| Regional carriers are drawn nationally, whatever the customer's home area | Carrier shares vary by state | CHOICE | NAIC publishes state tables [P, not read] | **REGISTERED** |
| No on-us lending or servicing | A real on-us loan payment goes to the customer's own loan account at the bank, which PhantomLedger does not model; one shared internal "bank lender" account would recreate a hub and break `cash-hub-defect-2026-08` rule 2; the on-us share is uncited (the only figure found: 48% of consumers look beyond their primary bank for a mortgage) | CHOICE | PYMNTS 2024 [S] | **REGISTERED** |
| No home and auto bundling: each market is its own key block, so State Farm auto, home and life are three accounts | 47% of home and auto holders report a bundle; independent draws with a shared insurer identity would give only about 6% same-carrier, and separate key blocks give 0% | CHOICE | NerdWallet [S] | **REGISTERED**: candidate follow-up is a shared insurer identity keyed by NAIC group code, with a bundling draw |
| Defaulted federal loans (the Default Resolution Group) are not modelled | Defaulted federal loans are served by the Default Resolution Group, not by the five servicers | CHOICE | none pulled (the FSA servicer table would size it) | **REGISTERED**: their borrowers are absorbed by the EdFinancial and CRI split |
| The federal servicer split for EdFinancial and CRI is uncited | FSA publishes recipients per servicer each quarter | UNCITED | FSA Data Center "Portfolio by Loan Servicer" (not retrieved) | **REGISTERED** (see the student row above) |
| Auto lender ranks 6-25 are the declared tail; no count-versus-balance adjustment | Ford Credit and American Honda rank 6th and 7th; values not retrieved | CHOICE | Big Wheels 2025 [S] | **REGISTERED** |
| Pre-existing: products are seeded by the constant `kDefaultProductsSeed` (0xB0A7F00D), not `--seed`, so a person gets the same provider in every run seed | The provider lane inherits the portfolio stream's seed | CHOICE | none needed | **REGISTERED, out of scope** (true of every product draw since before this round) |
| Pre-existing: the camouflage pool still contains SSA, the IRS, billers and cash endpoints | Camouflage P2P to a government payer or a biller is as unrealistic as to a lender | CHOICE | none needed | **REGISTERED, then CLOSED** by AMENDMENT bank-gl-2026-09 at review: the pool is now the customer deposit accounts alone (that amendment, "The camouflage pool, restricted at review") |
| `generateWindow` replays every `emitPerson`, so the lane's seed derivation runs once per contract per replay | Cost, not realism | INVARIANT | measured | **REGISTERED**: about 0.27 s per 200,000-person replay (2.37 s draw-free against 2.64 s with the lane, +11%). If profiling objects, switch the lane to a draw-free splitmix hash; the isolation is identical |

## Measured (pop 200,000, one year, leg A of `test_product_providers`)

| Market | Contracts | Providers used | Largest share (declared) | Top 10 (declared) | HHI |
|---|---:|---:|---|---|---:|
| Mortgage | 92,857 | 300 | 0.0719 (0.0730) | 0.5398 (0.5420) | 348 |
| Auto loan | 75,787 | 200 | 0.0541 (0.0540) | 0.3816 (0.3831) | 200 |
| Student loan | 63,718 | 15 | 0.2833 (0.2864) | 0.9862 (0.9874) | 1,856 |
| Auto insurance | 166,083 | 100 | 0.1870 (0.1864) | 0.7701 (0.7688) | 1,018 |
| Home insurance | 93,767 | 150 | 0.1866 (0.1869) | 0.6222 (0.6248) | 626 |
| Life insurance | 93,131 | 125 | 0.0872 (0.0873) | 0.4649 (0.4615) | 305 |

Before this round every one of those markets scored 1.0 on the largest-share
column (the singleton disarm in A3 still does, and must fail the check). The
largest provider hubs left are the top auto insurers, about 31,000 policies
each at this population, so roughly 370,000 premiums a year: still above the
MulePatternLearner hub threshold of 2,048 visible payments, so its hub
registry and history-withheld stubs remain necessary.

**Corpus movement.** `tests/golden_run.b2sum` moves (a30c535d... to
42c5c164..., 231,731 rows at both ends: the tie-order and camouflage cascades
moved bytes, not the row count). A re-pinned digest cannot tell that movement
from a stream shift, so `test_product_providers` A7 and B5 (the invariant row
above) carry that proof against the pre-round build. The three PostgreSQL
table goldens need the owner's re-pin. `kTableCount` stays 43. The
mule-temporal 2024 corpus, its TigerGraph snapshot and the MulePatternLearner
hub registry must be regenerated under a new dataset id.

═══════════════════════════════════════════════════════════════════════
# AMENDMENT: unknown-counterparty-2026-09
═══════════════════════════════════════════════════════════════════════

**What changed.** The external-unknown catch-all is retired. Before this
amendment three flows paid one account, `XM1000000001` (and before the
institutional-providers amendment, catalogue merchant 1): the spending
router's external-unknown slot (5% of spending events), P2P payments whose
contact was missing or unusable, and every funeral. On the 200,000-person
mule-temporal corpus that one account took 3,814,933 payments from 62% of
deposit accounts (`docs/research/counterparty_hubs_2026-09.md`). Owner
decision: realistic by channel. Each flow now pays the kind of counterparty a
bank actually records for it, and no row may land on one global account.

## The design (written before the code)

Read from `PaymentRouter::emitExternal` and `emitP2p`
(`activity/spending/routing/payments.cpp`), `prepareRouting`
(`simulator/run_planner.cpp`), `funeralPayee`
(`transfers/legit/routines/family/inheritance.cpp`),
`registerSystemAccounts` (`pipeline/stages/entities.cpp`) and the
mule-temporal exporter's rail mapping (`exporter/mule_temporal/streaming.cpp`).

1. **Only the destination changes.** Every retired row keeps its channel
   (`external_unknown` for the two spending flows, `bill` for the funeral), its
   amount draw, its timestamp and its source. No new draw is taken from any RNG
   stream: every new choice is a draw-free splitmix hash of the person id (and,
   per row, the row's timestamp), the construction `market/commerce/affinity.hpp`
   and `actors/instruments.hpp` already use. Every new destination is an
   EXTERNAL registered account, so `Ledger::decide` sees the same
   `dstIdx == invalid` it saw for the catch-all and no balance, decline or
   retry can move. The channel is deliberately unchanged: the impostor-scam
   rail pushes on `external_unknown` too, and a legit-only relabel would make
   the channel a fraud marker.
2. **The external-unknown slot splits into checks and identified remote
   payees.** A row is a paid check with probability
   `min(1, checkShare(year) / slotShare)`, where `slotShare` is the slot's
   probability mass in the configured channel CDF (0.05 by default) and
   `checkShare` is the DCPC check share of consumer payments by number
   (7% in 2016, 3% in 2024, linear between, flat outside). At the default slot
   that is 0.6 of the slot from 2024 on and the whole slot through 2020.
   The rest pays an identified remote merchant (step 4).
3. **A paid check is keyed by the payee's bank.** The only structured payee
   key on a paid check is the bank-of-first-deposit routing number, so each
   external bank is one account (`Role::business`, external, serials
   1,001,000,001 to 1,001,001,000). Each person has 4 check payees; each payee
   banks at a bank drawn once from the FDIC Summary of Deposits share table
   (hash of person and payee slot); each check picks one of the person's payees
   (hash of person and row timestamp). The pool is the 1,000 largest
   institutions by 2024 deposits: the top 25 at their exact shares, then a
   1/rank shape between the SOD cumulative anchors at ranks 50, 100, 250, 500
   and 1,000, renormalized by the top-1,000 mass.
4. **Identified remote payees are catalogue merchants.** The candidates are
   the EXTERNAL catalogue records whose footprint is `online` or
   `nationalService` (the ecommerce, telecom, utility and insurance outlets a
   customer pays remotely by ACH or bill pay), weighted by their volume
   weight. A row takes a hash-derived uniform per attempt and accepts the first
   candidate live at the row's timestamp, up to 8 attempts; a row that finds
   none (a catalogue with no remote external merchant) takes the check route.
   Internal (on-us) merchants are excluded because crediting them would move a
   customer balance, which step 1 forbids.
5. **P2P with no usable contact goes to a named P2P platform.** Zelle cannot
   carry such a payment (it needs an enrolled email or US mobile number, and an
   unclaimed payment expires after 14 days), so the platform set is Venmo and
   Cash App, which the bank sees as a named ACH counterparty
   (`Role::platform`, external, serials 1,000,000,001 and 1,000,000,002). Each
   person uses one platform, chosen once by a hash of the person id and
   weighted by the platforms' monthly active accounts. Measured after the
   code landed: this fallback does not fire at any configuration the gates
   or the owner run, because every person holds at least three
   in-population contacts (`Social::effectiveDegree` clamps the degree to
   3..24). The research's "P2P with no usable contact" was a code-path
   finding; the 3.8M catch-all rows were the slot and the funerals.
6. **A funeral pays a funeral home in the decedent's city.** Funeral homes
   (MCC 7261) are `Role::merchant`, external, serial
   `1,100,000,000 + area * 10,000 + ordinal`. Each geo area holds
   `max(1, round(area population * 15,375 / 334,017,321))` of them (Census CBP
   2022 establishments per resident), and a decedent's funeral pays the one
   their person id hashes to in their home area at the death date (the
   relocation schedule, as the cash-point rails read it).
7. **Registration.** The retired key is no longer registered, so
   `validateTransactionAccounts` throws on any row that still names it (its
   removal from the system block shifts every later record up by one). The
   new accounts are appended after every other entity-stage record: the
   banks some person's payee slots use (rank order), both platforms, and the
   funeral homes of every decedent who dies inside the window (key order). All
   three pools are excluded from the camouflage P2P pool, like the providers:
   a ring's cover transfer to a funeral home or a check-payee bank is a shape no
   customer produces.
8. **Exporters are unchanged except one label.** The mule-temporal exporter
   maps rails from the channel, so the retired rows keep the `unknown` rail
   (see the rail limitation below). The standard exporter labels a funeral home
   `merchant_external` / `funeral_services` instead of the unknown external
   account it would otherwise print for a `Role::merchant` key with no
   catalogue record.

## The degree gate (added at review, written before its code)

The first anti-hub gate bounded a destination's share of the retired ROWS.
Rows are not what made the catch-all harmful downstream: its DEGREE was
(196,989 distinct payers, 62% of deposit accounts), and degree is what merges
MulePatternLearner's weakly connected components and sinks PageRank. A
per-bank check hub is small in rows and large in degree, so the row bound
passed a population-scale hub without printing it, and the row bound's
authority row cited the research's degree-sanity target as support when the
build breaks that target by about ten times.

1. **Degree is measured per destination** as the share of the retired rows'
   distinct source accounts that pay it. At the gate every paying customer
   pays these flows from one account, so it is also the share of paying
   customers.
2. **The check-payee banks and the funeral homes are bounded.** A person
   reaches a bank's check hub only through their own 4 payees, so on any
   window the share of payers who pay the largest bank's hub cannot exceed
   the share whose payee slots include it: `1 - (1 - 0.1221)^4 = 0.406`,
   where 0.1221 is the largest bank's pool-normalized SOD share. The ceiling
   is that value plus 4 binomial sigma over the payers. Unlike the row share,
   it does not depend on the era. Funeral homes sit far below it (C5 bounds
   their rows).
3. **Named counterparties are printed, not bounded.** The research's
   degree-sanity target exempts named platforms and big billers ("can
   legitimately be hubs, and they are named"), and a remote merchant's degree
   moves with the window (see its limitation row), so an absolute band would
   fail a correct long window or pass anything on a short one.
4. **The disarms are scored on the same rows.** One catch-all per flow puts
   every check writer on one account. Drawing a fresh payee bank for every
   check (the research's per-check payee under the corrected per-bank key)
   lets the largest bank's degree grow with the number of checks a person
   writes. The ceiling must reject both.
5. **The per-bank hub itself is not a defect.** It is the owner decision and
   the reading the research's verification pass reached ("every check
   deposited at the same large bank collapses onto one key"). It is
   registered as a deviation from the degree-sanity target instead of being
   cited as conforming to it.

## The authority rows

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| Check share of consumer payments: 0.07 in 2016, 0.03 in 2024, linear between, flat outside | By count, 2024 consumer payments were 14% cash, 3% check, 35% credit, 30% debit, 13% ACH, 5% mobile app and other; the check share was 7% in 2016 | MEASUREMENT (the two anchors); CHOICE (the linear shape between them) | Federal Reserve Financial Services and Atlanta Fed, 2025 Findings from the Diary of Consumer Payment Choice, Figure 2 [P], confirmed in the research pass | **CONFORMS** on both anchors. Converted to a share of the slot by the configured slot mass, capped at 1 |
| A paid check is keyed by the bank of first deposit, one account per external bank | The X9.37 Check Detail Addendum A carries a mandatory 9-digit BOFD routing number; no payee-name field is specified, and the payee is identified only by OCR of the image | MEASUREMENT | FRB adoption of DSTU X9.37, standards reference [P]; Stellar Bank, Payee Positive Pay Best Practices [P] ("OCR is not an exact science"); Plaid returns a null merchant for checks [P] | **CONFORMS**: the verification pass corrected the research's per-payee key (the Fed specification lists no BOFD account number), so the key is per bank, as specified here. The addendum is conditional (required when the BOFD is the truncating bank) |
| Check payee banks: ranks 1-25 = .11541 .10926 .08062 .04271 .03029 .02429 .02283 .02140 .02050 .01667 .01287 .01188 .01161 .01083 .01035 .01031 .00989 .00988 .00940 .00915 .00894 .00870 .00858 .00854 .00741; cumulative anchors top 50 = .7251, top 100 = .7931, top 250 = .8651, top 500 = .9079, top 1,000 = .9451; pool of 1,000 renormalized by .9451 | JPMorgan Chase holds 11.54% of US deposits, Bank of America 10.93%, Wells Fargo 8.06%; 4,548 institutions hold $17.405T across 76,727 offices; HHI 388 | MEASUREMENT | FDIC Summary of Deposits, June 30, 2024, summed by institution (`banks.data.fdic.gov/api/sod`, `YEAR:2024`, `agg_by=CERT`, `agg_sum_fields=DEPSUMBR`, read 25 September 2026) [P] | **CONFORMS** on ranks 1-25 and all five anchors; the ranks between anchors are the 1/rank shape |
| Check payee bank pool truncated at 1,000 institutions | The 3,548 smallest institutions hold 5.49% of deposits | CHOICE | FDIC SOD 2024 [P] | **DEVIATES-BY-CHOICE**: the truncated mass is spread over the pool by renormalization; the smallest pooled bank is still low-degree |
| 4 check payees per person | People write checks repeatedly to a few payees (landlord, contractor, relatives, a church) | CHOICE | none found | **REGISTERED**: bounds a person's distinct check-payee banks at 4 |
| Identified remote payees are external `online` and `nationalService` catalogue merchants, weighted by volume weight | Deposit-funded remote payments (ACH debit, online bill pay) carry a company name or ID; card spending never lacks a descriptor | MEASUREMENT (that the payee is named); CHOICE (which catalogue records stand in for it) | Nacha Operations Bulletin 2-2024 [P] (the name field is required); Visa Merchant Data Standards Manual, April 2026 [P]; the verification pass's correction that the slot is deposit-funded remote spending, not card spending | **CONFORMS** on the claim; the candidate set is declared |
| P2P platforms: Venmo 64/121 = .529, Cash App 57/121 = .471; one platform per person | Venmo had more than 64 million monthly active accounts in Q4 2024; Cash App had 57 million monthly transacting actives in December 2024 | MEASUREMENT | PayPal Holdings Q4 2024 earnings call transcript [P]; Block, Inc. Form 10-K FY2024, "Our Cash App Customers" [P] | **CONFORMS [Likely]**: the two metrics are both monthly but defined differently (active accounts versus transacting actives) |
| Zelle is not a destination for a P2P payment with no usable contact | Zelle needs the recipient's email or US mobile number, and a payment to an unenrolled recipient expires after 14 days | MEASUREMENT | Zelle FAQ, "What if the person I'm sending money to hasn't enrolled with Zelle?" [P] | **CONFORMS** |
| A platform appears at the bank as a named ACH counterparty | Venmo standard transfers post with the note "VENMO-0 CASHOUT" | MEASUREMENT | Venmo Help Center, bank transfer timeline [P] | **CONFORMS**: the page covers cash-outs; the outgoing debit is inferred |
| Funeral homes: MCC 7261, `max(1, round(area population * 15,375 / 334,017,321))` per geo area, 0.460 per 10,000 residents | 15,375 funeral-home establishments (NAICS 812210) in 2022; funeral homes carry their own merchant category, 7261 Funeral Services and Crematories | MEASUREMENT | Census County Business Patterns 2022, US file `cbp22us.txt`, NAICS 812210 [P]; the 334,017,321 denominator is the 2022 population merchant-selection-2026-08 uses with the same CBP vintage; Visa Merchant Data Standards Manual [P] | **CONFORMS**: 2,431 homes across the 71 US cities of the geo catalogue (New York 390, the smallest cities 2 or 3) |
| The funeral pays a home in the decedent's area at the death date | Funerals are arranged locally | CHOICE | none needed | **ENFORCED** by the gate |
| Draw-free selection, every destination external, channel unchanged | Only the destination of a retired row may change | INVARIANT | none needed | **ENFORCED** by `test_remote_payees` (see the measured section) |
| The retired key `XM1000000001` is never registered and receives no row | Nothing lands on one global account | INVARIANT | none needed | **ENFORCED** by `test_remote_payees`, `test_product_providers` B2, `test_merchant_churn` and `test_estates` |
| No single external account receives more than 15% of the retired rows | A single global vertex receiving millions of payments has no real analogue. The largest destination a correct build produces is the largest payee bank: its SOD share, 0.1221 of the slot, when every slot row is a check (through 2020), and 0.6 x 0.1221 = 0.073 at 2024 on | INVARIANT (the bound is a CHOICE sized by that arithmetic and by measurement) | FDIC SOD 2024 [P] (the arithmetic); the owner decision (nothing lands on one global account). The research's degree-sanity target is NOT authority here: it bounds degree, not rows, and the check hubs break it (the two rows below) | **ENFORCED** by `test_remote_payees` C6 (measured 0.0747). The design pass wrote 25% before the P2P fallback was measured at zero rows; 15% is the tightened value. A row bound cannot see a hub that is small in rows and large in degree, so it is paired with the degree bound below |
| No check-payee bank or funeral home is paid by more than `1 - (1 - 0.1221)^4 = 0.406` of the retired rows' payers, plus 4 binomial sigma over the payers | A person's checks reach a bank only through their own 4 payees, so on any window a check hub's payers are at most the people whose payee slots include that bank | INVARIANT (derived from the SOD share and the registered 4-payee CHOICE) | FDIC SOD 2024 [P] (the largest bank's pool-normalized share, 0.1221) | **ENFORCED** by `test_remote_payees` C6: measured 0.3749 (JPMorgan Chase's hub, 3,685 of 9,830 payers) against a 0.4259 ceiling. The same rows score 0.9888 under one catch-all per flow and 0.7455 under a fresh payee bank per check, and paying every check to the largest bank (a code disarm) scores 0.9888; all three red the check. Named counterparties are printed, not bounded (the limitation rows) |
| The per-bank check hubs are population-scale: the largest bank's hub is paid by about 40% of the customers who make a retired-flow payment (0.3749 over one year in leg C, 0.4067 over five years in a scratch probe, against its 0.406 payee-slot reach), the second by 0.3578 and the third by 0.2702 over one year | "No single non-platform counterparty should connect more than a few percent of deposit accounts"; the verification pass found that on a paid check "every check deposited at the same large bank collapses onto one key" | CHOICE (owner decision: one hub per external bank, like the bank-of-first-deposit routing-number key) | the unknown-counterparty research note's degree-sanity target (in its recommended parameters) and its verification pass's counter-evidence on checks; DSTU X9.37 [P] | **DEVIATES-BY-CHOICE** (owner decision) from the degree-sanity target, by about ten times. Not a defect: the hubs are what the BOFD key produces, they are bounded by the row above, and they are named for the MulePatternLearner hub registry (see the corpus movement) |

## Registered limitations

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| Deposit share is the payee-bank weight | SOD deposits include custody and wholesale balances (BNY Mellon, State Street, Goldman Sachs, Morgan Stanley, Schwab), which receive few consumer check deposits; a branch-count or retail-account weighting would move them down | CHOICE | FDIC SOD 2024 [P] | **REGISTERED** |
| No on-us checks: every check payee banks elsewhere | A payee who banks at the simulated bank would be an on-us item | CHOICE | none needed | **REGISTERED**: the simulated bank's own share is negligible at simulated population sizes |
| The DCPC denominator is all consumer payments; the router's is spending-slot events | Rent, subscriptions, loans and insurance run on their own rails and are not in the slot's denominator | CHOICE | DCPC 2025 [P] | **REGISTERED**: the check share of the slot is an approximation of the check share of payments |
| The slot's size stays era-flat at 5% | Checks were far more than 5% of payments before about 2016, so the slot cannot carry their full historical share | CHOICE | DCPC 2025 [P] | **REGISTERED**: the fraction saturates at 1, so every slot row is a check through 2020 |
| The check payee's bank is era-flat | Bank shares move (mergers, failures) | CHOICE | none pulled | **REGISTERED** |
| Two P2P platforms only | PayPal P2P and Apple Cash also carry P2P; PayPal's 10-K reports no US P2P user count comparable to the two above | CHOICE | PayPal Holdings Form 10-K FY2024 [P] | **REGISTERED** |
| One platform per person, forever | Many people use more than one app | CHOICE | none pulled | **REGISTERED** |
| The platform shares are era-flat | Venmo launched in 2009 and Cash App in 2013, so a pre-2013 window still routes these rows to the 2024 platforms | CHOICE | the platforms' own histories | **REGISTERED**: a pre-2009 row has no real P2P app; the event count is unchanged by design (only the destination moves) |
| US funeral-home density applied to every area, the 15 international cities included | Density differs by country | CHOICE | none pulled | **REGISTERED** |
| Area populations are approximate municipal populations | The geo catalogue's population column is order-of-magnitude correct | CHOICE | `synth/geo/geo_data.hpp` [Likely] | **REGISTERED**: moves the home count per city, not the placement rule |
| A funeral home is registered for every in-window death, including a death whose funeral posts after the window closes | The funeral date is drawn inside the family routine and is not known at registration | CHOICE | none needed | **REGISTERED**: at most the deaths in a window's last 11 days add an unused external account |
| The selection hashes are unseeded | The same person id keeps the same check payee banks and platform across run seeds, like the favourite-affinity and instrument hashes | CHOICE | none needed | **REGISTERED** (the precedent in `market/commerce/affinity.hpp`) |
| The mule-temporal rail stays `unknown` for every retired row, and the AML exporter still maps `external_unknown` to the purpose `wire_transfer` | A paid check is a check, and a platform debit is ACH; the research calls the wire label wrong on volume and amount | CHOICE | Fedwire 2024 annual statistics [P] (209,916,835 transfers, $5.40M average) | **REGISTERED**: a destination-derived label is a follow-up. The impostor-scam rail shares the channel, so any relabel must cover both labels together or it becomes a fraud marker |
| The funeral keeps the `bill` channel | A funeral home is paid by card, check or ACH | CHOICE | none needed | **REGISTERED** (the same channel as before; a dedicated funeral channel stays a registered upgrade) |
| Both P2P platforms are always registered | The no-contact fallback does not fire at any measured configuration, so the two accounts carry no row there | CHOICE | none needed | **REGISTERED**: two unused external accounts; the mule-temporal exporter emits a vertex only when a row observes it, so only the standard exporter's external-account table lists them |
| A world with no home carriers sends every funeral to the one area-0 home | Production always binds both home carriers; only a hand-built harness has neither | CHOICE | none needed | **REGISTERED**: registration resolves area 0 the same way, so the row is still booked |
| The no-contact P2P fallback is unreachable | Every person holds 3 to 24 in-population contacts, so the three fallback conditions (no contact row, an out-of-population contact, an invalid or self destination) never hold | MEASUREMENT | `relationships/social/builder.hpp` | **REGISTERED**: 0 fallback rows in both legs of `test_remote_payees`; the platform route is checked at the selection function instead (C3) |
| An identified remote merchant's degree grows with the window | The remote route picks a merchant per row by volume weight, so a person with n remote rows pays merchant i with probability about `1 - (1 - w_i)^n`: the volume-as-membership amplification merchant-selection-2026-08 removed from the card favourites. Leg C: the three largest (a utility, an online retailer, an insurer) are paid by 0.5737, 0.5322 and 0.3967 of the payers in one year. A scratch probe at pop 2,000 over five years from 2020: 0.8169 for the largest, and an online outlet opened in April 2023, which 9 people pay on every other row, is paid by 1,261 of the 1,999 payers (0.6308) | CHOICE | the research's degree-sanity target exempts named counterparties ("can legitimately be hubs, and they are named") | **REGISTERED**: printed by C6, not bounded, because the level moves with the window. The utility and the insurer add little degree (in a scratch probe of leg C, 86% and 74% of the leg's people already pay them on other rows); the online retailer rises from 14% to 59% of the leg's people. A per-person remote payee set, like the 4 check payees, would bound it; that is the registered upgrade, and it moves the golden |

## Where the design deviates from the research note

- **Checks are keyed per bank, not per payee.** The research recommended a
  distinct counterparty per check keyed by a synthetic BOFD routing number and
  account; its own verification pass found that the Fed specification lists
  no BOFD account number, so a paid check's structured key identifies the
  payee's bank only. The owner decision (one hub per external bank) follows
  the corrected reading.
- **The rest of the slot pays identified catalogue merchants, not the
  external tail pool alone.** The counterparty-hubs note proposed the tail
  pool; the candidates here are every external online or national-service
  outlet (tail and external core), because on-us merchants would move a
  customer balance and local outlets are not paid remotely.
- **No-contact P2P goes to named platforms, not to the external family pool
  or nowhere.** Owner decision. It is measured unreachable (see the design
  and the limitation row), so the choice moves no row today.
- **The exporter labels are unchanged** (rail `unknown`, AML purpose
  `wire_transfer`); see the rail limitation. Relabelling only the legit rows
  would make the label a fraud marker, because the impostor-scam rail shares
  the channel.
- **The check hubs break the research's degree-sanity target.** It asks that
  no single non-platform counterparty connect more than a few percent of
  deposit accounts; the largest bank's check hub is paid by about 40% of the
  customers who make a retired-flow payment, about ten times that. This is
  the owner decision (one hub per external bank) and the key the
  verification pass says a paid check actually carries, so it is registered
  as a deviation, bounded at its payee-slot reach, and named for the
  downstream hub registry rather than cited as conforming.

## Measured (`test_remote_payees`)

**Only the destination moved.** On the run-golden gate world (pop 2,000,
60 days from 2025-01-01, seed 3405691582) the shared entity stream's next u64
after the build is `498e4bde6c6f83ea` on the pre-round build and on this one,
and the order-free digest of every legit row with the target masked on the
retired flows is `255b9e814eea12cc` on both, with 191,721 rows, 190,275 legit
rows and 7,902 retired rows on both. Paired domain predicate: 0 retired rows
outside a finite positive amount, an in-window timestamp, an internal source
and an external destination in one of the four families.

**The one other movement is the camouflage pool, and a bisect attributes it.**
The retired key was IN the camouflage P2P pool; removing it shrinks the pool
by one, which re-points ring cover transfers and cascades through balances.
At the golden configuration that reaches no legit row (the digest above is
unchanged; the all-row digest moves). At pop 10,000 over 365 days it moves
rows 5,254,299 to 5,254,057, legit rows 5,223,524 to 5,223,265 and retired rows
218,650 to 218,644. Putting the retired key back into the camouflage pool at
its old position, as a diagnostic only, reproduces the pre-round all-row and
legit digests exactly at both scales (`091eb9fa16f044d1` / `255b9e814eea12cc`
and `b43bbafc0e6453cd` / `f228e44897b54871`), so nothing else moved. Keeping
the key in the pool was not an option: after the retirement a cover transfer
to it would be a fraud-only shape.

| Leg C (pop 10,000, 365 days from 2025) | Value | Expected |
|---|---:|---:|
| Retired rows | 218,644 | |
| Slot rows paid as checks | 0.5999 (131,121) | 0.6000 (DCPC 3% over the 5% slot) |
| Slot rows paid to remote merchants | 87,440 | |
| No-contact P2P rows | 0 | (unreachable, see above) |
| Person payee slots at the largest bank | 0.1235 | 0.1221 (SOD, pool-normalized) |
| Person payee slots at the top 10 banks | 0.5119 | 0.5121 |
| People on Venmo | 0.5311 | 0.5289 |
| Funerals / homes they pay / busiest home | 83 / 83 / 1 | every funeral in the decedent's city at death: 0 wrong |
| Distinct destinations of the retired rows | 1,295 | the pre-round build: 1 |
| Largest destination | 0.0747 (JPMorgan Chase's check hub) | ceiling 0.15 |
| Next four | 0.0699, 0.0502 (banks 2 and 3), 0.0421, 0.0368 (remote merchants) | |
| Payers (distinct source accounts of the retired rows) | 9,830 | one account per paying person |
| Degree of the largest check hub (JPMorgan Chase) | 0.3749 (3,685 payers) | payee-slot reach 0.4060, ceiling 0.4259 |
| Degree of check hubs 2 and 3 | 0.3578, 0.2702 | |
| Degree of the largest named counterparties | 0.5737, 0.5322, 0.3967 (remote merchants: a utility, an online retailer, an insurer) | printed, not bounded |
| Largest check writer's distinct banks | at most 4 | 4 payees |

Leg B registers 653 remote payees from record 9,081 (647 banks, 2 platforms,
4 funeral homes for 3 posted funerals), external and ownerless in one block;
0 of 441 camouflage rows land on one.

**DISARMS, each run against this build:** drawing the check-or-merchant
uniform from the spending stream instead of the hash reds B2 (rows 191,721 to
142,713); dropping the camouflage filter reds B4 (16 of 440 camouflage rows on
a remote payee) and B2; paying every check to the largest bank reds C6 on
both ceilings (0.5997 of the rows and 0.9888 of the payers on one account);
ignoring the decedent's city reds C5 (3 funerals in the wrong city, and the
unregistered homes drop 71 of 83 funerals) and 47 checks in `test_estates`.
The in-gate disarms for C6, scored on the same rows: one catch-all per flow
scores 0.5997 against the 0.15 row ceiling and 0.9888 against the 0.4259
degree ceiling, and a fresh payee bank per check scores 0.7455 on degree.

**Corpus movement.** `tests/golden_run.b2sum` moves again (the staged
institutional-providers digest `42c5c164...` to `dd0363aa...`), with the row
count unchanged at 231,731; the digest is re-pinned once at the end of the
round. `test_remote_payees` B1 and B2 carry the pre-round proof a re-pinned
digest cannot. The three PostgreSQL table goldens need the owner's re-pin:
the external-account table loses the catch-all and gains the check-payee
banks, both platforms and the funeral homes (the standard exporter labels a
funeral home `merchant_external` / `funeral_services`). `kTableCount` stays 43.
The mule-temporal 2024 corpus, its TigerGraph snapshot and the
MulePatternLearner hub registry must be regenerated under a new dataset id.
`XM1000000001` disappears from the hub registry, and its replacements must be
read by DEGREE, not rows. None takes more than 7.5% of the former
catch-all's rows, but the three largest check-payee hubs (JPMorgan Chase,
Bank of America, Wells Fargo) are each paid by 27% to 37% of the customers
who make a retired-flow payment in a year, and the largest reaches its 41%
payee-slot reach on longer windows. The largest identified remote merchants
reach more (57% in a year, over 80% across five years); most are billers
that customers already pay on other rows, but one online retailer, which 14%
of customers pay on other rows, is paid by 59% once the retired rows are
added. These are customer shares; the corpus holds about
1.6 deposit accounts per customer, so the deposit-account shares are lower.
None is a placeholder to exclude from traversal: each is a named hub, and the
downstream hub handling (the registry and the history-withheld stubs) must
cover them, because at population scale they are the vertices that merge
weakly connected components and draw PageRank.

═══════════════════════════════════════════════════════════════════════
# AMENDMENT: bank-gl-2026-09
═══════════════════════════════════════════════════════════════════════

**What changed.** Every fee and interest posting is kept, and its contra is
retyped as a bank-owned income ledger account. Before this amendment the
four bank-originated posting kinds paid three accounts typed as external
business deposits (`Role::business` on `Bank::external`, registered
external): card interest and card late fees paid the card issuer key
`cash::cardIssuer()` (`XO3000000001`), overdraft fees paid
`bankFeeCollectionKey()` (`XO4294967041`) and overdraft line-of-credit
interest paid `bankOdLocKey()` (`XO4294967042`). On the 200,000-person
mule-temporal corpus they took 2,316,826, 361,991 and 113,386 payments, the
issuer from 73% of card accounts (`docs/research/counterparty_hubs_2026-09.md`).
Owner decision: retype as bank GL. The posting is real core-banking data (a
double-entry charge whose credit leg is an income GL), and its contra is
neither a customer nor an external party.

## The design (written before the code)

Read from `Session::accrueInterest` and `Session::postLateFee`
(`transfers/channels/credit_cards/session.cpp`), `bankFeeCollectionKey`,
`bankOdLocKey` and `ChronoReplayAccumulator::onLiquidityEvent`
(`transfers/legit/ledger/posting.cpp`), `Ledger::debitAndEmit` and
`Ledger::accrueLocInterestThrough` (`transactions/clearing/ledger.cpp`),
`cash::cardIssuer` (`entities/counterparties/cash_points.hpp`),
`registerSystemAccounts` (`pipeline/stages/entities.cpp`), the camouflage
pool in `makeAccountPools` (`transfers/fraud/injector.cpp`), and every
exporter typing path: `exporter::common::accountType` (AML and
aml-txn-edges), `writeAccountNumberRows` and `writeExternalAccountRows`
(standard), the account plan in `exporter/mule_temporal/streaming.cpp`, the
mule-ml party and IP-edge writers, and the card-fraud view filter.

1. **One new role, `Role::ledger`, internal only.** It is the role for an
   account the bank itself owns, rendered `GL` plus 8 digits. It is appended
   last to the role enum, so every existing key keeps its numeric role and
   its hash. Reusing `Role::account` or `Role::business` on `Bank::internal`
   was rejected: both are customer deposit roles, and every exporter would
   type the GL as a checking or business-checking account.
2. **Four income GLs, one per posting kind**
   (`entities/holdings/general_ledger.hpp`):

   | GL | Key | Posting kind (channel) |
   |---|---|---|
   | card interest income | `GL00000001` | `cc_interest` |
   | card fee income | `GL00000002` | `cc_late_fee` |
   | deposit fee income | `GL00000003` | `overdraft_fee` |
   | credit-line interest income | `GL00000004` | `loc_interest` |

   One function maps a posting channel to its GL. Both posting sites and the
   gates use it, so the mapping cannot drift between them.
3. **Only the destination and the copied session change.** No draw is added
   to or removed from any stream. The card session posts interest and late
   fees to their GLs instead of `env.issuerAccount`, and `onLiquidityEvent`
   posts the overdraft fee and the LOC interest to theirs. Amounts,
   timestamps, sources, channels and every clearing decision are unchanged:
   for these channels `Ledger::decide` reads only the source (liquidity
   channels skip the funding screen; card interest and late fees screen the
   card), so booking the credit leg cannot flip an accept or a decline.
   The one ordering effect is the replay sort's tie-break on the target key
   (the GL role sorts after every existing role), which reorders a posting
   against another row of the same source at the same second; the gate
   measures whether that reaches any row.
4. **What `bankOdLocKey` actually received: interest, never principal.**
   `Ledger::accrueLocInterestThrough` emits matured `loc_interest` through
   `debitAndEmit`, and `onLiquidityEvent` sent every event that was not an
   overdraft fee to that key; the liquidity family has only those two
   members. No principal was ever posted to it. An LOC draw is the deposit
   account's cash going negative inside its `overdrafts_` capacity, and a
   repayment is any inbound credit to the same account. So the interest
   goes to the credit-line interest income GL, and the per-customer LOC
   credit account (Regulation Z) is registered as a limitation below rather
   than built.
5. **Booked, ownerless, never seeded.** The GLs are `Bank::internal`, so
   every clearing book built from the registry gives each one a slot. The
   opening book seeds only owned records (`ownedAccountIndices`), so a GL
   opens at 0.00 with protection `none` and no seeding draw is spent. Only
   the credit leg posts; nothing ever debits a GL. In the posted book (the
   post-fraud replay the AML exporters read the balance from) a GL's balance
   is exactly the sum of the rows booked to it, which the gate checks. The
   pre-fraud authoritative replay debits the payer of an overdraft fee or LOC
   interest inside `debitAndEmit` and emits the row without a credit leg, so
   in that book the GL slots carry only the card postings. Nothing reads
   that book's GL slots.
6. **Registration.** The four GLs replace the three retired keys in the
   system block of `registerSystemAccounts`, registered internal. The card
   issuer key, `Directory::external.cardIssuer`, the blueprint's
   `issuerAcct` and the `issuerAccount` plumbing through the card lifecycle
   (whose only use was these two postings) are deleted rather than left as
   write-only fields. The block grows by one record, so every later record
   shifts up by one. That moves indices only, never a draw.
7. **Never a fraud or mule role.** Victims and mules are drawn over persons,
   and account flags follow the owner, so an ownerless GL can hold neither.
   The camouflage P2P pool is every registry record less the providers and
   remote payees; it now also excludes the GLs, because no customer can push
   a transfer into a bank income ledger. The three retired keys were in that
   pool, so it shrinks by three and ring cover transfers re-point (the same
   camouflage cascade the unknown-counterparty amendment measured and
   attributed). Superseded at review: the pool is now the customer deposit
   accounts alone (see "The camouflage pool, restricted at review" below).
8. **No customer device or IP on a system posting.** `onLiquidityEvent`
   copied the triggering row's device and IP onto the fee row. It no longer
   does, and the `currentTxn_` pointer it read is deleted. Card interest and
   late fees already carried none (`channels::isExternallyInitiated`).
9. **Exporters, typed truthfully.**
   - mule-temporal: the GL is an `Account` with `account_type` `gl`,
     `is_external` False and `is_mule` 0, no `Party_Owns_Account`, never a
     Zelle endpoint (it is not a deposit role). The rail stays
     `internal`/`bank`. The postings stay as ordinary payments with a
     `Transaction_To_Account` edge on the GL, the Oracle BD "Account plus
     Offset Account" shape, so a consumer that wants to drop or separate
     them filters on `account_type`.
   - AML and aml-txn-edges: the GL is an internal `Account` row with account
     type `general_ledger`, no customer edge and no counterparty vertex; its
     balance is the posted book's (the income posted in the window).
   - standard: `accountnumber.csv` carries the GL with `is_external` 0; it
     has no `HAS_ACCOUNT` row and is no longer in `external_accounts.csv`.
     The `GL` prefix names the type.
   - card-fraud: unchanged. The card view carries only the purchase channels,
     so no posting reaches it.
   - mule-ml: the GL is an ownerless party row with a blank identity. The
     account IP edges and the canonical IP histogram now skip the unassigned
     address (`0.0.0.0`): without that guard, removing the copied IP from the
     fee rows would add an edge from every overdrafting deposit account to one
     shared `0.0.0.0` vertex, a new artifact hub (card accounts already had
     one through their interest and fee rows).

The gate is `tests/test_bank_ledger.cpp`. On the run-golden gate world it
pins the shared entity stream (which also covers `makeCatalog`'s draw count)
and an order-free digest of every legit row with the target masked on the
four posting kinds and the device and IP masked on the two liquidity kinds,
both to the pre-round build; checks that every fee and interest row targets
the GL of its kind and that nothing else touches a GL; that the GLs are
internal, ownerless, unflagged, one registry block, absent from camouflage
and every fraud row; that no posting carries a device or IP; and that each
GL opens at zero and closes at the sum of its rows in a replayed posted book.

## The authority rows

[P] = primary document read, [S] = secondary summary, as reported by the
round's research and verification passes (the bank-postings research note,
summarized in `docs/research/counterparty_hubs_2026-09.md`, section "Fees and
interest have no counterparty").

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| Every fee and interest posting is kept: the customer account is debited and a bank-owned income GL credited | A charge or debit-interest liquidation is a double-entry posting. FLEXCUBE's CLIQ event debits CHG_BOOK and credits CHG_INCOME for every charge basis (ad hoc statement, cheque issue and return, stop payment, statements, item count, turnover); its ILIQ event debits ICDB-BOOK and credits ICDB-PNL | MEASUREMENT (accounting) | Oracle FLEXCUBE Universal Banking Interest and Charges User Guide 14.5.3.0.0 (Nov 2021) [P], confirmed by the verification pass; Temenos Transact CATEG journal entries hit the institution's P&L category [P] | **CONFORMS** |
| Four GLs, one per posting kind: card interest income, card fee income, deposit fee income, credit-line interest income | FLEXCUBE maps each accounting role to its own GL head per product and event, with the interest PNL role separate from CHG_INCOME, and its example calls the credit GL "a commission income GL for the branch". Banks also pool kinds (Fiserv sells an app that moves Premium Overdraft fees off the NSF fee GL) or split further by branch or currency | CHOICE | FLEXCUBE 14.5.3 [P] (the per-branch split rests on that one example); Fiserv DNA Premium OD GL Transfer app [P]; Jack Henry jXchange balanced-transaction tutorial, where a separate $25 fee-income GL is one of two options [P, corrected by the verification pass] | **DEVIATES-BY-CHOICE**: the research's suggested set, one GL per kind, with no branch, product or currency split |
| The GL is a bank-owned internal account: `Role::ledger`, `Bank::internal`, ownerless, rendered `GL` | FLEXCUBE allows direct postings only to internal leaf GLs, and its GL categories include Income; the verification pass corrected that FLEXCUBE "internal" GLs also hold customer loan and deposit balances, so the discriminator is internal leaf plus category Income, not "internal" alone. Temenos ledger accounts sit in categories 10000 to 19999 and internal accounts carry no customer. Fiserv DNA's FCRM extract space-fills Customer_Number on a GL account | TYPOLOGY | FLEXCUBE General Ledger 14.1 [P, corrected]; Temenos developer portal, accounting journal entries and Accounting Events Lifecycle Guide (22 May 2023) [P]; Fiserv DNA FCRM extract specification (Oct 2023) [P] | **CONFORMS**: the role is the income discriminator (the model holds no customer-owned GL), and the key sits outside every customer and external role. Supersedes the card-issuer part of the cash-hub-defect-2026-08 row on distinct external keys |
| The GL is booked: credited, never seeded, never debited | A GL carries a balance, the income posted to it | INVARIANT | none needed | **ENFORCED** by `test_bank_ledger` B2 (opens at 0.00 with no buffer) and B5 (closes at the sum of its rows in the replayed posted book) |
| The postings are exported as ordinary payments whose recipient is the GL, typed `gl` (mule-temporal) and `general_ledger` (AML), not dropped | Oracle Behavior Detection models a back-office transaction as exactly two parties, the Account and the Offset Account. The FLEXCUBE-to-Mantas feed sends internal movements as back-office transaction data, while sending only Nostro, Savings, Current and Deposit accounts as account records. Fiserv DNA's FCRM extract keeps GL-leg transactions in AML profiling by default (Exclude_From_Profile N). Verafin monitors customer-to-GL transfers as an insider-fraud indicator | TYPOLOGY | Oracle BD User Guide 8.1.2.10 and Administration Guide G27520-09 (Jan 2026) [P]; FLEXCUBE Mantas Interface 12.0 (May 2012) [P]; Fiserv DNA FCRM (Oct 2023) [P, the verification pass's correction]; Nasdaq Verafin, 2 Aug 2019 [P] | **CONFORMS**: kept and typed, so a consumer may drop or separate them by account type. The research's first verdict leaned toward dropping them; its verification pass found that real feeds keep these legs, and that dropping them by default is not supported |
| The postings carry no device or IP | A bank-posted entry has no customer session | CHOICE (inference) | the research marks this as inference, not a sourced claim | **ENFORCED** by B4. The pre-round build copied the triggering row's session onto every overdraft fee (346 rows at the gate configuration, 11,788 at pop 10,000 over a year) |
| The GLs are never a victim, mule, fraud or camouflage account | Victims and mules are persons. A customer cannot direct a payment into a bank income ledger; the FFIEC's controls on internal concentration accounts include barring customer access (it does not address income GLs) | INVARIANT | FFIEC BSA/AML Examination Manual, Concentration Accounts (2024 web build via Wayback) [P], by analogy only | **ENFORCED** by B2 (no flag on the record) and B3 (no fraud or camouflage row touches a GL, and the camouflage pool's predicate `fraud::camouflageEligible` rejects every GL) |
| Camouflage P2P pays only customer deposit accounts (`Role::account`) | A cover row must match the legitimate flow it mimics, and the model's legitimate P2P pays nothing else: 11,889 of 11,889 legit P2P rows at the gate configuration and 335,953 of 335,953 at pop 10,000 over 365 days. A cover transfer into another person's credit-card account, a biller, a government payer or an external family, client or business account was a destination only fraud used | INVARIANT | none needed (measured on the model's own legitimate P2P) | **ENFORCED** by `test_bank_ledger` B3 (added at review): the predicate admits exactly the registry's `Role::account` records, and 0 of 261 camouflage P2P rows land elsewhere. DISARM: the exclusion-list predicate it replaced reds both (6,229 of 10,581 records disagree; 177 of 261 rows off a deposit account) |
| Only a posting's destination and a liquidity posting's session move; no draw is added; the shared entity stream and `makeCatalog`'s draw count are unchanged | Retyping the contra must not move any other row | INVARIANT | none needed | **ENFORCED** by A1 (shared stream next u64 `498e4bde6c6f83ea` on both builds) and A2 (the fraud-free masked digest `cf3b75546c4c76da` over 190,402 rows, posting counts 1,397 / 412 / 346 / 41, on both builds) |
| The retired keys `XO3000000001`, `XO4294967041` and `XO4294967042` are never registered and receive no row | An export written before this round still carries them | INVARIANT | none needed | **ENFORCED** by B1 |

## Registered limitations

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| No per-customer overdraft line-of-credit account | Regulation E excludes from "overdraft service" a line of credit subject to Regulation Z, an overdraft line of credit included, so an OD LOC is the consumer's own credit account: draws run LOC to deposit account, repayments the other way, and interest debits the LOC and credits the interest income GL | CHOICE | 12 CFR 1005.17(a) [P]; the per-customer reading is the research's inference, which the verification pass called sound | **REGISTERED**. The retired `bankOdLocKey` only ever received interest (see "What `bankOdLocKey` actually received" in the design). The LOC principal is the deposit account's negative cash inside its `overdrafts_` capacity, so draws and repayments are not postings and the interest debits the deposit account |
| No NSF or returned-item fee | A declined item can carry an NSF fee at a bank that still charges one | CHOICE | CFPB overdraft and NSF report (Dec 2023) [P], via the counterparty-hubs note | **REGISTERED**: the deposit fee income GL carries overdraft fees only; a declined attempt charges nothing |
| Card fee income carries late fees only | Annual, cash-advance, balance-transfer and foreign-transaction fees exist | CHOICE | none pulled | **REGISTERED** |
| No deposit interest credited and no monthly statement or service fee | These are the two kinds that reach nearly every Berka account: interest credited (UROK) on 99.0% and the statement fee (SLUZBY) on 97.7% of 4,500 accounts | CHOICE | Berka PKDD'99 trans table, tallies recomputed by the verification pass [P] | **REGISTERED**: PL posts only debit interest and penalty fees, so its GLs reach fewer accounts than Berka's. Credited interest would need an interest-expense GL as the source leg |
| The income GLs are population-scale hubs | At pop 10,000 over one year the card interest GL is paid by 54.1% of card accounts and the card fee GL by 40.5%; the deposit fee GL by 16.3% and the credit-line interest GL by 5.0% of deposit accounts. The research measured 73% of card accounts on the card issuer (both card kinds together) on the 200,000-person corpus and did not verify the real share of card accounts that incur interest or late fees | CHOICE (owner decision: retype, keep) | the counterparty-hubs note; the research's "not verified" list | **REGISTERED**: a per-kind income GL is a hub in the bank's own books (the Berka row above); the GLs are typed so the MulePatternLearner hub handling can drop or separate them (see the corpus movement) |
| The GL balance is window income, not a running ledger balance | A real income GL carries a year-to-date balance and closes to retained earnings at year end | CHOICE | none needed | **REGISTERED**: every GL opens at 0.00 at window start |
| The pre-fraud replay book's GL slots carry only the card postings | `debitAndEmit` debits the payer of an overdraft fee or LOC interest and emits the row without a credit leg | CHOICE | none needed | **REGISTERED**: nothing reads that book's GL slots; the posted book the exporters read books both legs (B5) |
| Camouflage P2P draws a uniform deposit account per row, not a payee the ring account knows | Legitimate P2P pays the sender's contacts, so payees repeat: 54.7% of legit P2P rows repeat an earlier (sender, payee) pair at the gate configuration and 86.6% at pop 10,000 over 365 days, against 0.0% and 0.03% for camouflage P2P. The gap predates this round; the review fix changed only the destination type | CHOICE | none needed (measured) | **REGISTERED, out of scope**: closing it needs a per-ring payee set drawn on the ring's own lane, a separate round that re-points every cover transfer again |
| Cards are on-us | Cards are often hosted on a separate processor, and Fiserv DNA treats processor-hosted cards as external-major accounts; whether card interest reaches a deposit-core AML feed at all was not verified | CHOICE | the research's "not verified" list | **REGISTERED**: PL books cards on-us (`Role::card`, internal), so its card income GLs are on-us too |

## Where the design deviates from the research note

- **No new account-class, GL-category or bank-party fields.** The research
  recommended `account_class = INTERNAL_GL`, `gl_category = INCOME`, an ORG
  owner record for the home bank and `is_customer = false`. The role carries
  the class and category (`Role::ledger` exists only for income GLs), the
  exporters carry it as an account type, and the GL is ownerless rather than
  owned by a synthetic bank party, the same convention every other ownerless
  account uses. Adding columns would change four exporter schemas for no
  information the type does not already carry.
- **No `txn_kind`, `initiated_by` or `counterparty_class` columns.** The
  channel (`cc_interest`, `cc_late_fee`, `overdraft_fee`, `loc_interest`)
  already names the kind, the mule-temporal rail is already `internal`/`bank`,
  and the recipient's account type flags the GL leg.
- **The per-customer OD LOC credit account is not built.** Code reading shows
  the retired key never received principal (the design's "What
  `bankOdLocKey` actually received"), so the interest
  goes to the credit-line interest GL and the Regulation Z account is a
  registered limitation.
- **The mule-model switch is downstream.** The export carries the type; the
  switch that drops or separates GL edges belongs in MulePatternLearner.

## Also fixed, because this round would otherwise have made it worse

The mule-ml exporter wrote an account IP edge for every row, including rows
with no session, whose address renders as `0.0.0.0` (`network::format` never
returns empty, so the old emptiness guard could not fire), and counted the
sentinel in the canonical IP histogram. Removing the copied IP from the fee
rows would have linked every overdrafting deposit account to one shared
`0.0.0.0` vertex and could have made the sentinel an account's canonical IP.
Both now skip the sentinel (`infra_edges.hpp`, `canonical.cpp`). This also
removes the existing `0.0.0.0` edges from card accounts (their interest and
late-fee rows) and from external sources of externally initiated rows. Part C
of the gate pins it.

## Measured (`test_bank_ledger`, run-golden gate world: pop 2,000, 60 days from 2025-01-01, seed 3405691582)

**Only the destination and the copied session moved.** Leg A (fraud off):
the shared stream's next u64 after the build is `498e4bde6c6f83ea` on the
pre-round build (the staged unknown-counterparty tree, exported with
`git checkout-index` and built separately) and on this one. The masked
digest of every row is `cf3b75546c4c76da` on both, over 190,402 rows on
both, with 1,397 card interest, 412 card fee, 346 overdraft fee and 41 LOC
interest rows on both. Paired domain predicate: 0 posting rows outside a
finite positive amount, an in-window timestamp, a source of the right kind
and the GL of their kind. The replay sort's new tie-break on the GL target (see "Only
the destination and the copied session change" in the design) therefore
reaches no row at this configuration, and the diagnostic below shows it
reaches none at pop 10,000 either.

**The one other movement is the camouflage pool, and a bisect attributes
it.** The three retired keys were in the camouflage P2P pool; the GLs are
not, so the pool shrinks by three, which re-points ring cover transfers and
cascades through balances. With fraud on at the gate configuration it moves
rows 191,721 to 191,722 and legit rows 190,275 to 190,276. At pop 10,000 over
365 days it moves rows 5,254,057 to 5,253,962 and legit rows 5,223,265 to
5,223,210 (card interest 85,672 to 85,668, card fees 22,932 to 22,928).
Restoring the pre-round pool as a diagnostic only (the three retired keys
re-registered and re-inserted at their old pool position) reproduces the
pre-round masked digests exactly at both scales (`35f81eb8a5ac1c7a` over
190,275 legit rows at the gate configuration, `b96634da5e626522` over
5,223,265 at pop 10,000), so nothing else moved. Keeping the retired keys in
the pool was not an option: they no longer exist.

| Leg B (fraud on) | Rows | Distinct payers | Share of accounts | Closing balance |
|---|---:|---:|---:|---:|
| `GL00000001` card interest income | 1,396 | 1,390 | 0.2655 of 5,236 card accounts | $27,061.15 |
| `GL00000002` card fee income | 412 | 411 | 0.0785 of card accounts | $16,175.12 |
| `GL00000003` deposit fee income | 346 | 208 | 0.0655 of 3,177 deposit accounts | $13,619.87 |
| `GL00000004` credit-line interest income | 41 | 41 | 0.0129 of deposit accounts | $170.76 |

The GLs are records 3,013 to 3,016, internal, ownerless and unflagged; 0 of
1,005 fraud rows and 0 of 441 camouflage rows touch one; 0 of 2,195 postings
carry a device or IP; each GL closes at exactly the sum of its booked rows.

| Scratch leg (pop 10,000, 365 days from 2025) | Rows | Distinct payers | Share of accounts |
|---|---:|---:|---:|
| card interest income | 85,668 | 14,234 | 0.5406 of 26,330 card accounts |
| card fee income | 22,928 | 10,675 | 0.4054 |
| deposit fee income | 11,788 | 2,594 | 0.1631 of 15,909 deposit accounts |
| credit-line interest income | 5,884 | 803 | 0.0505 |

0 of 16,359 camouflage rows touch a GL at that scale.

**DISARMS, each run against this build:** copying the triggering row's session
back onto liquidity postings reds B4 (387 rows, because the disarm also
reaches LOC interest, which the pre-round build left bare); sending LOC
interest to the deposit fee GL reds A2, B1 and B5; sending late fees to the
card interest GL reds A2, B1 and B5; registering the GLs with the external
flag reds B2; dropping the GL clause from the camouflage predicate (the
exclusion-list form the review fix below replaced) reds B3
(the row count alone passed that disarm: about 440 camouflage rows put well
under one expected row on four GLs, which is why the predicate is checked
directly); removing the mule-ml sentinel guards reds all three Part C checks
(the canonical IP becomes `0.0.0.0`); dropping the `gl` type from the
mule-temporal exporter reds `test_mule_temporal`.

**Corpus movement.** `tests/golden_run.b2sum` moves again (the staged
unknown-counterparty digest `dd0363aa...` to `754ab129...`), with the row
count unchanged at 231,731; the digest is re-pinned once at the end of the
round. `test_remote_payees` B2, which pins every legit row but its own
retired flows, now also masks this round's two fields and is re-pinned once
(`255b9e814eea12cc` over 190,275 legit rows to `dbfdd62a2aca2846` over
190,276); under the extended mask the pre-round build and the diagnostic pool
both score `815871576f03eef2` over 190,275, so its movement is the camouflage
cascade alone. The three PostgreSQL table goldens need the owner's re-pin:
the standard exporter's `external_accounts` loses the three retired keys and
its `accountnumber` gains the four GLs (`is_external` 0); the AML Account
table gains four `general_ledger` rows and the counterparty tables lose three;
mule-ml loses its `0.0.0.0` IP edges and some canonical IPs change.
`kTableCount` stays 43. The mule-temporal 2024 corpus, its TigerGraph
snapshot and the MulePatternLearner hub registry must be regenerated under a
new dataset id: the card issuer, fee-collection and LOC keys disappear, and
four `account_type = gl` accounts appear in their place (`is_external`
False), which the hub handling should drop or treat as a separate edge type
rather than as customer neighbours.

## The camouflage pool, restricted at review (design written before the code)

**The finding.** The predicate this round created,
`fraud::camouflageEligible`, said the camouflage P2P pool leaves out
destinations no legitimate P2P row pays, yet it admitted every other registry
record, credit-card accounts included, and the re-pick loop in
`camouflage::generate` re-drew only merchants. On the run-golden gate world
(pop 2,000, 60 days from 2025-01-01, seed 3405691582, fraud on) 162 of 263
camouflage P2P rows (62%) paid another person's `Role::card` account, while
all 11,890 legitimate P2P rows paid a customer deposit account
(`Role::account`); every row in which an owned internal account paid someone
else's card was camouflage, so the destination alone marked the row. At pop
10,000 over 365 days it was 5,644 of 9,538 (59%) on cards, 3,202 on deposit
accounts and 692 on external family, business, client, brokerage, employer
and landlord accounts, none of which legitimate P2P pays either. HEAD's pool
held every registry record, so the leak predates this round. The review
proposed a separate round, because the fix re-points cover transfers and moves
legitimate rows. It ships in this one instead: this round already moves
`tests/golden_run.b2sum` and re-pins it once, and the movement is the same
camouflage cascade this series has measured and attributed twice.

1. **The predicate becomes positive.** An account is eligible exactly when it
   is a customer deposit account, `Role::account`. The role is internal only,
   and `synth::accounts::makePack` is its only producer, so every such record
   is owned. That is the destination set of `Legit::p2p`, the flow the
   channel mimics. It subsumes the named exclusions (providers, remote payees,
   GLs) and closes the institutional-providers limitation that the pool still
   held SSA, the IRS, billers and cash endpoints.
2. **The merchant re-pick loop is deleted.** No merchant is in the pool, so
   the loop can no longer fire. It spent a draw only when a pick landed on a
   merchant, so deleting it moves nothing under the new pool; the gate
   configuration is run with and without it to show that.
3. **No draw is added and none leaves the per-ring lane.** The pool is still
   built draw-free from the registry after the entity stage, each P2P row
   still spends one `choiceIndex` on `{"fraud", "ring", ring, "camo"}`, and
   the shared entity stream is not read (`test_bank_ledger` A1 keeps its
   pin). What moves: each ring's P2P destinations, the later draws on the same
   ring lane (a destination equal to the source is skipped at a different
   rate), and, through balances, the replay's accept or decline of later rows,
   legitimate ones included. That is the camouflage cascade the two earlier
   amendments of this series measured and attributed.
4. **The gate.** `test_bank_ledger` B3 gains two checks: the predicate admits
   exactly the registry's `Role::account` records, and every camouflage P2P
   row targets one. DISARM: the exclusion-list predicate reds both.
5. **Re-pins.** `test_remote_payees` B2 pins a fraud-on legit digest, so it
   moves with the cascade and is re-pinned once, attributed by a bisect;
   `tests/golden_run.b2sum` moves and is re-pinned once at the end of the
   round.

**Measured (a scratch probe runs the gate world's leg and hashes every row
order-free; the pre-fix numbers come from the staged tree, rebuilt from the
index).** At the gate configuration camouflage P2P goes from 263 rows (89 on
deposit accounts, 162 on cards, 12 on external client and family accounts and
on business accounts) to 261, all on deposit accounts, and the pool from
9,242 records to 3,013. At pop 10,000 over 365 days it goes from 9,538 (3,202
on deposit accounts, 5,644 on cards, 692 elsewhere) to 9,544, all on deposit
accounts, and the pool from 45,225 to 14,943. The domain moves toward the
legitimate rows, not past them: camouflage P2P to an owner already dead goes
from 50 to 81 rows (0.52% to 0.85%) and to an owner not yet joined from 29 to
41 (0.30% to 0.43%), against 0.76% and 0.42% for legitimate P2P (2,539 and
1,412 of 335,953).

**The loop deletion is byte-neutral, so the movement is the predicate.** The
fix scores the same with the re-pick loop kept and deleted: all-row digest
`d86a257b719200d2` and legit digest `c8ffaaa5db6a50b3` at the gate
configuration, `d70316fb523b8639` and `9191afb392a10e4c` at pop 10,000. The
fraud-free legs keep their pins (`test_bank_ledger` A1 `498e4bde6c6f83ea`, A2
`cf3b75546c4c76da` over 190,402 rows), so the shared entity stream and
`makeCatalog`'s draw count are unchanged.

**What cascades.** At the gate configuration rows go from 191,722 to 191,725,
legit rows from 190,276 to 190,273, camouflage rows from 441 to 447 (bills
109 to 108, salary 69 to 78) and fraud rows stay at 1,005; the four GLs keep
their row counts and closing balances. At pop 10,000 rows go from 5,253,962
to 5,253,975, legit rows from 5,223,210 to 5,223,454, camouflage rows from
16,359 to 16,106 (P2P +6, bills +11, salary -270) and fraud rows from 14,393
to 14,415. The salary mimic moves most because its coin runs after the P2P
branch on the same ring lane, so it re-rolls which ring accounts receive a
payroll stream, and one stream carries 12 to 52 rows a year. Fraud rows move
because the illicit budget counts camouflage rows (`illicitBudgetBase` in
`injector.cpp`).

**Re-pins.** `test_remote_payees` B2 goes from `dbfdd62a2aca2846` over 190,276
legit rows and 7,902 retired rows to `75d7339bc4b70b82` over 190,273 and
7,900. The staged pre-fix build scores the old values (the suite passed on
it), and the fix scores the new ones with and without the loop, so the
movement is the pool alone. `tests/golden_run.b2sum` moves from the staged
`754ab129...` to `0642235c...`, with the row count unchanged at 231,731, and
is re-pinned once at the end of the round. The three PostgreSQL table goldens
move with the camouflage destinations (the owner re-pins them). In
mule-temporal, camouflage P2P no longer exports as an unknown-rail payment
into a credit-card account: it is a deposit-to-deposit P2P row, Zelle-eligible
under the same sender and payment draws as a legitimate one.

═══════════════════════════════════════════════════════════════════════
# AMENDMENT: atm-spread-2026-09
═══════════════════════════════════════════════════════════════════════

**What changed.** A selection bug is fixed; the model is not changed. Every
resident of an area sits at its centroid, so all of the area's own ATMs,
depositories and check-capture points tie at distance zero. `buildNearbyPoints`
(`transfers/legit/blueprints/plans.cpp`) broke that tie by pool index and
stored one four-point list per area, so a whole city shared its four
lowest-numbered points: at pop 200,000 New York had 41 terminals and used 4,
and the busiest terminal took about 300,000 withdrawals a year
(`docs/research/counterparty_hubs_2026-09.md`, proposed change 4). Now every
person gets their own nearest four. Points in strictly nearer distance groups
are kept whole, as before; the group that straddles the four-point cut is
stored whole, and each person takes a window of consecutive points in it that
starts at `splitmix(splitmix(person ^ railDomain) ^ area) % groupSize`
(`cash::NearbyIndex::select`, `entities/counterparties/cash_points.hpp`).
Nothing is sampled, no parameter changes (4 nearby points, the 82% home
terminal, 13.5 ATMs and 2.5 depositories and check-capture points per 10,000
people), and nothing is stored per person or per account: the index holds at
most three fixed keys and one tie group per area and rail. The set is still
resolved at event time, so it follows relocation. The revenue book's copy of
the area lookup (`RevenueCounterparties::cashDepositoriesFor`, business cash
takings) switched with it. The pickers are unchanged: `terminalFor`,
`depositoryFor` and `stableExternalPoint` now pick inside the person's own
set. `checkCaptureFor` and `cryptoVenueFor`, which had no callers, are
deleted.

## The authority rows

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| Each person's cash points are their own nearest four; ties at the cut are broken by a per-(person, area, rail) hash window, not by pool index | A city's customers use all of its terminals, each customer a small, spatially tight set of nearby ones | CHOICE (mechanism) | Diebold Nixdorf Advisory Services blog (Weston, 2021): consumers use about 2.5 different locations a year [P, a consultant's observation with no dataset]; Burra and Lokanathan, arXiv 2011.08721 (UN Global Pulse, 2020): median yearly distance between a customer's ATMs 0.66 to 7.21 km by income group [S, abstract only]; Ueda, CIGS WP 22-003E (2022): Mizuho customers are habitually attached to specific sites [P, corrected: about 1,300 to 2,000 repeat users per terminal, not 4,000] | **CLOSED** (the concentration defect): at pop 200,000 all 270 ATMs are used (181 before) and New York's busiest own terminal carries 1.08 times its fair share of New York's rows (10.29 before); measured table below |
| 4 nearby points per person (`cash::kNearbyCount`), 82% of withdrawals at a stable home terminal, the rest at one of up to three neighbours (`terminalFor`) | Research recommends picking among the 3 to 5 nearest terminals, 2 to 4 distinct ATMs a year, and a primary ATM taking 50 to 70% of withdrawals, and marks the share as an assumption with only indirect evidence | CHOICE | Round research note (atm-employer-landlord), recommended parameters and gaps; Diebold Nixdorf 2021 [P] | **REGISTERED, unchanged**: 4 sits inside 3 to 5; 82% is above the assumed 50 to 70% and is not tuned here |
| 13.5 ATMs per 10,000 people | 451,500 US ATMs (2022) over 333.3M people is 13.5 per 10,000; ATMIA's 520,000 to 540,000 is 15.6 to 16.2 | MEASUREMENT | Euromonitor via Payments Dive 2023-06-23 [trade press]; ATM Marketplace reporting ATMIA, 12 Sep 2023 [S] | **CONFORMS** on the trade-press count (the cash-hub-defect-2026-08 density row is amended to match) |
| Withdrawals per terminal a year: generator mean about 27,400 (0.88 users x 3.5 a month x 12 = 37 per person-year, over 13.5 terminals per 10,000); measured p50 28,150 at pop 200,000, before the affordability screen and deaths | 3.4 billion US ATM withdrawals in 2024 (average $210) over 451,500 to 540,000 terminals is 6,296 to 7,530 a year each, or 10.2 per person-year; Cardtronics averaged 757 a month (9,084 a year); active debit cardholders made 1.9 ATM transactions a month in 2023 | MEASUREMENT | Federal Reserve Payments Study, top-line data CY2015-24 [P, verified 2026-09-25]; Cardtronics 10-K 2019 [P]; PULSE 2024 Debit Issuer Study [P] (all via the round research note and `docs/research/counterparty_hubs_2026-09.md`) | **DEVIATES, REGISTERED, NOT TUNED**: the 3.6x gap in the mean is the ATM withdrawal frequency (`atm::Config` userP 0.88 and U{1..6} a month, UNCITED: 37 per person-year against the Payments Study's 10.2), not the selection. `docs/cash_hub_defect.md` forbids tuning ATM volume to close a throughput gap in a selection round |
| Busiest terminal at most 50,000 withdrawals a year (scale gate G6, before the screen) | A realistic busiest terminal is about 5 to 10 times a mean of about 7,000, so about 20,000 to 50,000 a year, not 250,000; the average Australian bank-owned ATM handled about 128 transactions a day (withdrawals plus balance enquiries) against about 29 at independents | MEASUREMENT (derived) | Round research note, recommended parameters, from the Payments Study mean above; RBA Bulletin March 2016, Table 2 [P, corrected: 130 a day includes balance enquiries and is the bank-owned average, not a busy terminal] | **CONFORMS**: 44,136 at pop 200,000 and 44,622 at pop 500,000 (298,157 and 721,489 before). The hubs note's own proposal (at or below about 40,000) sits below the gate's pre-screen figure; whether production rows meet it needs the regenerated corpus (see the scale-gate limitation) |
| Distinct accounts per terminal (printed, not banded) | Bank of America has 14,893 ATMs for about 69 million clients, about 4,600 clients per ATM; Mizuho saw about 1,300 to 2,000 repeat users per closed terminal | MEASUREMENT | Bank of America 10-K 2024 [P]; Ueda 2022 [P, corrected] (both via the research notes) | **CONFORMS in order of magnitude**: p50 2,541 and max 3,888 at pop 200,000 (max 26,780 before) |
| No draw is added: the window start is a pure hash of (person, event-time area, rail); `buildCounterpartyAccess` draws nothing; `burnRetiredCounterpartySelection` still runs first with the same count; `synth::counterparties::make` and `makeCatalog` are untouched | The shared entity stream and every lane are where they were | INVARIANT | none needed | **ENFORCED**: every blueprint the unit test and the scale gate build checks that `addCounterparties` left its stream where it found it, including worlds with 12 tied points; `test_product_providers` B5 (`498e4bde6c6f83ea`) and `test_bank_ledger` A1 keep their shared-stream pins (both re-pinned later by counterparty-sizes-2026-09 for its salary-jitter cascade; that round's income-free pin `ddfd735e74a97cd2` in `test_counterparty_sizes` sub-gate A was measured on the tree this amendment left and on its own build, and agrees) |
| Where every distance group fits inside the cut (every world with four or fewer points per rail), the selection is exactly the former list | Pools that could not express the defect do not move | INVARIANT | none needed | **ENFORCED**: unit test A3 compares element by element against the former (distance, pool index) list for four points in four areas, seven points whose groups end at the cut, the directory-less two-point fallback and a person with no home area. At pop 2,000 (3 ATMs, 2 depositories and 2 check-capture points) nothing moves; see the corpus movement note |
| One hash domain per rail (`kWithdrawalSetDomain`, `kDepositSetDomain`, `kCheckSetDomain`) | Depositories and check capture have identical counts and placement, so one domain would give every person the same ordinals on both | INVARIANT | none needed | **ENFORCED**: A6, 1,835 of 2,000 persons get different depository and check windows (a shared domain scores 0) |
| Business cash takings use the same per-person set as household deposits | The revenue book resolves the owner's area itself; missing it would leave takings on the old concentrated set | INVARIANT | none needed | **ENFORCED**: the scale gate requires `cashDepositoriesFor` to equal `depositPointsFor` for every person at every leg and drives the depository rail through `cashDepositoriesFor` |
| Every emitted ATM, cash-deposit and check-deposit endpoint lies in its owner's own set (the domain predicate paired with the `golden_tables_aml.md5` re-pin) | The real emitters (`atm.cpp`, `deposits.cpp` and the revenue book's cash takings) pick inside the per-person set, neither over the pool nor over the former list | INVARIANT | none needed | **ENFORCED** by three gate-harness corpus legs in `test_cash_boundaries`: pop 300 over 730 days (2 points per rail, so each set is the whole pool and membership cannot exclude a point), pop 10,000 over 60 days from 1991-01-01 with seed 7 (the AML golden's population, window and seed: 14 ATMs, 3 depositories, 3 check-capture points) and pop 20,000 on the same window (27, 5 and 5). Every row's set has size min(4, pool) and holds the endpoint, and every ATM row is exactly `terminalFor` over its owner's set. The pop 10,000 leg requires ATM rows whose set is smaller than the pool (all 58,899) and rows outside the former list (66); the pop 20,000 leg requires rows whose set is smaller than the pool on all three rails (118,243 ATM, 13,703 cash-deposit, 1,857 check-deposit rows) |

## Registered limitations

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| Out-of-area fill points are taken whole | An area with one or two terminals beside several small areas absorbs their fill, so ATM busiest over mean is about 1.6 and depository about 2.6 at pop 500,000; a resident's home terminal can sit in a neighbouring city, because home is uniform over the four | CHOICE | none needed | **REGISTERED**: restricting the home terminal to the nearest distance group is a separate model change. It is also why G5 is banded for ATMs only (depositories score 5.70 armed against 6.47 before) |
| Load is flatter than reality: every terminal has the same weight | Bank-owned terminals carry most withdrawals (58% of US ATM withdrawals are on-us; Australian bank ATMs were 45% of the fleet and carried 75% of withdrawals); research recommends a busiest terminal about 5 to 10 times the mean and 3 to 4 times the weight for bank-branch terminals | CHOICE | PULSE 2024 [P]; RBA Bulletin March 2016 [P]; round research note | **REGISTERED**: ATM busiest over mean is 1.61 at pop 200,000. The scale gate's G4 ceiling (2.5) must be re-measured if terminal weighting is ever added |
| No distance decay inside the four | Research suggests distance decay at about 1 to 1.5 km urban and 4 to 6 km rural; residents here sit at area centroids, so the within-area choice has no distance to decay on | CHOICE | Bank of Canada SDP 2023-28 [P]; round research note | **REGISTERED** |
| The window is keyed by person, not household | Coresidents do not share a nearby set | CHOICE | none needed | **REGISTERED** |
| Ring order is pool order inside an area | Points carry no coordinates of their own, so neighbouring windows overlap in a banded pattern | CHOICE | none needed | **REGISTERED** |
| Terminals are placed only by initial home-area quantiles (`representativeAreas`, `synth/counterparties/make.hpp`) | Movers into an area with no initial residents use fill from other areas | CHOICE | none needed | **REGISTERED** (unchanged) |
| The ATM withdrawal frequency (0.88 users, U{1..6} a month) | 37 per person-year against the Payments Study's 10.2 and PULSE's 1.9 a month per active debit cardholder | UNCITED | Federal Reserve Payments Study CY2024 [P]; PULSE 2024 [P] | **REGISTERED, NOT TUNED** (see the throughput row) |
| A person whose area is missing from the index with a pool of more than four now gets a hashed window of four, not the whole pool | Production cannot reach it: `buildNearbyPoints` covers every home and relocation area, and every `usBankDefault` country has catalogue rows; pools of four or fewer (the standalone two-point fallbacks included) are returned whole, as before | INVARIANT | none needed | **REGISTERED** |
| Business cash takings resolve the owner's set once a month, at the month start (`activity/income/revenue/generate.hpp`, unchanged by this amendment) | A business owner who moves mid-month deposits that month's takings at the former area's points | CHOICE | none needed | **REGISTERED**: the corpus check accepts the month-start set for a cash deposit that its event-time set does not hold; 0 such rows at all three corpus legs |
| The scale gate is not a corpus | Homes are drawn per person (not per household), users by a 0.88 hash coin, 42 withdrawals a year each, and no affordability screen or deaths, so its rows run above production's; the check rail picks with `depositoryFor` as a stand-in for `deposits.cpp`'s `stableExternalPoint` (both a stable per-account hash modulo the local set) | CHOICE | none needed | **REGISTERED**: the production-rows figure for the busiest terminal at pop 200,000 needs a regenerated mule-temporal corpus |

## Where the implementation deviates from the design

1. **`LocalPoints` is not a range.** The design gave it `begin`, `end` and
   `data`. `std::span<const Key>` converts implicitly from any contiguous
   range, temporaries included, so a range would let a span to a temporary
   escape through an implicit conversion. It hands out a span only through
   `span() const &`, and `span() const &&` is deleted; the compiler rejected
   two such calls in this round's own test.
2. **G5 is banded for ATMs only**, and **G6 (busiest ATM rows a year) is
   added.** A depository G5 band could not fail (see the fill limitation).
3. **The scale gate checks the revenue lookup against the household one for
   every person** and a second build against the first (A4 at scale), rather
   than only the design's order-independence check on the unit fixture.
4. **The dead pickers are deleted** (the design's optional delete-first).
5. **An area whose ranking is empty gets no entry**, so it falls back to the
   pool. The former builder stored an empty list and fell back the same way.
6. **The corpus check runs at three legs, not one.** The design tightened
   the pop 300 checks and named them the domain predicates for the AML
   re-pin, but at two points per rail each set is the whole pool, so an
   emitter reverted to the pool passed them. Review found this; the pop
   10,000 and 20,000 legs are the ones that can fail, and each ATM row is
   also checked against `terminalFor`'s exact pick.

## Measured (`test_cash_boundaries`, scale gate; the reference arm is the former selection computed in-test from the same directory)

| Rail, pop | Arm | Used / pool (G1) | G2 within area | G3 own coverage | G4 max / mean | G5 resident ratio | Busiest rows a year | Max distinct accounts |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| ATM, 10,000 (printed) | armed | 14/14 | no area over 4 | 1.000 | 1.36 | 20.96 | 36,002 | 3,251 |
| | reference | 14/14 | no area over 4 | 1.000 | 1.45 | 20.96 | 38,303 | 3,319 |
| ATM, 200,000 | armed | 270/270 | 1.08 | 1.000 | 1.61 | 2.34 | 44,136 | 3,888 |
| | reference | 181/270 | 10.29 | 0.098 | 10.88 | 10.79 | 298,157 | 26,780 |
| ATM, 500,000 | armed | 675/675 | 1.10 | 1.000 | 1.63 | 2.42 | 44,622 | 3,839 |
| | reference | 265/675 | 26.13 | 0.038 | 26.35 | 26.46 | 721,489 | 64,911 |
| Depository, 200,000 (printed) | armed | 50/50 | 1.02 | 1.000 | 1.68 | 4.62 | | |
| | reference | 46/50 | 2.02 | 0.500 | 3.31 | 4.62 | | |
| Depository, 500,000 | armed | 125/125 | 1.03 | 1.000 | 2.56 | 5.70 | | |
| | reference | 102/125 | 4.78 | 0.211 | 6.53 | 6.47 | | |
| Check capture, 500,000 | armed | 125/125 | 1.04 | 1.000 | 2.56 | 5.70 | | |
| | reference | 102/125 | 4.78 | 0.211 | 6.53 | 6.47 | | |

Bands (armed side): G1 at least 0.99, G2 at most 1.5, G3 equal to 1, G4 at
most 2.5 (ATM) and 4.0 (depository and check), G5 at most 3.5 (ATM), G6 at
most 50,000. Preconditions: at least one area with more than four own points,
and a reference G2 of at least 5.0 (ATM) or 3.0 (depository and check), so a
leg that cannot express the defect fails instead of passing on no data. At
pop 10,000 no area holds more than four ATMs; its differences come from
small areas whose cut straddles a neighbouring group.

**DISARM.** Forcing the window start to 0 (every person takes the lowest
indices of the tied group) reproduces the former selection exactly: unit test
A1 reds (4 of 12 tied points used), and with A skipped the scale gate's armed
line equals its reference line at every leg and G1 reds at the first bounded
leg (ATM, pop 200,000, 181 of 270).
The same disarm reds the pop 10,000 corpus leg, which then finds no row
outside the former list. Each emitter reverted to its whole pool also reds a
corpus leg that the pop 300 leg alone would pass: `atm.cpp` reds the pop
10,000 leg on membership (and the pop 300 leg on the exact pick, because the
set is in distance order and the pool is not), and household cash deposits,
check deposits and business cash takings, whose pop 10,000 pools of 3 still
fit the cut, each red the pop 20,000 leg on membership.

**Corpus movement.** `tests/golden_run.b2sum` (pop 2,000, 3 ATMs and 2 each
of depositories and check-capture points, so every distance group fits the
cut): the stream after this change is `0642235c...` over 231,731 rows, the
digest the bank-gl-2026-09 amendment recorded for the tree before this
change, so this change moves no byte of the run golden. The pin file still
holds the pre-round `a30c535d...` and is re-pinned once at the end of the
round. `golden_tables.md5` (standard, pop 2,000) should not move, for the
same reason. `golden_tables_aml.md5` (pop 10,000, 14 ATMs) should move: the
scale gate's pop 10,000 leg shows armed and former sets differ (busiest
terminal 36,002 against 38,303 rows a year), so ATM endpoint ids move for
residents of areas whose cut straddles a group, while row counts and amounts
do not. In the gate harness's copy of that world (pop 10,000, 60 days from
1991-01-01, seed 7) 66 of 58,899 ATM rows reach a terminal the former list
could not, so the move should be small. Its re-pin needs PostgreSQL (the
owner's) and is paired with the corpus membership predicate registered above,
at the pop 10,000 and pop 20,000 legs (the pop 300 leg cannot express the
change), and with the scale gate above. `golden_tables_card_fraud.md5` keeps
only card and merchant rows and should not move; that was not verified here.
In mule-temporal every cash point is now observed (at pop 200,000, 270 ATMs
instead of 181 and all 50 depositories and 50 check-capture points), the
busiest ATM falls from about 298,000 to about 44,000 withdrawals a year
before the screen, and its distinct payers from about 26,800 to about 3,900.
The median terminal, about 28,000 a year, is still far above
MulePatternLearner's 2,048-payment hub threshold, so the hub registry keeps
cash hubs, more and smaller ones. The 2024 corpus, its snapshot and the hub
registry must be regenerated under a new dataset id.

═══════════════════════════════════════════════════════════════════════
# AMENDMENT: counterparty-sizes-2026-09
═══════════════════════════════════════════════════════════════════════

**What changed.** Payroll picked one of `25 per 10,000 people` employers and
rent one of `12 per 10,000` landlords, both uniformly, so at pop 200,000 about
480 external employers each paid about 308 people and 240 landlords each
collected from about 292 tenants, the "individual" ones included
(`docs/research/counterparty_hubs_2026-09.md`, proposed change 5). Both
rosters are now published size distributions thinned to the population
(`synth/counterparties/size_law.hpp`):

    N_c = clamp(round(P x s x m_c), 1, F_c)

for population P, payer share s (0.74 workers, 0.35 renters), the class's
share m_c of jobs or rental units and its real member count F_c. Employers
take 17 classes (13 SUSB 2022 enterprise rows, the 20,000+ row as a rank-size
tail, federal, state and local government) and are all external. Landlords
take 8 (the seven RHFS 2021 property-size columns, the 150+ column less the
NMHC 2024 Top-50 owners, and those owners as a rank-size row); each
landlord's type is drawn from its own class's unit mix on a
`{"landlord_type", serial}` lane, with the in-bank coin last. Every pick goes
through a draw-free class-then-member pool (`entity::counterparty::SizedPool`,
`SizedKeys`): `growth::pickSized` and `pickSizedDifferent` replace the
uniform `pickOne` and `pickDifferent` on the same lanes with the same draw
counts, and the O(N) `std::find` on every job switch and lease move is an
O(1) serial lookup. Camouflage salary picks through the same pool and pays
on the picked employer's own schedule. At pop
200,000 the rosters hold 89,231 employers and 66,658 landlords (was 500 and
240); at pop 500,000, 200,556 and 155,581. The payroll and rent amount laws,
the cadence law, tenure and every draw on the per-person lanes are
unchanged; the shared stream after the income pass moves, because salary
posting jitter draws on it per payday (see the corpus movement below).

## The research tension: a metro count against a national sample

The round research, after verification, puts a single-metro bank of 200,000
residents at about 3,000 to 6,000 employer firms (the national SUSB density,
188 firms per 10,000 residents, gives 3,760) and 3,000 to 7,000 rent payees.
This law gives 89,231 employers and 66,658 landlords at the same population.
Both are right, for different sampling frames, and the decision is to keep
the thinning law:

1. **The metro figure counts the firms that exist.** A bank that serves a
   whole metro sees every firm there, each with its full local workforce.
2. **PhantomLedger's 200,000 people are a thin national sample.** They live
   in 71 US home areas (merchant-selection-2026-08 step 2), about 2,800 per
   area, a small fraction of each city. Two sampled workers rarely share a
   small employer, so the number of distinct employers such a sample meets
   is about min(sampled workers in the class, firms in the class). Measured:
   148,000 workers use 58,611 of the 89,231 employers.
3. **The metro count would rebuild the defect this round removes.** 3,760
   employers under the same size law would give the 3,361 firms below 20
   employees about 148,000 x 0.139 / 3,361 = 6.1 payees each, above the real
   mean of 3.8 for that class (21,950,184 / 5,720,093), and those six payees
   would live in different cities. 5,000 rent payees for PhantomLedger's
   70,000 leases would give every landlord 14 tenants, where the IRS count
   puts about two units behind an individual landlord.
4. **Concentration follows the frame.** The research's local figures (the
   largest employer 4 to 8% of jobs, the top 10 at 20 to 30%, managers with
   100 to 3,000 households) are metro facts. In the national sample the
   largest payer is the federal government at 1.9% of workers (2,829
   payees), the largest private employer takes 1,737, the top 10 take 6.4%,
   and the largest landlord is a Top-50 owner with 148 tenants.

A regional-bank mode (employers and landlords with a home area, co-located
with their payees) would reproduce the metro figures; it is registered below,
not built.

## Renters per household against renters per person

`rent::Rules::paidFraction = 0.35` is a share of PEOPLE, each on their own
lease; the L-5 anchor (ACS, about 35%) is a share of HOUSEHOLDS. The round
research puts a 200,000-person region at 25,000 to 30,000 renter households,
0.125 to 0.15 per person, so PhantomLedger emits 2.3 to 2.8 times as many rent
payers as real renter households, and the L-5 per-capita rent reconciliation
multiplied a household share by a per-household rent (now marked SUPERSEDED
there). The landlord roster is sized against the 0.35 PhantomLedger actually
draws, because the size law allocates PhantomLedger's own leases: sizing it
to households while emitting 0.35P leases would put 2.3 to 2.8 times as many
tenants on each landlord as it has units. The tenants-per-landlord law is
therefore right relative to the lease count, and the lease count itself
inherits the L-5 overstatement. Changing the renter share moves every rent
row and every balance: an owner decision, registered below.

## The verification's four parameter corrections

| Correction | Disposition |
|---|---|
| (a) Processor-originated payroll is about 17 to 20% of WORKERS (ADP alone is about 8% of payer FIRMS), not 17 to 20% of payers | Not modelled: every salary row names its employer. Nacha keeps the originator's Company Name readily recognizable when a processor sends the file, so an employer-keyed payer is right for payer identity; whether processor files share one Company ID across employers was not verified (research gap). **REGISTERED**, with the corrected axis for whoever models it |
| (b) At most 55% of 5-49 unit tenants route to a manager payee (Terner's 55% not owner-managed includes owner-employed superintendents) | PhantomLedger has no separate manager payee; the corporate type (95% portal rent, `RentRouter`) is the professionally-managed proxy. Its share of 5-49 unit tenants is **0.345**, under the bound (`test_counterparties`, RHFS check). The 25-49 column alone is 0.796, because LLC and general-partnership owners type as corporate from 25 units; the bound applies to the 5-49 aggregate, Terner's survey population. **CONFORMS** |
| (c) The owner-type mix: only the 0.38 individual share was supported; the 0.15 / 0.47 split needed its types redefined against RHFS | Redefined per size column from CRS R47332 Table 3: individual = individual investor + trustee + tenant in common; small LLC = LLC/LP/LLP + general partnership below 25 units; corporate = every other reported form. Renter-weighted **0.433 / 0.138 / 0.429**; reported-only aggregate 0.447 / 0.141 / 0.412 (not-reported units sit mostly in large corporate-held properties, so imputing them within each column lowers the individual share). The individual type exceeds the 37.6% individual-investor share because it includes trustees (2.1%) and tenants in common (1.2%). **CONFORMS** |
| (d) Portal rent may show a processor, not the manager, as the counterparty in bank data (AppFolio settles through its clearing bank and processors) | Not modelled: a portal rent row names the landlord. A processor hub across landlords is a further source of hubs the research leaves out. **REGISTERED** |

## The authority rows

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| SUSB 2022 enterprise size table, 13 uniform rows plus the 20,000+ row: 6,395,635 firms, 135,748,407 employees | Firms under 20 employees hold 16.17% of private jobs; firms of 10,000 or more hold 30.59% | MEASUREMENT | Census SUSB 2022, `us_state_naics_detailedsizes_2022.xlsx` and `us_naicssector_large_emplsize_2022.xlsx` (release 2025-04-10) [Certain, read by the round design; the verification recomputed under-20 16.17%, 500+ 54.14%, 5,000+ 36.32% from the xlsx; the 20,000+ row itself was not re-checked] | **CONFORMS** (`test_counterparties`, SUSB check) |
| Government is 14.0% of payroll: federal 2.9M, state 4.5M, local 13.6M of 150.0M covered jobs; private rows scaled by 0.86 | Government employs about 14 to 15% of US workers | MEASUREMENT | BLS QCEW 2022 annual averages [Certain]; BLS Employment Situation Table B-1, Aug 2026: 14.66% of nonfarm jobs [Certain, via the verification] | **CONFORMS**. Axis: QCEW covered jobs against CES nonfarm payroll, which explains 14.0 against 14.7 |
| 90,837 local-government payers | Census of Governments 2022 counts 90,837 local governments | MEASUREMENT | census.gov `govtorg2225` [Likely: a search snippet in the round design] | **UNCITED, verify** at the owner's pass |
| Federal civilian payroll is ONE payer; the 50 states are equal payers; local governments are uniform in their class | Federal agencies originate payroll separately; state and local payrolls differ in size | CHOICE | ASPEP 2022 would weight states and give local government a tail (not read) | **REGISTERED** |
| The 20,000+ row as a rank-size tail, s_r = 20,000 x (546 / r)^b, b = 0.7201 (Pareto alpha 1.3886) solved so the row sums; rank 1 is 1.87M | The largest US employers follow a heavy upper tail; Walmart has about 1.6M US associates | TYPOLOGY + CHOICE | SUSB 20,000+ row [Certain]; Walmart figure [Guessing: recalled, not verified, the Walmart FY2023 10-K would settle it] | **REGISTERED**: an unfitted check bands rank 1 in [1.2M, 2.4M]; ranks 3 to 6 (0.85M to 0.52M) run heavy against real firms of about 0.5M |
| RHFS 2021 (2020 stock): units by property size 16,550 / 6,065 / 5,470 / 2,725 / 1,055 / 1,296 / 16,387k; properties 16,550 / 2,215 / 419 / 75 / 15 / 11 / 45k; units by ownership and size | Individual investors own 37.6% of units and 70.2% of units in 1-4 unit properties; LLC/LP/LLPs own 40.4% of units and 67.8% of units in 100+ unit properties; 85.6% of properties are single-unit; 37.8% of units are in 50+ unit properties | MEASUREMENT | CRS R47332 (Keightley, 2022) Tables 1 and 3 [Certain: the report PDF was read this round, and the tables reproduce every summary sentence above]; HUD/Census RHFS 2021 infographic [Certain, via the verification] | **CONFORMS** (`test_counterparties`, RHFS check) |
| Landlord type per size column (individual / small LLC / corporate as defined above); renter-weighted 0.433 / 0.138 / 0.429 | The ownership mix changes with property size: individuals dominate 1-4 unit properties, partnerships and corporations the large ones | MEASUREMENT (derived) | CRS R47332 Table 3 [Derived]; verification correction (c) | **CONFORMS**. Replaces the population-wide 0.38 / 0.15 / 0.47 mix, which assigned a landlord COUNT from a UNIT share |
| NMHC Top-50 owners: 2.4M units, rank 1 108k (Greystar), s_r = 108k x r^-b with b = 0.2850; carved out of the 150+ column (units, and properties in proportion: 45,000 to 38,409), all corporate | The 50 largest apartment owners hold more than 2.4M units | MEASUREMENT + CHOICE | NMHC 2024 Top Owners list [Likely: a search snippet in the round design; the verification could not render the NMHC pages] | **UNCITED, verify**. The fit undershoots ranks 2 to 4 by 12 to 14% (design); each owner collects into one account |
| The thinning law, with one counterparty per expected payer in every class below its real size (occupancy lambda = 1) | A national sample meets about one payee per small employer or landlord | CHOICE | Reconciled with the verification's metro counts in the section above | **REGISTERED**: P(one payee given used) is 0.532 at pop 200,000; pure national thinning would give about 1.0 |
| Worker share 0.74 and renter share 0.35 | The rosters are sized against the payers the generator draws | INVARIANT | none needed | **ENFORCED**: tied by test to `salary::Rules{}.paidFraction` and `rent::Rules{}.paidFraction` |
| In-bank landlord probability 0.06 / 0.04 / 0.01 by type (unchanged), now applied to the class-drawn type | Small landlords bank locally; corporate owners use national commercial banks | CHOICE | none (unchanged) | **REGISTERED**: 2,487 ownerless in-bank landlords at pop 200,000, against about 8 before |
| The retired roster draws are burnt at their original positions: `max(5, round(25P/1e4))` coins at 0.04 in `make()`, `2 x max(3, round(12P/1e4))` u64 in `buildLandlords`, frozen constants | The shared entity stream does not move; `makeCatalog`'s draw count is untouched | INVARIANT | none needed | **ENFORCED**: `test_counterparty_sizes` sub-gate A matches the verbatim retired loops at pop 1, 300, 2,000, 20,791 and 200,000 (clients identical) and pins the run-golden world's income-free shared stream at `ddfd735e74a97cd2`, measured on the tree before this round and on this build |
| `pickSized` spends one uniform for a pool of two or more and none for one; `pickSizedDifferent` none when the rest is one member | Every later draw on the employment and lease lanes keeps its value | INVARIANT | none needed | **ENFORCED**: `test_counterparties` draw contract against `choiceIndex`; sub-gate C, 146,105 job switches with 0 intervals off the retired lane and 0 repeats |
| Camouflage salary picks its employer through the payroll size law, same single u64 on the camo lane | Cover salary comes from employers the size of legitimate ones | INVARIANT (pool sharing) | none needed | **ENFORCED**: sub-gate B drives the real generator at pop 200,000; mean legitimate headcount of camouflage employers is 1.023 of legitimate payees', and a uniform pool scores 0.0150 |
| Camouflage salary pays on the picked employer's own schedule: `samplePayrollProfile` on `{employer_payroll_profile, number}` off `RngFactory{payrollSeed}` (the run seed, carried in `InjectorServices`), posted through `timestamps::jittered` with the salary jitter (the posting lag, no day offset, 06:00 to 11:59). The per-mule schedule draw on the camo lane is retired, which moves that isolated lane only | A mule's cover salary posts on the dates, and in the hours, its employer pays every other payee | INVARIANT (schedule sharing) | none needed | **ENFORCED**: sub-gate B, 15,807 camouflage pairs with a legitimate co-payee, 0 of their 80,369 rows off the schedule EmploymentInitializer handed that employer's payees; the disarm (schedules from the fraud factory, the retired draw's law) puts 13,158 of 15,701 pairs (0.838) and 59,588 of 79,196 rows off. Sub-gate D, both corpus legs: 0 legitimate salary rows off the derived schedule (the instrument check), 0 of 49 (60 days) and 0 of 356 (365 days) camouflage rows off it, against 36 and 294 on fraud-factory schedules. Before the fix the review measured 11,303 of 15,758 pairs (71.7%) with a row off, counting either the lagged or the unlagged pay date as on schedule; the old minute draw could also post at 12:00, which legitimate payroll never does |
| Camouflage salary derives the schedule under `PayrollRules{}` | Legitimate payroll reads `salary::Rules::employment.payroll`, the same defaults | CHOICE | none needed | **REGISTERED**: none of `LegitAssembly::incomePrograms`, `salaryRules` and `employmentRules` has a caller, so both sides run on the defaults. A caller that sets non-default payroll rules must carry them to the injector beside `payrollSeed`, or the two schedules part |
| Camouflage P2P never pays an employer or a landlord | Legitimate P2P pays only customer deposit accounts | INVARIANT | none needed | **ENFORCED**: `fraud::camouflageEligible` (the bank-gl-2026-09 review fix) admits only `Role::account`, so none of the 2,157 employer and landlord records (16.9% of the pop 2,000 registry) is admitted; sub-gate D finds 0 of 258 (60 days) and 0 of 1,572 (365 days) camouflage P2P rows on either |
| Employer serials 1..N stay below the SSA and disability keys (9,000,001 and 9,000,002); the landlord roster stays below 10^7 (the seven-digit internal layouts) | Keys never collide or overflow their rendering | INVARIANT | none needed | **ENFORCED**: `static_assert` on the sum of real members (6,486,523) and a throw in `makePack` (binds only above about 28M people) |

## Registered limitations

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| Employers and landlords have no location | A small employer's or landlord's payees live near it | CHOICE | none needed | **REGISTERED**: co-payees can live in different cities; there is no regional-bank mode (see the reconciliation) |
| Job switches are size-weighted | New hires follow firm hiring rates, which differ by firm size and age (BDS) | CHOICE | BDS (not read) | **REGISTERED** |
| No firm or landlord births or deaths | Firms and landlords enter and exit | CHOICE | none needed | **REGISTERED**: over 20 years job churn stacks about 7.5 distinct payees onto a small employer (design estimate) |
| Relocation ends neither the job nor the lease | A cross-state mover changes both | CHOICE | none needed | **REGISTERED** (pre-existing; a mover can keep paying the old landlord for up to 8 years) |
| Pay cadence is one draw per employer and does not depend on size | 72.9% of 1,000+ employee establishments pay biweekly (L-4); federal pay is biweekly | CHOICE | BLS CES Feb 2023 (L-4) | **REGISTERED**: the worker-weighted mix now rests on a few giant draws (about 7% of workers take their cadence from about 11 employers, design estimate) |
| Registered but never-paid employers and landlords are exported as isolated vertices in aml and mule_ml | A bank's counterparty table holds counterparties it has seen | CHOICE | none needed | **REGISTERED, owner decision** (filter to observed accounts, as card-fraud does): never paid at pop 2,000 are 44.9% of employers and 43.1% of landlords over 60 days, 33.6% and 33.3% over 365 days |
| In-bank landlords are ownerless internal accounts, and the standard exporter labels them `landlord_external` / `landlord` (its `landlordIndex` holds external landlords only) | An in-bank landlord is a customer | CHOICE | none needed | **REGISTERED, owner decision** (set inBankP to 0 or give them an owner); pre-existing, now about 300 times more visible |
| The renter share is per person | About 35% of HOUSEHOLDS rent | MEASUREMENT | ACS [Likely] (L-5); round research: 25,000 to 30,000 renter households per 200,000 people | **NONCONFORMING, REGISTERED, owner decision**: about 2.3 to 2.8 times as many rent payers as renter households (see above) |
| Processor-originated payroll is not modelled | About 17 to 20% of paychecks are processor-originated (ADP alone) | CHOICE | ADP Research 2025 [P, via the verification] | **REGISTERED** (correction a) |
| 1-4 unit tenants are under-routed to managers | About 22% of 1-4 unit properties and 84% of 150+ unit properties are professionally managed | MEASUREMENT | RHFS 2021 via Multifamily Executive [S, via the verification] | **DEVIATES, REGISTERED**: the corporate share is 3.6% (1 unit) and 4.8% (2-4 units), because individual owners who hire a manager still type as individual; 150+ is 0.93 to 0.94 (conforms on a property-against-unit axis) |
| Portal rent names the landlord | Portal rent can settle through a processor | CHOICE | AppFolio 10-K FY2024 [P, via the verification] | **REGISTERED** (correction d) |
| Spending volume is sensitive to pay cadence on short windows | none | CHOICE | none needed | **REGISTERED, not tuned**: at the run-golden configuration the realized cadence moved from 38% weekly / 62% biweekly (the 5 retired employers) to the law's mix (16 / 65 / 10 / 8% of 1,207 workers), and fraud-free gate-leg rows fell 190,402 to 146,901. Forcing the retired mix back into this build restores 189,767 (a diagnostic, not shipped), so the drop is the cadence law being realized, not a regression. It is a pre-existing engine property (the paycheck boost and a monthly worker's liquidity before the first in-window payday), exposed here |
| The hubs note's "38,000 to 41,000 payroll credits per employer" and "about 1,500 employees each" | The code can emit about 308 payees and 9,100 credits a year per employer at pop 200,000 | UNCITED | `docs/research/counterparty_hubs_2026-09.md` | **UNRECONCILED**: 4.3 times the code arithmetic; no exporter or routine multiplies salary rows (split deposits are self-transfers), and the note's graph could not be queried here. Do not quote it as the baseline |

## Where the implementation deviates from the design

1. **Landlord totals.** Class masses divide by the Table 3 class sum
   (49,548k), not the published total (49,547k), so they sum to exactly 1;
   the design's 66,662 at pop 200,000 is 66,658 here. The design's
   155,585 at pop 500,000 is 155,581: the carve-out keeps round(45,000 x
   13,987 / 16,387) = 38,409 properties in the 150+ column, which binds at
   that population.
2. **The type mix is the renter-weighted 0.433 / 0.138 / 0.429**, beside
   the design's reported-only 0.447 / 0.141 / 0.412. Both are tested.
3. **The camouflage P2P predicate change ships as a gate, not as code.**
   The bank-gl-2026-09 review fix had already replaced the merchant re-pick
   with a pool of customer deposit accounts only, which excludes employers
   and landlords; the re-pick loop the design names no longer exists.
4. **The camouflage headcount band runs through the real generator at pop
   200,000 (sub-gate B), not on the corpus.** At pop 2,000 the roster holds
   more employers than workers, so a uniform pick already scores about 0.3,
   and the corpus has about 20 camouflage salary pairs (measured ratios
   0.435 at 60 days and 0.471 at 365). The corpus legs print it.
5. **The corpus rent-type band is 4 sigma over the payer count**, because a
   payer's landlord is fixed within a lease and rows cluster by payer; the
   design's plus or minus 3 points is banded at scale in sub-gate B (70,000
   leases).
6. **The optional salary-jitter lane move was not taken.** The shared stream
   after the income pass moves, so three in-test pins were re-pinned
   (`test_bank_ledger` A1/A2, `test_remote_payees` B1/B2,
   `test_product_providers` B5) with the attribution in each, and sub-gate A
   adds the income-free pin that shows nothing before the income pass moved.
7. **`AccountPools::employers` borrows the pool** rather than copying it;
   `SizedKeys::indexOf` also answers for a one-key fallback pool whose serial
   is not 1; the landlord burn spends `nextU64` twice per retired landlord
   (the same count, proven by sub-gate A) instead of replaying the draws.
8. **Added:** sub-gate B bounds the most tenants at an individual landlord
   (at most 12; 7 measured) with the retired 240-landlord roster as its
   disarm (329); `test_pipeline_e2e` checks the production pipeline's salary,
   benefit and rent rows against the pools at pop 100.
9. **Added at review: camouflage salary pays on its employer's schedule.**
   The design changed only the pick, and the generator still drew its own
   cadence, weekday, fortnight parity and posting lag on the camo lane, so a
   mule's salary from a shared employer usually landed on dates that
   employer paid nobody else. The schedule is now derived from the lane
   legitimate payroll reads. The legitimate run seed reaches the injector
   as `InjectorServices::payrollSeed` rather than through
   `LegitCounterparties`: both production engines build their injector in
   `TransferStage::makeFraudInjector`, beside `fraudSeed`, so one production
   line covers both, and the three harness injectors set it the same way.
   The injector's own factory is keyed on `fraudSeed`, which is why the
   camouflage context carries a second, legitimate factory. The re-dated
   camouflage rows move three legit rows and one retired row in post-fraud
   settlement, so `test_remote_payees` B2 was re-pinned once more; the same
   build with the per-mule schedule restored (a diagnostic, not shipped)
   scores the previous pin exactly, and the shared stream is unmoved.

## Measured (`test_counterparty_sizes`; `test_counterparties` for the pure law)

| Quantity | Measured | Band or design figure |
|---|---:|---:|
| Employers at pop 0 / 300 / 2,000 / 10,000 / 200,000 / 500,000 | 17 / 218 / 1,453 / 6,068 / 89,231 / 200,556 | exact |
| Landlords at the same populations | 8 / 106 / 700 / 3,380 / 66,658 / 155,581 | exact |
| Federal share of 148,000 workers (pop 200,000) | 0.01911 (2,829 payees) | 0.01933 +- 0.00143 (4 sigma) |
| Workers at employers with 100+ payees | 0.1177 | [0.08, 0.16]; design Monte Carlo 0.117 |
| P(one payee given used) | 0.5324 | [0.45, 0.62]; MC 0.53 |
| Employers used | 58,611 | at least 50,000; MC 58,606 |
| Under-20 classes' share of workers | 0.1385 | 0.139 +- 0.005 |
| Largest private employer; top 10 share | 1,737 payees; 0.0637 | printed |
| Employers with 79+ payees (about MulePatternLearner's 2,048 payments a year) | 127 | printed; about 480 before |
| Uniform-pick disarm: federal share, 100+ share | 0.0000, 0.0000 | red |
| Camouflage salary headcount ratio; uniform disarm | 0.953; 0.0156 | [0.5, 2.0]; below 0.1 |
| Landlords used by 70,000 renters; most tenants; at an individual landlord | 42,152; 148; 7 | at least 100; at most 12 |
| Top-50 owners' share of renters | 0.0491 | 0.0484 +- 0.004 |
| Renter type mix against the roster | 0.428 / 0.140 / 0.432 against 0.432 / 0.138 / 0.430 | +- 0.03 |
| Retired 240-landlord disarm: most tenants at an individual landlord | 329 | red |
| Job switches over 20,000 chains and 20 years; repeats; intervals off the retired lane | 146,105; 0; 0 | 0; 0 |
| `pickSizedDifferent` at 1,453 and 200,556 employers | 38 ns and 38 ns | printed |
| Corpus, pop 2,000 x 60 days: salary, benefit and rent rows off their pools | 0 of 6,021; 0 of 489; 0 of 1,218 | 0 |
| Corpus: busiest employer over the mean payees (60 / 365 days) | 29 / 1.594 = 18.2; 37 / 1.804 = 20.5 | at least 5 |
| Corpus: camouflage P2P rows on an employer or landlord (60 / 365 days) | 0 of 258; 0 of 1,570 | 0 |
| Registry at pop 2,000 | 12,726 records (10,581 before) | printed |
| Pop 500,000: registry growth; resident bytes of the new rosters | +354,287 records; 55.4 MB (registry and lookup 24.3, directory 6.9, landlord pack 8.6, blueprint and fold copies 15.6); the two size laws 5.0 KB | below 128 MB; the design estimated about 84 MB |

**Corpus movement.** `tests/golden_run.b2sum` (pop 2,000, 60 days): the tree
before this change streams `0642235c...` over 231,731 rows; this change
streams `a1824bb5d32e71f1b94b2fb0bf4c7ffadd53dd66b97727a5a37e44b1066411c9`
over 164,833 rows (-28.9%). The pin still holds the pre-round `a30c535d...`
and is re-pinned once at the end of the round. The drop is the cadence law
(registered above); the domain predicates beside the digest are sub-gate D.
`golden_tables.md5`, `golden_tables_aml.md5` and
`golden_tables_card_fraud.md5` all move (every salary source, rent
destination and the shared-stream cascade; the AML counterparty tables grow
by about 2,150 rows at pop 2,000 and about 9,450 at pop 10,000): the owner
re-pins them against PostgreSQL. `kTableCount = 43` does not move. Gate
bands exposed to the cascade still pass unchanged: `test_econ_wiring` drift
parity 1.113 (0.80 floor) and fraud-rides-L mean 0.914 over 12 seeds,
`test_card_merchant_graph`, `test_card_baselines`.

**For MulePatternLearner.** Employer payer degree is now heavy-tailed: at
pop 200,000 about 127 employers clear the 2,048-payment hub threshold
(federal about 2,830 payees, the largest private about 1,740), against about
480 uniform ones before, and 53% of paying employers pay one person.
Landlord hubs disappear: the largest owner collects about 148 x 12 = 1,776
rents a year. The 2024 corpus, its snapshot and the hub registry must be
regenerated under a new dataset id.

═══════════════════════════════════════════════════════════════════════
# AMENDMENT: outlets-frequency-2026-09
═══════════════════════════════════════════════════════════════════════

**What changed.** The last of the proposed changes in
`docs/research/counterparty_hubs_2026-09.md`, outlets and category frequency,
in four steps measured one at a time (the fourth found by the round review).

1. **Biller picks on their own lane.** `buildMarket`
   (`activity/spending/market/bootstrap.cpp`) drew each person's biller count
   and biller set on the `{"payees", person}` lane AFTER the favourite pick,
   whose retry count depends on the catalogue. They now draw on
   `{"payee-billers", person}`, so the favourite pick is last on its lane and
   no merchant change can move a biller set again.
2. **Chain outlets.** A catalogue Record was already an acceptance endpoint,
   so the research's "brand-level accounts misuse the citation" was
   overstated (the round verification says so); what was missing was the
   grouping.
   `synth::merchants::expandOutlets` (new, `synth/merchants/outlets.hpp`),
   called by `buildMerchants` between `placeGeography` and
   `appendChurnReplacements`, turns each eligible core record (grocery, fuel,
   restaurant, pharmacy or retailOther; local or regional footprint; placed)
   of weight w into n = max(1, round(w / unit)) outlets, unit being the median
   eligible core weight. The weight is split equally, the n - 1 new outlets
   take population-weighted US areas on a `{"merchant-outlet", serial}` lane,
   keep the brand's bank, and carry `Record::brand` (new; 0 for every record
   that is its own organization). Churn replacements inherit the donor's
   brand. At pop 500,000 this adds 3,346 records (+12.9%) in 1,068 chains.
3. **Category-dependent frequency.** The favourite pick
   (`commerce::sampleFavoriteSlot` with a catalogue, `commerce/affinity.hpp`)
   keeps each row's Zipf rank multiset, {1 + floor(F x unitFor(p, m))}, and
   lets the category decide which favourite holds which rank: favourites are
   ordered by a Plackett-Luce race key -ln(1 - v) / w_category (v a hash in
   its own domain) and the sorted rank uniforms are handed out in race order.
   The pick still spends the one uniform the router hands it.
4. **Fraud venues carry the same category law.** The fraud venue pool
   (`buildMerchantPool`, `transfers/fraud/typologies/unauthorized.cpp`) draws
   from the catalogue by weight, not from a favourite row, so step 3 left it
   on the category-blind mix while legitimate visits moved, and the biller
   categories went from under-represented in fraud card rows to
   over-represented. Each candidate's weight, card-present and
   card-not-present, is now scaled by `commerce::kCategoryVisitLift`
   (`commerce/affinity.hpp`), the race's visit-to-favourite ratio per
   category, which sub-gate K10 asserts against the reading. The factor is
   positive everywhere, so the candidate set and the one uniform per venue
   slot are unchanged.

No draw is added to the shared entity stream, `makeCatalog`'s draw count is
unchanged (`coreCountFor` is the same arithmetic, extracted), and nothing
already stored is re-derived. The favourite set is not enlarged: the round
verification corrects the research here, because the Alessandretti set of
about 25 is a CURRENT set, while Circana's 20 restaurant chains a year and
Krumme's 64 merchants in six months are cumulative counts, which come from
exploration and monthly turnover.

## The visit-rate weights, derived

The targets are shares of card-present favourite visits, from the Diary of
Consumer Payment Choice 2022 in-person non-cash payments a month (SF Fed 2023
Findings, Figure 5, confirmed by the round verification):

    grocery and convenience            5.5
    restaurants = fast food 3.4 + sit-down 2.0 = 5.4
    general merchandise and department 3.1
    gas                                2.6
    sum                                16.6

These types are about 80% of in-person payments: the 2026 Findings count 30
in-person payments a month, 16 of them at grocery, convenience and
restaurants and 8 at gas and general merchandise, so (16 + 8) / 30 = 0.80
(the 2024 Findings' footnote 13 gives the same 80% of non-bill payments).
The base is therefore 16.6 / 0.80 = 20.75 and the shares are grocery
5.5 / 20.75 = 0.265, restaurant 5.4 / 20.75 = 0.260, general merchandise
3.1 / 20.75 = 0.149 and gas 2.6 / 20.75 = 0.125. The remaining 0.20 is a
CHOICE: retailOther takes general merchandise plus 0.10 (0.249), pharmacy
0.05, and the four biller categories 0.05 (0.0125 each). Each target is that
share times the physical visit share P = 1 - 0.3798 = 0.6202 (the online
visit share the membership law gives at 2022, which the solve holds):
grocery 0.164, restaurant 0.161, retailOther 0.154, gas 0.078, pharmacy
0.031, billers 0.031 together.

The weights were then solved on the production construction (outlets
included) at pop 500,000, 2022, F = 30 (the saturated set size,
merchant-selection-2026-08), over 24,000 sampled residents: damped
proportional fitting, factor (target ratio / realized ratio)^0.7 relative to
grocery, 100 iterations, with the ecommerce weight solved so the online visit
share does not move. Pharmacy and the billers cannot reach their targets
(even the bottom ranks give them more), so the solve drives them to zero and
they ship at a floor. Result, in `Category` order: grocery 1, fuel 0.173,
utilities 0.001, telecom 0.001, ecommerce 0.144, restaurant 0.792, pharmacy
0.001, retailOther 0.307, insurance 0.001, education 0.001.

**The design's table (fuel 0.134, ecommerce 0.0957, restaurant 0.773,
retailOther 0.226, floors 0.01) was solved on the catalogue without outlets
and is superseded.** Its note that "any floor below about 0.05 gives the same
result" is wrong: at 0.01 a biller outraces an ecommerce favourite about 6% of
the time, and the biller share rises from 0.121 to 0.131 (0.149 at 0.03). At
the shipped 0.001, a tenfold lower floor moves no category share by more than
0.0007 (sub-gate K4).

## The authority rows

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| Biller count and set drawn on `{"payee-billers", person}` | A draw whose count depends on data is last on its own lane (merchant-churn-2026-07) | INVARIANT | none needed | **ENFORCED** by construction; the favourite `pickFrom` is now last on `{"payees", person}`. Run-golden rows did not move (164,830), the digest did (biller destinations) |
| Outlet count n = max(1, round(w / median eligible core weight)); equal weight split | A chain is a brand whose volume takes several typical stores to carry; firms with 500+ employees hold 63.2% of retail receipts and 31.3% of establishments | DERIVED (no free constant), validated UNFITTED | SUSB 2022 `us_naicssector_large_emplsize_2022.xlsx` (released 2025-04-10), Retail Trade: 645,404 firms, 1,045,890 establishments, $6,850.9B receipts; firms under 500 employees 718,945 establishments and $2,523.6B [round design, read from the xlsx] | **CONFORMS**: chain volume share 0.652 and establishment share 0.279 at pop 500,000 (`test_card_merchant_graph` J4, bands +-0.10; the catalogue without outlets scores 0 and 0, J5) |
| Only core records expand; the tail stays single | Firms under 500 employees average 1.12 establishments | MEASUREMENT | SUSB 2022, as above | **CONFORMS** (J3: no tail record has siblings) |
| Online records, ecommerce, nationalService records and the four biller categories stay single endpoints | Card-absent merchants use one principal place of business; a utility or insurer is paid as one account | CHOICE | Visa Merchant Data Standards Manual, April 2026 [P, via the round verification, which corrects the research: the rule sets a LOCATION, not the number of merchant IDs] | **REGISTERED**: how many merchant IDs an online brand uses was not verified, so single accounts are a choice, not a finding |
| Outlet areas drawn i.i.d. by population, with replacement, on `{"merchant-outlet", serial}` off the geo seed | The acquirer assigns each outlet its own location | CHOICE (placement law) | Visa Merchant Data Standards Manual [P] | **REGISTERED**: no regional clustering; the largest chain (51 outlets) spans 28 areas with at most 10 in one |
| Outlets are stored as Records; the plan (count, areas) is derived | Every sampler, the favourite CSR and the ledger address merchants by catalogue index, and every endpoint is a registered account | INVARIANT | `docs/ram_derive_dont_store.md` | **ENFORCED**: O(outlets), +3,346 records at pop 500,000; the local pools grow 0.731 to 0.824 MiB (sub-gate H) |
| Outlet ownership stays keyed on the outlet's own counterparty key | A franchised outlet is its own proprietor's business; a brand-keyed owner would make one Party the owner of a 51-outlet chain, and ownerless chain outlets would correlate the register with footprint | INVARIANT (leak rule) | merchant-ownership-2026-07 | **ENFORCED**: `test_card_endpoint_graph` G' and G'' pass unchanged (G'' ratios 1.027 / 0.874 / 1.261 / 0.774) |
| The register-shape check is "as spread as a uniform key hash over the business-owner cohort": distinct proprietors at least 0.9 of the uniform expectation, and no Party at or above the load a uniform hash reaches one time in a thousand; the absolute claim is bounded at production scale | Most proprietors hold one outlet, a few hold several | CHOICE (instrument) | Multi-unit operators exist but are the minority: 7 in 10 restaurants are single-unit operations (National Restaurant Association [P]), about 55% of fuel-selling stores are single-store operators (NACS 2025 [P]) and 63% of convenience stores belong to companies with 10 or fewer stores (NACS 2026 [P]), all confirmed by the round verification | **ENFORCED**: the retired bands (at least owned/3 proprietors, at most 6 each) were uncited and sized on core-floor legs; the mean is coverage x catalogue / cohort, so outlets took leg-long to 4.07 per proprietor (max 9; uniform expectation 55.1 proprietors, ceiling 16), where "at least owned/3" asks for 76 proprietors from a cohort of 56 and cannot pass at all. A five-Party register reads CONCENTRATED at every leg; at pop 500,000 the derived mean is 0.31 to 0.42 per proprietor (bound: at most 1) |
| Churn replacements inherit `brand`; births are sized on the enlarged base count | BLS BED Table 7 is ESTABLISHMENT survival, so every outlet is an establishment | CITED (unchanged hazards) | BLS BED Table 7, NAICS 44 (bls-citation-2026-07) | **CONFORMS**: `test_merchant_churn` F1, 144 and 43 branded births, 0 off their donor's brand; F2, incumbent outlets survive 0.4808 and 0.7935 against 0.4677 and 0.7763 |
| Visit-rate weights, in `Category` order: grocery 1, fuel 0.173, utilities 0.001, telecom 0.001, ecommerce 0.144, restaurant 0.792, pharmacy 0.001, retailOther 0.307, insurance 0.001, education 0.001 | Card-present visits split as the DCPC 2022 non-cash in-person payments per consumer a month. Arithmetic: grocery 5.5; restaurant = fast food 3.4 + sit-down 2.0 = 5.4; general merchandise 3.1; gas 2.6; sum 16.6. Those types are (16 + 8) / 30 = 0.80 of in-person payments, so the base is 16.6 / 0.80 = 20.75 and the shares are grocery 5.5 / 20.75 = 0.265, restaurant 5.4 / 20.75 = 0.260, general merchandise 3.1 / 20.75 = 0.149 (retailOther 0.249 with the CHOICE row below) and gas 2.6 / 20.75 = 0.125. Times the physical visit share 1 - 0.3798 = 0.6202 the targets are 0.164, 0.161, 0.154 and 0.078. The weights are the damped proportional fit of each target's ratio to grocery, factor (target ratio / realized ratio)^0.7 for 100 iterations, at pop 500,000, 2022, F = 30, outlets included, with ecommerce solved to hold the online share and pharmacy and the billers driven to the floor | DERIVED | DCPC 2022, SF Fed 2023 Findings, Figure 5 [P, confirmed by the round verification]; DCPC 2026 Findings, 30 in person with 16 at grocery, convenience and restaurants and 8 at gas and general merchandise [P, confirmed by the round verification]; DCPC 2024 Findings footnote 13, about 80% of non-bill payments at those types [P, round design] | **CONFORMS** at 2022 / F = 30: realized grocery 0.166, restaurant 0.163, retailOther 0.155, fuel 0.078; restaurant/grocery 0.982 (DCPC 5.4 / 5.5 = 0.982), fuel/grocery 0.470 (DCPC 2.6 / 5.5 = 0.473), grocery visits 2.07 times its favourite share (K3); the all-ones disarm scores 1.069, 0.924 and 1.01 and fails K3 (K5) |
| retailOther +0.10, pharmacy 0.05, billers 0.05 of the remaining 0.20 | The 20% of in-person payments outside the five DCPC types | CHOICE | none (DCPC gives no split) | **REGISTERED**. Pharmacy frequency has only a biased anchor (Medicare median 13 visits a year, JAMA Network Open 2020, an older-adult upper bound), so it stays floored |
| The rank multiset is unchanged; only the assignment moves | Within-card top-1 visit share stays on Krumme's 13-22% band | INVARIANT | Krumme et al. 2013 (alpha 0.80, already cited) | **ENFORCED**: K1, 0 mismatched multisets over 76,000 rows at four (year, F) points; expected top-1 identical (0.1500 at F = 30, 0.1777 at F = 19) |
| The multiplicative alternative, w_c x rank^-0.80, rejected | Solved to the same targets it breaks the within-card law | MEASUREMENT (negative result) | none needed | **RECORDED**: within-card top-1 0.2059 at F = 30 and 0.2498 at F = 19 (K7 and the round probe), the second above Krumme's 0.22 |
| Floor 0.001 for pharmacy and the biller categories | A floored category is ranked last | CHOICE | none needed | **ENFORCED**: K4, floor 1e-4 moves no share by more than 0.0007 |
| Weights are era-flat, solved at 2022 | The DCPC split is a 2022 measurement | CHOICE | DCPC 2025 and 2026 Findings (16 a month at grocery, convenience and restaurants and 8 at gas and general merchandise in both 2024 and 2025) [P] | **REGISTERED**: at 2005 / F = 30 restaurant/grocery is 0.954 and fuel/grocery 0.503, but retailOther/grocery is 0.542 against 0.94, because the CNP share (0.059) leaves retailOther's online records in the physical race. K3 is bounded at 2022 only |
| Online visit share held on the dated CNP series | Remote card payments follow the Fed Payments Study anchor (merchant-selection-2026-08) | INVARIANT | Fed Payments Study 2022 [already cited] | **ENFORCED**: K2 within 0.02 at every point (0.3798 to 0.3792 at 2022 / F = 30; largest move +0.0088 at 2022 / F = 19) |
| Card rows put grocery plus restaurant at least 1.5 times the biller categories (the domain predicate paired with the `golden_run.b2sum` re-pin) | Everyday merchants, not billers, carry card frequency | INVARIANT | none needed | **ENFORCED**: K9 on both corpus legs, 0.4551 / 0.0992 (4.59) and 0.4499 / 0.1516 (2.97); the pre-round world scores 0.48 and 0.57 |
| The domain predicates paired with the three table goldens the owner re-pins: AML Counterparty vertices are exactly the external registry, every chain outlet is exported as a Counterparty or an Account, every Account balance is finite; every card-fraud `Merchant_Assigned` category is one of the ten names and no online record has a `Merchant_Location` (the existing coordinate gate pins each location to its own domestic centroid) | A digest pins whatever it is given (cash-hub-defect-2026-08) | INVARIANT | none needed | **ENFORCED**: `test_pipeline_e2e`, 864 counterparties against 864 external accounts, 132 outlets all exported, 0 non-finite balances; 181 assigned merchants, 0 off the names, 0 online with a location, 67 outlets observed |
| Physical fraud-only merchants: the 4-seed mean share of fraud card rows on a physical merchant no legitimate card pays is at most 0.01 | Outlets add low-traffic storefronts near victims and must not manufacture fraud-only endpoints | INVARIANT | none needed | **ENFORCED**: `test_card_merchant_overlap`, 0.0090 / 0 / 0 / 0 (mean 0.0023); the design's single-seed bound on the WHOLE fraud-only share could not hold (see the limitations) |
| Fraud venue weights scaled by `commerce::kCategoryVisitLift`, in `Category` order: grocery 2.07, fuel 1.04, utilities 0.45, telecom 0.45, ecommerce 0.98, restaurant 1.89, pharmacy 0.45, retailOther 1.30, insurance 0.45, education 0.45, in the card-present and the card-not-present pool alike | Fraud card rows carry each merchant category in the same proportion to legitimate rows as they did before the category law, so category alone scores fraud no better than it did | CHOICE (the principle); DERIVED (the values: visit share over favourite share under the race at pop 500,000, 2022, F = 30) | none: the round research has no series on card fraud by merchant category, so following the legitimate law is a declared neutral choice, not a finding (real card fraud concentrates on resellable goods, which ten categories cannot express) | **ENFORCED**: `test_card_merchant_graph` K10, worst gap 0.0030 against the reading (the legacy law reads 1.0738); `test_card_merchant_overlap`, pooled biller-category fraud/legit card-row lift over four seeds at pop 2,000, 2019, 365 days 1.018 in [0.60, 1.30] (per seed 0.781 / 1.155 / 0.940 / 1.207). Disarms, both red: the pool without the factor 1.617, the legacy frequency law with the factor 0.491. Outlets alone read 0.791 and the pre-round world 0.737. Category-only AUC 0.613 (0.669 without the factor, 0.606 outlets alone, 0.586 before the round) |
| The shared entity stream and every base record's lanes are untouched | `makeCatalog`'s draw count is load-bearing (merchant-churn-2026-07) | INVARIANT | none needed | **ENFORCED**: J1 (next u64 equal with and without outlets; 0 of 26,000 base records moved); `test_bank_ledger` A1 and `test_remote_payees` B1 keep `9e0a89591a4d861f`; `test_membership` green |

## Registered limitations

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| Typical outlet bank volume: expected payments a year per PHYSICAL record at pop 500,000, 2022, F = 30, at the 236.6 card payments per person-year the 6,000 x 731d acceptance corpus measured (K8). Medians grocery / restaurant / fuel / pharmacy: before the round 621 / 607 / 591 / 618; outlets alone 1,146 / 1,215 / 953 / 1,187; the category law alone 1,320 / 1,196 / 662 / 270; shipped 2,344 / 2,232 / 982 / 523 | A typical restaurant or gas outlet takes a few hundred payments a year from one bank's customers, not low thousands: Visa sees about 8,000 transactions per restaurant per quarter, about 150 to 250 a year at a 0.38% bank share, and gas about 300 | MEASUREMENT | Dev and Hamooni, arXiv 2009.02461 [P]; NACS, 2,500 gallons a day [P]; the round verification's correction of the research's hub sizes | **NONCONFORMING, REGISTERED, NOT TUNED.** The MEAN is set by catalogue density per customer, which neither step changes in kind: 3,348 physical restaurant records at pop 500,000 each serve about 150 customers making 0.163 x 236.6 = 38.5 restaurant payments a year, a mean near 5,750. A real restaurant serves about 550 residents (608,144 restaurants and eating places, 18.3 per 10,000 at 333 million, 2022 Economic Census [P, via the round research]), of whom a bank of 500,000 among the 71 areas' 52.9 million residents holds about 5, so about 200 a year. Few hundred therefore needs the areas' real outlets, about 96,600 restaurants and 1.3 million employer establishments at CBP density whatever the population: 29 times the restaurant records at pop 500,000 and about 26,000 records per 10,000 customers, far above the Nilson ceiling J4 bounds. The round moves the MEDIAN up because both steps compress the spread: outlets replace heavy brands with median-weight stores (grocery p90 9,228 to 7,218, max 54,438 to 37,449), and the category law hands grocery and restaurant favourites the head ranks. Closing it is the CBP-driven supply the research leaves open (resident-sized, on its own lane), not a retuned outlet unit |
| Supercenters and online brands stay above MulePatternLearner's 2,048 cap | A supercenter outlet takes about 5,000 a year and an online brand millions | MEASUREMENT (derived) | Round research, expected hub sizes, as corrected | **CONFORMS in kind**: ecommerce median 3,467, max 486,354; 51% of grocery and restaurant records exceed 2,048 a year (see the row above) |
| Reassignment cannot push the biller categories below about 11.5% of card visits | The DCPC-derived target is 0.031 | CHOICE | none needed | **REGISTERED**: shipped 0.1214 (0.2695 before), floor 0.1149 with billers always last (K4). Whether 12% is realistic depends on the share of card payments that are bills to these four categories, which the research did not find |
| Category supply is uniform, one tenth each | Real establishment supply is far from uniform (restaurants 19.3 per 10,000 residents, gas stations 3.41, supermarkets 1.98) | MEASUREMENT | 2022 Economic Census EC2244BASIC and EC2272BASIC [P, via the round research] | **REGISTERED**: `makeCatalog`'s category draw is a rejection-sampled `choiceIndex` on the shared stream, so its draw count depends on data; supply must be changed post-hoc on an isolated lane |
| Grocery and general-merchandise physical favourites per row: 4.18 at F = 30 | FMI: 5.4 separate grocery banners a month | MEASUREMENT | FMI U.S. Grocery Shopper Trends 2026 press release [P, via the round verification, which also corrects the research's 3 to 6 a year] | **REGISTERED**: no small banner count is imposed (membership is unchanged); the set sits below FMI's monthly count, a membership question, not a frequency one |
| One person may favour several outlets of one chain, with no cap | A person uses one primary outlet per chain plus 0 to 2 secondary ones | UNCITED | Round research, labelled a judgment (no source measures outlets of one chain per customer) | **REGISTERED** |
| No correlated brand-wide closure | A chain bankruptcy closes many outlets at once | CHOICE | none needed | **REGISTERED**: each outlet draws its own interval on its own `merchant-life` lane |
| No brand column is exported | The TigerGraph `Merchant` id identifies the acceptance endpoint until a Brand vertex exists | CHOICE | `data/commerce/README.md` | **REGISTERED**: `brand` is in world state only |
| nationalService records are single-centroid and card-present | A national biller has no storefront | CHOICE | none needed | **REGISTERED** (pre-existing) |
| The biller categories stay somewhat over-represented in card-present fraud at gate-leg populations | The category factor corrects the frequency law, not the membership law: the fraud pool weighs candidates by volume (`Record::weight`) while legitimate favourites are drawn by reach (a power of the weight below 1), and outlets widen that gap because a split brand gains reach mass that unsplit billers do not | MEASUREMENT | none needed | **REGISTERED, pre-existing mechanism widened by outlets**: inside card-present rows the four billers read a pooled lift of 1.68 on the 2019 gate legs above (utilities 2.71), against 1.06 before the round. In expectation at pop 500,000 in 2019 (a probe outside the repo over the real pool kernel and membership sampler) the card-present biller lift is 1.20 with the factor, 1.15 with outlets alone and 2.49 without the factor, so at production scale the factor restores the outlets-alone mix. Weighting the pool by reach would change the card-present kernel legitimate exploration shares, so it is its own round |
| Merchant category separates fraud strongly at 1991 | The stolen-card CNP share is era-flat at 0.70 (`kCardNotPresentShare`) while the legitimate CNP share is dated (0.010 in 1991), so online, mostly ecommerce, merchants carry fraud at several times their legitimate share | CHOICE (pre-existing) | none needed | **REGISTERED, pre-existing, measured this round**: pooled over the four pop-300 1991 legs of `test_card_merchant_overlap` (a probe outside the repo), ecommerce lift 5.01 and category-only AUC 0.787 (0.742 before the round, 0.807 without the factor); the biller lift is 0.726 (0.564 before, 1.572 without the factor). Dating the fraud modality split is the registered per-era payment-method item |
| Fraud card amounts do not follow the venue's category | `amounts::cardFraudSpend` draws a ticket independent of the venue, while the category law lowered the legitimate median card ticket from about $69 to $54 (the everyday categories carry cheaper baskets) | CHOICE (pre-existing) | none: the round research has no fraud ticket series by merchant category | **REGISTERED, measured by the round review** (pop 2,000, 2019, 365 days, two seeds): amount-only AUC 0.468 / 0.500 before the round and 0.513 / 0.552 after; the fraud median ($62 / $72) did not move, and the venue step cannot move it |
| The CNP explore branch samples the national CDF over every record, physical outlets included | A remote purchase pays a remote endpoint | CHOICE | none needed | **REGISTERED** (pre-existing; outlets make a far physical landing slightly more frequent) |
| Enumeration probes pick uniformly over live records, outlets included, while writing use_chip Online | A card-testing probe hits online merchants | CHOICE | none needed | **REGISTERED** (pre-existing) |
| Online fraud-only merchants in the 1991 overlap leg | At 1991 the legitimate CNP share is 0.010, so an online record born in-window can take card-not-present fraud before any cardholder favours it | CHOICE | none needed | **REGISTERED, pre-existing, surfaced**: seed 7777777 puts 28 of 238 fraud rows (0.1176; 32 before the fraud pool carried the category law) on three online churn births (none an outlet), because the larger base catalogue re-keys the churn cohort; the pre-round world scored 0 at that seed and 0.0143 at the main seed. Printed, not bounded |
| The gift-card share's "eligible categories carry about 40% of card payments" | Measured 0.569 before the category law and 0.638 after (pop 500,000, 2022), so 500 bp implies about 7.6 cards per person a year against the cited 5 | UNCITED input | `commerce/gift_cards.hpp` | **STALE, REGISTERED**: re-deriving (about 330 bp) moves the $500 precision gates, so it is its own round |
| A re-presented deposit debit can post after its remote merchant closed | The remote pick reads liveness at emission; the funding replay re-presents an unfunded debit up to two times, at most 108.5 hours later | CHOICE | none needed | **REGISTERED, instrument corrected**: `test_remote_payees` C4 now reads liveness at emission (live at the row, or closed within the retry horizon derived from `ReplayFundingBehavior`); 1 of 85,966 rows posts 37.5 hours after its merchant closed |
| The run-golden gate world's row count swings widely under any biller or favourite re-randomization | The monthly commerce evolver spends a data-dependent number of draws (`churnBillers` retries, `evolveFavorites` retries) on the session rng that then draws every day frame and population-dynamics multiplier | CHOICE (pre-existing coupling) | none needed | **REGISTERED, pre-existing, found this round**: in the gate harness (pop 2,000, 60 days, 2025) January's daily counts are byte-identical across the biller-lane step and diverge from day 31; two re-randomizations of the biller lane alone give 145,906 and 180,226 rows, and on the shipped tree renaming the biller lane moves the leg from 155,131 to 149,115 rows with January again byte-identical (a probe outside the repo). The production binary does not show it (183,820 rows under three biller lanes, and burning 1 to 3 draws at the month boundary moves rows by at most 0.024%). Fixing it moves every harness pin, so it is its own round |

## Where the implementation deviates from the design

1. **The weight table was re-solved on the catalogue with outlets**, and
   the floor is 0.001, not 0.01 (see the derivation). The design's online
   share drift (0.393 to 0.381) becomes 0.3798 to 0.3792.
2. **K3's grocery check is "at least 1.6 times the favourite share", not
   2x.** With outlets the grocery favourite share rises (0.070 to 0.080), and
   the measured lift is 2.07; the disarm scores 1.01. The DCPC ratio checks
   are the design's.
3. **The overlap bound is on PHYSICAL fraud-only rows, as a 4-seed mean.**
   The design's "fraudOnlyRowShare <= 0.01, 0.0000 today" was stale: the
   staged tree already read 0.0136 at the main seed, and the whole share
   includes an online mechanism outlets do not touch (registered above).
4. **`test_card_endpoint_graph` G's absolute register bands were replaced**
   by the uniform-spread check (see the authority row). The design allowed
   re-measuring and forbade the two tempting repairs; neither was taken.
5. **`test_econ_wiring`'s CPI band reads a mix-adjusted ticket ratio.** The
   raw ratio is 2.292 against 1.928 before, because the category law makes
   the category mix era-dependent (1991 physical visits shift toward
   grocery and restaurant baskets far more than 2019's, whose online share is
   27%). Weighting each category's ratio by its 1991 share reads 1.849
   against CPI 1.877, inside the unchanged band; the raw ratio is printed.
6. **`test_remote_payees` C4 reads liveness at emission** (see the
   limitation row). The one failing row was a retry, not a pick defect.
7. **`test_counterparties`** now builds the key-clearance catalogue with
   outlets, so the reserved-serial check covers them (58,891 records, max
   serial 58,891).
8. **Added:** J's disarm is computed in-file (the catalogue without outlets
   must fail J4), K8 prints outlet volume, K9 pairs the run digest with a
   corpus predicate, and `test_pipeline_e2e` carries the design's AML and
   card-fraud predicates for the three table digests.
9. **The design's "physical-chain hubs shrink roughly by outlet count" holds
   for outlets alone, and only at the head.** In expected payments a year
   per physical record (pop 500,000, 2022, F = 30), outlets alone take the
   largest grocery record from 54,438 to 37,449 and the largest restaurant
   from 118,267 to 61,804, and lower every outlet category's p90, but they
   RAISE the median. The category law then hands grocery and restaurant the
   head ranks, so the shipped maxima are 65,472 and 114,674. The
   verification's few-hundred figure for a typical restaurant or gas outlet
   is therefore not met, and the reason is catalogue density, not the outlet
   rule (the limitation row above). The outlet unit was not retuned to chase
   it, and a probe outside the repo shows it could not be: at half the unit
   the chain shares leave J4's band (0.757 / 0.487) and the restaurant median
   RISES to 4,252; at a tenth (1,567 records per 10,000, past the Nilson
   ceiling; shares 0.821 / 0.849) it is back to only 1,164.
10. **The fraud venue pool carries the category law (step 4), which the
    design did not cover.** The round review measured the gap the design
    left: with step 3 alone the biller categories' fraud/legit lift went from
    0.70 / 0.75 to 1.27 / 1.71 on two pop-2,000 2019 seeds. The factor is
    the race's own reading rather than a new constant, and the fraud/legit
    category gate carries a disarm on each side (see the authority row).

## Measured

`test_card_merchant_graph` (pop 500,000 unless stated):

| Quantity | Before | After |
|---|---:|---:|
| Records; chains; added outlets | 26,000; 0; 0 | 29,346; 1,068; 3,346 |
| Chain volume / establishment share (J4) | 0 / 0 | 0.652 / 0.279 |
| Largest chain: outlets, areas, most in one area | none | 51, 28, 10 |
| Pops 300 / 2,000 / 8,000: added outlets (printed) | 0 | 150 / 91 / 127 |
| Top-1 reach; hubs above 25% / 50% (G) | 0.0823; 0 / 0 | 0.0799; 0 / 0 |
| Mean home-to-favourite miles; P(within 50) (H) | 3.8; 0.9730 | 3.9; 0.9724 |
| Top physical OUTLET's home-area span | 1 | 8 (its brand 8) |
| Local pools; cutoff discard | 0.731 MiB; 2.74e-08 | 0.824 MiB; 2.22e-08 |
| Visit share at 2022 / F = 30: grocery, restaurant, fuel, retailOther | 0.080, 0.086, 0.075, 0.119 | 0.166, 0.163, 0.078, 0.155 |
| Visit share: pharmacy; ecommerce; four billers | 0.087; 0.284; 0.270 | 0.039; 0.278; 0.121 |
| Within-card top-1 (K1, expected) | 0.1500 | 0.1500 |
| Corpus legs: sub-gate D ratio; within-card top-1 | 2.669 / 2.637; 0.2454 / 0.2352 | 2.990 / 2.841; 0.2040 / 0.2166 |
| Corpus legs: grocery+restaurant over billers (K9) | 0.48 / 0.57 | 4.59 / 2.97 |
| 2019 overlap legs, 4 seeds pooled: biller fraud/legit lift; category-only AUC | 0.737; 0.586 | 1.018; 0.613 |

`test_merchant_churn`: live ratio 0.842 and 0.961 (0.832 and 0.955 before),
incumbent survival 0.4674 and 0.7755, traversal 1.67x and 1.22x (floor 1.10),
out-of-tenure 0.197% and 0.223% (ceiling 1%). `test_card_endpoint_graph` G'
lifts 0.982 / 1.043 / 1.034 / 0.925 (0.974 / 0.988 / 1.023 / 0.958 before
the fraud venue step, band unchanged). `test_econ_wiring` drift parity 1.071
(1.113 before), volume ratio 0.635, fraud-rides-L 12-seed mean 0.915.

**Corpus movement.** `tests/golden_run.b2sum` (pop 2,000, 60 days): the tree
before this round streams `b8b5cb01...` over 164,830 rows; the biller lane
gives `8c343ee2...` over 164,830 (rows unchanged); outlets give `3e821ce4...`
over 193,302 (+17.3%); the category law gives
`c5b502f0c272cfa194fdc81f27952b8d4d2f6587ace0ba93e010bfe4e8fafcbc` over
183,820 (-4.9%); the fraud venue step gives
`5b6ec79229b1f7fc05129d73c77756dd41308ee5bf117e712ffa854780c98336` over
183,820 (rows unchanged; the step moves fraud destinations and the
chargeback credits sourced from them). Diagnostics, not shipped: with a category-independent amount
law the outlet step still moves +16.5% and the category step +1.2%, so the
category step's move is the amount mix and the outlet step's is not; it is
the core-floor regime (330 base records gain 91 outlets at pop 2,000). At pop
20,000 over the same 60 days the three steps give 2,039,577, 1,976,176
(-3.1%) and 2,238,916 (+13.3%, the cheaper-ticket direction the design
predicted). The pin still holds the older `a30c535d...` and is re-pinned once
at the end of the round. In-test pins re-pinned with the per-step values in
their comments: `test_bank_ledger` A2 and `test_remote_payees` B2 (their
shared-stream pins hold; B2 moves again at the fraud venue step, on its 109
chargeback credits only). `golden_tables.md5`, `golden_tables_aml.md5` and
`golden_tables_card_fraud.md5` all move (merchants, card rows, counterparty and
Account vertices gain the outlet accounts): the owner re-pins them against
PostgreSQL and re-runs `docs/card_fraud_postgres_acceptance.sql` and
`docs/card_fraud_device_ip_investigate.sql`. `kTableCount = 43` does not move.

**For MulePatternLearner.** Physical chain hubs split: the largest chain is
51 outlet nodes, not one. Online brands and billers stay single hubs, and
card-merchant hubs remain above 2,048 payments a year (the outlet-volume row
above). They become MORE numerous, not fewer: at pop 500,000 in 2022 the
records expected above 2,048 a year go from 8,974 of 26,000 (0.345) to
10,964 of 29,346 (0.374), because the typical grocery and restaurant outlet
now sits near 2,300, so the hub registry grows. Per-payer frequency is now category-dependent: grocery and
restaurant visits roughly double and biller-category card visits fall by half,
replacing the flat 4.7 payments per payer a year. The 2024 corpus must be
regenerated and reloaded under a new dataset id before the hub registry is
rebuilt.

═══════════════════════════════════════════════════════════════════════
# AMENDMENT: hub-realism-2026-09 round close
═══════════════════════════════════════════════════════════════════════

**The run golden is re-pinned once, for the whole round.**
`tests/golden_run.b2sum` moves from the pre-round `a30c535d...` over 231,731
rows to `5b6ec79229b1f7fc05129d73c77756dd41308ee5bf117e712ffa854780c98336`
over 183,820 rows (-20.7%), the digest the outlets-frequency-2026-09
amendment recorded for its last step. Every link of the chain was recorded by
its own amendment above: institutional-providers `42c5c164...`,
unknown-counterparty `dd0363aa...`, bank-gl `754ab129...` and its
camouflage-pool review fix `0642235c...` (all at 231,731 rows), atm-spread
(no byte moved), counterparty-sizes `a1824bb5...` over 164,833 and its
camouflage-schedule review fix `b8b5cb01...` over 164,830 (three legit rows
re-settled), then outlets-frequency `8c343ee2...` (164,830), `3e821ce4...`
(193,302), `c5b502f0...` (183,820) and `5b6ec792...` (183,820). The row count
moved at three steps only, each attributed where it happened: the payroll
cadence law (-28.9%), outlets (+17.3%) and the category law (-4.9%). A
scratch probe outside the repository that replays the binary's construction
in-process (the pipeline's `buildWorld` and `runWindowedTransfers` behind the
same CLI parse and setup) reproduces both ends exactly: the pre-round tree
streams `a30c535d...` over 231,731 rows and this tree `5b6ec792...` over
183,820. The pin was captured the way `tests/test_run_golden.cpp` documents
(the baseline deleted and the test re-run, which reports the capture as a
skip), and the full non-PostgreSQL suite then passed: 68 of 74 tests pass,
the five PostgreSQL tests skip with code 77 and `test_scale_soak` skips
because it is opt-in (`PL_SOAK`).

## The authority row

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| `test_run_golden` reads the run's own summary lines and checks them before it compares or captures the digest: the pinned population (2,000 people), at least one account per person, the summary's row count equal to the rows the Golden sink digested, and 0 < fraud rows < rows. A failing predicate fails the capture run too | A digest pins whatever it is given, an absurd corpus included (cash-hub-defect-2026-08), so a re-pin must not be able to record an empty or fraud-free stream, or a run whose summary disagrees with its digest line | INVARIANT | none needed | **ENFORCED**: 2,000 people, 12,893 accounts, 183,820 rows, 502 fraud rows. Disarm, red: a predicate that rejects the observed fraud count fails the test before the baseline is read. The content predicates for this corpus stay where the stages put them (K9 in `test_card_merchant_graph`, sub-gate D in `test_counterparty_sizes`, the leg checks in `test_product_providers`, `test_remote_payees` and `test_bank_ledger`) |

## Hub sizes at scale, measured without PostgreSQL

A scratch probe outside the repository (the same in-process replay that
reproduces the run golden above) ran the mule-temporal 2024 corpus's own
generation configuration, pop 200,000 from 2024-01-01 for 366 days at seed 42,
on this tree: 123,362,638 stream rows in 633 s at an 18.2 GB peak RSS. It
counts, per account, every stream row that names it as source or target, so
it reads the generator's stream, not the exporter's vertices; compare classes,
not exact figures. Counts are per year (366-day counts times 365/366). The
baseline column is the pre-round corpus read in TigerGraph (the round
research's measured table).

| Counterparty class | Records (with a row) | Busiest five a year | Above 2,048 a year | Pre-round corpus, full year |
|---|---:|---|---:|---|
| External-unknown catch-all | retired | no row | 0 | 3,814,933 |
| Bank GL, card interest | 1 | 1,743,705 | 1 | card issuer 2,316,826 (interest and late fees together) |
| Bank GL, card fees | 1 | 537,614 | 1 | (in the card issuer) |
| Bank GL, deposit fees | 1 | 368,463 | 1 | fee collection 361,991 |
| Bank GL, credit-line interest | 1 | 119,898 | 1 | overdraft line 113,386 |
| Auto insurers | 100 (100) | 339,498; 337,019; 213,195; 184,806; 111,098 | 74 | one insurer, 1,808,916 |
| Home insurers | 150 (150) | 4,350; 2,204; 1,365; 1,295; 1,096 | 2 | (in the insurer's key set) |
| Life insurers | 125 (125) | 92,864; 63,937; 59,334; 55,407; 44,847 | 88 | one insurer, 1,074,202 |
| Mortgage servicers | 300 (300) | 62,459; 57,474; 57,263; 54,384; 52,951 | 46 | on the student servicer |
| Student-loan servicers | 15 (15) | 157,498; 97,071; 90,653; 88,984; 77,282 | 8 | one servicer with every mortgage, 1,430,365 |
| Auto lenders | 200 (200) | 38,329; 37,946; 35,987; 32,481; 31,863 | 72 | one lender, 714,971 |
| ATM terminals | 270 (270) | 42,865; 42,018; 32,800; 32,788; 32,474 | 270 | busiest four about 250,000 each |
| Cash depositories | 50 (50) | 25,491; 24,372; 24,191; 23,978; 23,561 | 50 | (with the ATMs, 227 cash-point hubs) |
| Check-capture points | 50 (50) | 3,701; 3,490; 3,406; 3,395; 3,394 | 25 | (with the ATMs) |
| Card merchants, online records | 1,423 (1,417) | 355,252; 322,638; 300,776; 268,225; 240,174 | 1,017 | busiest merchant 341,126 |
| Card merchants, chain outlets | 1,831 (1,831) | 35,830; 32,694; 31,327; 30,058; 26,743 | 1,721 | no outlets |
| Card merchants, independent physical | 4,735 (4,644) | 120,860; 70,624; 66,103; 62,814; 53,987 | 1,400 | |
| Card merchants, biller categories | 4,333 (4,313) | 230,067; 156,108; 149,633; 132,422; 130,396 | 1,258 | |
| Card merchant brands (outlets summed; 404 chains) | 10,778 active | 718,791 (53 outlets); 369,781 (21); 363,037 (26); 355,252 (1); 322,638 (1) | 4,078 | 4,384 card-merchant hubs |
| Check-payee banks | 1,000 (1,000) | 302,960; 286,521; 209,781; 111,916; 78,843 | 103 | on the catch-all |
| Funeral homes | 1,785 (948) | 5; 4; 3; 3; 3 | 0 | on the catch-all |
| P2P platforms | 2 (0) | no row | 0 | on the catch-all |
| Employers | 89,232 (63,762) | 60,360; 37,805; 21,854; 17,139; 13,454 | 76 | about 480 at 38,000 to 41,000 each |
| Landlords | 66,659 (44,379) | 1,202; 989; 965; 922; 856 | 0 | 240 at about 2,300 each |
| Subscription billers | 160 (160) | 47,777; 47,707; 47,121; 47,036; 46,979 | 160 | 160 at about 47,000 each |
| SSA | 1 | 207,023 | 1 | a hub |
| Disability payer | 1 | 73,780 | 1 | a hub |
| IRS | 1 | 262,063 | 1 | a hub |
| Every account | | | 6,443 (one a customer account at 2,084) | 5,602 |

The busiest external account falls from 3,814,933 rows (the catch-all) to
355,252 (an online merchant); the only accounts above that are three of the
bank's own income GLs, which are internal and typed `gl`. Accounts above
2,048 a year rise from 5,602 to 6,443: the provider pools (290 of them), the
check-payee banks (103) and every ATM (the busiest now about 43,000 a year)
are new hubs, employer hubs fall to 76 and landlord hubs to 0, and the card
merchant records above the cap grow to 5,396, as the outlets-frequency
amendment predicted. MulePatternLearner's hub registry and history-withheld
stubs stay necessary.

**The instrument, checked against the baseline.** The same probe over the
pre-round tree (60 days from 2024-01-01, annualized; that run alone peaked
at 14.9 GB, so a pre-round full year was not attempted) reads the
catch-all at 4,182,444, the one auto insurer at 1,902,210, the student
servicer at 1,525,566, the life insurer at 1,108,858, the auto lender at
825,089, fee collection at 302,542, the busiest four ATMs at 261,291 to
273,555, the busiest card merchant at 385,172 and 5,825 accounts above 2,048,
all within 17% of the TigerGraph figures. Two classes do not: the card
issuer reads 1,319,335 (card balances build through the year, and this tree's
own January and February annualize the two card GLs to 1,138,532 against
2,281,319 over the full year), and the pre-round employers read at most
15,683 a year, the gap the research already registered as unreconciled.

## Registered limitations

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| Every tuition installment pays one education merchant: `tuition::generate` (`transfers/legit/routines/family/tuition.cpp`) draws the payee once per run with `run.education().pick(rng)` | A population's students attend many schools; the Tuition row of the Family transfers table defines tuition as paid parent to STUDENT account, not a university payment | MEASUREMENT | the Tuition row of the Family transfers table in this document | **NONCONFORMING, REGISTERED, NOT FIXED** (pre-existing; found by this measurement, not moved by the round): 32,398 tuition rows in 2024 at pop 200,000, all on one biller-category merchant, the only flow outside the government payers and the GLs whose every row names one external account. Code and definition disagree, so the fix starts by deciding which is right; a per-student payee must draw on its own lane |
| Accounts above 2,048 payments a year rise from 5,602 to 6,443 | Realistic card merchants, ATMs, providers, large employers, SSA and the IRS are hubs at a bank of 200,000 customers | MEASUREMENT | the round research's hub section | **RECORDED**: the round removes the artifacts (the catch-all, the singleton providers, the ATM tie-break, the uniform rosters), not the hubs; the card-merchant growth is the density limitation the outlets-frequency amendment registered |

## Owner must do

Re-pin `tests/golden_tables.md5`, `tests/golden_tables_aml.md5` and
`tests/golden_tables_card_fraud.md5` against PostgreSQL (each amendment of
this round lists what moves), then re-run
`docs/card_fraud_postgres_acceptance.sql` and
`docs/card_fraud_device_ip_investigate.sql`. Regenerate the mule-temporal 2024
corpus under a new dataset id, load a new TigerGraph snapshot and rebuild
MulePatternLearner's hub registry, dropping or separating the four GL
accounts' edges. `kTableCount = 43` does not move.
