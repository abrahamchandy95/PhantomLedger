# PhantomLedger master citation document (model ground truth)

Every research-sensitive number in PhantomLedger is a row here: the code's
value, its real-world claim, the citation and the status. The filename is
historical; the scope is the whole model.

Consolidated 2026-07-27 from a 2,318-line predecessor (citation tables,
per-round citation logs, a model-version history and per-round engineering
"U-sections"). Rows carry final verdicts; load-bearing reversals are in
"Superseded claims". Round narratives: `docs/h1_nominal_scale_wiring.md`,
`docs/h2_persona_timeline.md`, `docs/h3_mortality_estate.md`,
`docs/h4_macro_modulation.md`, `docs/card_fraud_victimization.md`,
`docs/era_data_provenance.md`. Later rounds are appended as amendments.

## The authority rule (owner directive, 2026-07-18)

- UNCITED row: mirrors the code, provisional; if doc and code disagree, fix
  the doc.
- CITED row (owner-verified source): the document governs. A contradicting
  model value is NONCONFORMING and PhantomLedger changes to fit, only
  through the model-version pipeline (owner-approved ADJUST, one named
  commit, re-pin of every golden touched), never silently.
- CHOICE row: the normative content is the documented deviation (real
  value, deviation, reason); it conforms when the deviation is explicit and
  justified.

Classes: MEASUREMENT (should match real data), TYPOLOGY (shape should match
documented patterns), CHOICE (documented deviation), INVARIANT
(model-consistency law). Never conflate prevalence axes; labels
(`is_fraud`, SARs, alerts/CTRs, chain/shell) are calibrated separately.

Statuses: CONFORMS, DEVIATES-BY-CHOICE, NONCONFORMING → ADJUST, UNCITED.
Confidence: [Certain] read from the source; [Likely] strong secondary
inference; [Derived] arithmetic on cited values; [Guessing] recalled,
unverified.

Axis discipline: three of the four reversals were axis misreads
(conditional vs marginal, per-transaction vs per-case, flow vs stock).
State both axes and check they match before any verdict.

# Part I: Fraud / AML model

### F-1. Prevalence & fraud budget

| Parameter | PL value | Class | Anchor & source | Status |
|---|---|---|---|---|
| Fraud rings per 10k customers | mean 6.0, lognormal σ 0.4 | CHOICE | Real density far lower; oversampled for label density. Europol EMMA; UK Finance Annual Fraud Report | DEVIATES-BY-CHOICE |
| Solo fraudsters per 10k | 4.0 | CHOICE | Oversampled. FTC Consumer Sentinel; FBI IC3 | DEVIATES-BY-CHOICE |
| Max fraud participation / illicit persons | 6% / ≤0.5% of population | CHOICE | internal caps | - |
| Fraud budget p | 0.0012 of transactions (F = pL/(1−p), exact L) | CHOICE | Nilson: US-issued cards $14.32B on $13.007T in 2023 ≈ 11.0 bp of value [Certain, Derived]; worldwide 6.58¢/$100 (2023), $33.41B (2024) [Certain]. Fed Reg II covered-issuer debit 17.6 bp of value (2023) [Certain]. Axis: PL's 12 bp is by count; all benchmarks are by value, so the count oversampling factor is unknown | DEVIATES-BY-CHOICE |

### F-2. Ring topology

| Parameter | PL value | Class | Anchor & source | Status |
|---|---|---|---|---|
| Ring size | lognormal μ2.0 σ0.7, clamp [3,150], mean ≈ 9.4 | TYPOLOGY | Europol EMMA 7/8/9 publish network totals (18,351 / 8,755 / 10,759 mules; 324 / 222 / 474 recruiters), not ring sizes; mule-to-recruiter ~23-57 [Certain totals, Derived ratios] | Unverifiable; DEVIATES-BY-CHOICE |
| Mule fraction of ring | Beta(2,4) → [0.10,0.70], mean ≈ 0.30 | TYPOLOGY | Europol EMMA | as above |
| Mule multi-ring reuse | p 0.06 | TYPOLOGY | Europol EMMA (recurring mules) | as above |
| Victims per ring | lognormal μ3.0 σ0.8, clamp [3,500], mean ≈ 27.7 | TYPOLOGY | IC3 / FTC | UNCITED |
| Repeat victimization | p 0.10 per victim slot of being a victim of an earlier ring; window = the run, ≤12 months standard (`Victims::repeatP`, `synth/people/fraud.hpp`, applied in `people/make.hpp`) | MEASUREMENT | Literature 10-45% by window, type and definition | UNCITED (low risk) |

### F-3. Playbook mix (17 playbooks, weights sum 1.00)

classic .12, pureMule .12, placementToIntegration .12 (structuring
.25→layering .55→invoice .20), rapidFunnelMule .10 (.20/.65/.15),
smurfThenLayer .08 (.40/.60), shellLaundering .06 (.65/.35),
pureScatterGather .05, pureLayering / pureFunnel / pureStructuring /
pureCycle / pureBipartite / classicWithLayering / scatterGatherWithLayering
.04 each, bipartiteWeb .03, pureInvoice / mixingService .02 each.
Structuring mass 0.36, so P(no structuring per ring) ≈ 0.64. The
placement→layering→integration skeleton is FATF's three-stage model
(fatf-gafi.org; *Professional Money Laundering*, 2018) [Likely on edition,
Certain on framework]: CONFORMS. The weight vector: DEVIATES-BY-CHOICE.

### F-4. Typology structure & fraud amounts

| Parameter | PL value | Class | Anchor & source | Status |
|---|---|---|---|---|
| CTR threshold | strictly > $10,000, currency only | MEASUREMENT (statutory) | eCFR 31 CFR §1010.311 (retrieved 2026-07-18); same-day aggregation §1010.313(b) [Certain] | CONFORMS (see Superseded claims) |
| Layering hops | 3-8 | TYPOLOGY | FATF Professional ML (2018) | UNCITED (page cite pending) |
| Structuring ε below threshold | U[$50, $1,500] | TYPOLOGY | FinCEN structuring guidance; FFIEC BSA/AML manual | UNCITED |
| Structuring profile mix | 60% threshold ($8.5k-$9.95k) / 25% medium ($3-7k) / 15% small ($300-1.5k) | CHOICE | FinCEN SAR narratives | - |
| Splits per victim burst | 3-12; burst 3-8 d; sub-burst 1-2 d; 08-22 h; secondary target p .20 | CHOICE | - | - |
| Classic-fraud amount | LN($900, .70) floor $50 | CHOICE | FTC CSN Data Book 2024: median loss $497 ($500 in 2021-23); mean per loss report $12,651; 63% of loss reports under $1,000 [Certain]. UK Finance 2026: APP £2,324/case in 2025 [Certain, Derived]. Declared target: money-movement/bank-drain scams (phone-contact median $1,400 in 2022; APP ~$2,950/case), above the all-fraud median | DEVIATES-BY-CHOICE (with that declaration) |
| Cycle amount | LN($600, .25) | CHOICE | - | - |
| Card-test charge | U[$0.50,$5.00], ~40% on {.50,1,2,5} (test-pinned) | MEASUREMENT | Sub-$5 authorization testing in issuer/network advisories [Likely]; the pattern is the claim | UNCITED (pull a Visa card-testing bulletin) |
| Card fraud spend | per txn median ≈ $79, mean ≈ $162, clamp [$1,$5k]×priceScale (test-pinned); per case targetEvents U{5..14} ⇒ ≈ $1.2-1.5k | MEASUREMENT (txn) / CHOICE (case) | UK Finance 2026: remote-purchase card fraud £423.5M / 3.2M cases ≈ £132 (~$167) per case [Certain, Derived]. PL ~8× per case on purpose (device/IP/burst learning, F-1 rationale) | txn UNCITED; case DEVIATES-BY-CHOICE |
| Compromised card instrument | unauthorized `Rail::card` uses the victim's primary account and exports as derived debit. A Round 7 swap to an issued credit-card liability was reverted: fraud is planned after `CardCycleDriver` closes statements and generates payments/interest, so it created unserviced debt | KNOWN GAP + guard | existing-card misuse spans credit and debit | DEVIATES: `test_card_prevalence` requires zero late-injected credit-liability sources until fraud planning joins card servicing |
| ATO drain | per drain median ≈ $180, mean ≈ $554, clamp [$10,$85k]×priceScale, ~0.4% ≥ $10k; per case U{3..8} ⇒ ≈ $3.0k | MEASUREMENT | UK Finance 2026: remote-banking fraud £104.4M / 37,646 cases ≈ £2,773 (~$3.5k) per case [Certain, Derived]; PL ≈ 87% | CONFORMS as a band |
| Unauthorized rail mix | card .48 / gift-card scam .12 / impostor push .12 / ATO .28 | MEASUREMENT-adjacent | FTC CSN payment-method mix; the two authorized rails equal: gift cards most-reported, bank transfers largest by loss | UNCITED (verify) |
| Gift-card scam (victim-authorized) | 2-6 cards in one 1-4 h coached burst; 75% {$100,$200,$500 triple-weighted}, else $50-$500 in $10 steps (mean ≈ $339/card ⇒ ≈ $700-2,000/case, test-pinned); retail merchants; `card_purchase`; label `scam_gift_card`; never reimbursed; not age-graded | MEASUREMENT-adjacent | FTC gift-card Data Spotlights: most-reported scam payment method for years; ~$217M losses 2023; victims coached to buy several max cards; retailer caps commonly $500; median $500-$1,000 per scam [Likely on vintages] | UNCITED (verify) |
| Impostor push (victim-authorized) | `FraudType::scamImpostor`; 50/50 `externalUnknown` (wire-shaped) / `p2p`; `scamWireAmount` LN($900, σ1.3) clamp [$50,$50k] × priceScale(era) × age severity; never reimbursed | CHOICE (magnitudes) + MEASUREMENT (order) | UK Finance APP per-case losses one to two orders above card fraud [Certain on ordering]; FTC CSN medians. Crypto declined: the era lock ends in 2020 | UNCITED |
| Victim susceptibility | incidence falls with age (1.35/1.30/1.15/0.95/0.75/0.60/0.50 by decade from the 20s); severity rises (0.70/0.80/0.90/1.00/1.30/1.70/2.20, ~3×). Persona factors non-age only: student 1.10, freelancer 1.15, smallBusiness 1.25, salaried/highNetWorth/retiree 1.00 (no double count). Tilt share 0.65, clamp [0.25, 3.00]× eligible mean | MEASUREMENT (directions) + CHOICE (magnitudes) | FTC CSN Data Books (reports peak 20s-30s, decline after 60); FTC "Protecting Older Consumers" reports to Congress (median loss rises, oldest ~3× youngest) [Certain] | directions CONFORM; magnitudes DEVIATE-BY-CHOICE |
| Card-fraud reporting | reported p .85 per case → each fraudulent spend refunded by a merchant chargeback (flag 0, `cc_chargeback`, lag 1-10 d, outside the budget); sub-$5 tests never | MEASUREMENT-adjacent | Reg Z / 15 U.S.C. §1643 caps liability at $50, network zero-liability waives it [Certain]; Security.org: most victims made whole [Likely] | UNCITED (statute Certain) |
| No reimbursement on authorized rails | gift-card and impostor-push never made whole | MEASUREMENT (regulatory) | Reg E (15 U.S.C. 1693) covers unauthorized transfers only; the UK reimbursement code postdates the window | CONFORMS |
| Membership across a case | every rail requires `[joinTs, closeTs)`, `closeTs = death + 120-day settlement`; authorized scams also alive. Planning takes the earliest horizon (victim close, victim death for authorized rails, payee close) and accepts a case only if its whole span fits; the generator rejects malformed plans and suppresses post-horizon chargebacks | MEASUREMENT (repair) + CHOICE (scope) | deceased-identity fraud advisories; estate settlement | CONFORMS; card/ATO after death only in the estate tail |
| ATO / Reg E remediation | Unmodeled. Owner-gated design: reported p ≈ .90, victim's bank credits each drain, lag ~2-10 business days; needs a bank-remediation counterparty and a dedicated credit channel (not `cc_chargeback`) | CHOICE (declared gap) | 12 CFR 1005.6 ($50 if reported ≤2 business days, $500 ≤60); 1005.11 (provisional credit within 10 business days) [Certain] | KNOWN GAP |

Reimbursement credits are flag-0 rows outside the budget F = pL/(1−p), like
camouflage; `unauthorized::generate` bounds flag-1 rows only.

### F-5. Camouflage

Small P2P p .03/day, monthly bill p .35, salary inbound p .12 (CHOICE).
Small P2P pays a uniformly drawn customer deposit account, the only
destination legitimate P2P pays (bank-gl-2026-09, "The camouflage pool,
restricted at review"). Amounts scale with the index of the mimicked flow
(bill/p2p × priceScale, salary × wageScale); any other scaling would be a
detectable artifact.

### F-6. Detection & label layer

| Parameter | PL value | Class | Anchor & source | Status |
|---|---|---|---|---|
| Below-CTR alert band | [$9,000, $10,000] → sev 2, all channels (exactly $10,000 lands here, files nothing) | CHOICE | FFIEC; TM vendor catalogs (generic high-amount monitoring) | - |
| CTR record | > $10,000 and `channels::isCurrency` = {atm_withdrawal, cash_deposit, fraud_structuring} → sev 3 + CTR row | MEASUREMENT (statutory) | eCFR 31 CFR §1010.311 [Certain] | CONFORMS |
| Velocity alert | ≥5 txns/(account, day) → sev 2 | CHOICE | TM vendor docs | - |
| Alert→case escalation | 1 in 8 (content hash) | CHOICE | "TM conversion 5-15%" was uncitable folklore [Guessing]; regulators quote >90% false positives (<10% conversion) | re-annotated CHOICE |
| SAR filing probability | 0.70 per group (content-keyed) | CHOICE | FinCEN FY2024: 4.7M SARs, 20.5M CTRs (12,870 and 56,160/day); ~4.4 CTRs per SAR; fraud-typed SARs ~52% [Certain]. Sanity band only (PL oversamples) | UNCITED |
| SAR monetary floor | ≥ $5,000 group total | MEASUREMENT (statutory) | eCFR 31 CFR §1020.320(a)(2): "involves or aggregates at least $5,000" [Certain]. Unmodeled: insider abuse at any amount; $25,000+ tier with no suspect | CONFORMS |
| SAR filing lag | activity end + 30 days | MEASUREMENT, re-classed | eCFR 31 CFR §1020.320(b)(3): 30 calendar days from initial detection, +30 if no suspect [Certain]; detection date not modeled | DEVIATES-BY-CHOICE |
| shell_score | round2(passThrough × (1 − organicShare)) | CHOICE | FATF shell typologies | - |

Known simplification: no same-business-day aggregation (31 CFR
§1010.313(b)); five same-day $2,500 cash deposits file no CTR.

### F-7. Measured emergent properties

Probe pop 10k / 60 d / seed 7 unless noted.

| Measurement | Value |
|---|---|
| CTR rows | 117 vs FinCEN per-adult anchor ≈128 [Derived] (20.5M CTRs ÷ ~262M adults ≈ 0.078/adult-yr); analytic ≈129 pre-attrition (quiet months, weekend rolls, window edges) |
| Alerts / SARs | 24,231 / 2 |
| Threshold splits below alert band | 0.375 (analytic 0.345) |
| Posted structuring mix | 15/32/53 vs sampler 60/25/15 (unfunded victim debits bounce; emergent) |
| Card-view fraud rate | 0.1347% (12,997 of 9,645,706 view rows, pop 20k / 730 d). The anchor (a third-party artifact's 0.11675%) was removed 2026-08 with its lineage: uncalibrated. A replacement must be an issuer-side rate by count, never value bp. Axis: flag-1 share of card-view rows (card_purchase + merchant) |
| Liquidity coupling | added legitimate cash inflows moved the corpus −3.2% (fewer overdraft-fee and retry rows); invariance gates green |

Moving the card-view rate toward any future anchor is a fraud-budget change
(targetEvents, rail mix, every denominator): owner-gated ADJUST with golden
re-pins, never a silent tune.

# Part II: Legitimate economy

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

`cardP` gates credit-card issuance (`synth/cards/issue.hpp`); `ccShare` =
credit share of spend; `limit` = credit limit. Paycheck sensitivity
Beta(α,β): student (4,2), retiree (3,3), freelancer (2,4), smallBusiness
(2,5), HNW (1,8), salaried (2,3). Medians jittered LN σ.15; probabilities
Normal σ.08 clamp [.01,.99].

| Row | Anchor & source | Status |
|---|---|---|
| cardP weighted mean .826 | S-DCPC Table 3: credit adoption 82.3%, debit 90.3% (2024) [Certain]; comparator is credit | CONFORMS |
| Initial balances (weighted ~$1,960/person) | Fed SCF 2022: median household transaction account $8,000 (mean $62,410); checking-only median $2,800 (mean $16,891); under-35 median $5,400; top income decile $111,600 [Certain] | DEVIATES-BY-CHOICE (day-zero, not steady state). Flags: retiree $1,500 low (65-74 medians are multiples); HNW $25,000 low vs $111,600 unless wealth is off-ledger by design |

### L-2. Spending engine

| Parameter | PL value | Class | Anchor & source | Status |
|---|---|---|---|---|
| Transaction load | 40 txns/person/month (engine) | MEASUREMENT | Fed Diary 2026 (Oct 2025): 47 payments/consumer/month, 6 cash. Atlanta Fed 2024 S-DCPC Table 6: 48.2 total, cash 6.7, check 1.2, debit 14.3, credit 16.6; average $142 [Certain]. PL bank rows = 40 engine + ~2.5 subscriptions + ~3 ATM + ~1 internal + ~2-5 loan/insurance/card ≈ 48-52; Diary comparator 41.5 non-cash + 3-5 ATM ≈ 45-46; PL ~5-15% above [Derived] | CONFORMS as a band |
| Daily counts | gamma-Poisson k=1.5; weekend ×0.8; day shock Gamma(1.3, 1/1.3), unit mean | TYPOLOGY | payment-count dispersion literature | UNCITED |
| Slot mix | merchant .82 / bills .10 / p2p .08 around external .05 ⇒ 77.9/9.5/7.6/5.0 | MEASUREMENT | S-DCPC Tables 9a/11/13: bills 10.2 of 48.2 = 21.2% of count (62% of value, avg $418); purchases incl. P2P 78.8%; "A person" 1.8/mo = 3.7%, avg $181 [Certain]. PL's .10 slot + out-of-engine recurring debits ≈ 20-25% of rows | bills CONFORM; P2P DEVIATES-BY-CHOICE (~2× 3.7%; feeds fraud typologies) |
| Seasonality (unit mean) | Jan .94, Feb .96, Mar 1.02, Apr 1.01, May 1.00, Jun .99, Jul .98, Aug 1.03, Sep 1.01, Oct 1.00, Nov 1.05, Dec 1.15 | MEASUREMENT | Census MARTS NSA Dec/Jan ~1.22 (Dec 2025 $817B, Jan 2025 $668B); PL 1.22 after damping | CONFORMS |
| Momentum | AR(1) φ .45, σ .15, clamp [.20, 3.00] | CHOICE | - | - |
| Dormancy | enter .0012/day; 7-45 d at ×.05; wake 2-5 d | CHOICE | - | - |
| Paycheck boost | ≤ +10% × sensitivity, 4-day decay | TYPOLOGY | payday-response literature (JPMC Institute) | UNCITED |
| Liquidity throttle | relief ≤2 d post-payday (+.04+.06·sens); stress from day 7 over 7 d (−.10−.15·sens); cash factor .85+.15·(avail/max($75,baseline)); burden max(.88, 1−.08·ratio); clamp [.70, 1.10]; count factor (.5+.5·liq)²; amount factor 1→.85 over liq .95→.70 | CHOICE | consumption-smoothing literature | - |
| Biller preference / exploration / evolution | .55, retry limit 6, pick attempts 250; exploration .02/txn, propensity Beta(1.6, 9.5), bursts .487/yr for 3-9 d; merchant add .35 / drop .10 per month (max 30, was 40); contacts add .08 / drop .03 (max 20) | CHOICE | - | - (cadence is monthly: the evolver's only hook is a month boundary) |

### L-2b. Merchant selection: reach vs volume (`merchant-selection-2026-08`)

`Record.weight` is a volume weight and was the sampling law for
favourite-set membership (a graph edge). With `favK ~ U[8,30]` draws,
`P(card → merchant) = 1 − (1 − w)^favK`. At the owner's 8,000-person /
20-year run the top merchant's ~5% weight reached 51% of 68,618 cards, the
monthly evolver took it to 85%, and `P(two random cards share a merchant)`
was 1.000. The count was right (570 base + 563 churn births = 1,133 records
≈ 712 per 10,000 people).

| Parameter | PL value | Class | Anchor & source | Status |
|---|---|---|---|---|
| Merchants per 10,000 population | 250 floor + 120/10k core + 400/10k tail ⇒ 712/10k at pop 8,000, 520/10k at pop ≥ 20,875 | MEASUREMENT | Census CBP 2022 `cbp22us.txt`: 8,298,562 establishments / 334,017,321 = 248.4 per 10k (CBP 2023 248.25); consumer-facing (NAICS 44-45 + 72 + 81 + 71) 2,778,542 = 83.2. Nilson YE2024: 34M card-accepting locations / 340,110,988 = 999.7 per 10k (press release; report paywalled) [Certain for CBP] | CONFORMS as a band at every population |
| Top-1 merchant card reach | 0.12 target, 0.25 ceiling (`kTargetTop1Reach`, `kMaxTop1Reach`) | CHOICE | No source reports per-card reach (Krumme et al.: card side only). Numerator's household ladder (Great Value 86% in 12 mo to 6/30/24, McDonald's 87%, Amazon 83%) is brand granularity; a Record is one acceptance location (`place.hpp` gives non-online records one GeoArea; the exporter writes one `cf_Merchant_Location` centroid each; `entities/counterparties/merchants.hpp`). Band set by consequence: R-GCN's fixed `1/|N_i^r|` is "particularly problematic for nodes of high degree" (Schlichtkrull et al., ESWC 2018) | Class S, UNCITED at level. 0.830 → 0.129 (pop 300), 0.356 → 0.107 (pop 2,000); `test_card_merchant_graph` A/B, disarms red |
| Membership flattening exponent γ | bisection so `max π = target`; π = 1 − (1 − q)^k̄, q ∝ w^γ | DERIVED | - | draw-free, stateless; sub-gate E prints it and reds if it pins at a bound (a solved constant that saturates silently is a failure merchant-churn recorded twice) |
| Within-card visit rank law | Zipf α = 0.80 (`kVisitZipfAlpha`); pseudo-rank a draw-free hash of (person, merchant) | MEASUREMENT | Krumme, Llorente, Cebrian, Pentland, Moro, "The predictability of consumer visitation patterns", Scientific Reports 3:1645 (2013), Results + Fig. 1: α 0.80 (North American issuer, >50M accounts), 1.13 (European, 4M); top merchant ~13% (NA) / ~22% (EU) of visits; independent of set size [Certain] (2026-08-04) | CONFORMS. Was uniform (1/F = 5.3% at F=19); now 0.183, ratio 3.65 vs same-cards baseline (2.12-2.18 disarmed). Sub-gate F evaluates Σ r^−0.80 (r=1..64) = 7.067 ⇒ 0.1415 vs 0.13 |
| Favourite-set size | U[8,30]; `maxFavorites` 40 → 30 | CHOICE at level | Alessandretti, Sapiezynski, Sekara, Lehmann, Baronchelli, "Evidence for a conserved quantity in human mobility", Nature Human Behaviour 2:485-491 (2018): ~25 familiar locations, size conserved while membership turns over, ~40,000 people [Certain] (2026-08-04) | CITED for conservation. Old cap > seed max, and add .35 vs uniform drop .10 grew sets 19.1 → 37.8 over 240 months |
| Home→favourite distance | 1,206 mi mean, 4.96% within 50 mi → 3.8 mi, 97.3% (pop 500,000) | - | home-conditioned distance-decay pool | CLOSED; also closes the inverted shortcut (fraud card-present sits 0-11 mi from home: `within 50 mi ⇒ fraud` had ~7x lift at 0.31% base). Sub-gate H at both scales; disarm (ignore home) reds at 1,184 mi / 0.158 |
| Card-not-present share by number | was 0.118-0.138 era-flat; now 0.010 (1991) → 0.271 (2019) → 0.362 (2022) → 0.416 (2026) | - | Fed Payments Study, National Payment Volumes Detailed Data (CY 2021-2022): "In 2022, in-person payments were 63.8 percent of total GP card payments by number" ⇒ remote 36.2% [Certain] (2026-08-05). Shape from Census Quarterly Retail E-Commerce Sales (from 1999) × 2.46 to hit 2022 | CITED for 2022 and shape; 2.46x and pre-1999 points class S. In-person is still ~2:1; CNP exceeds e-commerce's 16.9% of retail by adding phone/mail, recurring and in-app. Rebuilt monthly |
| Distance-decay pool memory | dense, held twice: 26.75 MB at pop 500,000, O(A·M) → 0.481 MB, O(M + A·k), 56x less | - | the within-area factor is home-independent (one vector stored 71 times) | exact within areas; the inter-area cutoff drops 2.7e-08 of mass (sub-gate H). Unblocks `geo_data.hpp` past ~300 areas (was 197 MB / 1.24 GB) |
| National top-1 volume share | emergent: 0.64% at n=570, 0.48% at n=26,000 | MEASUREMENT | NRF 2025 Walmart $568.70B / Census MARTS Dec-2024 $8,544,433M = 6.66% of retail+food; ~4-5% of the Payments Study's $11.50T card value [Certain] | Deviates by construction, correctly: 6.66% is a brand number (0.86 penetration × ~8% within-household ≈ 6.9%); an outlet at 0.12 reach tops out near 2%. Pinning both reach and volume is over-determined: an earlier design pushed within-card top-1 to 31%, breaking Krumme |

### L-3. Amount catalog (LN = lognormal(median, σ); Γ(shape, scale)+add)

Draws are calibration-year (2019) dollars; Part III scale classes apply.

| Channel | PL model | Class | Anchor & source | Status |
|---|---|---|---|---|
| Salary (monthly) | LN($4,500, .55) floor $50, ×12 | MEASUREMENT | BLS OEWS May 2024 median $49,500 = $4,125/mo [Certain]; CPS full-time ~$1,192/wk ≈ $5,165/mo [Likely]; PL between, fits a salaried persona | CONFORMS |
| Rent | Γ(2, 700)+$100 (mean $1,500) | MEASUREMENT | ACS B25064 median gross rent ~$1,406 (2023), ~$1,487 (2024, +5.8% per CBPP) [Derived]. Each renter is a sole tenant paying a household rent, so the household axis governs (not DCPC's $824 per transaction) | CONFORMS |
| P2P | LN($55, .80), mean ≈ $75.7 | MEASUREMENT | Diary Table 8 mobile-app average $71.9; "A person" $181 [Certain]; app-like comparator | CONFORMS |
| Bill | Γ(2, 55)+$15 (mean $125) | MEASUREMENT | Fed Diary bills | UNCITED |
| ATM | LN($80, .30) floor $20 | MEASUREMENT | Diary counts cash payments, not withdrawals; holdings (avg $66.7, conditional median $46, Table 14) fit sub-$100 withdrawals [Likely] | UNCITED (cash-withdrawal supplement) |
| Subscription (fallback) | LN($15, .40) floor $5 | MEASUREMENT | L-6 | CONFORMS |
| Client ACH credit | LN($1,500, .75) floor $50 | MEASUREMENT | freelance invoice data | UNCITED |
| External unknown / Self transfer / Card settlement / Platform payout / Owner draw / Investment inflow | LN($120,.95) f$5 / LN($250,.80) f$10 / LN($650,.60) f$20 / LN($400,.65) f$10 / LN($2,500,.80) f$100 / LN($5,000,1.0) f$100 | CHOICE | - | - |
| Cash deposit (takings/tips) | per persona, L-10 (LN, $10-rounded, floor $100) | MEASUREMENT-adjacent | FinCEN CTR volume; Fed Diary; Yale Budget Lab; IRS ATG | L-10 |

Merchant tickets LN(median, σ): grocery 50/.55, fuel 32/.35, restaurant
28/.60, pharmacy 25/.65, ecommerce 85/.70, retailOther 45/.75, utilities
120/.40, telecom 75/.30, insurance 150/.35, education 200/.60; default
45/.70. Against S-DCPC Table 13 averages: utilities $132.4 vs PL $130,
communications $78.7 vs $78.5, education $250 vs $239, grocery $52.2 vs
$58.2, restaurant+fast-food $27.8 vs $33.5, stores $82.0 vs the
ecommerce/retailOther blend, gas $32.8 vs fuel $34.0: CONFORMS.

Rent level: constant within a lease year, stepping up each anniversary by 1
+ 2.5% inflation + real raise N(2.0%, 1.5%) floor −1% (one keyed draw per
lease year), ≈ 4.5%/yr nominal. A move re-draws the base; leases are
backdated at world creation. Comparator: CPI rent of primary residence
(UNCITED).

### L-4. Income & employment

| Parameter | PL value | Class | Anchor & source | Status |
|---|---|---|---|---|
| Employment probability | effective: salaried .98, student .40, retiree .02, freelancer .08, smallBiz .04, HNW .12. Fit target `paidFraction` .65 = the weighted mean under L-1 shares (.6508), so scale ≈ 1.0 and nothing clamps | MEASUREMENT | BLS employment-population ratios; NCES Condition of Education (40% of full-time undergraduates employed); BLS USDL-25-0563 (student LFP 44.6%, Oct 2024) [Certain]. Retirees are fully retired (L-4b income); the BLS 65+ ratio (~19%) lives in the salaried persona | CONFORMS |
| Pay cadences | weekly .20 / biweekly .55 / semimonthly .15 / monthly .10, one draw per employer | CHOICE | BLS CES Feb 2023 establishment shares: biweekly 43.0%, weekly 27.0%, semimonthly 19.8%, monthly ~10%; 72.9% of 1,000+ employee establishments biweekly [Certain]. PL needs worker-weighted shares | DEVIATES-BY-CHOICE |
| Employer roster and pick | `sizes::employerLaw`, 17 classes, N_c = clamp(round(P x 0.74 x m_c), 1, F_c); every job and switch picks through it (`growth::pickSized`, `pickSizedDifferent`) | MEASUREMENT (tables) + CHOICE (thinning) | Census SUSB 2022, BLS QCEW 2022 [Certain]; Census of Governments 2022 [Likely]. Authority rows: counterparty-sizes-2026-09 | CONFORMS on tables |
| Payday mechanics | Friday default (25% Thu↔Fri); semimonthly {15,31} (35% {1,15}); monthly ∈ {28,30,31}; roll to previous business day; lag 0-1 d; posts 06:00-12:00. Biweekly aligns to the anchor's fortnight parity (a phase, not a start bound) | MEASUREMENT | payroll conventions | CONFORMS |
| Job tenure | 1.5-4.0 y/job | MEASUREMENT | BLS median tenure 3.9 years (2024) [Likely]; PL churns faster | DEVIATES-BY-CHOICE (short windows need job changes) |
| Wage growth | real raise N(1.5%, 2.0%) floor −2% over the AWI index; switch bump N(+8%, 6%) floor −5% | MEASUREMENT | Atlanta Fed Wage Growth Tracker ~4-4.5% nominal [Likely]. The flat 2.5% inflation is retired (AWI is the path, Part III) | base CONFORMS; bump UNCITED |
| Floors/jitter | ≥$50/paycheck (wage-scaled); salary jitter LN σ.03 | CHOICE | - | - |

Residual: employed students draw the same LN($4,500,.55); a part-time tier
is registered.

### L-4b. Government benefits

| Parameter | PL value | Class | Anchor & source | Status |
|---|---|---|---|---|
| SSA retirement | retirees; eligibleP .87; LN($2,071, .30) floor $900/mo; paid on the 3 SSA Wednesday cohorts by birth day-of-month (1-10 / 11-20 / 21-31 → 0/1/2) | MEASUREMENT | ~90% of 65+ receive SS; average retired-worker benefit ≈ $1,907/mo (Dec 2024), ≈ $1,976 after the Jan 2025 COLA; PL mean ≈ $2,166, a few % high [Likely]. SSA schedule [Certain] | UNCITED (verify SSA Monthly Statistical Snapshot) |
| SSDI | non-retiree/non-student; eligibleP .04; LN($1,630, .25) floor $500/mo | MEASUREMENT | average ≈ $1,540/mo (PL ≈ $1,682, ~9% high); 7.2-7.4M beneficiaries ≈ 3-4% of working age [Likely] | UNCITED |

Benefits end at death; survivor benefits are registered.

### L-5. Housing

Lease tenure 3-8 y (mean 5.5 ⇒ ~18%/yr turnover); a move gets a new
landlord and fresh base rent (jitter LN σ.05). Anchor: CPS ASEC renter mover
rate 21.7% (2017, a then-low); BLS continuing-tenant work (new-tenant share
~15%) ⇒ ~15-22%/yr, stays ~4.5-7 years [Certain rates, Derived
implication]: CONFORMS.

Landlords (counterparty-sizes-2026-09): `sizes::landlordLaw` over the seven
RHFS 2021 property-size columns (CRS R47332 Tables 1 and 3 [Certain]) plus
the NMHC 2024 Top-50 carved out of 150+ [Likely], N_c = clamp(round(P x 0.35
x m_c), 1, F_c); type from each class's unit mix, renter-weighted .433 /
.138 / .429 individual / small LLC / corporate (the old .38 / .15 / .47 is
retired).

Renter share `rent::Rules::paidFraction` = .35 vs ACS ~.34-.36 of
households [Likely]. Registered axis mismatch: PL counts people, each on own
lease; at .125-.15 renter households per person, PL has ~2.3-2.8× as many
rent payers as renter households (an owner decision; it moves every rent
row). Effective persona shares: student ≈ .33, retiree ≈ .12, freelancer ≈
.38, smallBusiness ≈ .23, HNW ≈ .07, salaried ≈ .41. PL rent outflow ≈ .35 ×
$1,500 = $525/person-month; the real figure is ~$186-223 (the old .35 ×
$1,487 ≈ $520 check multiplied a household share by a household rent), so
PL runs ~2.4-2.8× high.

Known simplification: `RentRoll.isHomeowner` is unwired, so a mortgage
payer can rent too (≈ .35 × mortgage adoption ≈ 16% of people).

### L-6. Recurring debits

| Routine | PL value | Class | Anchor & source | Status |
|---|---|---|---|---|
| Subscriptions | 4-8 candidates, 55% become debits; 18-point pool $6.99-$99.99; day U[1,28] | MEASUREMENT | Named comparator Bango 2025 (5.2 active, $69/mo); PL 2.2-4.4 at ~$27 ⇒ $60-119 [Derived]. Surveys vary (Self Financial 2026: 3.4/$35; Whop-style trackers 8.2/$219) | CONFORMS |
| ATM | 88% users; 1-6/mo; LN($80,.30) floor $20 | MEASUREMENT | S-DCPC Table 5: 82.6% used cash in 30 days (2024) | borderline; amount UNCITED |
| Internal transfers | 55% active; 1-3/mo; LN($120,.75) floor $10; 25% round from {25…2000} | CHOICE | - | - |

### L-7. Credit cards

| Parameter | PL value | Class | Anchor & source | Status |
|---|---|---|---|---|
| Ownership / share / limits | per persona (L-1); cardP ≈ .826 | MEASUREMENT | S-DCPC Table 3: 82.3% [Certain] | CONFORMS (limits/balances UNCITED) |
| Grace period | 25 days | MEASUREMENT | CARD Act 15 U.S.C. 1666b; Reg Z 12 CFR 1026.5(b)(2)(ii): statement ≥21 days before due; issuers 21-25 [Certain] | CONFORMS |
| Minimum payment | max(2%, $25), floor price-scaled | MEASUREMENT | issuer norms | UNCITED |
| Late fee | $32 (price-scaled) | MEASUREMENT | Reg Z safe harbors 12 CFR 1026.52(b): ~$32 first / $43 subsequent; the $8 rule never took effect, vacated Apr 15 2025 (Chamber of Commerce v. CFPB, N.D. Tex.); CFPB average $32 (2022) [Certain] | CONFORMS ($43 tier unmodeled) |
| Autopay | full .40 / minimum .10 / manual .50 per card; autopay bypasses the manual mixture | MEASUREMENT-adjacent | ~.40-.50 of accounts on autopay [Guessing]; the .40/.10 split is CHOICE | UNCITED, falsifiable |
| Payment mixture (manual only) | full .35 / partial .30 / minimum .25 / miss .10; partial Beta(2,5) of statement. Effective: full .575 / minimum .225 / partial .15 / miss .05 | MEASUREMENT | S-DCPC Table 4: 42.1% of adopters carried a balance last month ⇒ ~58% paid in full; 45.4% at some point in 12 months; mean unpaid $3,078 per adopter, $6,794 per revolver [Certain] | CONFORMS |
| Miss share | effective .05 | MEASUREMENT | PL's is a per-statement flow; ~3-4% delinquency is a 30+ dpd stock; with ~one-cycle cure they coincide. Never compare the manual-only .10 | CONFORMS-adjacent |
| Payment timing | cutoff 17:00 on the resolved due date; autopay 12:00; manual late p .08, 1-20 d after; fee 10:00 next day. `test_card_payment_timing` closes the old +12 h autopay-late defect, weekends included | MEASUREMENT-adjacent | issuer delinquency curves | ordering CONFORMS; late p UNCITED |
| Disputes | refund p .006/purchase (1-14 d); chargeback p .001 (7-45 d) | CHOICE | Visa legacy 0.9% (0.65% early warning); VAMP combined 1.5% eff. Apr 2026; Mastercard ECM 1.5%; e-commerce ~0.6-0.65% of transactions [Certain]. PL's 0.1% over all channels is directionally right. A refund is a post-purchase merchant credit; the comparator is return incidence per purchase, not e-commerce order returns (~15%+) | DEVIATES-BY-CHOICE; refund UNCITED |
| Cycle finalization | 32-day session lag | CHOICE (architecture) | - | - |

### L-8. Credit & obligation products

Adoption in the order student / retiree / freelancer / smallBiz / HNW /
salaried.

- Mortgage: .02/.55/.30/.55/.65/.55; LN($1,750, .55); late 4% (1-7 d),
  miss .5%, partial 1% (30-80%), cure 30%, cluster ×1.6, ≤6 cure cycles.
- Auto loan: .10/.20/.40/.45/.45/.45; 35% new; new LN($715,.30) / used
  LN($525,.35) floor $100; term new 68±6 / used 67±8 mo, clamp 24-84; late
  5% (1-10 d), miss 1%, partial 1.5%, cure 30%, cluster ×1.7, ≤4 cycles.
- Student loan: .85/.05/.20/.20/.10/.30; standard .65 (120 mo) / extended
  .20 (240) / IDR .15 (55% 240 else 300); grace 6 mo (65% deferred);
  LN($295,.55) floor $50; late 6% (1-14 d), miss 1.5%, partial 2%, cure
  25%, cluster ×1.8, ≤4 cycles.
- Insurance: auto .30/.85/.85/.90/.95/.92, home .05/.55/.30/.55/.70/.55,
  life .10/.55/.30/.45/.55/.55; mortgage⇒home .99, auto-loan⇒auto .997;
  premiums LN auto $225/.30 (floor 25), home $200/.30 (25), life $28/.40
  (5); claims auto 4.2%/y → LN($4,700,.80) floor $500, home 5.5%/y →
  LN($12,500,.80) floor $1,000.
- Tax: .05/.20/.65/.85/.50/.10; quarterly LN($1,250,.65) floor $100; refund
  65% LN($2,500,.55) / balance due 20% LN($1,100,.65).

Payments are origination-anchored, fixed-nominal (Part III class D); tax
scales at the due year. Each contract pays a provider drawn from a national
market-share table; tax pays the IRS (institutional-providers-2026-09).

| Row | Anchor & source | Status |
|---|---|---|
| Auto loan | Experian Q4 2025/Q1 2026: payment new $767/$770, used $537/$531; terms 69.5 / 67.7 mo; financed $43,925 / $27,070 [Certain]. PL ~$748 / ~$558 | CONFORMS (84-mo clamp cuts 73-84 mo ~30% of new, 85+ ~2%) |
| Mortgage payment | ACS 2024 median $1,521 all mortgaged, $2,225 for 2024 movers; MBA applications ~$2,067-2,127 (2025) [Certain]. PL median $1,750 / mean ~$2,036 | CONFORMS as a band |
| Student loan payment | Fed SHED: typical $200-299; 60% ≤$299; 45% owed nothing in the month (2025); secondary averages $336-503 [Certain]. PL median $295 / mean ~$343 | CONFORMS. Flag: IDR .15 vs ~a third in FSA [Likely]; expect ADJUST or CHOICE |
| Insurance premiums | 2026: auto full coverage $190-244/mo; home $1,824-2,543/yr (Philadelphia Fed $2,530 in 2023, ~$3,000 by 2026); term life ~$26-30/mo [Certain auto/home, Likely life]. PL ~$235/mo, ~$2,510/yr, $28 | CONFORM |
| Insurance claims | III/ISO: 5.3% of insured homes claimed (2023); average severity $18,311 (2022), >$17k over five years [Certain]. PL 5.5%, mean ≈ $17.2k | CONFORMS. Auto 4.2%/yr, mean ~$6.5k fit ~5-6 per 100 car-years, ~$5.7-6.6k [Likely]; verify |
| Tax | IRS: 64.1% (2024), ~63% (2025) refunded; average $3,167-3,170 (2025) [Certain]. PL .65; mean ≈ $2,908 (~8% under) | CONFORMS |
| Delinquency ladders | MBA NDS (30+ ~4%), NY Fed CCP transitions, FSA stats; per-payment lateness does not map onto 30/60/90 buckets | UNCITED |

### L-9. Family transfers

| Flow | PL value | Anchor & source | Status |
|---|---|---|---|
| Spousal | 60% separate accounts = couples routing transfers between individually owned accounts (no joint accounts exist; means "some money separate"; `separateAccountsP`, `family/spouse.cpp`); 2-6 txns/mo; breadwinner-directional 65%; LN($85, .90) | Bankrate 2026: 62% keep some money separate (36% hybrid + 26% fully); Census SIPP 2023: 23% no joint account [Certain]; comparator 62% | CONFORMS (definition is load-bearing) |
| Allowances | weekly 70% (else monthly); Pareto($8, 1.8), mean ≈ $18/wk | Greenlight ($14.72/wk 2023, $13.15 2025); Till Financial 2025-26 (avg $17, median $10); AICPA 2019 (~$30, self-reported, teen-heavy) [Certain] | CONFORMS |
| Tuition | 65% of students, one plan per run: 4-5 installments 30 days apart from 0-9 days after the window's first month start. Plan total LN($7,712, .35) (calibration $), split evenly with 3% noise (≈ $1,540-1,930 each). A school payment plan (owner decision 2026-09-26, tuition-payee-2026-09): one parent's local account pays the student's school, an education record open on every date, home area first (`family/tuition.cpp`, `family/schools.cpp`) | CFPB, *Tuition Payment Plans in Higher Education* (2023-09-14): payments within one term; ~4 million students each term; 87% of ~450 institutions offer plans [Certain]. College Board 2025-26 per year: public in-state $11,950 / out-of-state $31,880 / private $45,000; net public $2,300 / private $16,910 [Certain]; per term $5,975 / $15,940 / $22,500, net $1,150 / $8,455 [Derived] | Payee CONFORMS. Amount re-opened, UNCITED (the old CONFORMS misread the axis; see Superseded claims): $7,712 sits in the per-term published band, above public net; the axis is the owner's call. 65% share and installment count UNCITED; one plan per run registered |
| Parental support | 35% of eligible; Pareto(xm=$25, α=2.4)/txn | Fed SHED; AARP (incidence only) | UNCITED (expect CHOICE) |
| Sibling / grandparent / parent gifts | 15% pairs, 18%/mo, LN($120,.90) · 8%, LN($150,.70) · 12%, Pareto($75,1.6) | - | UNCITED (expect CHOICE) |
| Inheritance | death-caused estates only; LN($25,000, σ1.0) interim | Fed SCF transfer / net-worth tables | UNCITED; SCF re-derivation registered |
| External recipients | 18% leave the bank | CHOICE | - |

Gifts stop when either party is dead; external family deaths are unmodeled.

### L-10. Business / freelancer revenue

- Freelancer: clients .88, 2-5, 1-4/mo, LN($1,400,.70); platforms .42,
  1-2, 1-4, LN($425,.60); owner draw .70, 1-2, LN($1,800,.75); cash takings
  .25, 1-4/mo, LN($450,.60) $10-rounded floor $100; quiet month .12.
- Small business: clients .55, 2-6, 0-3, LN($2,600,.75); platforms .22,
  1-2, 0-3, LN($950,.70); card settlements .74, 4-12/mo, LN($680,.55);
  owner draw .86, 1-2, LN($3,400,.70); cash takings .40, 4-10/mo,
  LN($2,800,.72) $10-rounded floor $100; quiet month .06.
- High net worth: owner draw .55, 1-2, LN($6,000,.65); investment .72,
  1-3, LN($12,000, 1.0); quiet .02.
- Retiree: draw-like .33, 1, LN($1,100,.50); investment .50, 1-2,
  LN($400,.65); quiet .05.
- Salaried tips: .03, 2-4/mo, LN($200,.55). Student tips: .16, 1-3/mo,
  LN($140,.55).

Revenue stops at death. Cash-handling split (recalled named sources,
awaiting the owner's retrieval pass):

| Persona (pop share) | Cash-active | Basis |
|---|---|---|
| smallBusiness (.06) | .40 | IRS *Cash Intensive Businesses ATG* sectors (restaurants/bars, convenience & grocery, salons, laundromats, car washes, taxis, vending, parking, scrap) [Certain it exists]; SUSB/CBP cash-heavy core ≈ 25-30% [Derived, Likely]; Square "Making Change" in-person cash ~37% (2015) → ~30% (2019) → high-teens/low-20s post-2020 [Likely]. Above the core: a Main-Street storefront archetype (74% card-settlement active) |
| freelancer (.10) | .25 | Fed SHED gig (~16%/month); Fed EIWA 2015: cash dominates offline informal work [Likely] |
| salaried (.60) | .03 | Yale Budget Lab, "Who Are Tipped Workers?" (Jun 2024): ~4.0M tipped ≈ 2.5% of employment [Certain]; card tipping carries most tips [Likely] |
| student (.12) | .16 | student employment .40 × ~.4 in tipped jobs [Derived, Likely] |
| retiree (.10) / HNW (.02) | 0 | CHOICE: 65+ are the heaviest cash users, not depositors of takings [Likely]; HNW have no takings channel |

Context: cash ≈ 14-16% of payment count, ~6-7 cash payments/person-month,
82.6% used cash in 30 days [Certain ballpark]. CTR check [Derived]:
smallBusiness 600/10k × .40 = 240 depositors × ~7/mo; P(> $10,000 |
LN($2,800,.72)) ≈ 3.9% ⇒ ≈129 CTRs pre-attrition, 117 measured (≈ .94),
vs FinCEN ≈128. CTR:SAR runs far above 4.4 because SARs are ring-driven
and sparse.

### L-11. Population scaffolding

Accounts per person 1 + Binomial(2, .25), mean 1.5, max 3: checking-like
only (`entity::account` has no savings type). S-DCPC Table 1: bank account
95.4%, checking 94.7%, savings 77.2%; no official accounts-per-person count.
CONFORMS under that scope.

Merchants core 120/10k + tail 400/10k; per 10k (floor): platforms 2 (2),
processors 1 (2), owner businesses 200 (25), brokerages 40 (5), clients 250
(25, 2% internal-bank). Re-classed CHOICE: no per-10k-customer source
exists.

Employers and landlords are size laws, not densities
(counterparty-sizes-2026-09): the retired 25 per 10k employers (floor 5, 4%
internal-bank) and 12 per 10k landlords (floor 3) became N_c = clamp(round(P
x s x m_c), 1, F_c) over SUSB 2022 plus government (employers, all
external) and RHFS 2021 plus the NMHC Top-50 (landlords): 89,231 employers
and 66,658 landlords at 200,000 people (was 500 and 240); 200,556 and
155,581 at 500,000.

SSA payment cohort from the birth day-of-month (1-10 / 11-20 / 21-31 →
0/1/2).

# Part III: Macro / era model

The era series are constexpr tables in `synth/econ/era_data.hpp` (the
`data/econ/` files are retired under the minimize-repo-data-files
directive); provenance and refresh contract: `docs/era_data_provenance.md`.
Generation reads them, so every refresh moves the model.

### M-1. The embedded series (1990-2024)

| Series | Values & axis | Class | Source & verification | Status |
|---|---|---|---|---|
| CPI-U annual averages | 1990 130.658 → 2019 255.657 → 2020 258.811 → 2024 313.689; 2019/1991 ≈ 1.877; 2009 the only annual deflation (−0.4%) | MEASUREMENT | FRED CPIAUCNS (BLS CUUR0000SA0 mirror), read 2026-07-24; the annual average is the mean of 12 NSA months, recomputed to the third decimal | VERIFIED EXACT |
| SSA Average Wage Index | 1991 $21,811.60 → 2019 $54,099.99 (≈2.48×) → 2024 $69,846.57; 2009 −1.51% vs 2008 | MEASUREMENT | ssa.gov/oact/cola/awiseries.html, read 2026-07-24; all 31 values 1990-2020 matched | VERIFIED EXACT |
| Nominal per-capita PCE | $15,225 (1990) → $43,682 (2019) → $42,886 (2020) → $58,501 (2024); 2019/1990 ≈ 2.87 | MEASUREMENT | FRED A794RC0A052NBEA (BEA NIPA), vintage 2026-04-09 | VERIFIED EXACT |
| Population | BEA midperiod: 250,181k (1990) → 330,513k (2019) → 331,840k (2020) → 340,095k (2024), strictly increasing | MEASUREMENT | FRED B230RC0A052NBEA, vintage 2026-02-20. The PCE denominator, not Census July-1 (<0.3% apart) | VERIFIED EXACT |
| U-3 unemployment, annual average | 5.6% (1990), 7.5% (1992), 9.3%/9.6% (2009/2010), 3.7% (2019), 8.1% (2020), 4.0% (2024); not monthly peaks (7.8% 1992-06, 10.0% 2009-10, 14.7% 2020-04) | MEASUREMENT | BLS LNS14000000; FRED UNRATENSA monthly means within 0.1pp (the official figure is a ratio of annual averages) | transcribed + cross-checked |
| NBER recession months per year | 1990:5, 1991:3, 2001:8, 2008:12, 2009:6, 2020:2 (sums 8/8/18/2); months strictly after the peak through the trough | MEASUREMENT | NBER dating [Certain on dates; counting convention PL's] | CONFORMS |
| Mortality qx | exact SSA period life table for 2023 (Actuarial Table 4C6, 2026 Trustees Report): ages 0-119, male/female qx to six decimals, equal from 109 | MEASUREMENT | ssa.gov/oact/STATS/table4c6.html, read 2026-07-24; survival 65→94 = 8,320/79,084 ≈ 10.5%, 22→51 = 90,659/98,458 ≈ 92.1% | VERIFIED EXACT (24-pivot approximation retired) |
| Funeral cost anchors | NFDA median adult funeral: 1991 ~$3,742 → 2019 $7,640 (2021 $7,848; cremation $6,970) | MEASUREMENT | NFDA General Price List surveys [Likely] | UNCITED (owner spot-check) |

2025 cannot be pinned as of 2026-07: AWI 2025 publishes ~2026-10, and the
October 2025 CPI release and CPS survey were cancelled (federal shutdown). A
tripwire in `test_app_options` flips when the 2025 row lands.

### M-2. Calibration year and the level primitives

| Item | PL value | Class | Source | Status |
|---|---|---|---|---|
| Calibration year | 2019 (`kCalibrationYear`); CPI 255.657, AWI $54,099.99 | CHOICE (owner-approved 2026-07-24) | A provenance fact of the data (constants measured ~2015-2024, 2019-denominated: last full canonical-window and pre-COVID year), never "the present", coverage tail or wall-clock; changes only with its constants. Alternatives in `docs/era_data_provenance.md` | ADOPTED |
| priceScale(y) / wageScale(y) | CPI-U(y)/CPI-U(2019), AWI(y)/AWI(2019); 1.0 at 2019 | MEASUREMENT | M-1 | - |
| pceScale(y) / realPceLevel(y) | pceScale = per-capita PCE over 2019; realPceLevel = pceScale/priceScale (≈0.67 at 1991, 1.0 at 2019) with its dips (1991, 2008-09, 2020 collapse, 2021 rebound); 2001 has no per-capita dip | MEASUREMENT + CHOICE (level definition) | BEA A794RC | - |
| Freeze-and-declare | outside 1990-2024 every scale holds the nearest covered year, with one stderr notice; never extrapolated, no new CLI | MEASUREMENT (code fact) | `test_app_options` | - |

### M-3. Nominal-scale wiring classes

Wiring: draw → × scale → (denomination re-snap) → roundMoney → emit. RNG
streams, lanes and entity order are byte-identical to the pre-wiring
engine; only amounts move.

| Class | Scope | Index | Notes |
|---|---|---|---|
| W wage-indexed | salaries at pay date; freelancer/business revenue at month; SSA retirement + disability at deposit | wageScale(realization year) | Real SSA wage-indexes at award then CPI-COLAs; one index era-wide is declared |
| P price-indexed | rent; session tickets; subscriptions at debit date; premiums at billing, claims at claim date; family routines; ATM and internal transfers; card late fee and minimum floor at cycle date | priceScale(realization year) | Frozen per-contract subscription pricing rejected (would hold 1991 prices for decades) |
| P-stock | opening balances, overdraft fees, protection buffers, LOC limits, card limits; persona initialBalance/baselineCash | priceScale(window-start year), once | Declared: nominal balances lag late-window flows; ratios stay coherent |
| D origination-anchored | mortgage/auto/student payments | priceScale(origination year), fixed nominal | Originations before 1990 clamp to 1990. Tax uses priceScale(due year) |
| S statutory fixed-nominal | BSA/CTR $10,000 and structuring band (≤$9,950); ATM $20 and cash-deposit $10 lattices (scale then re-snap: a 1991 withdrawal is fewer $20s); $0.01 interest and $1 amount floors | none | 31 CFR 1010.311's threshold has been unindexed since the 1970s (in 1991 it bit at ~2× today's real value) |
| F fraud (continuous) | kFraud ($900) / kFraudCycle ($600); cardFraudSpend ($79 median, [$1,$5k]×scale); atoDrainAmount ($180, [$10,$85k]×scale); scamWireAmount | priceScale(event year) | Structuring excluded (S). The prevalence target is a count rate; funnel floors scale with amounts |
| F-lattice exception | cardTestCharge ($0.50/$1/$2/$5) and giftCardScamAmount rack denominations ($100/$200/$500 + the $10-step range) fixed-nominal | none | Owner-approved 2026-07-25: the round-amount signature is the typology; racks are physical like the $20 note. Load-bearing for gates (Superseded claims) |
| Screens | behavioral screens scale with what they screen (paycheck $50 → wageScale; revenue floors $20-250 → wageScale; ATM reserve $40-120, liquidity $75, card $25/$32 → priceScale); statutory/de-minimis stay fixed | - | - |

The flat `.025 annualInflation` in SalaryGrowthRules and RentGrowthRules is
retired. The seeded `salary_real_raise` / `rent_real_raise` lanes stay as
progression on top (μ .015 / .020); acceptance bands allow that drift.

### M-4. Macro modulation: the real consumption level

| Item | PL value | Class | Source |
|---|---|---|---|
| Channel | real consumption scales only the discretionary session's count; amounts stay class P. The budget F = pL/(1−p) rides L, so fraud density stays proportional across eras | CHOICE (owner-adopted 2026-07-26) | Fed Payments Study per-capita noncash counts |
| Budget semantics | window budget at the calibration level; realized volume = target × realPceLevel(year): 2019 reproduces today, 1991 runs ~0.67×; one lookup per day frame, no draws, lanes or CLI | INVARIANT + CHOICE | M-3 consistency |
| Scope | discretionary session only (wages/revenue/benefits ride AWI; contractual flows carry their price level); ATM cadence, cash-vs-card mix and gift cadences declared era-flat | CHOICE | cash-share era model registered |
| Unemployment | not modeled (PCE carries downturns annually); separation spells and within-year NBER shading registered | CHOICE | BLS U-3; NBER |
| COVID / EIP | 2020 collapse and 2021 rebound via realPceLevel; EIPs (CARES Apr 2020 $1,200/adult; Dec 2020-Jan 2021 $600; ARPA Mar 2021 $1,400) registered as a class-S table; the card-fraud window ends 2020-01-01 | CHOICE | CARES / CAA 2021 / ARPA |
| Harness drain (analysis) | the ~27% deflated year-over-year drain in 300-person second-year legs is a small-world budget artifact (income under-provision → falling balances → liquidity suppression) predating the macro rounds; gates use cross-era ratios of same-position years | MEASUREMENT (harness) | `test_econ_wiring` diagnostics |

### M-5. Persona timeline, mortality and membership

| Item | PL value | Class | Source & anchor |
|---|---|---|---|
| Full retirement age | `fraMonths(birthYear)`: 65y through 1937; +2 months/year 1938-1942; 66y 1943-1954; +2 months/year 1955-1959; 67y from 1960; pinned test-exact. SSA's "born January 1" quirk is simplified away | MEASUREMENT | Social Security Amendments of 1983; ssa.gov chart |
| Claiming-age mixture | .30 at 62; .10 uniform [63y, FRA); .45 at FRA; .05 uniform (FRA, 70y); .10 at 70; 0-60 day jitter; one distribution era-wide | CHOICE | SSA Annual Statistical Supplement (OASI claiming ages). Claiming at 62 was far more common in the early 1990s; per-cohort shares registered |
| Student work-start | ages 19-28, mass at 22-26 (.05/.05/.08/.20/.20/.15/.10/.07/.05/.05 from 19), from birth date; then salaried .85 / freelancer .15 | CHOICE | NCES completion ages; L-4's .40 is the during-study rate |
| Small-business churn | memoryless exponential, median 5 years, clamp [30 d, 40 y] (backdating-invariant); then salaried .70 / freelancer .30; retirement dominates | TYPOLOGY on MEASUREMENT (~50% five-year survival) | BLS Business Employment Dynamics |
| Seed-consistency clamps | `personaAt(simStart) == seed type` pinned; past dates settle forward; in-window dates stand | INVARIANT | the seed is the state at sim start |
| highNetWorth exemption | no transitions (retired-HNW is CEX work) | CHOICE | revisit at the CEX round |
| Retirement spending step | ~−12% (`kRetiredSpendScale` .88) from claiming; payday sensitivity re-anchors to SSA days. Only working-seed archetypes retiring in-window; seed retirees and HNW none (retiree archetype already rate ×0.6 / amount ×0.9) | CHOICE on MEASUREMENT | Aguiar-Hurst (JPE 2005); BLS CEX |
| Death dates | annual hazard walk over the SSA 2023 table (sex-specific, log-linear interpolation), inverted at one uniform per person on `{"mortality", personId}`; from birth dates; mass beyond 120 dies at the cap. Latent sex 50/50 (the table's ~2.7-year gap kept) | MEASUREMENT + CHOICE | M-1. Simplifications: one period table, no persona/SES differential, deaths uniform within the year |
| Alive at start | the walk starts at current fractional age; death strictly after sim start | INVARIANT | mortality analog of `personaAt(simStart)==seed` |
| Death stops | salary ends at min(retirement, death); SSA/disability end at death; revenue stops; the session skips dead days; ATM, internal transfers, rent and gifts stop. Contractual flows (subscriptions, premiums, loan/tax, card cycles) post against the estate until account closure | CHOICE | estate practice |
| Estates and funerals | estate at death+30-90 days when heirs exist. Funeral: one bill-channel payment from the decedent's account at death+3-10 days, LN median $6,300 calibration dollars = NFDA 2019 GPL blend (viewing+burial $7,640; cremation with viewing $5,150; ~55% cremation), σ .40, floor $1,000, CPI-realized at death year | MEASUREMENT + CHOICE (σ/floor) | NFDA 2019 GPL; NFDA/CANA cremation rate (owner spot-check) |
| Membership interval | [joinTs, closeTs): window start for the seed roster, a drawn day for joiners; closeTs = death + 120-day settlement (contains funeral death+3-11 d and estate death+30-90 d). The standard exporter filters both owned endpoints; Round 7 applies it in fraud victim selection and the streamed card graph (authorized scams need alive; card/ATO only the post-death tail). The raw ledger stays full | CHOICE (owner: the population persists and dies) | deposit closure norms |
| Join-cohort sizing | joinerCount = population × Σ over window days of r(year(day)) / 365.2425, r(y) = pop(y+1)/pop(y) − 1 (BEA), rate-clamped at coverage edges. Joiners are the last K ids (seed draws byte-identical); one draw each on `{"join-cohort", personId}`, inverse-CDF over day weights ∝ r; dob, timeline and lifespan anchor at the join date | MEASUREMENT + CHOICE | tracks resident growth; per-bank acquisition registered; flat 2%/yr retired |
| Account closure | subscriptions, premiums, loan/tax stop at closeTs via emission filters after draws burn; card servicing stops at the last statement close ≥50 days before closeTs (grace 25 + late tail 20 + fee morning); insurance claims stop at death | CHOICE | card ToS cycle norms |
| Rings never recruit the dead | each ring carries the minimum death epoch of its fraud + mule participants; bursts and the camouflage window clamp to it minus a 22-day guard. Victims exempt (deceased-account fraud is real), as is the solo/unauthorized rail | TYPOLOGY + CHOICE | deceased-identity advisories |

Declared inconsistency: aml / aml_txn_edges Customer onboarding stays the
synthetic backdated date, while `customer.csv` and the card_fraud Party
table export joinTs; alignment registered.

# Part IV: Card-fraud use case (exporter contract)

Presentation for the TigerGraph TF_GNN_v3 target; the settled corpus is
unchanged. Feature safety: `docs/card_fraud_feature_contract.md`.

| Item | PL value | Class | Notes |
|---|---|---|---|
| Card view | {card_purchase, merchant}; merchant-channel rows read as debit-card; impostor push excluded | CHOICE | standard for transaction-fraud graphs |
| Card attribution | a card-registry source → that credit card (≤1 per person); else the account's derived debit card (unauthorized positives use only this); parsing the `C`/`D` tag as a feature is prohibited | CHOICE + KNOWN GAP | credit-fraud servicing must move into `CardCycleDriver`; expiry, replacement, reissue, multiple cards unmodeled |
| Identifier scheme | C/D/M = role.bank.number of the Key; P&lt;person&gt;; T&lt;row_seq&gt;; canonical Party, Merchant and IP ids; Device ids opaque pseudonyms in one fixed-width `D` namespace (no role by prefix, width or range) | CHOICE | categorical, not ordinal or a security boundary |
| Withheld entity labels | `Card.is_fraud`, `Party.is_fraud`, `Device.is_blocked`, `IP.is_blocked` are full-window verdicts, rendered 0; columns kept because the TigerGraph loader maps columns by position. Verdicts in `card_fraud."cf_Ground_Truth_Label"` (entity_type, entity_id, label): positives only, no edge, not loaded. The export has `kTableCount` = 43 tables (`exporter/card_fraud/schema.hpp`; bls-citation-2026-07) | CHOICE (owner ruling) | `Payment_Transaction.is_fraud` is the target, never input; delayed label availability still needed |
| use_chip / error | `use_chip` causal since Round 8 (use-chip-causal-2026-07): "Online Transaction" ⟺ a geography-free destination (catalog `Footprint::online` or a non-catalog remote biller); physical outlets split Chip/Swipe by the dated EMV mix (`chipShareBasisPoints`: 0 before 2012, .10 at 2015, .65 in 2019, frozen .90 outside), content-keyed on `kUseChipLane`. `error` (2.0%; Insufficient Balance .40 / Bad PIN .20 / Technical Glitch .20 / Bad Card Number .08 / Bad Expiration .05 / Bad CVV .05 / Bad Zipcode .02) stays a content-keyed FNV hash, mechanism-free | MEASUREMENT-adjacent (EMV [Likely], verify vs EMVCo) + CHOICE | The value sets come from a third-party corpus's vocabulary; the strings stay because TF_GNN_v3 loads them (renaming is a schema change). Gate `test_card_use_chip`. Authorization-outcome model for `error` registered |
| Transaction-time sessions | `Transaction_Uses_Device(txn_id, device_id, edge_unix_time)`, `Transaction_Uses_IP(txn_id, ip_id, edge_unix_time)`: append-only stream-prefix edges; score from prior state before appending. `Has_Device` / `Has_IP` were header-only (populated since attacker-infra-2026-07) | INVARIANT | one edge per endpoint, test-pinned |
| Device/IP vertices | `cf_Device`, `cf_IP` = roster ∪ endpoints observed in card-view rows; roster flags withheld and quarantined. Static Party ownership was withheld for all endpoints (superseded by attacker-infra-2026-07) | INVARIANT | no missing-vertex shortcut |
| Legitimate credit-card sessions | the router owner map merges card-registry and deposit ownership (no liabilities in deposit slices), so credit purchases get the owner's session | MECHANISM REPAIR | closes "credit-card row has no session" |
| mer_cat | 10 categories stand in for MCC | DEVIATES-BY-CHOICE | for a TGN a coarse edge feature, not identity; MCC is its own round |
| Merchant geography | `Record.location` resolved through the build-fixed catalogue: one Has_City/Has_State/Has_Zip plus Assigned_To/Located_In chain per physical record; `online` and non-catalog destinations geography-free; City.population = `GeoArea.population`; no exporter geo hash or PII zip draw. Since merchant-coordinates-2026-07 `latitudeE6`/`longitudeE6` ship as degrees on `Merchant_Location`, `Zipcode`, `City` (area centroids) | CHOICE | Input is a 71-US-city + 15-international placeholder; row order defines `GeoAreaId`; no land area, so no true density. Target: Census Gazetteer + ACS |
| Party.gender | content-keyed even F/M split (not modeled) | CHOICE | mechanism-free |
| Party.created_at | Membership joinTs, as in the public customer table | CONFORMS | prohibition lifted |
| Is_Merchant | was header-only (superseded by merchant-ownership-2026-07, in attacker-infra-2026-07) | DEVIATES-BY-CHOICE | - |
| PII layer | Address/Phone/Email/ID(ssn)/Full_Name/DOB deduplicated over the roster; TF_GNN_v3 marks it demo only | CHOICE | - |

Anti-shortcut design: fraud card destinations come from the population
legitimate sessions use (card-present from the victim's distance-decayed
pool with the shared kernel; card-not-present from the online footprint by
popularity; a flat national draw would create a distance shortcut).
Attacker IPs come from `randomIpv4`, not TEST-NET-2. Devices share one
role-neutral namespace, and every observed endpoint is a vertex before an
edge references it. Since Round 8 `use_chip` reads selection's footprint
axis: a real CNP-majority correlation, not a shortcut.

Corrected 2026-08 (the 0.0000 pair recorded here was stale after the
join-cohort flip in `harness-world-shape-2026-07` and
`merchant-selection-2026-08`); `test_card_baselines` prints its levels:

| leg | fraud-only-merchant share | recall@P≥0.90 | band |
|---|---|---|---|
| pop 300 × 730 d (gate) | 0.0079 (1 of 41 touched) | 0.0079 | <0.10 / <0.25 |
| pop 900 × 1461 d | 0.0661 (6 of 153) | 0.0726 | <0.10 / <0.25 |

Both pass, unwidened. Best precision at any threshold is 1.0000 (lift
295.65x), not 0.1111: a fraud-only merchant scores 1.0, so the bound holds
because recall is tiny. The larger leg has 0.034 of headroom. Measured by
execution 2026-08; supersedes the 0.0000 pair.

# Superseded claims

Claims this document once made and later measured false. Each produced a
law; a reader with only the corrected row would re-propose the error.

| The claim | What falsified it | The law it produced |
|---|---|---|
| CTR fires at ≥ $10,000, any channel | eCFR 31 CFR 1010.311: strictly more than $10,000, currency only. Both defects were in code (`>= 10000.0`, no channel filter) | Verify statutory boundaries against the primary text |
| Fraud budget deviation is "a few bp" vs real card fraud | US card fraud is ~11 bp of value (Nilson), 17.6 bp debit (Fed); PL's 12 bp is a share of count | Never conflate prevalence axes; the count-vs-value oversampling factor is unknown, not "~100×" |
| Credit-card full-payment share .35 vs measured ~.58 → NONCONFORMING | .35 is the manual-payer mixture; autopay-full (.40) bypasses it; effective .575 | Conditional vs marginal; reversed with no code change |
| Student employment .12 is a third of the measured ~40% | The salary selector's fit target scale-clamped every persona but retirees to ~100%; the printed table was base weights, and effective student employment was 2.5× the measured rate | Read the selection function before characterising a distribution |
| Card fraud spend CONFORMS (PL mean $162 vs UK ~$167) | Compared a per-transaction mean to a per-case average; per case PL runs ~8× | Per txn vs per case; re-classed CHOICE (label density) |
| Miss share .05 vs delinquency ~3-4% | Per-statement flow vs point-in-time stock (30+ dpd) | Flow vs stock; map through cure duration first |
| Rent mean $850 CONFORMS (DCPC per-transaction $824) | Each PL renter is a sole tenant paying a full household rent, so ACS ~$1,487 is the comparator | A unit definition is a code-reading decision |
| Severity buys more gift cards for older victims | Grading the count put an 80-year-old at 13 × $500 = $6,500 in four hours from a checking account: mostly unfundable rows the ledger discards, a decline burst no FTC spotlight describes | The plan was wrong; severity grades the impostor amount only, the gift-card rail is ungraded |
| The gate harness measured the shipped population | `GateWorld` defaulted `withJoinCohort = false` while production always sets it; every card-arc behavioural band was calibrated on a world the generator never emits; `test_arch_equivalence` reported a settlement-side "SEMANTIC divergence" and diagnostics blamed the wrong layer for a round | An equivalence gate must pin the world shape it assumes; when a harness default freezes existing gates, every gate compared against production must opt out in the round it is introduced |
| Re-derive the world-shape witness from `joinerCount()` | Both legs would evaluate the same formula and never disagree | A precondition guarding a construction must measure it, never re-derive it |
| Per-year deflated card fraud amounts flat < 2.5× is "the only gate proving class F reaches the card rail" | The card view mixes two fixed-nominal lattices (M-3 F-lattice) with one CPI-scaled sampler, so deflating the combined mean contradicts the F-lattice exception (the ring-rail gate already excluded that rail for that reason). Also under-powered: 42-92 lognormal(σ1.2) draws give CV 19-28% per year; purging the resolvable lattice made the spread worse (2.69× → 3.11×), the signature of noise | A flatness gate over mixed era-scaled and fixed-nominal families measures mixture weights. Replaced by a cross-era deflated-quantile gate that sizes its band from realized n and fails as under-powered unless it excludes the fixed-nominal null. Prefer an effect you can see over a null you must resolve |
| Population 900 exercises solo and ring card spends | The gate's first run printed `ring 0` in both legs: `buildCompromisePlans` excludes ring participants and victims, so the rail is ring-free by design | Audit the justification against the gate's printed output; the ring counter stays as a documented tripwire |
| The TEST-NET attacker-IP claim is stale (grep found nothing) | The defect was written as integer octets: `Ipv4::pack(198, 51, 100, …)` | Grep the constructor, not the rendered literal |
| Fee and interest postings pay external business counterparties (card issuer, fee-collection and OD LOC keys, `Role::business` on `Bank::external`) | Every documented core types the contra as a bank-owned internal income GL (FLEXCUBE internal leaf GL of category Income, Temenos ledger categories with no customer, Fiserv DNA GL majors with blank customer number), and the card accounts the issuer key charged were themselves `Bank::internal` (bank-gl-2026-09) | Internal is not customer; the bank's own ledgers need their own role |
| Tuition is paid parent to student: "`tuition.cpp pickPayer` draws a parent, the payee is the student" (C4 definition, 2026-07-18) | The code already paid an education catalogue merchant, one per run (`fhelp::pickEducationMerchant`, then `EducationPayees::pick` from `c874d15`, May 2026); `pickPayer` names the payer (tuition-payee-2026-09) | A definition verified against code must name the line that sets the field |
| Tuition is LN($7,712, .35) per installment, ~$31-39k a year, and CONFORMS to public cost of attendance | `buildPlan` draws the lognormal once as the plan total over 4-5 installments, one plan per student per run (tuition-payee-2026-09) | Per row, per plan, per year: read the draw's axis off the code first |

# Open items

No known numeric contradiction remains; all were resolved by shipped
ADJUSTs or documented CHOICEs.

1. Citations to pull verbatim at the owner's verify pass:
   - General: eCFR section-text snapshots; FATF *Professional ML* (2018)
     page cites; a named Visa card-testing advisory; FinCEN structuring
     guidance and FFIEC manual pages; ISS/III auto claim
     frequency-severity; FSA portfolio IDR shares; BLS Employee Tenure 2024;
     OEWS May 2025; the current Atlanta Fed tracker; the Diary
     cash-withdrawal supplement (ATM amount row); BLS CPI-U and U-3 direct
     reads (bls.gov timed out; values are the standard annual averages).
   - Providers (institutional-providers-2026-09): FSA Data Center
     "Portfolio by Loan Servicer" (EdFinancial/CRI split); NAIC
     individual-life ranks 11-125; auto-lender ranks 6-25; mortgage servicer
     shares by count over all 1-4 family loans.
   - Cash split: IRS Cash Intensive Businesses ATG; Census SUSB/CBP; Square
     "Making Change"; Yale Budget Lab (Jun 2024); Fed SHED gig + EIWA 2015;
     BLS student-employment industry mix.
   - Scam/fraud: FTC gift-card Data Spotlights; FTC CSN payment-method mix;
     retailer $500 per-card caps; Reg Z / §1643 + network zero-liability;
     Security.org reimbursement share (p .85); UK Finance 2026 per-case
     averages; FTC CSN age-band incidence and loss tables.
   - Household: ACS B25064 + renter share; CPS renter turnover; NCES/BLS
     student employment; Greenlight/Till allowances; Diary Table 13 gas and
     Table 8 mobile-app; home premium ~$2,530/yr; III/ISO home severity; IRS
     2025 average refund; SSA Monthly Statistical Snapshot.
   - Card: S-DCPC Tables 3 and 4; an issuer autopay-enrollment source
     ({.40/.10/.50} is UNCITED); 12 CFR 1005.6/1005.11 (Reg E design); the
     EMVCo US chip-share series and the October 2015 liability-shift
     milestones (Round 8's EMV values are [Likely]).
   - Macro: NFDA GPL surveys; SCF intergenerational transfers; SSA OASI
     claiming-age tables.
2. Thin tail, expect CHOICE: L-9 family distributions (SHED gives
   incidence only); L-10 revenue profiles (platform studies non-comparable);
   L-11 densities (already CHOICE).
3. Funnel calibration: fit SAR p and alert-to-case to FinCEN FY2024 (4.7M
   SARs, 20.5M CTRs, fraud-typed ~52%) as sanity bands under oversampling;
   re-measure CTR liveness at each re-pin.
4. Owner-gated designs: ATO Reg E remediation (bank-remediation
   counterparty plus a new credit channel; reusing `cc_chargeback` corrupts
   F-4's reporting row); homeowner/renter overlap (wire `isHomeowner` into
   `RentRoll`); a student part-time wage tier. (The victim-session item is
   closed by victim-session-2026-07.)
5. Registered upgrades: per-cohort SSA claiming shares; historical mortality
   tables and SES gradients; survivor benefits; SCF-anchored estate sizes; a
   dedicated funeral channel (the funeral-home counterparty shipped in
   unknown-counterparty-2026-09); repeat founders; latent sex to PII with a
   measured ratio; a cash-share era model; separation spells and
   within-year NBER shading; the EIP statutory table; a monthly
   unemployment path; an MCC taxonomy; the Census Gazetteer geography round
   (blocks true density); compromise incidence by home-area population
   (Bettencourt β ≈ 1.15, only after a home-area-only baseline); elder scam
   sub-typologies (FinCEN / CFPB SAR calibration); a per-era scam
   payment-method mix. Card depth: effective expiry, renewal, compromise
   replacement/reissue, product migration, multiple instruments per
   lifetime, dated adoption/authentication/CNP mechanisms. Integrating
   unauthorized credit-card events into statement/payment/interest
   servicing is a separate P0 prerequisite. Closed: transaction-time
   device/IP edges (Round 7); card-present modality in `use_chip` (Round
   8). Still registered from Round 8: a modeled authorization outcome for
   `error`, a time-varying legitimate CNP share, the EMV citation pull.
6. Gaps blocking a benchmark claim: level calibration of card-fraud and
   scam prevalence to a named issuer-side series (Nilson / FTC), CNP share
   included; credit-card fraud integrated before lifecycle servicing;
   era/concept drift in compromise incidence, payment method,
   authentication and attacker infrastructure; delayed
   report/chargeback/verdict availability; an executable GSQL temporal
   feature query, training pipeline, temporal splits, baselines and
   evaluation harness. The arc measures separability and stability of an
   export, not a production detector; the README and online-GNN contract
   say so.

# AMENDMENT: victim-session-2026-07

Closes the victim-own-device clause once in the owner-gated designs open item; its
rationale ("routing the victim's device through `infra::Router` would
perturb legitimate routing") was false when written.

| The claim | What falsified it | The law it produced |
|---|---|---|
| Attaching the victim's device to an authorized-push row needs a new carrier (routing from the fraud planner would advance the sticky index and diverge the engines) | Every unauthorized row has `ringId = -1`, `source = victimAccount` and a customer-session channel, so `transactions::Factory::make` already called `routeDeviceFor`/`routeIpFor` for the victim on the plan's lane, and `unauthorized.cpp` overwrote it. The fix removes two lines, adds no draws | A deferral rationale rots like any claim: check the cost is not already paid, and whether the value is already on the row, before building a carrier |
| A non-advancing `devicesByPerson[person].front()` read is the safe attachment | It pins authorized rows to slot 0 while the victim's legitimate rows follow the sticky index, so "not this person's current device" becomes the label | The more explicit fix can open the shortcut; systematic difference from a person's own rows is a label |

Session semantics (normative): `card`/`ato` use the attacker's device+IP;
`giftCardScam`/`scamImpostor` the victim's routed session (the victim
operates). Gated in `tests/test_unauthorized_keyed.cpp` (card/ato half a
tripwire). TYPOLOGY; CONFORMS.

Round 6 finding: the `FD` device render was a deterministic label (the old
`OwnerType::ring` branch wrote literal `FD…`; person and legitimate-shared
layouts differed), stronger than the TEST-NET-2 shortcut (deterministic,
not 1-in-14M). Superseded by Round 7: every identity renders through a
stable opaque digest in one fixed-width `D` namespace; the gate rejects role
prefixes and width/range differences. `device_id` is feature-safe only as a
categorical identifier.

# AMENDMENT: card-session-lifecycle-2026-07

Round 7 repairs and the lifecycle blocker:

| Finding | Repair | Residual scope |
|---|---|---|
| Join-only victim selection allowed cases after closure; a case could extend across death/closure | All rails require `[joinTs, death + 120d settlement)`; authorized scams also alive; the whole span must fit the earliest victim/payee horizon; post-horizon chargebacks suppressed; the card view applies Membership to both owned endpoints | raw ledger keeps full-world rows; no out-of-interval owned endpoint in generator or card graph |
| Every card-compromise positive uses a deposit account (derived debit card) | Issued-card swap reverted: fraud is planned after card cycles are serviced, so it bypassed statements, payments, interest and screening; the swap is test-rejected | Open: fraud planning into `CardCycleDriver`, then credit/debit mix, expiry/reissue/replacement |
| Credit-card keys missing from the access-router owner map | Card-registry ownership merged in, liabilities not treated as deposits | device adoption/replacement era-flat |
| `FD` / person / legitimate-shared layouts exposed role | Opaque digest, one fixed-width `D` layout | pseudonym, not a cryptographic boundary |
| Only whole-window Party→Device/IP associations; attacker endpoints missing from the roster | Timestamped `Transaction_Uses_Device` / `Transaction_Uses_IP`; Device/IP vertices = roster ∪ observed; `Has_Device` / `Has_IP` header-only (inverted by attacker-infra-2026-07) | event-time edges; no false ownership |
| Autopay added 12 h to an already-timed due value | One 17:00 cutoff; autopay noon; manual samples around the cutoff; late fee 10:00 next day | late rate UNCITED |

The schema became 37 tables. This supports causal point-in-time features,
not the rest of a benchmark claim (dated concept drift, delayed labels,
modality calibration, external GSQL/training/evaluation).

# AMENDMENT: use-chip-causal-2026-07

Round 8 (exporter only) closes the entry-mode half of Part IV's `use_chip /
error` row and supersedes its "the real card-present modality … is NOT
exported" clause.

- Finding: `use_chip` was a content-keyed FNV hash (Swipe .63 / Chip .26 /
  Online .11) for all rows: mechanism-free and incoherent (a physical
  outlet could render "Online Transaction", a 1994 row "Chip Transaction").
- Repair: entry mode reads the destination's `Footprint`, the axis both
  legitimate selection (`payments.cpp pickMerchantIndex`) and the fraud
  rails (`unauthorized.cpp pickMerchantDestination`) partition on. Online ⟺
  catalog `Footprint::online` or a non-catalog remote biller (declared
  CHOICE); physical outlets split Chip/Swipe by the dated EMV table (0 before
  2012, .10 at the Oct 2015 liability shift, .65 in 2019, frozen .90 outside
  coverage). Per-row draw content-keyed on `kUseChipLane`: no generation
  randomness, carrier or schema change.
- Residual: `error` stays a mechanism-free hash (open half of online-GNN
  gate 4); chip/swipe is a presentation-layer mix, not adoption state; the
  legitimate CNP share (`kCardPresentShare` .89) is era-flat; EMV values
  [Likely].

Anti-shortcut: causal `use_chip` correlates with the label (fraud
CNP-majority by F-4's design, legitimate spend CNP-minority), a real signal
already visible through merchant geography; the feature now agrees with the
structure. The merchant-ID baseline gate caps destination-derived
separability.

Gate `tests/test_card_use_chip.cpp`: coherence pin (Online ⟺ geography-free
destination), pre-EMV zero-chip pin (1991), 2019 chip-share band behind a
power precondition, compile-time pins on the EMV table; fraud-vs-legit CNP
printed, not gated. `use_chip` moves from USE WITH CARE to FEATURE-SAFE.
Only `golden_tables_card_fraud.md5` re-pins. CONFORMS as entry mode; `error`
stays the documented gap.

# AMENDMENT: econ-wiring-power-2026-07

T3 (owner ruling 2026-07-27): `test_econ_wiring`'s fraud-rides-L sub-gate
moves from one seed to a pinned seed-pair panel. Harness only; zero golden
movement; the model measured unmoved first.

- Finding: single-seed parity sd ~0.076-0.086 (12 paired seeds, measured
  twice: means 0.9072 / 0.9095), while the defect (budget pinned to
  population or window constants, parity → 1/legit-ratio ≈ 0.79) sits ~1.6σ
  below. The shipped seed drew 0.7795, below the defect value, on a model
  at p ≈ 0.16 no-effect; ~10% of re-rolls fall below the old 0.80 edge.
- Repair: mean over 12 pinned seed pairs (same seed both eras; pair 0 the
  shipped legs); each pair asserts its preconditions (join cohort present,
  flagged rows > 30); the panel must be complete; a power check fails the
  gate if mean − t₀.₉₉₅,₁₁·se cannot exclude the defect. Observed: mean
  0.9095, se 0.0248, defect 0.7932 vs exclusion edge 0.8325.
- Unchanged: the 0.80/1.25 edges, now with ~4.4 se headroom on a √12
  tighter estimator.

CONFORMS; the suite's only red cleared without touching model or goldens.

# AMENDMENT: email-minhash-2026-07

Owner-requested, additive: an email LSH layer; the schema becomes 39
tables. `cf_Email_Minhash` (buckets) + `cf_Has_Email_Minhash` (Email →
bucket, 10 per email) via the shared `exporter/common/minhash` stack (already
serving AML name/address buckets) through `emailMinhashIds`: trim +
lowercase → 3-gram shingles → 10-permutation signature → bands, prefix
`EMH`, b=10 × r=1, derived from the emails in `cf_Email`; draw-free. Safe: a
pure function of an exported string, no time axis or label; card-view emails
are customer PII only (ring/attacker identities have none), so buckets
cannot encode role. FEATURE-SAFE as structure, bucket id opaque.

Count sweep: nine 37-table assertions updated (schema/export/streaming
headers, `test_pipeline_e2e` list + comment, `test_table_golden` floor 37 →
39, tests/CMakeLists note, acceptance manifest + three count checks). Only
`golden_tables_card_fraud.md5` moves (then unpinned, T2); full non-PG suite
56/56, `test_run_golden` unmoved. CONFORMS.

# AMENDMENT: attacker-infra-2026-07

Model round. Attacker endpoints were minted one per compromise
(`buildCompromisePlans` wrote `Identity{ring, 0xACE00000 + seq, 0}` and a
fresh `network::randomIpv4`), so cross-victim reuse was zero by
construction, though one endpoint touching many cards is why card fraud is
a graph. Four gates, the acceptance script and two normative documents
checked presence, vertex membership, role-free ids and empty ownership;
none measured degree. A count of endpoints is not a measurement of the
graph. Supersedes and inverts Part IV's withheld Party ownership and
header-only `Has_Device`/`Has_IP`.

| Change | Construction | Safety basis |
|---|---|---|
| Attacker infrastructure is a world entity | `infra::AttackerInfra` built by `synth::infra::attackers` on `{"infra","attackers"}`. An operator is a campaign: lognormal length (median 110 d, σ 0.95) clipped to the window, 1-3 device and 1-3 IP lines, each a `timeline::sampleChain` chain tiling the campaign; case load Pareto(α 1.35, cap 80). Operators are not roster Parties (the attacker population is exogenous) | 74-82% of attacker devices seen by >1 victim, mean 5.1-8.8, max 37-40; IPs 59-71%, mean 3.2-5.9. `tests/test_card_endpoint_graph.cpp` sub-gate A bands mean and tail; disarming reuse gives mean 1.03 / max 3, 12 checks red |
| Draw-free resolution over the whole case span | `operatorAt(u, ts)` / `deviceAt(op, ts, endTsExcl, salt)` / `ipAt(...)` stateless; four unconditional uniforms per plan (the old four `randomIpv4`) | `golden_run.b2sum` 189,035 → 189,035 rows, only `device_id`/`ip_address` move; `test_arch_equivalence`, `test_spool_equivalence` green; sub-gate D a hard zero outside tenure |
| Ownership restored in the generator | `infra::enrollment`: incomplete registry, draw-free hash of (party, endpoint), coverage 0.72 device / 0.61 address (`Usage::enrolled`); `Has_Device`/`Has_IP` export on-file associations from `world.infra.*.usages`. 18% of cases use the victim's own endpoint; 30% of operator sessions exit through a residential proxy (another customer's address) | "endpoint not on file ⇒ fraud" precision 0.027 / 0.016 at 2.9x / 1.8x lift (was 1.0); sub-gate C bands precision and requires lift > 1.0; tables world-derived, so `test_card_point_in_time` holds |
| Truthful endpoint ground truth | `export.cpp` used `try_emplace(identity, false)`, leaving the 5 AML ring-shared devices as the only `device/flagged` positives (5/66,964, anti-correlated with the transaction label); the verdict is now attacker-set membership; residential proxies are not marked | Verdicts stay in `cf_Ground_Truth_Label`, `is_blocked` 0 |

Sizing defect caught by sub-gate B′ (mean concurrent campaigns): 4.92 and
4.32 vs nominal 6.0. Starts were drawn over [0, W), leaving early-window
compromises unattributed; now [−L, W). Then the analytic mean length (169
d) overstated the clipped span (~137-144 d); `effectiveCampaignDays` is now
measured and printed. No band widened. After: 6.62 / 7.07; unattributable
victim-endpoint residual ~8% → ~2%.

Inverted hard-fail points: `tests/test_pipeline_e2e.cpp` (non-empty plus
four-way referential integrity), `tests/test_table_golden.cpp` (STATIC
ENDPOINT LEAK → UNREACHABLE ENDPOINT LAYER),
`docs/card_fraud_postgres_acceptance.sql` (`RAISE EXCEPTION` on an empty
table), `tests/golden_tables_card_fraud.md5` (re-pins populated). TF_GNN_v3
reaches Device and IP only through `Party_Has_Device`/`Party_Has_IP`, so
empty ownership had isolated the whole endpoint layer.

Follow-on `giftcard-channel-2026-07`: coached gift-card purchases were
hardcoded card-present (`cardPresent = plan.rail == Rail::giftCardScam ? true
: …`), deleting the digital branch: an online e-gift code (Apple / Google
Play / Amazon) bought from the victim's own home IP, where an issuer can
interrupt before the codes are read out. Now a dated digital share
(`digitalGiftCardShareBasisPoints`, class S UNCITED, built like
`derive::chipShareBasisPoints`): 0 before 2005, 3,500 bp by 2019, physical
the majority throughout. One coin on `{"fraud","unauth","merchant",seq}`
moves only gift-card destinations. Sub-gate F′: online 0.2354 (2012-16),
0.2630 (2016-18). Rows unmoved at 189,035.

The sweep it prompted: `grep` for hardcoded modality across
`src/transfers/fraud/` and `include/phantomledger/transfers/fraud/` found
only that ternary. Everything else (`kCardNotPresentShare` 0.70, `isTest`
0.7, `reported` 0.85, `wireRail` 0.5, rail mix .48/.12/.12/.28) is a drawn
split; the era-flat ones (post-2015 CNP migration; app-push barely existing
before ~2017 against a flat 0.5) are the registered per-era payment-method
item. A mis-weighted split degrades realism; an absent branch deletes a
detection opportunity.

Follow-on `merchant-ownership-2026-07`: the TigerGraph loader refused the
push because `cf_Is_Merchant` was empty (so `Party_Is_Merchant` would not
load); supersedes Part IV's `Is_Merchant` row. Business keys
(`synth::accounts::assignBusinessOwners`, `Role::business`, owned) and
merchant keys (`synth::merchants::makeCatalog`, `Role::merchant`,
ownerless) were disjoint. The schema declares `Party_Is_Merchant(FROM
Party, TO Merchant)` with `REVERSE_EDGE="Merchant_Owned_By_Party"`, and a
party's first-seen time is the minimum over linked cards and merchants:
ownership semantics.

| Change | Construction | Safety basis |
|---|---|---|
| `entity::merchant::Record::owner` | `entity::merchant::ownership::ownerFor` after `synthesizeBusinessOwners`, from the business-owner cohort in the registry (sorted, deduplicated); coverage 0.45, class S UNCITED; takes no Rng | `golden_run.b2sum` unmoved (`22db0e33…`, 189,035 rows) |
| Membership hashes the merchant key alone | `ownership::onFile` reads role, bank, serial only | Leak containment at a realism cost: a footprint- or weight-based rule ("local outlets have proprietors") inherits the modality split (card rail ~70% CNP from `Footprint::online` only) and so the label |
| Keyed on ownership, not acquiring | `Bank::internal` on a merchant key (`internalP = 0.02`) is five merchants at pop 10,000 | A ~5-row table silences the abort with no structure; refused |
| Only merchants observed in the view | `cf_Merchant` is built from `artifacts.merchants` | Avoids dangling edges in prefix exports; `test_card_point_in_time` classes `Is_Merchant` a growing line subset |

Owner ruling 2026-07-28: the edge asserts identity, not money flow (a
purchase settles to the `Role::merchant` sink, nothing reaches the
proprietor's account); no remittance leg is owed, and the earlier
"registered limitation" framing is withdrawn. Use the edge for structure,
never as evidence funds moved. Sub-gate G: 119 of 286 and 135 of 322
merchants owned (41.6% / 41.9% vs 45%); fraud lift on "destination has an
owner" 1.122x and 0.950x; the sign flip across seeds shows no construction
correlation (finite-catalogue effect via the CNP share); band `(0.80,
1.25)`.

Golden: `golden_run.b2sum` re-pinned to `22db0e33…` at 189,035 rows
(checked against the failing run's prediction); ownership moves
`golden_tables_card_fraud.md5` on `cf_Is_Merchant` only; the three table
goldens (all digest `public.transactions`, whose device/IP cells moved) are
deleted for the owner's PostgreSQL re-pin. Model, named re-pin; CONFORMS;
non-PG suite 57/57 accounted (56 pass, `test_scale_soak` skipped).

# AMENDMENT: merchant-coordinates-2026-07

Exporter only, owner-requested. Merchant coordinates existed since
`geo-causal-v1` and drove selection (`GeoArea` `latitudeE6`/`longitudeE6`
microdegrees; `haversineMiles` in `popularity(weight) *
exp(-distanceMiles / scaleMiles(homeArea))` and the fraud rail in
`unauthorized.cpp`), but the exporter wrote only `stateCode`, `city`,
`postalAreaCode`. TF_GNN_v3 declares `lat DOUBLE, lon DOUBLE,
has_coordinates BOOL DEFAULT "false"` on `Merchant_Location`, `City`,
`Zipcode` and `Street_Address`, and the TigerGraph loader filled defaults.

| Change | Construction | Safety basis |
|---|---|---|
| `cf_Merchant_Location(merchant_id, lat, lon)`, the 40th table | Written in the merchant loop via `derive::degreesFromE6`; row presence is the `has_coordinates` mask (online merchants and non-catalog billers absent, never a 0,0 in the Gulf of Guinea) | Draw-free; `golden_run.b2sum` unmoved at `22db0e33…`; `test_card_point_in_time` classes it keyed-stable (prefix-invariant coordinate) |
| `lat`/`lon` appended to `cf_City`, `cf_Zipcode` | `kCityCols` 3 → 5, `kZipcodeCols` 1 → 3, appended (positional mapping of `id`/`city`/`population` unmoved; matches TF_GNN_v3 order) | First-writer-wins like `population`, so a row's attributes come from one area (moot now: city+state and postal code unique across 86 rows) |

These are area centroids: co-located merchants share a point (149 merchants,
48 centroids on the e2e window). `card_fraud_feature_contract.md` prohibits
nearest-neighbour-merchant and intra-ZIP clustering features.

Gate (`tests/test_pipeline_e2e.cpp`), four checks since constant `0,0` would
pass presence, integrity and agreement: coverage equal to `cf_Has_Zip` both
ways; the US bounding box (`domesticAreas()` filters `Country::us`);
byte-identity with the merchant's own `cf_Zipcode` row; more than one
distinct point. A lat/lon swap reds 149/149 on bounds and agreement; a
constant point reds bounds and distinct-point while passing agreement.

Party geography stayed unexported here (`cf_Address` a bare string,
`pii::Address::geoArea` never written; the loader asserted the
`Party_Has_Std_*` edges empty, like `Tax_Id_Number`). An absent table can be
a hard abort (`cf_Is_Merchant`) or a hard assertion of emptiness,
indistinguishable from inside this repository. Closed by
party-geography-2026-07.

Golden: only `golden_tables_card_fraud.md5` moves (`cf_City`,
`cf_Zipcode`, `cf_Merchant_Location`, three of forty). CONFORMS; non-PG suite
62/62 accounted (56 pass, 5 PostgreSQL skips, `test_scale_soak` skipped).

# AMENDMENT: party-geography-2026-07

Exporter round plus a lockstep TigerGraph loader change: cardholder-to-
merchant distance becomes computable downstream. `pii::Address::geoArea`
(assigned at PII synthesis on `{"home-geo", <household>}`, shared by
coresidents) had no exported form, though selection and the fraud rails are
built on that distance. The loader listed the `Party_Has_Std_*` edges as not
loadable and asserted them empty, so this repo alone could not ship them.

| Change | Construction | Safety basis |
|---|---|---|
| `cf_Has_Std_City` / `cf_Has_Std_Postcode` / `cf_Has_Std_State` (40 → 43 tables) | A pass before the City/State/Zipcode writers over `p = 1..roster.count` (= `cf_Party`'s bound), reading `pii.records[p-1].address.geoArea` through the catalogue, guarded on `contains(geoArea)` | Draw-free; `golden_run.b2sum` unmoved at `22db0e33…`; world-derived identical in `test_card_point_in_time` |
| Party areas unioned into City / State / Zipcode | Pass order is load-bearing (an unoccupied home area must be a vertex); `Assigned_To`, `Located_In` span both | `City` stays keyed-stable, `Zipcode`/`State` line subsets; integrity gated both ways |
| Foreign-domiciled parties emitted | ~4% under `LocaleMix::usBankDefault`; the 15 foreign areas' subdivision codes (LND, ON, CMX, MH, SH, SEO, …) collide with no US state | Dropping them hides a key population and turns "has a Std_City edge" into a US-residency flag |

Harness divergence: the first disarm (drop foreign parties) passed because
every harness ran `LocaleMix::usOnly()` while production runs
`usBankDefault()`. `test_pipeline_e2e` now builds all 16 locale pools
(minority pools sized 512), runs the production mix, and asserts a home
centroid outside the US box; the disarm then reds twice (coverage 94/100,
foreign centroids 0). Disarming is mandatory.

Measured: 100 parties → 29 home centroids, 3 foreign, zero unreachable on
`Party → Zipcode → coordinate`; merchants 140 / 50 centroids.

The TigerGraph loader changed in lockstep (outside this repository): it
loads the three party edges count-matched to the manifest, reads
`has_coordinates` from row absence in `cf_Merchant_Location`, widens its
City/Zipcode/State sets to party places, and refuses the push if either end
of the distance pair is empty.

Golden: only `golden_tables_card_fraud.md5` moves (the three new tables
plus `cf_City`, `cf_Zipcode`, `cf_State`, `cf_Assigned_To`,
`cf_Located_In`). CONFORMS; non-PG suite 62/62 accounted.

# AMENDMENT: merchant-churn-2026-07

Model round, owner-raised. Over the owner's 20-year window no merchant
opened or closed and every favourite set was frozen for 7,305 days;
supersedes the implicit "live for the whole run" assumption.

1. `makeCatalog` took no window and `merchant::Record` had no time field:
   490 fixed endpoints for two decades.
2. `math::evolution::evolveFavorites` was dead code: its `merchantAddP` /
   `merchantDropP` / `maxFavorites` config validated at startup, never
   called (`evolveAll` evolved only P2P contacts); a `TODO(structural)`
   cited fixed-length `primitives::utils::Csr` rows.

| Change | Construction | Safety basis |
|---|---|---|
| Operating interval | half-open `[firstEpoch, lastEpochExcl)` on `Record` (like `infra::Tenure`); three-band annual death hazard, each band reproducing one BLS retail survival point (levels: bls-citation-2026-07); NBER modulation from `MacroYear::recessionMonths` | Default always-live kept unit harnesses unchanged. Incumbent survival 0.4343 vs derived 0.4349 over 15 years; 0.7448 vs 0.7577 over 5 (pre-recalibration) |
| Incumbents use the mature band | The BLS curve is a birth cohort; a window's opening merchants are survivors | 20-year survival ~33%, not ~25% (a modelling fact) |
| Churn replacements on an isolated lane | `appendChurnReplacements`: births = expected deaths (`base * h * years`) from `churnSeed`; replacements inherit a donor's shape (age cannot proxy size) | Replaced a first design that sized the catalogue inside `makeCatalog` (one shared-stream `lognormal` per core record), which shifted every downstream entity: 51,079 closure violations in `test_membership`, `test_econ_wiring` out of band, 7 outlets per proprietor |
| Liveness reaches all three selection sites | national and biller CDFs rebuilt monthly; `GeographicMerchantPools::rebuildLive` masks cached decay weights (~10M haversines saved per run); the fraud rail filters on the case timestamp (draw-neutral) | out-of-tenure rows 49% → 10.2% → 5.0% → 0.27% (15 y), 24% → 0.21% (5 y) |
| `Csr` capacity + live count; `evolveFavorites` wired | O(1) `pushBack` at `count++`, `swapRemove` moves the last entry into the hole; count-less instances report full spans (`Billers` bit-identical) | A forced-drop pass (closed merchants, world state, no draw) precedes add/drop |
| Paired biller replacement | closed favourites drop; closed utilities are replaced | otherwise long-run recurring debits shrink |
| Behaviour draws on their own lane | `exploreProp`, `burstStart`, `burstLen` on `{"payee-behavior", id}` | `WeightedPicker::pick` retries, so its count depends on data (+4,138 unrelated rows). Data-dependent draws go last on their own lane |

Residual, structural: 0.18-0.49% of merchant rows post out of tenure
because liveness changes at month boundaries (bills highest; fraud exempt).
The ceiling sits just above that floor: frozen favourites measured 49%,
biller staleness 5%, no liveness 100%.

Instrument defects:

1. `external_unknown` read 85% out of tenure: `PaymentRouter::emitExternal`
   routed the catch-all to `makeKey(merchant, external, 1)`, catalogue
   serial 1 (fixed by institutional-providers; flow retired by
   unknown-counterparty).
2. `test_table_golden`'s divergence report truncated silently at ten lines
   shared by both lists (ten `changed-or-new` rows, zero `was-in-baseline`),
   hiding six moved tables (`cf_Merchant_Location`
   looked unmoved while `cf_Has_Zip` moved, impossible since one branch
   writes both). Each list now has its own budget and counts suppressed
   lines. Silent truncation of a re-pin input is the same defect as an
   unstated coverage bound.

Misattribution corrected by the goldens: `ALERT_ON` fell 17% (18,699 →
15,559), first blamed on a camouflage change skipping merchant-destined
P2P; skip and re-pick give identical digests at production ring rates. The
cause is threshold amplification: `Rule::velocityBurst` fires at `count >=
5` and the corpus moved −1.3%, while `fraudMlFlag` covers at most 1,003
fraud rows in 720,053, leaving ≥14,556 threshold-driven alerts. Fraud stays
0.139% (target ~0.1%); `cf_Ground_Truth_Label` moved one row. The re-pick
stays (it fixes a real wrongness at the gate's fraud profile).

`kRecessionHazardLift` (0.60) is class S UNCITED (direction BLS-anchored).

Golden: model re-pin, all four baselines. `golden_run.b2sum` `22db0e33…` →
`9d0a9399…`, 189,035 → 188,478 rows (merchant churn −859, behaviour lane
+302). Tables: 7 of 38 standard, 23 of 59 fraud, 16 of 40 card_fraud, all
matched pairs. `cf_Merchant_Location` 540 → 546 = `cf_Has_Zip`;
`cf_Has_Std_*` unmoved at 10,000, `cf_City`/`cf_Zipcode` at 86. CONFORMS;
non-PG suite 62/63 accounted, zero failures.

# AMENDMENT: card-churn-2026-07 + burst-rate-2026-07

Card churn (model) plus a corpus-neutral burst-rate repair, both
owner-raised. The card number was `'C'/'D' + renderAccountKey`, a function
of the account, so no cardholder ever got a replacement in 20 years.

Source (a decomposition): Auriemma Consulting Group US reissuance research,
~50% of cardholders reissued within a year: ~33% expiry, ~26% EMV
migration, ~14% lost/stolen/damaged, ~27% fraud [Likely].
`kDispersionSigma` (0.80) and the 36-60-month validity span are class S
UNCITED (span anchored on Mercator 2019 U.S. PaymentsInsights: three-year
terms common vs a ~5-year tradition).

Mechanism: `entities/holdings/card_reissue.hpp` tiles `[windowStart,
windowEndExcl)` with generations, draw-free: dates hash the card key and
generation over four independent FNV domains (validity / proneness / event
/ EMV). Unscheduled replacement rides a mean-1 lognormal proneness
(Box-Muller) keyed on the card (Auriemma's repeat-victim finding is per
instrument). One `cf_Card` vertex and one `cf_Party_Has_Card` edge per
observed generation.

Measured: 6.672 generations/card over 20 years (was 1.000); proneness mean
0.982, p99/median 6.20x; 60-day churn 1.2%; EMV wave 2081 of 4000 cards.
Sub-gate H fraud lift 1.012x / 0.990x.

Registered limitation: fraud-driven reissue (~27%) is omitted on purpose:
it is downstream of the label, and the only export-time signal is
`seen.fraud`, the verdict `cf_Card.is_fraud` withholds. Churn runs low and
fraud-free (both safe; sub-gate H proves the second).

Sub-gate H:

1. The flag is rule-level: observed generations confound with exposure
   (busy cards straddle more boundaries and, via `exposure.hpp`'s activity
   tilt, are victimized more).
2. The band is measured: eight readings over four seed pairs give mean
   0.991, SD 0.0366 vs binomial 0.015 (fraud rows cluster by case); band
   3.5 SD = 0.87-1.13.
3. Power falls with window length: marking every compromised card reissued
   reds 1.478x (wide leg) but only 1.153x (long leg), since 66% of view
   cards already reissue over four years. The inherited G band
   (0.80-1.25) caught one leg.

Burst rate: `buildPersonBursts` applied `burstProbability = 0.08` once per
run (0.49 bursts/year at 60 days, 0.004/year over 20 years; 92% of a
20-year population had none). Now `burstsPerYear = 0.487` × `segmentSpan /
365.25`, equal to the old coin at the 60-day golden. Measured 0.482 at 365
days, 4.843 at 3652. The gate runs two horizons (rate and per-run
probability look identical at one); it caught the rate without its
duration (6x at 60 days) and a first gate that re-derived the formula.

Technical debt: `day.cpp:15` closed (owner directive 2026-07-29):
`time::weekday` is Mon=0..Sun=6 (`calendar.cpp:65`), so `>= 5` selects
Sat/Sun; the duplicate now uses
`time::isWeekend` (`weekday(tp) >= 5`, `calendar.cpp:70`), output-identical;
zero live markers remain.

Golden: exporter only (no corpus or standard/AML table carries a card
number). `golden_run.b2sum` unmoved (`9d0a9399…`/188,478); `standard` (38)
and `fraud` (59) identical; three of 44 card_fraud tables move: `cf_Card`
and `cf_Party_Has_Card` 18,199 → 18,350, `cf_Card_Send_Transaction`
373,733 rows with a new digest. `cf_Ground_Truth_Label` (one row per
generation of a fraud-touched card) unmoved: no fraud-touched card crossed
a boundary in 60 days. CONFORMS; suite 64/64; H's disarm reds both legs.

# AMENDMENT: relocation-2026-07

Model round, owner-approved within the merchant-churn arc. `geoArea` was
fixed per household on `{"home-geo", <household>}`, so 20 years had zero
moves. Amends party-geography-2026-07: home geography is prefix-invariant
and observable, not static.

Source: Census CPS ASEC mobility: 15.9% (1998-99) → 8.4% (2021); 53.5%
within-county, 24.3% same-state other county, 17.3% other state, 4.9% from
abroad [Likely]. Linear interpolation, `kAreaChangingShare` (0.85) and
folding within-county into same-state are class S UNCITED.

Mechanism: `entities/parties/relocation.hpp`, a contiguous ascending tenure
history per person; tenure 0 at window start equals the `homeAreas`
snapshot (zero moves reproduce the old corpus). Own lane
`{"home-relocation", <group>}`, four unconditional uniforms per group-year
(move coin, day, destination position, same-state coin).

Measured (20 years, 400 people): 0.1047 moves/person-year vs 0.1033
nominal; halves 0.1212 → 0.0882; same-state 0.674; cross-country 0; 62 areas
ever occupied vs 46 at start; 60-day leg 0.0175. Coresidence: 20
multi-member groups, 14 move, 0 divergent.

Findings:

1. The rate came in at half (0.0511 vs 0.103, same-state 0.63 vs 0.818): a redraw
   onto the origin was a no-op, and few areas per state made in-state
   self-hits common. `sampleExcluding` renormalises over the complement;
   one fix moved both toward nominal.
2. The 0.674 residual is the catalogue: 51 of 64 states with residential
   areas have one, so in-state intentions there go national. The check is
   a floor.
3. The group key is (household, initial area): `buildRecord` samples
   `country` per person before `homeAreaFor`, so members can already differ
   (reachable under `usBankDefault`).
4. Engine divergence (monolith 252,517 vs windowed 257,673, 5,156 rows, in
   `test_arch_equivalence`) was a harness gap: `GateWorld` builds its own
   market, so wiring only `windowed_run.cpp` left immobile homes. Any
   carrier the fold reads must also be filled in `gate_world.hpp` (second
   time paid).

Exported form: `cf_Has_Std_City` / `_Postcode` / `_State` carry one row per
occupied tenure with `since_unix_time` appended as column 3 (positional
mapping of `party_id` and area id unchanged); a non-mover emits one row at
window start; every tenure's area joins the City/State/Zipcode vertices.

Registered limitations: static households (nobody leaves home); no foreign
moves (`country` drives locale, PII and identity); no age or tenure tilt
(rates fall steeply with age, owners move less, but households have no
single age); re-occupying an area collapses to one edge at the earliest
occupancy (TigerGraph keys edges by (from, to)).

The TigerGraph loader changed in lockstep (outside this repository): it
takes the earliest occupancy per (party, place), stamps each
`Party_Has_Std_*` edge at the tenure start (never before the party enters
the graph), uses the latest tenure as the current home point, and requires
`since_unix_time`.

Golden: model re-pin, `golden_run.b2sum` `9d0a9399…` → `2afaf188…`,
188,478 → 188,477 rows (~1.75% relocate in 60 days; a destination change
moves the count only via an insufficient-funds flip); table goldens
deleted for the same commit. CONFORMS; suite 65/65,
`test_arch_equivalence` byte-identical.

# AMENDMENT: bls-citation-2026-07 + the acceptance-script table count

Calibration round plus a blocking repair; supersedes merchant-churn's
calibration status (manual download needed to promote the hazards).

Promoted [Likely] → CITED (accessed 2026-07-30): BLS Business Employment
Dynamics, "Table 7. Survival of private sector establishments by opening
year", NAICS 44 (Retail Trade), `bls.gov/bdm/us_age_naics_44_table7.txt`.
March-1994 cohort: 80,604 at birth; 82.8% at 1 year, 57.7% at 4 (46,469),
48.0% at 6 (38,669), 13.7% at 31. `bls.gov` returns HTTP 403 to programs,
so the figures came through a search index, checked by arithmetic
(46,469/80,604 = 57.65%, 38,669/80,604 = 47.97%); revisions must repeat
that check.

The old calibration was wrong twice: its figures (~84.2% at 1 year, ~58.3%
at 5) are not NAICS 44's (82.8%, ~51%; 58.3% is near the 4-year 57.7%),
and its stated `0.842 * (1 - 0.1145)^4 = 0.583` evaluates to 0.5177. The
errors partly cancelled (within 1.5pp of every point).

Recalibrated to the 1-, 4- and 31-year points: `kHazardFirstYear` 0.158 →
0.1720, `kHazardYears1To5` 0.1145 → 0.1134, `kHazardMature` 0.0540 →
0.0494 (fitted points ±0.01pp). The unfitted 6-year point falls out at
48.63% vs 48.0% (0.63pp), evidence for the three-band structure.

Gate split: `test_merchant_churn` check C hardcoded 0.0540; now C1
(fidelity, derived from `kHazardMature`, passes for any value) and new C2
(the curve vs four published points; fitted 0.005, 6-year 0.02). The old
constants red 6 checks.

Golden: none (`2afaf188…`/188,477; no liveness flipped in 60 days, though
15-year incumbent survival moved 0.4343 → 0.4599). At the target config
20-year survival moves 0.3295 → 0.3630. Calibration, no re-pin.

Acceptance script: `docs/card_fraud_postgres_acceptance.sql` asserted
`registered_count <> 39` and `physical_count <> 39` (`RAISE EXCEPTION`)
while the export ships 43 (merchant-coordinates 39 → 40, party-geography
40 → 43 updated the manifest and header, not the scalars).
`test_table_golden` asserted `size >= 39`: a lower bound is not a count.
Fixed: `kTableCount = 43` in `exporter/card_fraud/schema.hpp`,
`test_table_golden` asserts equality, both SQL scalars updated with their
history; the manifest matched. Instrument defect, no re-pin.

Third instance (after the truncating divergence report and the sentinel
collision): a count copied where the header cannot be included goes stale,
and an inequality gate misses it. Disbelieve a number before the model
defect it implies; one constant over four copies.

# AMENDMENT: device-sharing-evidence-2026-08

Research + instrument round. Owner question: does research support that
high device sharing correlates with transaction fraud? Yes for the
mechanism, no for the magnitude. First authority row for device fan-out:
the prior record lived only in git-ignored `CLAUDE.md` (`.gitignore:94`),
so the Group-IB / fraud.net glossary behind `fanout-bimodality-2026-08` is
not in the tracked repository.

## F-8. Device / endpoint fan-out vs the label

| Parameter | PL value | Class | Anchor & source | Status |
|---|---|---|---|---|
| Fan-out is a fraud signal (direction) | attacker devices reach 23-32 victims, enumeration probes 47-92 cards | MEASUREMENT (direction) | Visa, "Anti-Enumeration and Account Testing Best Practices for Merchants" V1.2, April 2023 (Visa Public): different payment accounts sharing one email and device ID may trigger review. Mastercard US 10552836 B2 (filed 2016-10-11, granted 2020): risk scoring from accounts per device. Accessed 2026-08-11 | CONFORMS (direction) |
| P(fraud \| N cards per device), the level | emergent from case-load division | UNCITABLE | No cards-per-device series in any domain: OpenAlex body-text search zero for "cards per device" and 11 variants; zero across 968 works citing Ianus, GEM, InfDetect, TitAnt, Cash-Out, Financial Defaulter, SynchroTrap. Real-card literature has no endpoint layer: TitAnt (Ant Financial) defers device; xFraud (eBay, 1.1B nodes) has no device type; APATE (Van Vlasselaer et al., *Decision Support Systems* 75, 2015) never mentions device, fingerprint, IP or terminal | UNCITED; absence evidenced |
| Nearest published series | - | MEASUREMENT (wrong population) | Ianus (Yuan, Miao, Gong, Yang, Li, Song, Wang, Liang; ACM CCS '19) Fig. 3(b): P(Sybil \| N accounts on a device) = 44.6 / 57.7 / 73.7 / 81.8 / 88.0 / 91.8 / 97.6 / 91.8 / 94.7 / 98.9% for N = 1…≥10; base 45.7% (647k / 1,417k); max lift 2.16x; read by pixel at 600 dpi. WeChat registration, not payments; non-monotone at N=8; its IP curve has four of six buckets below base, max 1.46x | DEVIATES (account abuse) |
| Measured card anchor (this round) | sub-gate K | MEASUREMENT | IEEE-CIS Fraud Detection (Vesta Corporation, 2019), the only public CNP set with device columns: 118,666 rows with `DeviceInfo`, base 0.07253, fingerprint = `DeviceInfo\|DeviceType\|id_30\|id_31\|id_32\|id_33\|id_13\|id_17\|id_19\|id_20`, 61,050 groups; recomputed from the Kaggle CSVs, cross-checked against four published figures. Lift by cards {1, 2, 3-5, 6-10, 11-25, 26+} = 0.485 / 1.080 / 1.756 / 1.888 / 1.719 / 1.567; AP ratio from degree 1.455, from row count 1.546. Accessed 2026-08-11 | four deviations below |
| Approved probe → later fraud | absent: every probe declined (`Do Not Honor`, `kProbesPerCard = 1`) | KNOWN GAP | Visa VAAI Score datasheet 2025 (VisaNet): "Globally, enumerated accounts have 22x higher fraud rates than regular accounts"; 33% of those with fraud saw it within 5 days of an approved enumeration transaction. A network census, so it does not inherit the `giftcard-ratio-2026-08` under-reporting factor. Accessed 2026-08-11 | KNOWN GAP |
| Fingerprint ≠ device | one device chain per person for all cards | DEVIATES-BY-CHOICE | Gómez-Boix, Laperdrix & Baudry (WWW 2018): 2,067,942 fingerprints, 33.6% unique. Berke et al. (PoPETs 2025): ~60% unique (US panel). Vastel et al. (IEEE S&P 2018, FP-Stalker): churn within days | DEVIATES (gap 1) |

### The four measured deviations, with sub-gate K's armed readings

| # | Quantity | This corpus | IEEE-CIS | Disposition |
|---|---|---|---|---|
| 1 | Rows on a single-card fingerprint | 6.5% | 50.5% | Root cause: real fingerprints fragment. Registered fix (Vastel et al.): fragmentation in legitimate device synthesis; spends draws, moves every golden |
| 2 | Degree-1 lift | 1.96-2.46 | 0.485 | From (1). `kMaxDegreeOneLift = 2.95` stays; its "bimodal curve is real" justification is withdrawn |
| 3 | Curve shape | U (hot at 1 and 26+, trough 2-5) | unimodal hump at 6-10 | Printed, not banded (`merchant-selection-2026-08`: never band a shape the construction cannot produce) |
| 4 | AP(degree) ÷ AP(row count) | 1.75-2.20x | 0.941x | Corpus row count is anti-predictive (AP ratio 0.656-0.710) vs 1.546 real: a GNN trained here leans on structure too hard. K.3 bounds it at 2.65; fixing it changes endpoint row mass (POS terminals carry thousands of legitimate rows vs a case's 5-14) |

### What shipped

Sub-gate K in `tests/test_card_endpoint_graph.cpp` (asked for by `fanout-bimodality-2026-08`:
"prefer an AP/AUC-PR bound over a lift bound"). Test only; no golden
moves; `kTableCount` 43.

| Check | Band | Armed (4 legs) |
|---|---|---|
| K.1 ceiling, AP ratio from degree | ≤ 1.75 (mean + 3.5 SD of 1.318 / 1.440 / 1.151 / 1.218) | 1.151-1.440 |
| K.2 floor, same | ≥ 1.05 | 1.151-1.440 |
| K.3 ceiling, AP(degree) ÷ AP(row count) | ≤ 2.65 (mean + 3.5 SD of 1.855 / 2.195 / 1.747 / 1.814) | 1.747-2.195 |
| K.4 non-vacuity | rows, fraud, base rate > 0; support spans degree 1 and 26+ | - |
| K.5 | six-bucket curve printed beside the anchor | - |

| Disarm | K.1 | K.2 | K.3 |
|---|---|---|---|
| `kDisarmInstrumentCeiling` (pre-round world) | red 24.39 / 36.91 | - | red 40.09 / 60.89 |
| `kDisarmFraudOffLowDegree` (all fraud on shared infra) | red 11.48 / 9.65 / 8.10 | - | green 1.15 / 1.21 / 2.10 |
| `kDisarmDegreeApNoise` (label independent of degree) | - | red 1.0002 / 0.9998 / 1.0005 / 1.0013 | - |

K.3 misses the second leak, stated rather than hidden (the fifth recorded
check that cannot fail on a given leak): fraud on the widest
endpoint raises both rankers (row count 9.99x); a ratio is blind to a leak
common to both terms; K.1 catches it. Instrument defects: K.2 at the
analytic 1.0 left three noise legs green (banded at 1.05); the first noise
disarm rounded per endpoint, zeroing endpoints under `1/baseRate` ≈ 107 rows
and making row count 25x predictive (fixed by cumulative apportionment). A
disarm is a construction.

### Registered, not closed

- Gap 1, fingerprint fragmentation (Vastel et al. IEEE S&P 2018;
  Gómez-Boix et al. WWW 2018): highest-value change, moves every golden,
  needs sign-off; expected to repair deviations 2 and 3.
- Gap 4: an endpoint row-mass change, not a dial.
- Approved probes: need a new band on `test_card_enumeration`'s
  straddle-1.0 requirement first (correct only for the declined tail).
- Every positive finding measures accounts or users per device (Ianus,
  GEM, InfDetect, iovation, ThreatMetrix; ThreatMetrix's five-card rule is
  per account), not cards per device.

### Counter-evidence located, recorded so the direction is not over-read

- Two Ant Financial production systems delete single-account devices as
  low-risk (InfDetect: <0.1% loss).
- Rappi/UC Berkeley/UCSD (KDD-MLF '21), a real user-device-card graph:
  structure alone AUC 0.5626-0.6538; device relation importance 4.0970 →
  0.1048 (~39x) with behavioural features.
- GEM (Alipay, CIKM 2018): component size AUC 0.665-0.694 vs 0.916-0.936.
- PCI SSC / NCFTA (2020): attackers spread across sites (5 attempts × 12
  sites; ~200 for CVV discovery); Visa (2024) saw enumeration "distributed
  across hundreds of merchants".
- Amazon FDB (only public CNP set with raw `device_id`): four of five
  AutoML frameworks at chance (AUC 0.515-0.636).

Research + instrument; CONFORMS (direction cited, level uncitable,
deviations registered); no golden moves.

## Step 2: the consumer-visible view, and the dilution runs the wrong way

Sub-gate K.6 (test only). Merging `posted.declined` found zero probes
against 4,421-5,225 declined rows: probes are synthesized at export
(`streaming.hpp`'s `writeEnumerationProbe`) and exist only in the CSV, so
every degree ceiling (I.1 all-fraud bucket, I.2 best precision, K.1 AP) was
blind to them since `device-fanout-2026-08`. K.6 reproduces the synthesis
(one probe per card on its first view row, with `backdatedRowIsObservable`
and membership guards), valid because `probeFor` is draw-free and
stateless (the property `enumeration.hpp` keeps for golden containment).

| leg | probes in view | AP ratio settled → merged | best precision | degree-1 lift |
|---|---|---|---|---|
| leg-long | 138 | 1.318 → 1.402 | 0.0383 → 0.0433 | 2.330 → 2.269 |
| leg-wide | 272 | 1.440 → 1.597 | 0.0563 → 0.0559 | 1.960 → 1.838 |
| leg-sizeA | 182 | 1.151 → 1.231 | 0.0254 → 0.0299 | 2.463 → 2.404 |
| leg-sizeB | 220 | 1.218 → 1.331 | 0.0446 → 0.0441 | 2.337 → 2.192 |

Dilution makes degree more predictive (+6.4% / +10.9% / +7.0% / +9.3%);
leg-wide's 1.597 exceeds IEEE-CIS's 1.455. Only the low tail improves
(degree-1 lift −2.7% to −6.2%). Corrections: `device-fanout-2026-08`'s "the
top-degree endpoint is no longer a fraud endpoint" fails here (max degree
322→322, 324→325, 355→355, 418→418: a public terminal at 322-418 cards,
above the probe 47-92 and compromise 37-40 ranges measured in
`test_card_enumeration`'s harness); probes add 7-16 mid-degree, zero-fraud
endpoints per leg (~8.6 cards each, a mean). Printed, not banded; one
assertion ships (`probeViewRows > 0`); bands measured on the settled view
are not re-pointed (a band measured against a superseded construction is
not a measurement).

## Step 3: the probe share was swept, and raising it amplifies the leak

The owner proposed raising `kProbedBasisPoints`; sub-gate K.7 says no (it
stays 400). `probeFor` gained a defaulted `probedBasisPoints` so the sweep
uses the real resolver.

| bp | leg-wide AP ratio | leg-long best precision | leg-long top probe degree |
|---|---|---|---|
| 400 (shipped) | 1.598 | 0.0434 | 69 |
| 1000 | 1.754 | 0.0526 | 151 |
| 2000 | 1.839 | 0.0638 | 262 |
| 3000 | 1.860 | 0.1667 | 390 |
| 4000 | 1.881 | 0.1301 | 517 |
| 6000 | 1.828 | 0.0899 | 778 |

From 2000 bp the AP ratio exceeds `kMaxDegreeApRatio = 1.75`. Probe
endpoints already out-degree compromise devices at 400 bp (58-98 vs
attacker max 23-32): fan-out intent met, dilution not.

Cause: `probeFor` draws from the same `AttackerInfra` inventory as the
compromise planner (another salt, same lines). Every probe row on a device
already in the settled view lands on a fraud device:

| leg | probe rows on a settled device | of those, on a fraud-carrying device |
|---|---|---|
| leg-long | 71 of 138 | 71 (100%) |
| leg-wide | 229 of 272 | 229 (100%) |
| leg-sizeA | 138 of 182 | 138 (100%) |
| leg-sizeB | 191 of 220 | 191 (100%) |

(Attacker inventory carries no legitimate traffic; 7-16 of each leg's
16-24 probe devices carry settled fraud.) Raising the share is a documented
anti-fix, recorded at the constant. Registered next step: a probe pool
disjoint from compromise lines (exporter-side; `golden_run.b2sum`
unmoved, only `golden_tables_card_fraud.md5`; a resolver change to
`probeFor` and `AttackerInfra`; re-measure `test_card_enumeration`).

Law: an anti-shortcut population must be disjoint from what it masks; a
diluent from the signal's pool concentrates it (third case, after the
ownership register and residential proxies).

# AMENDMENT: cash-hub-defect-2026-08

Closed in the customer-ledger projection; full record and gates in
`docs/cash_hub_defect.md`. The former defect: every ATM withdrawal credited
one random roster person's primary checking account, which reported
infinite liquidity and exported as `inf`. Now: ownerless, area-local
external cash endpoints with one-sided boundary posting and no credited
endpoint balance.

## The authority rows

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| ATM destination = a registered `Bank::external` processor endpoint among the customer's nearest four at the event-time area, cut ties by a per-(person, area, rail) hash window (`cash_points.hpp`, `plans.cpp`, `atm.cpp`; atm-spread-2026-09) | An on-us withdrawal reduces the bank's currency-and-coin asset; off-us adds network settlement; neither is a depositor's account | MEASUREMENT (accounting) | Fed FR 2900 instructions (vault cash includes own-ATM currency); Oracle FLEXCUBE Core Banking ATM User Manual, ATM01/ATM02 (2026-08-18) [Certain on mechanism] | CONFORMS as a declared customer-ledger projection (only the customer leg booked) |
| Centralized vault-cash / settlement GL | Cores centralize or modestly shard these GLs; terminals are transaction context | MEASUREMENT (accounting) | FLEXCUBE ATM02 (2026-08-18); Philadelphia Fed, *Clearing and Settlement of Interbank Card Transactions* (Oct 2013) [Certain] | CONFORMS by explicit omission (not a graph target) |
| Cash withdrawal points, deposit points, card issuer, billers, employers and landlord fallback use distinct keys/pools | Cores carry distinct cash, settlement, issuer and counterparty roles | TYPOLOGY | FLEXCUBE ATM01/ATM02 field lists (2026-08-18) [Certain] | CLOSED |
| Combined terminal/acceptor endpoint, no DE41/DE42 columns | The issuer-side identity is the ISO 8583 pair DE 41 Card Acceptor Terminal Identification + DE 42 Card Acceptor Identification Code (one acceptor, many terminals; unique as a pair) | MEASUREMENT | Elavon Developer Portal field 41; Galileo/SoFi data-elements map (DE41 `terminal_id`, DE42 `merchant_id`); Marqeta `card_acceptor` (2026-08-18) [Certain] | Partially closed; carrier gap |
| Distributed, geographically resolved cash points | None of five public AML/fraud datasets uses one customer cash node: AMLSim shards onto `Branch`, PaySim onto merchants, IBM AMLworld makes cash an edge attribute, the Neo4j reference reifies the transaction, Sparkov omits cash | TYPOLOGY | AMLSim `Branch.java`/`CashOutModel.java`; PaySim `Client.java`; NeurIPS 2023 D&B *Realistic Synthetic Financial Transactions for AML* Table 5; Neo4j `fraud-detection.adoc`; Sparkov README (2026-08-18) [Certain] | Closed; consumers must keep external type/channel |
| Interchange direction on ATM rows | Purchase: acquirer pays issuer; withdrawal: issuer pays acquirer | INVARIANT (accounting) | Philadelphia Fed (Oct 2013) [Certain] | Not modelled; recorded so no one adds it with the purchase sign |
| US ATM density (if a terminal layer is sized) | No current official per-capita figure: IMF FAS via World Bank stops at 2009 (425,010 ATMs, 172.76 per 100k adults). Commercial: 451,500 (2022), down from 470,000 (2019). Official withdrawals: 3.7 billion in 2021, average $156 (2018) → $198 (2021) | MEASUREMENT | World Bank `FB.ATM.TOTL.P5`; Euromonitor via Payments Dive 2023-06-23; Fed Payments Study 2022 (2026-08-18) [Certain on withdrawals; 451,500 trade press] | Amended by atm-spread-2026-09: 451,500 / 333.3M = 13.5 per 10,000 is the `atmTerminals` density (`synth/counterparties/make.hpp`); ATMIA's 520,000-540,000 (ATM Marketplace, 12 Sep 2023 [S]) gives 15.6-16.2. Density CONFORMS; the ~8x gap was throughput, registered there |

## Pre-fix measurements, retained as the regression baseline

- 23,866,506-row production export (499,409 accounts): one destination,
  `A0000247513`, took 1,823,332 rows = 7.64% (1,230,944 `atm_withdrawal` +
  352,271 `cc_interest` + 239,313 `cc_late_fee`) from 344,576
  counterparties = 69.0% of accounts; the only non-`X` vertex in the top
  eight (next `XM00000001` at 176,196).
- Money conservation off by +$36,248,870.40 (+42.31% of gross; pop 900, 731
  d, seed `0xC0FFEE`, 668,247 rows, gross $85,682,895.37); most of it a
  separate bug: 30 `fundingHubs` `createHub`'d but not `seedHubAccounts`'d,
  at cash 0.00 with a bypassed screen, sourcing $33,260,807.20.
- `kHubCash = 1e18` has a $128 ulp: credits ≤ $64 vanished (7 of 18
  `kAtmAmounts`: $20, $40×3, $60×3), 89.7% of hub-credit value
  ($2,376,059.08 of $3,697,214.85).
- `docs/cash_hub_inf_repro.cpp` (real `clearing::Ledger` and
  `exporter::csv::Writer`) emits `A0000000012,inf`; via `aml::exportAll`, 4
  of 1,674 `Account` rows at pop 400, 700 at the default `--population
  70000`.

## Why the pre-fix suite could not see it

`grep -rn 'isfinite|isinf|std::isnan' tests/*.cpp`: zero hits in 68 tests.
`test_pipeline_e2e` rendered the `inf` rows and passed (`expectTable` checks presence only); plain `aml` had no golden;
`tests/golden_tables_aml.md5` line 35 digests
`aml_txn_edges_vertices_Account` (column 7 `balance`), so the pin encoded
the `inf` and the fix had to red it.

## Rules this produced

1. A digest golden pins absurdities too; pair every digest with a domain
   predicate (finiteness is cheapest). Sibling of bls-citation-2026-07's
   "a lower bound is not a count".
2. A synthetic sink must not come from the population it serves (fourth
   disjointness instance, after the ownership register, residential
   proxies and the probe pool; the worst: a customer still eligible as
   victim or mule, 69% of accounts one hop away).
3. An infinity is a sentinel and must not cross an export boundary (cf.
   `loc-accrual-perf-2026-08`: `ts == 0` is a sentinel, not an instant;
   card-fraud
   `device_risk_score = -1`). Give the ledger a separate reporting
   accessor.
4. `std::to_chars` succeeds on infinity, so `errc` is not validity:
   `csv.cpp`'s throw never fired, and its trailing-zero fixup matched the
   `n` in `inf` via `.eEnN`.
5. Seeding and flagging walks over different key sets leave silent holes
   (`createHub` vs `seedHubAccounts`); assert they cover the same set.
6. `1e18` has a $128 quantum; saturation needs a flag, not a magnitude.
7. Centralised is right, customer-owned wrong: cores centralise the ATM
   cash GL; the fraud signal is the ISO 8583 pair on the transaction.
   Separate the money leg from the context leg before choosing
   cardinality.
8. Hub selection broke the merchant-churn-2026-07 rule that a
   data-dependent draw goes last on its own lane: its draw count
   depended on `populationCount`, first on the shared stream (Floyd's
   sampler: `j` over `[n-k, n)`, `range = j+1`, so the hub count changes the
   first draw). Move it to its own `RngFactory` lane and re-pin once before
   changing the model, or pay two re-pins.

## Implemented disposition and residual work

1. Removed `createHub`, hub flags, `kHubCash`, hub seeding, customer
   selection, hub exclusions and customer fallbacks.
2. Replay and screening post by key; external keys are boundary markers;
   unknown internal keys reject as unbooked; CSV rejects non-finite
   doubles.
3. Population-scaled ATM and depository pools, home-area placement,
   event-time relocation, nearest-point selection, draw-free customer
   affinity; billers and the issuer in separate pools.
4. Typed clearing contracts bind endpoint kind, direction and channel for
   ATM withdrawals, cash deposits, settled check deposits and crypto USD
   ramps; household cash/check deposits have an isolated routine; crypto
   ramp-in is capped by prior per-account ramp-out
   (`docs/customer_ledger_boundaries.md`).
5. Residual: DE41 + DE42 carriers, cash-point geography and on-us/off-us
   export, an ATM interchange leg with the withdrawal sign.
6. Residual: downstream flow queries must filter external boundary nodes or
   use a typed cash-access edge; terminal density stays class S.

## Also found

Closed: the 30 unseeded `fundingHubs`; `LegitCounterparties::hubAccounts`,
`CounterpartyAccess::isHub`, `CounterpartyAccess::firstHub` removed. API
hardening item: `unauthorized.cpp` still assumes a non-empty
`billerAccounts` span (production has an external fallback).

## Downstream, and it is why the generator fix alone is not enough

The customer-owned `A…` supernode is gone, but
MulePatternLearner's pre-graph aggregator uses only `src_acct, dst_acct,
amount, ts` and drops `channel`; `mule_ml`'s `Transfer_Transaction.csv` has
no channel, so an ATM withdrawal looks like a P2P transfer. Its GSQL
features lack an `is_external` guard, so WCC merges the population (making
the live feature `com_size` meaningless) and PageRank can mishandle shared
external endpoints. SALT-GNN (arXiv:2607.10131) measures degree-stratified
AML GNN degradation on these datasets and says aggregate F1 hides it; GCNs
favour high-degree nodes (Tang et al., CIKM 2020, arXiv:2006.15643).

# AMENDMENT: institutional-providers-2026-09

Three defects made a few external accounts population-wide hubs (200,000-
person mule-temporal corpus, `docs/research/counterparty_hubs_2026-09.md`):

1. Bug A: every mortgage paid the student-loan servicer (`mortgage.cpp` →
   `Lending::studentServicer`, flagged "SUSPECTED DEFECT" since
   2026-07-19): 1.43M payments on one account. Closed.
2. Bug B: the catch-all `makeKey(merchant, external, 1)` was catalogue
   merchant serial 1 whenever it banked externally (98% of seeds). Now the
   reserved `XM1000000001`; the flow (5% unattributed spending, no-contact
   P2P, funerals) is retired by unknown-counterparty-2026-09. Closed.
3. Change 3: six population-wide lender/insurer keys became six markets
   (mortgage, auto loan, student loan, auto, home, life). Each contract
   draws its provider once at issuance with one uniform on
   `{"product-provider", market, person}` (`synth/products/providers.hpp`,
   `entities/counterparties/providers.hpp`). SSA, disability and the IRS
   stay single. Bank-originated postings untouched here.

## The authority rows

Shares normalized within the pool. [P] primary, [S] secondary, from the
research pass (`docs/research/counterparty_hubs_2026-09.md`, "Lenders and
insurers are many firms, not one") and the design's parameter pass (NAIC,
FSOC, Big Wheels).

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| Mortgage servicers: ranks 1-20 = .073 .067 .067 .061 .060 .054 .052 .052 .031 .025 .024 .024 .023 .018 .017 .015 .014 .011 .011 .010, 1/rank tail to 300 | Largest 7.3%, top 10 54.1%, top 20 70.9%, HHI ~350; Mr. Cooper served 6.7M customers Dec 2024 (~13% of 50.8M, subservicing included) | MEASUREMENT | FSOC 2024 Nonbank Mortgage Servicing Report, Table 1 (Inside Mortgage Finance, agency UPB, Q4 2023) [P]; Mr. Cooper 2024 [P] | CONFORMS (value-weighted); built HHI 351 |
| Auto insurers: ranks 1-25 = .1864 .1860 .1156 .1015 .0619 .0357 .0281 .0193 .0190 .0153 .0142 .0139 .0137 .0110 .0096 .0083 .0080 .0071 .0062 .0047 .0044 .0043 .0042 .0042 .0041, tail to 100 | State Farm, Progressive lead; top 10 76.88%, top 21 87.0%, HHI ~1,000 | MEASUREMENT | NAIC Property and Casualty market share report (2025), Private Passenger Auto Total [P]; supersedes the note's 2024 [S] read (State Farm 18.87%, Progressive 16.73%, top 10 76.15%) | CONFORMS; HHI 1,012 |
| Home insurers: ranks 1-25 = .1869 .0942 .0702 .0551 .0546 .0515 .0457 .0250 .0219 .0197 .0185 .0177 .0107 .0103 .0101 .0095 .0091 .0089 .0080 .0078 .0070 .0069 .0068 .0066 .0065, tail to 150 | State Farm, Allstate, USAA lead; top 5 46.11%, top 10 62.48%, HHI ~620 | MEASUREMENT | NAIC P&C (2025), Homeowners Multiple Peril [P] | CONFORMS; HHI 631 |
| Life insurers: ranks 1-10 = .0866 .0576 .0552 .0508 .0408 .0373 .0349 .0328 .0318 .0302; top 25 = .7283, top 125 = .9924; pool 125 renormalized by .9924 | Northwestern Mutual, New York Life, MassMutual lead; top 10 45.79%, HHI ~300 | MEASUREMENT | NAIC life/fraternal (2024), individual life [P] | CONFORMS on ranks 1-10 and anchors; 11-125 are 1/rank between (not transcribed); HHI 302 |
| Auto lenders: ranks 1-5 = .054 .054 .050 .046 .044 (Toyota Financial, GM Financial, Ally, Chase, Capital One), tail to 200 | Largest ~5.4% of ~$1.9T, top 5 ~25% (note: Toyota ~6% of $1.8T) | MEASUREMENT | Auto Finance News "Big Wheels" 2025 [S]; research note [S] | CONFORMS [Likely] |
| Student, federal (92.4% of pool): Nelnet .31, Aidvantage .19, MOHELA .15, EdFinancial .175, CRI .175, × .924 | Nelnet 14.0M of ~45M (~31%); MOHELA 6.7M (~15%); Aidvantage 8.4-9M (~19%) | MEASUREMENT (three); CHOICE (EdFinancial, CRI) | Nelnet 10-K 2024 [P]; MOHELA [P]; Aidvantage [S]. Design pass: MOHELA 6.8M (Feb 2025); a stale 2021 Aidvantage 5.6M not adopted | PROVISIONAL: EdFinancial/CRI split the remainder equally (absorbing the Default Resolution Group); replace with FSA "Portfolio by Loan Servicer" (timed out twice) |
| Student, private (7.6%): Sallie Mae .63, nine lenders on 1/rank (15 total) | Private loans 7.6% of balances; Sallie Mae ~63% of private originations | MEASUREMENT | MeasureOne via PR Newswire [S]; Sallie Mae [S] (design pass) | CONFORMS [Likely]; the tail is CHOICE |
| SSA, disability, IRS one account each | Single ACH originators: "SOC SEC", "TAX REF", company "IRS TREAS 310" | MEASUREMENT | Bureau of the Fiscal Service Green Book; Treasury refund direct-deposit FAQ [P] | CONFORMS |
| Pools: mortgage 300, auto loan 200, student 15, auto 100, home 150, life 125 | Real markets have thousands (most banks and credit unions service their own; CFPB small-servicer exemption) | CHOICE | Butler Snow summary [S] | DEVIATES-BY-CHOICE: no tail provider nears a hub (smallest last share 0.2%); each market is a 99,999-serial block |
| Tail past named ranks is 1/rank to residual mass | Research reports HHI independently | CHOICE | research HHI (FSOC, NAIC) | CONFORMS as a shape: auto 1,012 vs ~1,000, home 631 vs 620, life 302 vs 300, mortgage 351 vs 350; A0 within 10% |
| One uniform per contract at issuance on `{"product-provider", market, person}`; portfolio stream, shared stream, `makeCatalog` untouched | At most one contract per market per person | INVARIANT | - | ENFORCED: A1 vs draw-free singleton tables at pop 200,000 (0 field diffs over 2,947,290 events, 232,651 loans, 181,290 holders; alone it only shows no draw depends on the table). A7 pins a digest of every key-free product value to the pre-round build (`da72a2306ca4622e` on HEAD 843f447 and this build) with a domain predicate. B5 pins the next shared u64 after the build at the run-golden config (`498e4bde6c6f83ea` both; covers `makeCatalog`). Disarms: one portfolio draw on half the providers moves 3.67M fields (A1); one extra draw per mortgage passes A1, reds A7; one extra shared draw in `buildLandlords` reds B5 |
| Only used providers registered, external, ownerless, in (market, ordinal) order after the entity stage | Small populations must not export unpaid providers | INVARIANT | - | ENFORCED: A5 (set = re-picked set; no index moves); leg B 682 providers from record 9,246 at pop 2,000 |
| Camouflage P2P excludes providers | A cover transfer to a servicer or insurer is a label shortcut | TYPOLOGY | - | ENFORCED: B3, 0 rows; without the filter 15 of 440 |
| Mortgages and student loans never share a servicer | Bug A predicate | INVARIANT | - | CLOSED: A2 0 cross-market events; every event, policy, premium and claim on its market (A2, B1) |
| Catch-all = `makeKey(merchant, external, 1'000'000'001)` (`XM1000000001`) | A bucket must not share a catalogue key | INVARIANT | merchant-churn-2026-07 (the `external_unknown` sentinel collision) | Closed, then superseded by unknown-counterparty-2026-09 (key reserved, unregistered, unpaid). At this amendment `test_merchant_churn` counted 0 catch-all rows on a catalogue key (old key 156,259 and 86,150), a 500,000-person catalogue with 20 years of churn (max serial 51,688) had no collision, and `test_estates` / B2 required the funeral and catch-all rows on the reserved key (now the opposite) |
| Offsets 1, 2, 3, 5, 6, 7 retired, never reused | Older exports carry the singleton keys | INVARIANT | - | ENFORCED: `test_counterparties`; leg B |

## Registered limitations

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| Shares value-weighted (UPB, premium, balances), assigned per contract | Count shares differ; portfolio mortgages fatten the count tail | CHOICE | FSOC 2024 Table 1 UPB [P] | REGISTERED: needs count shares over all 1-4 family loans |
| Markets era-flat (2023-2025) | Rocket closed its Mr. Cooper acquisition Oct 2025 (~1 in 6 mortgages); Navient left federal servicing in 2021 | CHOICE | Rocket Companies press release; Maximus novation notice [S] | REGISTERED |
| No servicing transfers or carrier switching | Both happen | CHOICE | - | REGISTERED |
| No subservicing | Mr. Cooper's 6.7M includes it | CHOICE | Mr. Cooper 2024 [P] | REGISTERED |
| Regional carriers drawn nationally | Shares vary by state | CHOICE | NAIC state tables [P, not read] | REGISTERED |
| No on-us lending | A shared internal "bank lender" would recreate a hub (cash-hub-defect-2026-08: a sink must not come from the population it serves); on-us share uncited (48% look beyond their primary bank for a mortgage) | CHOICE | PYMNTS 2024 [S] | REGISTERED |
| No home/auto bundling (three blocks, so State Farm is three accounts) | 47% of home and auto holders bundle; independent draws with shared identity would give ~6% same-carrier, separate blocks 0% | CHOICE | NerdWallet [S] | REGISTERED: candidate shared identity by NAIC group code plus a bundling draw |
| No Default Resolution Group | It serves defaulted federal loans | CHOICE | - | REGISTERED (in the EdFinancial/CRI split) |
| EdFinancial/CRI split uncited | FSA publishes quarterly | UNCITED | FSA "Portfolio by Loan Servicer" | REGISTERED |
| Auto lender ranks 6-25 are tail; no count-vs-balance adjustment | Ford Credit and American Honda rank 6th, 7th | CHOICE | Big Wheels 2025 [S] | REGISTERED |
| Products seed from `kDefaultProductsSeed` (0xB0A7F00D), not `--seed` | Same provider in every run seed | CHOICE | - | REGISTERED, out of scope (pre-existing) |
| Camouflage pool held SSA, IRS, billers, cash endpoints | As unrealistic as lenders | CHOICE | - | Closed by bank-gl-2026-09 ("The camouflage pool, restricted at review") |
| `generateWindow` replays every `emitPerson`, re-deriving lane seeds | Cost only | INVARIANT | measured | REGISTERED: ~0.27 s per 200,000-person replay (2.37 → 2.64 s, +11%); a splitmix hash would isolate identically |

## Measured (pop 200,000, one year, leg A of `test_product_providers`)

| Market | Contracts | Providers used | Largest share (declared) | Top 10 (declared) | HHI |
|---|---:|---:|---|---|---:|
| Mortgage | 92,857 | 300 | 0.0719 (0.0730) | 0.5398 (0.5420) | 348 |
| Auto loan | 75,787 | 200 | 0.0541 (0.0540) | 0.3816 (0.3831) | 200 |
| Student loan | 63,718 | 15 | 0.2833 (0.2864) | 0.9862 (0.9874) | 1,856 |
| Auto insurance | 166,083 | 100 | 0.1870 (0.1864) | 0.7701 (0.7688) | 1,018 |
| Home insurance | 93,767 | 150 | 0.1866 (0.1869) | 0.6222 (0.6248) | 626 |
| Life insurance | 93,131 | 125 | 0.0872 (0.0873) | 0.4649 (0.4615) | 305 |

Before, every market scored 1.0 on largest share (the A3 singleton disarm
must still fail). The largest remaining hubs are top auto insurers, ~31,000
policies (~370,000 premiums a year) each, above MulePatternLearner's 2,048
threshold, so its hub registry and history-withheld stubs stay necessary.

Corpus movement: `tests/golden_run.b2sum` a30c535d... → 42c5c164..., 231,731
rows (tie-order and camouflage cascades; A7 and B5 carry the pre-round
proof). PostgreSQL table goldens need the owner's re-pin; `kTableCount`
stays 43; regenerate the mule-temporal 2024 corpus, snapshot and hub
registry under a new dataset id.

# AMENDMENT: unknown-counterparty-2026-09

The external-unknown catch-all is retired. Three flows paid one account,
`XM1000000001` (catalogue merchant 1 before institutional-providers): the
spending router's external-unknown slot (5% of spending events), P2P whose
contact was missing or unusable, and every funeral. On the 200,000-person
mule-temporal corpus it took 3,814,933 payments from 62% of deposit
accounts (196,989 distinct payers;
`docs/research/counterparty_hubs_2026-09.md`). Owner decision: realistic by
channel; no row lands on one global account.

## The design (written before the code)

Code: `PaymentRouter::emitExternal` and `emitP2p`
(`activity/spending/routing/payments.cpp`), `prepareRouting`
(`simulator/run_planner.cpp`), `funeralPayee`
(`transfers/legit/routines/family/inheritance.cpp`), `registerSystemAccounts`
(`pipeline/stages/entities.cpp`), the mule-temporal rail mapping
(`exporter/mule_temporal/streaming.cpp`).

1. Only the destination changes; channel (`external_unknown` for the two
   spending flows, `bill` for funerals), amount, timestamp and source stay.
   No RNG draw is added: each choice is a draw-free splitmix hash of the
   person id (per row, also the timestamp), as in
   `market/commerce/affinity.hpp` and `actors/instruments.hpp`. Every
   destination is external, so `Ledger::decide` sees the same `dstIdx ==
   invalid` and no balance, decline or retry moves. The channel stays
   because the impostor-scam rail also uses `external_unknown`.
2. A slot row is a paid check with probability `min(1, checkShare(year) /
   slotShare)` (`slotShare` = 0.05 default): 0.6 of the slot from 2024, the
   whole slot through 2020. The rest pays a remote merchant.
3. Checks: one account per external bank (`Role::business`, external,
   serials 1,001,000,001 to 1,001,001,000). Each person has 4 check payees,
   each at a bank drawn once from the SOD table (hash of person and slot);
   each check picks a payee (hash of person and timestamp).
4. Remote merchants: external catalogue records with footprint `online` or
   `nationalService`, by volume weight; a hash uniform per attempt, first
   candidate live at the timestamp, up to 8 attempts, else the check route.
   On-us merchants are excluded (crediting them moves a customer balance).
5. No-contact P2P: Venmo or Cash App (`Role::platform`, external, serials
   1,000,000,001 and 1,000,000,002), one per person by hash, weighted by
   monthly actives. Never fires: `Social::effectiveDegree` clamps every
   person to 3..24 in-population contacts, so the 3.8M catch-all rows were
   the slot and funerals.
6. Funerals: a funeral home in the decedent's home area at the death date
   (relocation-aware, as the cash-point rails read it); MCC 7261,
   `Role::merchant`, external, serial `1,100,000,000 + area * 10,000 +
   ordinal`, picked by person-id hash.
7. Registration: the retired key is unregistered, so
   `validateTransactionAccounts` throws on any row naming it (its removal
   shifts later system records up one). New accounts are appended after the
   entity stage: banks some payee slot uses (rank order), both platforms,
   funeral homes of in-window decedents (key order). All three pools are
   excluded from camouflage P2P.
8. Exporters: mule-temporal maps rails from the channel, so retired rows
   keep rail `unknown`; the standard exporter labels a funeral home
   `merchant_external` / `funeral_services`.

Degree gate (added at review, before its code): a row-share bound passed a
hub small in rows and large in degree (degree is what merges
MulePatternLearner's weakly connected components and sinks PageRank).
Degree = share of the retired rows' distinct source accounts paying a
destination. Check banks and funeral homes are bounded at the largest
bank's payee-slot reach; named counterparties (platforms, remote
merchants) are printed, not bounded, since the degree-sanity target exempts
named hubs and a remote merchant's degree moves with the window. Disarms on
the same rows: one catch-all per flow; a fresh payee bank per check.

## The authority rows

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| Check share of consumer payments: 0.07 in 2016, 0.03 in 2024, linear between, flat outside | By count, 2024: 14% cash, 3% check, 35% credit, 30% debit, 13% ACH, 5% mobile app and other; check 7% in 2016 | MEASUREMENT (anchors); CHOICE (linear) | Federal Reserve Financial Services and Atlanta Fed, 2025 Findings from the Diary of Consumer Payment Choice, Figure 2 [P] | CONFORMS on both anchors |
| A paid check is keyed by the bank of first deposit, one account per external bank | X9.37 Check Detail Addendum A carries a mandatory 9-digit BOFD routing number and no payee-name field; the payee is identified only by OCR | MEASUREMENT | FRB adoption of DSTU X9.37 [P]; Stellar Bank, Payee Positive Pay Best Practices [P] ("OCR is not an exact science"); Plaid returns a null merchant for checks [P] | CONFORMS: verification corrected the research's per-payee key (the spec lists no BOFD account number). The addendum is required when the BOFD is the truncating bank |
| Check payee banks: ranks 1-25 = .11541 .10926 .08062 .04271 .03029 .02429 .02283 .02140 .02050 .01667 .01287 .01188 .01161 .01083 .01035 .01031 .00989 .00988 .00940 .00915 .00894 .00870 .00858 .00854 .00741; cumulative top 50 = .7251, 100 = .7931, 250 = .8651, 500 = .9079, 1,000 = .9451; 1/rank between; pool of 1,000 renormalized by .9451 | JPMorgan Chase, Bank of America, Wells Fargo lead; 4,548 institutions hold $17.405T across 76,727 offices; HHI 388 | MEASUREMENT | FDIC Summary of Deposits, June 30, 2024, by institution (`banks.data.fdic.gov/api/sod`, `YEAR:2024`, `agg_by=CERT`, `agg_sum_fields=DEPSUMBR`, read 25 September 2026) [P] | CONFORMS on ranks 1-25 and all anchors |
| Pool truncated at 1,000 institutions | The 3,548 smallest hold 5.49% of deposits | CHOICE | FDIC SOD 2024 [P] | DEVIATES-BY-CHOICE: mass renormalized; the smallest pooled bank stays low-degree |
| 4 check payees per person | People write checks repeatedly to a few payees (landlord, contractor, relatives, a church) | CHOICE | none found | REGISTERED |
| Remote payees are external `online` and `nationalService` catalogue merchants by volume weight | Deposit-funded remote payments (ACH debit, online bill pay) carry a company name or ID | MEASUREMENT (named); CHOICE (stand-ins) | Nacha Operations Bulletin 2-2024 [P] (name field required); Visa Merchant Data Standards Manual, April 2026 [P]; verification's correction that the slot is deposit-funded remote spending, not card | CONFORMS on the claim |
| P2P platforms: Venmo 64/121 = .529, Cash App 57/121 = .471 | Venmo >64M monthly active accounts Q4 2024; Cash App 57M monthly transacting actives Dec 2024 | MEASUREMENT | PayPal Holdings Q4 2024 earnings call [P]; Block, Inc. Form 10-K FY2024, "Our Cash App Customers" [P] | CONFORMS [Likely]: metrics defined differently |
| Zelle is not a no-contact P2P destination | Zelle needs an enrolled email or US mobile; unclaimed payments expire after 14 days | MEASUREMENT | Zelle FAQ, "What if the person I'm sending money to hasn't enrolled with Zelle?" [P] | CONFORMS |
| A platform appears as a named ACH counterparty | Venmo transfers post as "VENMO-0 CASHOUT" | MEASUREMENT | Venmo Help Center, bank transfer timeline [P] | CONFORMS (page covers cash-outs; the debit is inferred) |
| Funeral homes: `max(1, round(area population * 15,375 / 334,017,321))` per area (0.460 per 10,000) | 15,375 funeral-home establishments (NAICS 812210) in 2022; MCC 7261 Funeral Services and Crematories | MEASUREMENT | Census County Business Patterns 2022 `cbp22us.txt`, NAICS 812210 [P]; denominator as in merchant-selection-2026-08; Visa Merchant Data Standards Manual [P] | CONFORMS: 2,431 homes across 71 US cities (New York 390, smallest 2 or 3) |
| The funeral pays a home in the decedent's area | Funerals are arranged locally | CHOICE | - | ENFORCED (C5) |
| Draw-free, every destination external, channel unchanged; `XM1000000001` never registered and paid | Only the destination moves; nothing lands on one global account | INVARIANT | - | ENFORCED by `test_remote_payees`, `test_product_providers` B2, `test_merchant_churn`, `test_estates` |
| No external account receives >15% of retired rows | The largest correct destination is the largest bank: 0.1221 of the slot when all slot rows are checks (through 2020), 0.6 × 0.1221 = 0.073 from 2024 | INVARIANT (bound a CHOICE sized by that and measurement) | FDIC SOD 2024 [P]; owner decision | ENFORCED by C6 (0.0747). Design said 25% before the P2P fallback measured zero; tightened |
| No check bank or funeral home is paid by more than `1 - (1 - 0.1221)^4 = 0.406` of payers + 4 binomial sigma | Checks reach a bank only through a person's 4 payees (0.1221 = largest bank's pool-normalized share); era-free | INVARIANT | FDIC SOD 2024 [P] | ENFORCED by C6: 0.3749 (JPMorgan Chase, 3,685 of 9,830 payers) vs ceiling 0.4259. Same rows: one catch-all per flow 0.9888; a fresh bank per check 0.7455; every check to the largest bank 0.9888; all red |
| Per-bank check hubs are population-scale: largest ~40% of retired-flow payers (0.3749 in one year, 0.4067 over five in a scratch probe), second 0.3578, third 0.2702 | Degree-sanity target: no non-platform counterparty should connect more than a few percent of deposit accounts; verification: "every check deposited at the same large bank collapses onto one key" | CHOICE (owner decision: one hub per external bank, the BOFD key) | research note's degree-sanity target and its verification; DSTU X9.37 [P] | DEVIATES-BY-CHOICE by ~10x. Bounded by the row above, named for the MulePatternLearner hub registry |

## Registered limitations

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| Deposit share weights payee banks | SOD includes custody and wholesale balances (BNY Mellon, State Street, Goldman Sachs, Morgan Stanley, Schwab) with few consumer checks | CHOICE | FDIC SOD 2024 [P] | REGISTERED |
| No on-us checks | A payee at the simulated bank would be on-us | CHOICE | - | REGISTERED (negligible at simulated sizes) |
| DCPC's denominator is all payments; the router's is spending-slot events | Rent, subscriptions, loans and insurance run on own rails | CHOICE | DCPC 2025 [P] | REGISTERED (approximation) |
| Slot era-flat at 5% | Checks far exceeded 5% before ~2016 | CHOICE | DCPC 2025 [P] | REGISTERED: fraction saturates at 1 through 2020 |
| Payee banks era-flat | Mergers, failures | CHOICE | - | REGISTERED |
| Two P2P platforms; one per person forever; shares era-flat | PayPal P2P and Apple Cash also exist (PayPal reports no comparable US P2P count); many use several apps; Venmo launched 2009, Cash App 2013 | CHOICE | PayPal Holdings Form 10-K FY2024 [P] | REGISTERED (a pre-2009 row has no real P2P app; event count unchanged) |
| US funeral-home density everywhere, the 15 international cities included | Density differs by country | CHOICE | - | REGISTERED |
| Area populations approximate | Catalogue populations are order-of-magnitude right | CHOICE | `synth/geo/geo_data.hpp` [Likely] | REGISTERED: moves home counts, not placement |
| A home is registered for every in-window death, even if the funeral posts after window close | Funeral date drawn later in the family routine | CHOICE | - | REGISTERED: at most deaths in the last 11 days add an unused account |
| Selection hashes unseeded | Same person, same banks and platform across seeds | CHOICE | - | REGISTERED (precedent `market/commerce/affinity.hpp`) |
| Mule-temporal rail stays `unknown`; AML maps `external_unknown` to `wire_transfer` | A check is a check, a platform debit ACH; the wire label is wrong on volume and amount | CHOICE | Fedwire 2024 (209,916,835 transfers, $5.40M average) [P] | REGISTERED: a destination label must relabel both together (impostor rail shares the channel) |
| Funeral keeps the `bill` channel | Funeral homes take card, check or ACH | CHOICE | - | REGISTERED (dedicated channel a registered upgrade) |
| Both platforms always registered, carry no row | The fallback never fires | CHOICE | - | REGISTERED: mule-temporal emits vertices only when observed; only the standard external-account table lists them |
| No home carriers → every funeral to the area-0 home | Only hand-built harnesses lack carriers | CHOICE | - | REGISTERED (row still booked) |
| No-contact P2P fallback unreachable | 3-24 contacts each, so no contact row, out-of-population contact or invalid/self destination never occurs | MEASUREMENT | `relationships/social/builder.hpp` | REGISTERED: 0 fallback rows on both legs; route checked at the selection function (C3) |
| A remote merchant's degree grows with the window | Per-row volume picks give `1 - (1 - w_i)^n` for n remote rows: the volume-as-membership amplification merchant-selection-2026-08 removed from favourites. Leg C: a utility, an online retailer and an insurer paid by 0.5737, 0.5322, 0.3967 of payers in a year; at pop 2,000 over five years from 2020, 0.8169 for the largest, and an online outlet opened April 2023 (9 people pay it on every other row) paid by 1,261 of 1,999 (0.6308) | CHOICE | degree-sanity target exempts named counterparties | REGISTERED, printed by C6. The utility and insurer add little (86% and 74% of leg C already pay them); the online retailer rises from 14% to 59%. A per-person remote payee set would bound it (moves the golden) |

## Where the design deviates from the research note

- Checks keyed per bank, not per payee (no BOFD account number in the spec).
- Remote route uses every external online or national-service outlet (tail
  and core), not the external tail pool alone.
- No-contact P2P to named platforms (owner decision), not the family pool;
  unreachable, moves nothing.
- Exporter labels unchanged; relabelling only legit rows would mark fraud.
- Check hubs break the degree-sanity target ~10x; registered, bounded,
  named.

## Measured (`test_remote_payees`)

Only the destination moved: on the run-golden gate world (pop 2,000, 60
days from 2025-01-01, seed 3405691582) the shared stream's next u64 is
`498e4bde6c6f83ea` before and after, and the order-free digest of legit
rows with retired targets masked is `255b9e814eea12cc` on both (191,721
rows, 190,275 legit, 7,902 retired). Domain predicate: 0 retired rows
outside a finite positive amount, in-window timestamp, internal source and
external destination in one of the four families.

The camouflage pool is the only other movement (bisected): the retired key
was in it. Golden config: no legit row moves. Pop 10,000 over 365 days:
rows 5,254,299 → 5,254,057, legit 5,223,524 → 5,223,265, retired 218,650 →
218,644. Restoring the key at its old position (diagnostic) reproduces the
pre-round digests exactly (`091eb9fa16f044d1` / `255b9e814eea12cc`,
`b43bbafc0e6453cd` / `f228e44897b54871`); keeping it would be a fraud-only
shape.

| Leg C (pop 10,000, 365 days from 2025) | Value | Expected |
|---|---:|---:|
| Retired rows | 218,644 | |
| Slot rows paid as checks | 0.5999 (131,121) | 0.6000 |
| Slot rows to remote merchants | 87,440 | |
| No-contact P2P rows | 0 | unreachable |
| Payee slots at the largest bank / top 10 | 0.1235 / 0.5119 | 0.1221 / 0.5121 |
| People on Venmo | 0.5311 | 0.5289 |
| Funerals / homes paid / busiest home | 83 / 83 / 1 | 0 in the wrong city |
| Distinct retired-row destinations | 1,295 | pre-round 1 |
| Largest destination; next four | 0.0747 (JPMorgan Chase); 0.0699, 0.0502 (banks 2, 3), 0.0421, 0.0368 (remote merchants) | ceiling 0.15 |
| Payers | 9,830 | one account per paying person |
| Check hub degree: largest; 2 and 3 | 0.3749 (3,685); 0.3578, 0.2702 | reach 0.4060, ceiling 0.4259 |
| Named counterparty degree | 0.5737, 0.5322, 0.3967 | printed |
| Largest check writer's distinct banks | ≤4 | 4 payees |

Leg B registers 653 remote payees from record 9,081 (647 banks, 2
platforms, 4 funeral homes for 3 posted funerals), external and ownerless
in one block; 0 of 441 camouflage rows land on one.

Disarms: check-or-merchant uniform from the spending stream reds B2 (rows
191,721 → 142,713); no camouflage filter reds B4 (16 of 440 rows on a
remote payee) and B2; every check to the largest bank reds C6 on both
ceilings (0.5997 of rows, 0.9888 of payers); ignoring the decedent's city
reds C5 (3 wrong-city funerals; unregistered homes drop 71 of 83) and 47
checks in `test_estates`.

Corpus movement: `tests/golden_run.b2sum` `42c5c164...` → `dd0363aa...`,
231,731 rows, re-pinned once at round end; B1 and B2 carry the pre-round
proof. PostgreSQL table goldens need the owner's re-pin (the external
account table loses the catch-all, gains banks, platforms and funeral
homes). `kTableCount` stays 43. Regenerate the mule-temporal 2024 corpus,
snapshot and MulePatternLearner hub registry under a new dataset id;
`XM1000000001` leaves the registry, and its replacements must be read by
degree, not rows. None takes over 7.5% of the former rows, but the three
largest check hubs (JPMorgan Chase, Bank of America, Wells Fargo) each
reach 27-37% of retired-flow payers a year (the largest its 41% reach on
longer windows), and the largest remote merchants 57% a year, over 80% over
five years (mostly billers already paid; one online retailer goes 14% →
59%). These are customer shares (~1.6 deposit accounts per customer). Each
is a named hub the registry and history-withheld stubs must cover.

# AMENDMENT: bank-gl-2026-09

Every fee and interest posting is kept, its contra retyped as a bank-owned
income ledger account. Before, the four bank-originated posting kinds paid
three accounts typed as external business deposits (`Role::business` on
`Bank::external`): card interest and late fees the card issuer
`cash::cardIssuer()` (`XO3000000001`), overdraft fees
`bankFeeCollectionKey()` (`XO4294967041`), overdraft line-of-credit
interest `bankOdLocKey()` (`XO4294967042`). On the 200,000-person
mule-temporal corpus: 2,316,826, 361,991 and 113,386 payments, the issuer
from 73% of card accounts (`docs/research/counterparty_hubs_2026-09.md`).
Owner decision: retype as bank GL (a double-entry charge credits an income
GL; its contra is neither customer nor external party).

## The design (written before the code)

Code: `Session::accrueInterest`, `Session::postLateFee`
(`transfers/channels/credit_cards/session.cpp`); `bankFeeCollectionKey`,
`bankOdLocKey`, `ChronoReplayAccumulator::onLiquidityEvent`
(`transfers/legit/ledger/posting.cpp`); `Ledger::debitAndEmit`,
`Ledger::accrueLocInterestThrough` (`transactions/clearing/ledger.cpp`);
`cash::cardIssuer` (`entities/counterparties/cash_points.hpp`);
`registerSystemAccounts` (`pipeline/stages/entities.cpp`); the camouflage
pool in `makeAccountPools` (`transfers/fraud/injector.cpp`); exporter typing
in `exporter::common::accountType` (AML, aml-txn-edges),
`writeAccountNumberRows` and `writeExternalAccountRows` (standard),
`exporter/mule_temporal/streaming.cpp`, the mule-ml party and IP-edge
writers, the card-fraud view filter.

1. New role `Role::ledger`, internal only, rendered `GL` + 8 digits,
   appended last to the role enum so existing keys keep role and hash.
   `Role::account`/`Role::business` on `Bank::internal` were rejected
   (customer deposit roles; exporters would type the GL as checking).
2. Four income GLs (`entities/holdings/general_ledger.hpp`); one function
   maps channel to GL for posting sites and gates alike:

   | GL | Key | Posting kind (channel) |
   |---|---|---|
   | card interest income | `GL00000001` | `cc_interest` |
   | card fee income | `GL00000002` | `cc_late_fee` |
   | deposit fee income | `GL00000003` | `overdraft_fee` |
   | credit-line interest income | `GL00000004` | `loc_interest` |

3. Only the destination and the copied session change; no draw added or
   removed. The card session posts to its GLs instead of
   `env.issuerAccount`; `onLiquidityEvent` posts the overdraft fee and LOC
   interest to theirs. For these channels `Ledger::decide` reads only the source
   (liquidity channels skip the funding screen; card interest and late
   fees screen the card), so the credit leg cannot flip a decision. One
   ordering effect: the replay sort's tie-break on target key (GL sorts
   last) can reorder a posting against a same-source, same-second row; the
   gate measures it (it reaches no row).
4. What `bankOdLocKey` actually received: interest, never principal.
   `accrueLocInterestThrough` emits matured `loc_interest` through
   `debitAndEmit`, and `onLiquidityEvent` sent every non-overdraft-fee event
   (the family has two members) to that key. An LOC draw is the deposit
   account's cash going negative inside its `overdrafts_` capacity; a
   repayment is any inbound credit.
5. Booked, ownerless, never seeded: GLs are `Bank::internal`, so every
   clearing book has a slot; the opening book seeds only owned records
   (`ownedAccountIndices`), so a GL opens at 0.00 with protection `none`
   and no seeding draw. Only the credit leg posts; nothing debits a GL. In
   the posted book (the post-fraud replay AML exporters read) a GL's
   balance is the sum of its rows. The pre-fraud replay debits the payer
   of an overdraft fee or LOC interest inside `debitAndEmit` with no credit
   leg, so its GL slots carry only card postings; nothing reads them.
6. Registration: the four GLs replace the three retired keys in the system
   block, internal. The card issuer key, `Directory::external.cardIssuer`,
   the blueprint's `issuerAcct` and the `issuerAccount` plumbing through
   the card lifecycle are deleted. The block grows by one record (indices
   shift, no draw).
7. Never a fraud or mule role (victims and mules are persons; flags follow
   the owner). Camouflage excludes the GLs (superseded at review: the pool
   is customer deposit accounts only, below).
8. No customer device or IP on a system posting: `onLiquidityEvent` no
   longer copies the triggering row's session (its `currentTxn_` pointer is
   deleted). Card interest and late fees already carried none
   (`channels::isExternallyInitiated`).
9. Exporters:
   - mule-temporal: `Account` with `account_type` `gl`, `is_external`
     False, `is_mule` 0, no `Party_Owns_Account`, never a Zelle endpoint;
     rail `internal`/`bank`; postings stay payments with a
     `Transaction_To_Account` edge on the GL (Oracle BD "Account plus Offset
     Account"); consumers filter on `account_type`.
   - AML and aml-txn-edges: internal `Account`, type `general_ledger`, no
     customer edge or counterparty vertex; balance = posted-book income in
     the window.
   - standard: in `accountnumber.csv` with `is_external` 0, no
     `HAS_ACCOUNT` row, not in `external_accounts.csv`.
   - card-fraud: unchanged (purchase channels only).
   - mule-ml: an ownerless party row with blank identity; account IP edges
     and the canonical IP histogram skip `0.0.0.0` (see "Also fixed").

Gate `tests/test_bank_ledger.cpp` on the run-golden gate world: pins the
shared entity stream (covering `makeCatalog`'s draw count) and an order-free
legit-row digest (target masked on the four kinds, device and IP on the two
liquidity kinds) to the pre-round build; every fee and interest row targets
its GL and nothing else touches one; GLs internal, ownerless, unflagged, one
block, absent from camouflage and fraud rows; no posting has a device or IP;
each GL opens at zero and closes at the sum of its rows.

## The authority rows

[P] = primary, [S] = secondary, from the research and verification passes
(summarized in `docs/research/counterparty_hubs_2026-09.md`, section "Fees
and interest have no counterparty").

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| Every fee and interest posting kept: customer debited, bank income GL credited | Double-entry: FLEXCUBE's CLIQ debits CHG_BOOK and credits CHG_INCOME for every charge basis (ad hoc statement, cheque issue and return, stop payment, statements, item count, turnover); ILIQ debits ICDB-BOOK and credits ICDB-PNL | MEASUREMENT (accounting) | Oracle FLEXCUBE Universal Banking Interest and Charges User Guide 14.5.3.0.0 (Nov 2021) [P], confirmed; Temenos Transact CATEG entries hit the P&L category [P] | CONFORMS |
| Four GLs, one per kind | FLEXCUBE maps each accounting role to its own GL head per product and event, interest PNL apart from CHG_INCOME ("a commission income GL for the branch"). Banks also pool kinds (Fiserv's app moves Premium Overdraft fees off the NSF fee GL) or split by branch or currency | CHOICE | FLEXCUBE 14.5.3 [P] (per-branch split rests on that one example); Fiserv DNA Premium OD GL Transfer app [P]; Jack Henry jXchange balanced-transaction tutorial, a separate $25 fee-income GL one of two options [P, corrected] | DEVIATES-BY-CHOICE: the research's set, no branch, product or currency split |
| `Role::ledger`, `Bank::internal`, ownerless, rendered `GL` | FLEXCUBE posts directly only to internal leaf GLs, with an Income category; verification: FLEXCUBE "internal" GLs also hold customer balances, so the discriminator is internal leaf plus Income. Temenos ledger accounts sit in categories 10000-19999, internal accounts carry no customer. Fiserv DNA's FCRM extract space-fills Customer_Number on a GL | TYPOLOGY | FLEXCUBE General Ledger 14.1 [P, corrected]; Temenos developer portal and Accounting Events Lifecycle Guide (22 May 2023) [P]; Fiserv DNA FCRM extract specification (Oct 2023) [P] | CONFORMS: the role is the income discriminator, outside every customer and external role. Supersedes the card-issuer part of cash-hub-defect-2026-08's distinct-keys row |
| Credited, never seeded, never debited | A GL's balance is the income posted to it | INVARIANT | - | ENFORCED: B2 (opens 0.00, no buffer), B5 (closes at its rows' sum in the replayed posted book) |
| Exported as payments to the GL, typed `gl` / `general_ledger`, not dropped | Oracle BD models a back-office transaction as Account and Offset Account; the FLEXCUBE-to-Mantas feed sends internal movements as back-office transactions and only Nostro, Savings, Current and Deposit accounts as account records; Fiserv DNA's FCRM keeps GL-leg transactions in AML profiling by default (Exclude_From_Profile N); Verafin monitors customer-to-GL transfers for insider fraud | TYPOLOGY | Oracle BD User Guide 8.1.2.10, Administration Guide G27520-09 (Jan 2026) [P]; FLEXCUBE Mantas Interface 12.0 (May 2012) [P]; Fiserv DNA FCRM (Oct 2023) [P, corrected]; Nasdaq Verafin, 2 Aug 2019 [P] | CONFORMS: kept and typed. The research first leaned to dropping; verification found real feeds keep these legs |
| Postings carry no device or IP | A bank-posted entry has no customer session | CHOICE (inference) | the research marks it inference | ENFORCED by B4. Pre-round, every overdraft fee copied the session (346 rows at the gate config, 11,788 at pop 10,000 over a year) |
| GLs never victim, mule, fraud or camouflage | A customer cannot pay into a bank income ledger; FFIEC controls on internal concentration accounts include barring customer access (it does not address income GLs) | INVARIANT | FFIEC BSA/AML Examination Manual, Concentration Accounts (2024 web build via Wayback) [P], by analogy | ENFORCED: B2 (no flag), B3 (no fraud or camouflage row; `fraud::camouflageEligible` rejects every GL) |
| Camouflage P2P pays only customer deposit accounts (`Role::account`) | A cover row must match the flow it mimics; legitimate P2P pays nothing else (11,889 of 11,889 rows at the gate config; 335,953 of 335,953 at pop 10,000 over 365 days); cover transfers to card accounts, billers, government payers or external family, client or business accounts were fraud-only | INVARIANT | measured | ENFORCED: B3 (added at review), predicate = exactly the `Role::account` records; 0 of 261 camouflage P2P rows elsewhere. Disarm (the old exclusion list): 6,229 of 10,581 records disagree, 177 of 261 rows off a deposit account |
| Only a posting's destination and a liquidity posting's session move; no draw added | Retyping moves no other row | INVARIANT | - | ENFORCED: A1 (next u64 `498e4bde6c6f83ea`, both builds), A2 (fraud-free masked digest `cf3b75546c4c76da` over 190,402 rows; postings 1,397 / 412 / 346 / 41; both builds) |
| `XO3000000001`, `XO4294967041`, `XO4294967042` never registered or paid | Older exports carry them | INVARIANT | - | ENFORCED by B1 |

## Registered limitations

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| No per-customer overdraft line-of-credit account | Reg E excludes from "overdraft service" a line of credit under Reg Z, OD LOCs included, so an OD LOC is the consumer's own credit account (draws LOC → deposit, repayments back, interest debits the LOC and credits interest income) | CHOICE | 12 CFR 1005.17(a) [P]; per-customer reading is the research's inference, called sound | REGISTERED: LOC principal is negative cash inside `overdrafts_`, so draws and repayments are not postings, and interest debits the deposit account |
| No NSF or returned-item fee | Declines can carry an NSF fee | CHOICE | CFPB overdraft and NSF report (Dec 2023) [P] | REGISTERED: deposit fee income is overdraft fees only |
| Card fee income is late fees only | Annual, cash-advance, balance-transfer and foreign-transaction fees exist | CHOICE | - | REGISTERED |
| No deposit interest credited, no statement or service fee | These reach nearly every Berka account: interest credited (UROK) 99.0%, statement fee (SLUZBY) 97.7% of 4,500 | CHOICE | Berka PKDD'99 trans table, recomputed [P] | REGISTERED: PL posts only debit interest and penalty fees; credited interest would need an interest-expense GL as source |
| The income GLs are population-scale hubs | At pop 10,000 over a year: card interest GL 54.1% of card accounts, card fee 40.5%; deposit fee 16.3%, credit-line interest 5.0% of deposit accounts. The research measured 73% of card accounts on the issuer and did not verify the real share incurring interest or fees | CHOICE (owner decision: retype, keep) | counterparty-hubs note | REGISTERED: a per-kind income GL is a hub in the bank's own books; typed so MulePatternLearner can drop or separate it |
| GL balance is window income | Real income GLs carry year-to-date balances and close to retained earnings | CHOICE | - | REGISTERED: opens 0.00 at window start |
| Camouflage P2P draws a uniform deposit account per row, not a known payee | Legitimate payees repeat: 54.7% of legit P2P rows repeat a (sender, payee) pair at the gate config, 86.6% at pop 10,000 over 365 days, vs 0.0% and 0.03% for camouflage; predates this round | CHOICE | measured | REGISTERED, out of scope: needs a per-ring payee set on the ring lane (re-points every cover transfer) |
| Cards are on-us | Cards often sit on a separate processor (Fiserv DNA treats them as external-major); whether card interest reaches a deposit-core AML feed is unverified | CHOICE | research's "not verified" list | REGISTERED: `Role::card` is internal, so card income GLs are on-us |

## Where the design deviates from the research note

- No new account-class, GL-category or bank-party fields (the research
  proposed `account_class = INTERNAL_GL`, `gl_category = INCOME`, an ORG
  owner and `is_customer = false`): `Role::ledger` exists only for income
  GLs, exporters carry it as account type, and the GL is ownerless like
  every ownerless account; new columns would change four exporter schemas
  for no new information.
- No `txn_kind`, `initiated_by` or `counterparty_class`: the channel names
  the kind, the rail is `internal`/`bank`, the account type flags the leg.
- No per-customer OD LOC account (the key never received principal).
- Dropping or separating GL edges belongs in MulePatternLearner.

## Also fixed, because this round would otherwise have made it worse

The mule-ml exporter wrote an account IP edge for every row, including
sessionless rows rendering `0.0.0.0` (`network::format` never returns
empty, so the emptiness guard never fired), and counted it in the canonical
IP histogram. Dropping the copied IP would have linked every overdrafting
account to one `0.0.0.0` vertex. Both now skip the sentinel
(`infra_edges.hpp`, `canonical.cpp`), also removing existing `0.0.0.0`
edges from card accounts and external sources of externally initiated
rows. Gate Part C.

## Measured (`test_bank_ledger`, run-golden gate world: pop 2,000, 60 days from 2025-01-01, seed 3405691582)

Leg A (fraud off) matches the pre-round build (the staged
unknown-counterparty tree, exported with `git checkout-index` and built
separately) on A1 and A2. Domain predicate: 0 postings outside a finite
positive amount, in-window timestamp, right-kind source and the right GL.

Camouflage pool movement (bisected): the three retired keys left the pool.
Fraud on at the gate config: rows 191,721 → 191,722, legit 190,275 →
190,276. Pop 10,000 over 365 days: rows 5,254,057 → 5,253,962, legit
5,223,265 → 5,223,210 (card interest 85,672 → 85,668, fees 22,932 →
22,928). Restoring the pre-round pool (diagnostic) reproduces the masked
digests exactly (`35f81eb8a5ac1c7a` over 190,275 legit rows,
`b96634da5e626522` over 5,223,265).

| Leg B (fraud on) | Rows | Distinct payers | Share of accounts | Closing balance |
|---|---:|---:|---:|---:|
| `GL00000001` card interest income | 1,396 | 1,390 | 0.2655 of 5,236 card accounts | $27,061.15 |
| `GL00000002` card fee income | 412 | 411 | 0.0785 of card accounts | $16,175.12 |
| `GL00000003` deposit fee income | 346 | 208 | 0.0655 of 3,177 deposit accounts | $13,619.87 |
| `GL00000004` credit-line interest income | 41 | 41 | 0.0129 of deposit accounts | $170.76 |

GLs are records 3,013-3,016; 0 of 1,005 fraud rows and 0 of 441 camouflage
rows touch one; 0 of 2,195 postings carry a device or IP; each closes at
its rows' sum.

| Scratch leg (pop 10,000, 365 days from 2025) | Rows | Distinct payers | Share of accounts |
|---|---:|---:|---:|
| card interest income | 85,668 | 14,234 | 0.5406 of 26,330 card accounts |
| card fee income | 22,928 | 10,675 | 0.4054 |
| deposit fee income | 11,788 | 2,594 | 0.1631 of 15,909 deposit accounts |
| credit-line interest income | 5,884 | 803 | 0.0505 |

0 of 16,359 camouflage rows touch a GL at that scale.

Disarms: session copied back reds B4 (387 rows, LOC interest included);
LOC interest to the deposit fee GL, or late fees to the card interest GL,
red A2, B1, B5; GLs registered external red B2; no GL clause in the
exclusion-list predicate reds B3 (the row count alone passed: ~440 rows put
under one expected row on four GLs); no mule-ml sentinel guards reds all
three Part C checks (canonical IP becomes `0.0.0.0`); no `gl` type reds
`test_mule_temporal`.

Corpus movement: `tests/golden_run.b2sum` `dd0363aa...` → `754ab129...`,
231,731 rows, re-pinned at round end. `test_remote_payees` B2 also masks
this round's two fields: `255b9e814eea12cc` over 190,275 legit →
`dbfdd62a2aca2846` over 190,276; under the extended mask the pre-round
build and the diagnostic pool both score `815871576f03eef2` over 190,275
(the camouflage cascade alone). PostgreSQL table goldens need the owner's
re-pin: standard `external_accounts` loses the three keys, `accountnumber`
gains the four GLs; AML Account gains four `general_ledger` rows,
counterparty tables lose three; mule-ml loses `0.0.0.0` edges and some
canonical IPs change. `kTableCount` stays 43. Regenerate the mule-temporal
corpus, snapshot and hub registry under a new dataset id; hub handling
should drop the four `gl` accounts (`is_external` False) or treat them as a
separate edge type.

## The camouflage pool, restricted at review

`fraud::camouflageEligible` claimed to exclude destinations no legitimate
P2P pays, yet admitted every other record, card accounts included, and
`camouflage::generate` re-drew only merchants. At the gate config (fraud
on) 162 of 263 camouflage P2P rows (62%) paid another person's `Role::card`
account, while all 11,890 legitimate P2P rows paid a `Role::account`, so
the destination alone marked the row. At pop 10,000 over 365 days: 5,644 of
9,538 (59%) on cards, 3,202 on deposit accounts, 692 on external family,
business, client, brokerage, employer and landlord accounts. The leak
predates this round (HEAD's pool held every record). It ships here, not in
a separate round, because this round already re-pins
`tests/golden_run.b2sum` and the movement is the same attributed cascade.

1. The predicate is positive: eligible exactly when `Role::account`
   (internal only; `synth::accounts::makePack` is its only producer, so
   every such record is owned), `Legit::p2p`'s destination set. It subsumes
   the named exclusions and closes institutional-providers' "pool held SSA,
   IRS, billers and cash endpoints" limitation.
2. The merchant re-pick loop is deleted (no merchant in the pool; byte
   neutral).
3. No draw added; each P2P row still spends one `choiceIndex` on
   `{"fraud", "ring", ring, "camo"}`; the shared stream is not read (A1
   keeps its pin). What moves: ring P2P destinations, later draws on the
   ring lane (self-destination skips at a different rate), and through
   balances later accepts and declines, legitimate included.
4. Gate: B3 checks the predicate admits exactly the `Role::account` records
   and every camouflage P2P row targets one; the exclusion list reds both.

Measured (scratch probe hashing every gate-leg row; pre-fix from the staged
tree): gate config camouflage P2P 263 rows (89 deposit, 162 cards, 12
external client, family and business) → 261 all deposit, pool 9,242 →
3,013 records; pop 10,000 over 365 days 9,538 → 9,544 all deposit, pool
45,225 → 14,943. Camouflage P2P to an already-dead owner 50 → 81 rows
(0.52% → 0.85%), to a not-yet-joined owner 29 → 41 (0.30% → 0.43%), vs
0.76% and 0.42% for legitimate P2P (2,539 and 1,412 of 335,953): toward the
legitimate rows, not past them. Loop kept or deleted scores the same
(all-row `d86a257b719200d2`, legit `c8ffaaa5db6a50b3` at the gate config;
`d70316fb523b8639`, `9191afb392a10e4c` at pop 10,000); fraud-free pins hold
(A1 `498e4bde6c6f83ea`, A2 `cf3b75546c4c76da`).

Cascade: gate config rows 191,722 → 191,725, legit 190,276 → 190,273,
camouflage 441 → 447 (bills 109 → 108, salary 69 → 78), fraud 1,005; GL
counts and balances unchanged. Pop 10,000: rows 5,253,962 → 5,253,975,
legit 5,223,210 → 5,223,454, camouflage 16,359 → 16,106 (P2P +6, bills +11,
salary −270), fraud 14,393 → 14,415. Salary moves most: its coin follows
the P2P branch on the ring lane, re-rolling which ring accounts get a
payroll stream (12-52 rows a year each). Fraud moves because the illicit
budget counts camouflage rows (`illicitBudgetBase` in `injector.cpp`).

Re-pins: `test_remote_payees` B2 `dbfdd62a2aca2846` (190,276 legit, 7,902
retired) → `75d7339bc4b70b82` (190,273, 7,900); `tests/golden_run.b2sum`
`754ab129...` → `0642235c...`, 231,731 rows, at round end; PostgreSQL table
goldens move with camouflage destinations (owner re-pins). In
mule-temporal, camouflage P2P is now deposit-to-deposit P2P, Zelle-eligible
under the same draws as legitimate P2P, no longer an unknown-rail payment
into a card account.

# AMENDMENT: atm-spread-2026-09

A selection bug fixed, model unchanged. Residents sit at the area centroid,
so the area's ATMs, depositories and check-capture points tie at distance
zero; `buildNearbyPoints` (`transfers/legit/blueprints/plans.cpp`) broke
ties by pool index into one four-point list per area, so a city shared its
four lowest-numbered points (pop 200,000: New York used 4 of 41 terminals,
the busiest ~300,000 withdrawals a year;
`docs/research/counterparty_hubs_2026-09.md`, change 4).

Now each person gets their own nearest four: nearer distance groups whole;
the straddling group stored whole, each person taking a consecutive window
from `splitmix(splitmix(person ^ railDomain) ^ area) % groupSize`
(`cash::NearbyIndex::select`, `entities/counterparties/cash_points.hpp`).
No sampling, no parameter change (4 points, 82% home terminal, 13.5 ATMs
and 2.5 depositories and check-capture points per 10,000), nothing stored
per person (at most three fixed keys and one tie group per area and rail);
resolved at event time, so it follows relocation. The revenue lookup
(`RevenueCounterparties::cashDepositoriesFor`) switched too. `terminalFor`,
`depositoryFor`, `stableExternalPoint` pick inside the set;
`checkCaptureFor` and `cryptoVenueFor` (no callers) deleted.

## The authority rows

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| Own nearest four; cut ties by a per-(person, area, rail) hash window | Customers use a city's terminals, each a small nearby set | CHOICE | Diebold Nixdorf Advisory Services blog (Weston, 2021): ~2.5 locations a year [P, no dataset]; Burra and Lokanathan, arXiv 2011.08721 (UN Global Pulse, 2020): median 0.66-7.21 km between a customer's ATMs by income [S, abstract]; Ueda, CIGS WP 22-003E (2022): Mizuho customers attach to sites, ~1,300-2,000 repeat users per terminal [P, corrected from 4,000] | CLOSED: pop 200,000 uses all 270 ATMs (181 before); New York's busiest own terminal 1.08× fair share (10.29) |
| 4 points (`cash::kNearbyCount`), 82% home terminal, else one of three neighbours (`terminalFor`) | Research: 3-5 nearest, 2-4 ATMs a year, a primary at 50-70% (assumed) | CHOICE | round research note (atm-employer-landlord); Diebold Nixdorf 2021 [P] | REGISTERED, unchanged (82% above 50-70%, not tuned) |
| 13.5 ATMs per 10,000 | 451,500 (2022) / 333.3M; ATMIA 520,000-540,000 gives 15.6-16.2 | MEASUREMENT | Euromonitor via Payments Dive 2023-06-23; ATM Marketplace (ATMIA), 12 Sep 2023 [S] | CONFORMS (cash-hub density row amended) |
| Withdrawals per terminal: mean ~27,400 (0.88 × 3.5/mo × 12 = 37 per person-year over 13.5 per 10,000); p50 28,150 at pop 200,000 before screen and deaths | 3.4 billion US withdrawals in 2024 (average $210) over 451,500-540,000 terminals = 6,296-7,530 each, 10.2 per person-year; Cardtronics 757/month (9,084/yr); active debit holders 1.9 ATM transactions/month (2023) | MEASUREMENT | Fed Payments Study CY2015-24 [P, verified 2026-09-25]; Cardtronics 10-K 2019 [P]; PULSE 2024 [P] | DEVIATES, not tuned: the 3.6× gap is withdrawal frequency (`atm::Config` userP 0.88, U{1..6}/month, UNCITED); `docs/cash_hub_defect.md` forbids tuning volume in a selection round |
| Busiest terminal ≤ 50,000 a year (G6, pre-screen) | Busiest ≈ 5-10× a ~7,000 mean (~20,000-50,000); Australian bank ATMs ~128 transactions a day (with balance enquiries) vs ~29 independent | MEASUREMENT (derived) | research note; RBA Bulletin March 2016 Table 2 [P, corrected: bank-owned average incl. enquiries] | CONFORMS: 44,136 (pop 200,000), 44,622 (500,000) vs 298,157, 721,489 before. The note's ≤ ~40,000 needs the regenerated corpus |
| Distinct accounts per terminal (printed) | Bank of America 14,893 ATMs / ~69M clients ≈ 4,600; Mizuho ~1,300-2,000 | MEASUREMENT | Bank of America 10-K 2024 [P]; Ueda 2022 [P] | CONFORMS in magnitude: p50 2,541, max 3,888 (max 26,780 before) |
| No draw added (window start a pure hash; `buildCounterpartyAccess` draws nothing; `burnRetiredCounterpartySelection` first, same count; `synth::counterparties::make` and `makeCatalog` untouched) | Streams stay put | INVARIANT | - | ENFORCED: every unit and scale blueprint checks `addCounterparties` leaves its stream (12 tied points included); B5 (`498e4bde6c6f83ea`) and `test_bank_ledger` A1 hold (re-pinned later by counterparty-sizes; its income-free `ddfd735e74a97cd2` agrees on this tree) |
| ≤4 points per rail ⇒ selection equals the former list | Pools that cannot show the defect do not move | INVARIANT | - | ENFORCED: A3 element-wise vs (distance, pool index) for four points in four areas, seven points ending at the cut, the two-point fallback, no home area; pop 2,000 moves nothing |
| One hash domain per rail (`kWithdrawalSetDomain`, `kDepositSetDomain`, `kCheckSetDomain`) | Depository and check pools match, one domain would align them | INVARIANT | - | ENFORCED: A6, 1,835 of 2,000 differ (shared domain 0) |
| Business takings use the household set | The revenue book resolves the area itself | INVARIANT | - | ENFORCED: scale gate requires `cashDepositoriesFor` = `depositPointsFor` for everyone and drives the depository rail through it |
| Every emitted ATM, cash-deposit and check-deposit endpoint is in the owner's set (predicate for the `golden_tables_aml.md5` re-pin) | `atm.cpp`, `deposits.cpp`, revenue takings pick inside the set | INVARIANT | - | ENFORCED in `test_cash_boundaries` at pop 300 × 730 d (2 points per rail: membership cannot fail), pop 10,000 × 60 d from 1991-01-01 seed 7 (the AML golden world: 14 ATMs, 3 and 3) and pop 20,000 (27, 5, 5): set size min(4, pool), endpoint inside, ATM rows exactly `terminalFor`. Pop 10,000 requires smaller-than-pool ATM sets (all 58,899) and 66 rows outside the former list; pop 20,000 smaller sets on all rails (118,243 ATM, 13,703 cash, 1,857 check) |

## Registered limitations

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| Out-of-area fill taken whole | Small neighbours' fill lands on a one- or two-terminal area (ATM busiest/mean ~1.6, depository ~2.6 at pop 500,000); home can be in a neighbouring city | CHOICE | - | REGISTERED; why G5 is ATM-only (depositories 5.70 armed vs 6.47) |
| Equal terminal weights | 58% of US withdrawals are on-us; Australian bank ATMs were 45% of the fleet, 75% of withdrawals; busiest ~5-10× mean, bank branches 3-4× weight | CHOICE | PULSE 2024 [P]; RBA March 2016 [P]; research note | REGISTERED: busiest/mean 1.61; re-measure G4 (2.5) if weighted |
| No distance decay inside the four | ~1-1.5 km urban, 4-6 km rural; centroids give no distance | CHOICE | Bank of Canada SDP 2023-28 [P] | REGISTERED |
| Window keyed by person | Coresidents differ | CHOICE | - | REGISTERED |
| Ring order is pool order | Banded window overlap | CHOICE | - | REGISTERED |
| Placement by initial home-area quantiles (`representativeAreas`, `synth/counterparties/make.hpp`) | Movers into empty areas use fill | CHOICE | - | REGISTERED |
| Withdrawal frequency 0.88, U{1..6}/month | 37 per person-year vs 10.2 (Payments Study) and 1.9/month (PULSE) | UNCITED | Payments Study CY2024 [P]; PULSE 2024 [P] | REGISTERED, not tuned |
| Area missing from the index with pool >4 gets a hashed four | Unreachable: every home and relocation area is indexed; pools ≤4 return whole | INVARIANT | - | REGISTERED |
| Takings resolve the set at month start (`activity/income/revenue/generate.hpp`) | Mid-month movers use the old area that month | CHOICE | - | REGISTERED: accepted by the corpus check; 0 such rows |
| The scale gate is not a corpus | Homes per person, 0.88 hash users, 42 withdrawals a year, no screen or deaths; check rail via `depositoryFor` for `stableExternalPoint` | CHOICE | - | REGISTERED: production busiest figure needs the regenerated corpus |

## Where the implementation deviates from the design

1. `LocalPoints` is not a range (a `std::span<const Key>` from a temporary
   could escape): spans only via `span() const &`, `span() const &&`
   deleted (caught two calls in this round's test).
2. G5 ATM-only (a depository band could not fail); G6 added.
3. The scale gate checks revenue vs household lookups for everyone and a
   second build against the first (A4 at scale).
4. Dead pickers deleted.
5. An empty ranking gets no entry and falls back to the pool (as before).
6. Three corpus legs, not one: at pop 300 an emitter reverted to the pool
   passed (found at review); pop 10,000 and 20,000 can fail, and ATM rows
   are checked against `terminalFor`.

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

Bands (armed): G1 ≥ 0.99, G2 ≤ 1.5, G3 = 1, G4 ≤ 2.5 (ATM) / 4.0, G5 ≤ 3.5
(ATM), G6 ≤ 50,000. Preconditions: an area with more than four own points
and reference G2 ≥ 5.0 (ATM) / 3.0, so a leg that cannot show the defect
fails. Pop 10,000 differences come from small areas straddling a
neighbouring group.

Disarm (window start 0 = the former selection): A1 reds (4 of 12 tied
points used); the scale gate's armed line equals its reference and G1 reds
first at ATM pop 200,000 (181 of 270); the pop 10,000 corpus leg finds no
row outside the former list. Each emitter reverted to its pool reds a leg
pop 300 alone would pass: `atm.cpp` pop 10,000 on membership (and pop 300
on the exact pick); household cash deposits, check deposits and business
takings (pools of 3 fit at pop 10,000) pop 20,000.

Corpus movement: `tests/golden_run.b2sum` (pop 2,000; every group fits)
streams `0642235c...` over 231,731 rows, bank-gl's pre-change digest: no
byte moves (pin still `a30c535d...`, re-pinned at round end).
`golden_tables.md5` should not move. `golden_tables_aml.md5` (pop 10,000,
14 ATMs) should: endpoint ids move for straddling residents (busiest 36,002
vs 38,303), not counts or amounts (66 of 58,899 ATM rows in the gate copy);
the owner re-pins with PostgreSQL, paired with the membership predicate and
the scale gate. `golden_tables_card_fraud.md5` should not move (not
verified). Mule-temporal at pop 200,000: all 270 ATMs, 50 depositories and
50 check-capture points observed; busiest ATM ~298,000 → ~44,000 a year
pre-screen, distinct payers ~26,800 → ~3,900; the median (~28,000) stays
far above the 2,048 hub threshold (more, smaller cash hubs). Regenerate the
2024 corpus, snapshot and hub registry under a new dataset id.

# AMENDMENT: counterparty-sizes-2026-09

Payroll picked uniformly among `25 per 10,000 people` employers and rent
among `12 per 10,000` landlords: at pop 200,000 ~480 external employers
paid ~308 people each and 240 landlords collected from ~292 tenants each,
"individual" ones included (`docs/research/counterparty_hubs_2026-09.md`,
change 5). Both rosters are now published size tables thinned to the
population (`synth/counterparties/size_law.hpp`):

    N_c = clamp(round(P x s x m_c), 1, F_c)

P population, s payer share (0.74 workers, 0.35 renters), m_c the class's
share of jobs or units, F_c its real member count.

- Employers: 17 classes (13 SUSB 2022 rows, the 20,000+ row as a rank-size
  tail, federal, state, local government), all external.
- Landlords: 8 classes (seven RHFS 2021 columns, 150+ less the NMHC 2024
  Top-50, the Top-50 as a rank-size row); type from the class's unit mix on
  `{"landlord_type", serial}`, in-bank coin last.
- Picks go through a draw-free class-then-member pool
  (`entity::counterparty::SizedPool`, `SizedKeys`): `growth::pickSized` and
  `pickSizedDifferent` replace `pickOne`/`pickDifferent` with the same draw
  counts; O(1) serial lookup replaces `std::find`. Camouflage salary uses
  the same pool and its employer's schedule.
- Pop 200,000: 89,231 employers, 66,658 landlords (was 500, 240); pop
  500,000: 200,556 and 155,581. Amount and cadence laws, tenure and
  per-person draws unchanged; the post-income shared stream moves (salary
  jitter draws per payday).

## The research tension: a metro count against a national sample

Verified research puts a 200,000-resident metro bank at ~3,000-6,000
employer firms (SUSB 188 per 10,000 gives 3,760) and 3,000-7,000 rent
payees; this law gives 89,231 and 66,658. Different frames; the law stays:

1. A metro bank sees every local firm with its full workforce.
2. PL's 200,000 are a thin national sample in 71 home areas (~2,800 each,
   the merchant-selection-2026-08 geography): distinct employers met ≈ min(sampled
   workers, firms) per class. Measured: 148,000 workers use 58,611 of
   89,231 employers.
3. The metro count rebuilds the defect: 3,760 employers give the 3,361
   firms below 20 employees 148,000 × 0.139 / 3,361 = 6.1 payees each
   (real class mean 3.8 = 21,950,184 / 5,720,093), across cities; 5,000
   rent payees for ~70,000 leases is 14 tenants per landlord vs ~two units
   behind an individual landlord (IRS).
4. Concentration follows the frame: the research's metro figures (largest
   employer 4-8% of jobs, top 10 20-30%, managers with 100-3,000
   households) vs the national sample: federal government 1.9% of workers
   (2,829), largest private 1,737, top 10 6.4%, largest landlord a Top-50
   owner with 148 tenants.

A regional-bank mode (co-located employers and landlords) would give the
metro figures; registered, not built.

## Renters per household against renters per person

L-5's renter share counts people (each on own lease), ACS counts
households: at 0.125-0.15 renter households per person, PL has 2.3-2.8× as
many payers, and the old per-capita rent check is superseded (L-5). The
roster is sized to the 0.35 PL draws (sizing to households would give
landlords 2.3-2.8× more tenants than units); the lease count inherits L-5's
overstatement. Changing it moves every rent row: owner decision.

## The verification's four parameter corrections

| Correction | Disposition |
|---|---|
| (a) Processor payroll is ~17-20% of workers (ADP ~8% of payer firms), not of payers | Not modelled: each salary row names its employer, which Nacha keeps recognizable when a processor sends the file; shared Company IDs across employers unverified. REGISTERED |
| (b) ≤55% of 5-49 unit tenants route to a manager (Terner's 55% not owner-managed includes owner-employed superintendents) | No manager payee; the corporate type (95% portal rent, `RentRouter`) is the proxy: 0.345 of 5-49 unit tenants (`test_counterparties` RHFS check); 25-49 alone 0.796 (LLCs and partnerships type corporate from 25 units). CONFORMS on the 5-49 aggregate |
| (c) Only the 0.38 individual share was supported | Types per size column from CRS R47332 Table 3: individual = individual investor + trustee + tenant in common; small LLC = LLC/LP/LLP + general partnership below 25 units; corporate = every other form. Renter-weighted 0.433 / 0.138 / 0.429; reported-only 0.447 / 0.141 / 0.412 (not-reported units are mostly large corporate). Individual exceeds 37.6% because it adds trustees (2.1%) and tenants in common (1.2%). CONFORMS |
| (d) Portal rent may show a processor (AppFolio settles via its clearing bank) | Not modelled; a cross-landlord processor hub is unmodelled too. REGISTERED |

## The authority rows

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| SUSB 2022 enterprise sizes, 13 rows + 20,000+: 6,395,635 firms, 135,748,407 employees | Firms <20 hold 16.17% of private jobs; 10,000+ hold 30.59% | MEASUREMENT | Census SUSB 2022 `us_state_naics_detailedsizes_2022.xlsx`, `us_naicssector_large_emplsize_2022.xlsx` (2025-04-10) [Certain; verification recomputed <20 16.17%, 500+ 54.14%, 5,000+ 36.32%; 20,000+ not re-checked] | CONFORMS (`test_counterparties`) |
| Government 14.0% of payroll: federal 2.9M, state 4.5M, local 13.6M of 150.0M; private rows × 0.86 | ~14-15% of workers | MEASUREMENT | BLS QCEW 2022 [Certain]; BLS Employment Situation B-1, Aug 2026: 14.66% [Certain] | CONFORMS (QCEW covered vs CES nonfarm) |
| 90,837 local-government payers | Census of Governments 2022 | MEASUREMENT | census.gov `govtorg2225` [Likely: snippet] | UNCITED, verify |
| Federal payroll one payer; 50 equal states; uniform local governments | Agencies pay separately; sizes differ | CHOICE | ASPEP 2022 (not read) | REGISTERED |
| 20,000+ row: s_r = 20,000 × (546 / r)^b, b = 0.7201 (Pareto α 1.3886); rank 1 = 1.87M | Heavy employer tail; Walmart ~1.6M US associates | TYPOLOGY + CHOICE | SUSB [Certain]; Walmart [Guessing; FY2023 10-K would settle it] | REGISTERED: rank 1 banded [1.2M, 2.4M] unfitted; ranks 3-6 (0.85M-0.52M) heavy vs ~0.5M real |
| RHFS 2021 (2020 stock): units by size 16,550 / 6,065 / 5,470 / 2,725 / 1,055 / 1,296 / 16,387k; properties 16,550 / 2,215 / 419 / 75 / 15 / 11 / 45k; units by ownership and size | Individuals own 37.6% of units (70.2% in 1-4 unit); LLC/LP/LLPs 40.4% (67.8% in 100+); 85.6% of properties single-unit; 37.8% of units in 50+ | MEASUREMENT | CRS R47332 (Keightley, 2022) Tables 1, 3 [Certain]; HUD/Census RHFS 2021 infographic [Certain] | CONFORMS (`test_counterparties`) |
| Type per column; renter-weighted 0.433 / 0.138 / 0.429 | Individuals dominate small properties, entities large ones | MEASUREMENT (derived) | CRS R47332 Table 3; correction (c) | CONFORMS; replaces 0.38 / 0.15 / 0.47 (a count from a unit share) |
| NMHC Top-50: 2.4M units, rank 1 108k (Greystar), s_r = 108k × r^-b, b = 0.2850; carved from 150+ (properties 45,000 → 38,409), all corporate | Top 50 hold >2.4M units | MEASUREMENT + CHOICE | NMHC 2024 Top Owners [Likely: snippet; pages did not render] | UNCITED, verify; ranks 2-4 undershoot 12-14%; one account per owner |
| Thinning with occupancy λ = 1 below real size | A national sample meets ~one payee per small firm or landlord | CHOICE | the reconciliation above | REGISTERED: P(one payee given used) 0.532 (pure thinning ≈ 1.0) |
| Shares 0.74 / 0.35 | Sized to drawn payers | INVARIANT | - | ENFORCED: tied to `salary::Rules{}.paidFraction`, `rent::Rules{}.paidFraction` |
| In-bank landlord p 0.06 / 0.04 / 0.01 by type (unchanged) | Small landlords bank locally | CHOICE | - | REGISTERED: 2,487 ownerless in-bank landlords at pop 200,000 (~8 before) |
| Retired roster draws burnt in place: `max(5, round(25P/1e4))` coins at 0.04 in `make()`, `2 x max(3, round(12P/1e4))` u64 in `buildLandlords` | Shared stream and `makeCatalog` unmoved | INVARIANT | - | ENFORCED: sub-gate A matches the retired loops at pop 1, 300, 2,000, 20,791, 200,000 and pins the income-free stream at `ddfd735e74a97cd2` before and after |
| `pickSized`: one uniform for pools ≥2, none for one; `pickSizedDifferent`: none when one member remains | Later lane draws keep values | INVARIANT | - | ENFORCED: draw contract vs `choiceIndex`; sub-gate C 146,105 switches, 0 off-lane, 0 repeats |
| Camouflage salary picks through the payroll law (same u64 on the camo lane) | Cover employers sized like real ones | INVARIANT | - | ENFORCED: sub-gate B (pop 200,000) headcount 1.023 of legitimate payees'; uniform 0.0150 |
| Camouflage salary on the employer's schedule: `samplePayrollProfile` on `{employer_payroll_profile, number}` off `RngFactory{payrollSeed}` (run seed, in `InjectorServices`), via `timestamps::jittered` (lag, no day offset, 06:00-11:59); the per-mule camo-lane schedule draw retired | Cover salary posts when the employer pays everyone else | INVARIANT | - | ENFORCED: B, 15,807 pairs with a legitimate co-payee, 0 of 80,369 rows off EmploymentInitializer's schedule; disarm (fraud-factory schedules) 13,158 of 15,701 pairs (0.838), 59,588 of 79,196 rows off. D: 0 legitimate rows off; 0 of 49 (60 d) and 0 of 356 (365 d) camouflage rows off vs 36 and 294. Before: 11,303 of 15,758 pairs (71.7%) off, and 12:00 posts payroll never makes |
| Camouflage schedule under `PayrollRules{}` | Payroll reads `salary::Rules::employment.payroll` (same defaults) | CHOICE | - | REGISTERED: `LegitAssembly::incomePrograms`, `salaryRules`, `employmentRules` have no caller; non-default rules must reach the injector beside `payrollSeed` |
| Camouflage P2P never pays employers or landlords | Legit P2P pays only deposit accounts | INVARIANT | - | ENFORCED: `fraud::camouflageEligible` (`Role::account`) admits none of 2,157 such records (16.9% of the pop 2,000 registry); D: 0 of 258 (60 d), 0 of 1,572 (365 d) |
| Employer serials below the SSA and disability keys (9,000,001, 9,000,002); landlords below 10^7 | Keys never collide or overflow | INVARIANT | - | ENFORCED: `static_assert` on 6,486,523 real members; `makePack` throws (only above ~28M people) |

## Registered limitations

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| No employer or landlord location | Payees live near them | CHOICE | - | REGISTERED (no regional-bank mode) |
| Size-weighted job switches | Hiring varies by size and age (BDS) | CHOICE | BDS (not read) | REGISTERED |
| No firm or landlord births/deaths | They enter and exit | CHOICE | - | REGISTERED: 20 years of churn stacks ~7.5 payees on a small employer (estimate) |
| Relocation ends neither job nor lease | A cross-state mover changes both | CHOICE | - | REGISTERED (an old landlord can be paid up to 8 years) |
| One cadence draw per employer, size-free | 72.9% of 1,000+ employee establishments pay biweekly; federal biweekly | CHOICE | BLS CES Feb 2023 (L-4) | REGISTERED: ~7% of workers take cadence from ~11 employers (estimate) |
| Never-paid employers and landlords export as isolated vertices (aml, mule_ml) | Counterparty tables hold seen counterparties | CHOICE | - | REGISTERED, owner decision: at pop 2,000 never paid 44.9% of employers, 43.1% of landlords (60 d); 33.6%, 33.3% (365 d) |
| In-bank landlords ownerless; standard exporter labels them `landlord_external` / `landlord` (`landlordIndex` is external-only) | An in-bank landlord is a customer | CHOICE | - | REGISTERED, owner decision (inBankP 0 or an owner); ~300× more visible now |
| Renter share per person | ~35% of households rent | MEASUREMENT | ACS [Likely]; research 25,000-30,000 renter households per 200,000 | NONCONFORMING, owner decision (2.3-2.8× payers) |
| No processor payroll | ~17-20% of paychecks (ADP alone) | CHOICE | ADP Research 2025 [P] | REGISTERED (a) |
| 1-4 unit tenants under-routed to managers | ~22% of 1-4 unit, 84% of 150+ unit properties managed | MEASUREMENT | RHFS 2021 via Multifamily Executive [S] | DEVIATES: corporate share 3.6% (1 unit), 4.8% (2-4); 150+ 0.93-0.94 (property vs unit axis) |
| Portal rent names the landlord | May settle via a processor | CHOICE | AppFolio 10-K FY2024 [P] | REGISTERED (d) |
| Spending sensitive to cadence on short windows | - | CHOICE | - | REGISTERED, not tuned: run-golden cadence 38% weekly / 62% biweekly (5 retired employers) → 16 / 65 / 10 / 8% of 1,207 workers; fraud-free gate rows 190,402 → 146,901; the retired mix restores 189,767 (diagnostic). Pre-existing engine property (paycheck boost, a monthly worker's pre-payday liquidity) |
| The hubs note's "38,000 to 41,000 payroll credits per employer", "about 1,500 employees each" | Code: ~308 payees, ~9,100 credits a year per employer at pop 200,000 | UNCITED | `docs/research/counterparty_hubs_2026-09.md` | UNRECONCILED (4.3×; nothing multiplies salary rows; split deposits are self-transfers). Do not quote |

## Where the implementation deviates from the design

1. Landlord masses divide by the Table 3 class sum (49,548k, not 49,547k),
   so 66,658 at pop 200,000 (design 66,662) and 155,581 at 500,000 (design
   155,585; the carve-out keeps round(45,000 × 13,987 / 16,387) = 38,409
   properties in 150+, which binds there).
2. Renter-weighted type mix shipped; reported-only also tested.
3. The camouflage P2P change is a gate only (bank-gl's review fix already
   restricted the pool).
4. Headcount band at pop 200,000 via the real generator (at pop 2,000
   employers outnumber workers, uniform scores ~0.3, ~20 pairs: 0.435 at 60
   d, 0.471 at 365; printed).
5. Corpus rent-type band 4 sigma over payers (rows cluster by lease); ±3
   points banded at scale (70,000 leases).
6. The salary-jitter lane move was not taken: three pins re-pinned with
   attribution (`test_bank_ledger` A1/A2, `test_remote_payees` B1/B2,
   `test_product_providers` B5); sub-gate A adds the income-free pin.
7. `AccountPools::employers` borrows the pool; `SizedKeys::indexOf`
   handles a one-key fallback pool whose serial is not 1; the landlord burn
   spends `nextU64` twice per retired landlord (same count, sub-gate A).
8. Added: sub-gate B caps tenants at an individual landlord at 12 (7
   measured; retired roster 329); `test_pipeline_e2e` checks salary,
   benefit and rent rows against the pools at pop 100.
9. Added at review: camouflage salary follows its employer's schedule (the
   camo lane had drawn its own cadence, weekday, parity and lag). The run
   seed reaches the injector as `InjectorServices::payrollSeed`, set in
   `TransferStage::makeFraudInjector` beside `fraudSeed` (both engines) and
   in the three harness injectors; the camouflage context carries a second,
   legitimate factory. Re-dated rows move three legit rows and one retired
   row in settlement, so `test_remote_payees` B2 was re-pinned again;
   restoring the per-mule schedule (diagnostic) reproduces the prior pin.

## Measured (`test_counterparty_sizes`; `test_counterparties` for the pure law)

| Quantity | Measured | Band or design figure |
|---|---:|---:|
| Employers at pop 0 / 300 / 2,000 / 10,000 / 200,000 / 500,000 | 17 / 218 / 1,453 / 6,068 / 89,231 / 200,556 | exact |
| Landlords at the same populations | 8 / 106 / 700 / 3,380 / 66,658 / 155,581 | exact |
| Federal share of 148,000 workers (pop 200,000) | 0.01911 (2,829) | 0.01933 ± 0.00143 (4σ) |
| Workers at employers with 100+ payees | 0.1177 | [0.08, 0.16]; design Monte Carlo 0.117 |
| P(one payee given used) | 0.5324 | [0.45, 0.62]; MC 0.53 |
| Employers used | 58,611 | ≥ 50,000; MC 58,606 |
| Under-20 classes' share of workers | 0.1385 | 0.139 ± 0.005 |
| Largest private employer; top 10 share | 1,737; 0.0637 | printed |
| Employers with 79+ payees (~2,048 payments a year) | 127 | ~480 before |
| Uniform disarm: federal share, 100+ share | 0.0000, 0.0000 | red |
| Camouflage headcount ratio; uniform disarm | 0.953; 0.0156 | [0.5, 2.0]; < 0.1 |
| Landlords used by 70,000 renters; most tenants; at an individual landlord | 42,152; 148; 7 | ≥ 100; ≤ 12 |
| Top-50 owners' share of renters | 0.0491 | 0.0484 ± 0.004 |
| Renter type mix vs roster | 0.428 / 0.140 / 0.432 vs 0.432 / 0.138 / 0.430 | ± 0.03 |
| Retired-roster disarm: tenants at an individual landlord | 329 | red |
| Job switches (20,000 chains, 20 years); repeats; off-lane | 146,105; 0; 0 | 0; 0 |
| `pickSizedDifferent` at 1,453 and 200,556 employers | 38 ns, 38 ns | printed |
| Corpus pop 2,000 × 60 d: salary, benefit, rent rows off pools | 0 of 6,021; 0 of 489; 0 of 1,218 | 0 |
| Corpus busiest employer / mean payees (60 / 365 d) | 29 / 1.594 = 18.2; 37 / 1.804 = 20.5 | ≥ 5 |
| Corpus camouflage P2P on employer or landlord | 0 of 258; 0 of 1,570 | 0 |
| Registry at pop 2,000 | 12,726 (10,581 before) | printed |
| Pop 500,000: registry growth; resident bytes | +354,287 records; 55.4 MB (registry and lookup 24.3, directory 6.9, landlord pack 8.6, blueprint and fold copies 15.6); size laws 5.0 KB | < 128 MB; design ~84 MB |

Corpus movement: `tests/golden_run.b2sum` (pop 2,000, 60 d) `0642235c...`
over 231,731 rows → `a1824bb5d32e71f1b94b2fb0bf4c7ffadd53dd66b97727a5a37e44b1066411c9`
over 164,833 (−28.9%, the cadence law); re-pinned at round end; domain
predicates in sub-gate D. All three table goldens move (salary sources, rent
destinations, cascade; AML counterparty tables +~2,150 rows at pop 2,000,
+~9,450 at 10,000); owner re-pins. `kTableCount = 43`. Cascade-exposed
bands pass unchanged: `test_econ_wiring` drift parity 1.113 (0.80 floor),
fraud-rides-L mean 0.914, `test_card_merchant_graph`, `test_card_baselines`.

For MulePatternLearner: employer degree is heavy-tailed (pop 200,000: ~127
employers above 2,048 payments, federal ~2,830 payees, largest private
~1,740; ~480 uniform before; 53% of paying employers pay one person).
Landlord hubs vanish (largest owner ~148 × 12 = 1,776 rents a year).
Regenerate the 2024 corpus, snapshot and hub registry under a new dataset
id.

# AMENDMENT: outlets-frequency-2026-09

The last proposed change in `docs/research/counterparty_hubs_2026-09.md`,
in four steps measured one at a time (the fourth from review):

1. Biller picks on their own lane: `buildMarket`
   (`activity/spending/market/bootstrap.cpp`) drew biller count and set on
   `{"payees", person}` after the catalogue-dependent favourite pick; now on
   `{"payee-billers", person}`, leaving the favourite pick last.
2. Chain outlets: a Record was already an acceptance endpoint (the
   research's "brand-level accounts misuse the citation" was overstated);
   grouping was missing. `synth::merchants::expandOutlets`
   (`synth/merchants/outlets.hpp`), between `placeGeography` and
   `appendChurnReplacements` in `buildMerchants`, splits each eligible core
   record (grocery, fuel, restaurant, pharmacy or retailOther; local or
   regional; placed) of weight w into n = max(1, round(w / unit)) outlets
   (unit = median eligible core weight), weight split equally; the n − 1 new
   outlets take population-weighted US areas on `{"merchant-outlet",
   serial}`, keep the brand's bank and carry `Record::brand` (new; 0 = own
   organization). Churn replacements inherit the donor's brand. Pop
   500,000: +3,346 records (+12.9%) in 1,068 chains.
3. Category-dependent frequency: `commerce::sampleFavoriteSlot`
   (`commerce/affinity.hpp`) keeps each row's Zipf rank multiset {1 +
   floor(F × unitFor(p, m))} and deals ranks in Plackett-Luce race order,
   key −ln(1 − v) / w_category (v a hash in its own domain). Still one
   uniform per pick.
4. Fraud venues carry the law: `buildMerchantPool`
   (`transfers/fraud/typologies/unauthorized.cpp`) draws by weight, so step
   3 left it category-blind and billers went from under- to
   over-represented in fraud card rows. Each candidate's weight (CP and
   CNP) is scaled by `commerce::kCategoryVisitLift` (the race's
   visit-to-favourite ratio, asserted by K10); positive, so candidates and
   draws are unchanged.

No shared-stream draw added; `makeCatalog`'s count unchanged
(`coreCountFor` extracted); nothing stored re-derived. The favourite set is
not enlarged: Alessandretti's ~25 is a current set; Circana's 20 restaurant
chains a year and Krumme's 64 merchants in six months are cumulative
(exploration and turnover).

## The visit-rate weights, derived

Targets are card-present favourite visit shares from DCPC 2022 in-person
non-cash payments a month (SF Fed 2023 Findings, Figure 5, confirmed):

    grocery and convenience            5.5
    restaurants = fast food 3.4 + sit-down 2.0 = 5.4
    general merchandise and department 3.1
    gas                                2.6
    sum                                16.6

These are ~80% of in-person payments (2026 Findings: 30 a month, 16 at
grocery, convenience and restaurants, 8 at gas and general merchandise;
(16 + 8) / 30 = 0.80; 2024 Findings footnote 13 agrees). Base 16.6 / 0.80 =
20.75: grocery 0.265, restaurant 0.260, general merchandise 0.149, gas
0.125. The other 0.20 is CHOICE: retailOther = general merchandise + 0.10
(0.249), pharmacy 0.05, four billers 0.05 (0.0125 each). Targets = share ×
physical share 0.6202 (1 − the 2022 online share 0.3798): grocery 0.164,
restaurant 0.161, retailOther 0.154, gas 0.078, pharmacy 0.031, billers
0.031.

Solved with outlets at pop 500,000, 2022, F = 30 (saturated set size) over
24,000 residents: damped proportional fit, factor (target ratio / realized
ratio)^0.7 vs grocery, 100 iterations, ecommerce solved to hold the online
share; pharmacy and billers cannot reach target and ship floored. Weights
in `Category` order: grocery 1, fuel 0.173, utilities 0.001, telecom 0.001,
ecommerce 0.144, restaurant 0.792, pharmacy 0.001, retailOther 0.307,
insurance 0.001, education 0.001.

The design's table (fuel 0.134, ecommerce 0.0957, restaurant 0.773,
retailOther 0.226, floors 0.01; solved without outlets) is superseded; its
"any floor below about 0.05 gives the same result" is wrong (at 0.01 a
biller outraces an ecommerce favourite ~6% of the time; biller share 0.121
→ 0.131, 0.149 at 0.03). At 0.001 a tenfold lower floor moves no share by
more than 0.0007 (K4).

## The authority rows

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| Biller set on `{"payee-billers", person}` | Data-dependent draws go last on their own lane (merchant-churn-2026-07) | INVARIANT | - | ENFORCED: the favourite `pickFrom` is last on `{"payees", person}`; run-golden rows unmoved (164,830), digest moved |
| n = max(1, round(w / median core weight)), equal split | Chains need several typical stores; 500+ employee firms hold 63.2% of retail receipts, 31.3% of establishments | DERIVED, validated unfitted | SUSB 2022 `us_naicssector_large_emplsize_2022.xlsx` (2025-04-10), Retail Trade: 645,404 firms, 1,045,890 establishments, $6,850.9B; firms <500: 718,945 establishments, $2,523.6B | CONFORMS: 0.652 / 0.279 at pop 500,000 (J4, ±0.10; without outlets 0 / 0, J5) |
| Only core records expand | Firms <500 average 1.12 establishments | MEASUREMENT | SUSB 2022 | CONFORMS (J3) |
| Online, ecommerce, nationalService and biller records stay single | Card-absent merchants use one principal place of business | CHOICE | Visa Merchant Data Standards Manual, April 2026 [P; verification: the rule sets a location, not ID count] | REGISTERED (IDs per online brand unverified) |
| Outlet areas i.i.d. by population, with replacement, on `{"merchant-outlet", serial}` off the geo seed | Acquirers assign each outlet a location | CHOICE | Visa manual [P] | REGISTERED: no clustering; largest chain (51) spans 28 areas, ≤10 in one |
| Outlets stored as Records; the plan derived | Every sampler, the favourite CSR and the ledger index the catalogue | INVARIANT | `docs/ram_derive_dont_store.md` | ENFORCED: +3,346 records; local pools 0.731 → 0.824 MiB (H) |
| Ownership keyed on the outlet's own key | A franchise outlet is its proprietor's; a brand key would give one Party 51 outlets; ownerless outlets would tie the register to footprint | INVARIANT (leak) | merchant-ownership-2026-07 | ENFORCED: G' and G'' unchanged (G'' 1.027 / 0.874 / 1.261 / 0.774) |
| Register as spread as a uniform hash over the owner cohort (distinct proprietors ≥ 0.9 of uniform expectation; nobody at the uniform 1-in-1,000 load) | Most proprietors hold one outlet | CHOICE (instrument) | 7 in 10 restaurants single-unit (National Restaurant Association [P]); ~55% of fuel stores single-store (NACS 2025 [P]); 63% of c-stores in firms of ≤10 (NACS 2026 [P]) | ENFORCED. Old bands (≥ owned/3 proprietors, ≤6 each) were uncited: outlets took leg-long to 4.07 per proprietor (max 9; uniform expectation 55.1, ceiling 16), where owned/3 asks 76 of a 56 cohort. A five-Party register reads concentrated; pop 500,000 mean 0.31-0.42 (≤1) |
| Churn replacements inherit `brand`; births on the enlarged base | BLS BED Table 7 is establishment survival | CITED | bls-citation-2026-07 | CONFORMS: F1 144 and 43 branded births, 0 off-brand; F2 outlets survive 0.4808 / 0.7935 vs 0.4677 / 0.7763 |
| Visit-rate weights (derived above) | Card-present visits follow DCPC 2022 | DERIVED | DCPC 2022 (SF Fed 2023, Fig. 5) [P]; 2026 Findings [P]; 2024 footnote 13 [P] | CONFORMS at 2022 / F = 30: grocery 0.166, restaurant 0.163, retailOther 0.155, fuel 0.078; restaurant/grocery 0.982 (DCPC 0.982), fuel/grocery 0.470 (0.473); grocery 2.07× its favourite share (K3); all-ones disarm 1.069, 0.924, 1.01, fails K3 (K5) |
| retailOther +0.10, pharmacy 0.05, billers 0.05 | The 20% outside the DCPC types | CHOICE | none | REGISTERED; pharmacy floored (only anchor: Medicare median 13 visits/yr, JAMA Network Open 2020, an upper bound) |
| Rank multiset unchanged | Top-1 stays in Krumme's 13-22% | INVARIANT | Krumme 2013 | ENFORCED: K1 0 mismatches over 76,000 rows at four points; top-1 0.1500 (F = 30), 0.1777 (F = 19) |
| Multiplicative w_c × rank^−0.80 rejected | Breaks the within-card law | MEASUREMENT (negative) | - | top-1 0.2059 (F = 30), 0.2498 (F = 19), above 0.22 (K7) |
| Floor 0.001 | A floored category ranks last | CHOICE | - | ENFORCED: K4 (1e-4 moves ≤0.0007) |
| Era-flat, solved at 2022 | DCPC is a 2022 measurement | CHOICE | DCPC 2025, 2026 (16 and 8 in 2024 and 2025) [P] | REGISTERED: at 2005 / F = 30 restaurant/grocery 0.954, fuel/grocery 0.503, retailOther/grocery 0.542 vs 0.94 (CNP 0.059 leaves online retailOther records in the race); K3 at 2022 only |
| Online share held on the dated CNP series | merchant-selection-2026-08 | INVARIANT | Fed Payments Study 2022 | ENFORCED: K2 within 0.02 (0.3798 → 0.3792; largest +0.0088 at F = 19) |
| Card rows: grocery + restaurant ≥ 1.5× billers (domain predicate for the `golden_run.b2sum` re-pin) | Everyday merchants carry card frequency | INVARIANT | - | ENFORCED: K9, 0.4551 / 0.0992 (4.59), 0.4499 / 0.1516 (2.97); pre-round 0.48, 0.57 |
| Predicates for the three table goldens: AML Counterparty vertices = the external registry, every outlet exported, balances finite; card-fraud `Merchant_Assigned` in the ten names, no online `Merchant_Location` | A digest pins whatever it is given | INVARIANT | cash-hub-defect-2026-08 | ENFORCED (`test_pipeline_e2e`): 864 vs 864, 132 outlets, 0 non-finite; 181 assigned, 0 off-name, 0 online located, 67 outlets |
| Physical fraud-only merchants: 4-seed mean share ≤ 0.01 | Outlets must not create fraud-only endpoints | INVARIANT | - | ENFORCED (`test_card_merchant_overlap`): 0.0090 / 0 / 0 / 0 (mean 0.0023) |
| Fraud venue weights × `kCategoryVisitLift` (`Category` order: 2.07, 1.04, 0.45, 0.45, 0.98, 1.89, 0.45, 1.30, 0.45, 0.45), CP and CNP | Category alone scores fraud no better than before the law | CHOICE (principle); DERIVED (values) | none: no fraud-by-category series (real fraud favours resellable goods, beyond ten categories) | ENFORCED: K10 worst gap 0.0030 (legacy 1.0738); overlap biller fraud/legit lift 1.018 in [0.60, 1.30] (pop 2,000, 2019, 365 d; 0.781 / 1.155 / 0.940 / 1.207). Disarms: no factor 1.617; legacy law with factor 0.491. Outlets alone 0.791; pre-round 0.737. Category-only AUC 0.613 (0.669 no factor, 0.606 outlets alone, 0.586 before) |
| Shared stream and base lanes untouched | `makeCatalog`'s count is load-bearing | INVARIANT | merchant-churn-2026-07 | ENFORCED: J1 (equal u64; 0 of 26,000 base records moved); A1 and B1 keep `9e0a89591a4d861f`; `test_membership` green |

## Registered limitations

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| Outlet bank volume (K8; pop 500,000, 2022, F = 30, 236.6 card payments per person-year): medians grocery / restaurant / fuel / pharmacy before 621 / 607 / 591 / 618; outlets alone 1,146 / 1,215 / 953 / 1,187; law alone 1,320 / 1,196 / 662 / 270; shipped 2,344 / 2,232 / 982 / 523 | A typical restaurant or gas outlet takes a few hundred payments a year from one bank: Visa ~8,000 per restaurant per quarter ≈ 150-250 a year at a 0.38% share; gas ~300 | MEASUREMENT | Dev and Hamooni, arXiv 2009.02461 [P]; NACS 2,500 gallons a day [P] | NONCONFORMING, not tuned. Catalogue density sets the mean: 3,348 restaurant records each serve ~150 customers at 0.163 × 236.6 = 38.5 payments (mean ~5,750). A real restaurant serves ~550 residents (608,144 eating places, 18.3 per 10,000 at 333 million, 2022 Economic Census [P]); a 500,000-customer bank among the 71 areas' 52.9 million holds ~5 (~200 a year). That needs ~96,600 restaurants and 1.3 million establishments at CBP density (29× the records, ~26,000 per 10,000, beyond Nilson). Medians rose because both steps compress spread (grocery p90 9,228 → 7,218, max 54,438 → 37,449). Fix: CBP-driven supply on its own lane |
| Supercenters and online brands above 2,048 | A supercenter outlet ~5,000 a year, an online brand millions | MEASUREMENT (derived) | research, corrected | CONFORMS in kind: ecommerce median 3,467, max 486,354; 51% of grocery and restaurant records exceed 2,048 |
| Billers cannot fall below ~11.5% of card visits | DCPC target 0.031 | CHOICE | - | REGISTERED: 0.1214 (0.2695 before), floor 0.1149 (K4); realism unknown |
| Category supply uniform (a tenth each) | Restaurants 19.3 per 10,000, gas 3.41, supermarkets 1.98 | MEASUREMENT | 2022 Economic Census EC2244BASIC, EC2272BASIC [P] | REGISTERED: `makeCatalog`'s rejection-sampled `choiceIndex` has a data-dependent count; fix post hoc on an isolated lane |
| Grocery/general-merchandise physical favourites 4.18 at F = 30 | FMI: 5.4 grocery banners a month | MEASUREMENT | FMI U.S. Grocery Shopper Trends 2026 [P; corrects the research's 3-6 a year] | REGISTERED (membership question) |
| Several outlets of one chain per person, no cap | One primary plus 0-2 secondary | UNCITED | research judgment | REGISTERED |
| No brand-wide closure | Chain bankruptcies close many outlets | CHOICE | - | REGISTERED: per-outlet `merchant-life` lanes |
| No brand column exported | `Merchant` id is the acceptance endpoint until a Brand vertex exists | CHOICE | `data/commerce/README.md` | REGISTERED |
| nationalService records single-centroid, card-present | No storefront | CHOICE | - | REGISTERED (pre-existing) |
| Billers somewhat over-represented in card-present fraud on gate legs | The factor fixes frequency, not membership: the fraud pool weighs volume (`Record::weight`), favourites reach (a power of weight < 1), and outlets widen the gap | MEASUREMENT | - | REGISTERED: card-present biller lift 1.68 on the 2019 gate legs (utilities 2.71) vs 1.06 before; at pop 500,000 in 2019 (probe outside the repo) 1.20 with the factor, 1.15 outlets alone, 2.49 without. Reach weighting is its own round |
| Category separates fraud strongly at 1991 | Stolen-card CNP is era-flat 0.70 (`kCardNotPresentShare`) vs legitimate 0.010 in 1991 | CHOICE (pre-existing) | - | REGISTERED: pooled pop-300 1991 legs, ecommerce lift 5.01, category AUC 0.787 (0.742 before, 0.807 no factor); biller lift 0.726 (0.564, 1.572). Per-era payment-method item |
| Fraud amounts ignore venue category | `amounts::cardFraudSpend` is venue-free; the law cut the legitimate median ticket ~$69 → $54 | CHOICE (pre-existing) | none | REGISTERED (review, pop 2,000, 2019, 365 d, two seeds): amount-only AUC 0.468 / 0.500 → 0.513 / 0.552; fraud median $62 / $72 unmoved |
| CNP explore samples all records, outlets included | Remote purchases pay remote endpoints | CHOICE | - | REGISTERED (pre-existing) |
| Enumeration probes pick uniformly over live records while writing Online | Probes hit online merchants | CHOICE | - | REGISTERED (pre-existing) |
| Online fraud-only merchants at 1991 | CNP fraud can land on an online churn birth before anyone favours it | CHOICE | - | REGISTERED: seed 7777777 puts 28 of 238 fraud rows (0.1176; 32 before the fraud venue step) on three online births (re-keyed churn cohort); pre-round 0 there, 0.0143 at the main seed. Printed |
| Gift-card "eligible categories ~40% of card payments" | Measured 0.569 → 0.638 (pop 500,000, 2022), so 500 bp ≈ 7.6 cards per person a year vs 5 | UNCITED input | `commerce/gift_cards.hpp` | STALE: re-deriving (~330 bp) moves the $500 gates; own round |
| Re-presented debits can post after the remote merchant closed | Up to two re-presentments, ≤108.5 hours later | CHOICE | - | REGISTERED: C4 reads liveness at emission (or within the `ReplayFundingBehavior` retry horizon); 1 of 85,966 rows 37.5 hours late |
| Gate-world row count swung under biller or favourite re-randomization | The monthly evolver spent data-dependent draws (`churnBillers`, `evolveFavorites`) on the session rng | CHOICE (pre-existing) | - | Found here: harness January byte-identical, diverging from day 31; two biller-lane re-randomizations gave 145,906 and 180,226 rows; renaming the lane moved 155,131 → 149,115. Closed by evolver-lanes-2026-09, which also showed production couples the same way (10,372 and 10,434 session draws at the February and March boundaries; one extra draw at February moves 183,820 → 180,818) but never flipped because no production customer holds a biller closing before March (13 and 45 in the harness). The earlier 0.024% figure is not reproduced |

## Where the implementation deviates from the design

1. Weights re-solved with outlets, floor 0.001; online drift 0.393 → 0.381
   became 0.3798 → 0.3792.
2. K3 checks ≥ 1.6× (not 2×) the favourite share: outlets raised grocery's
   favourite share 0.070 → 0.080; lift 2.07, disarm 1.01.
3. The overlap bound covers physical fraud-only rows as a 4-seed mean (the
   design's "≤ 0.01, 0.0000 today" was stale: 0.0136 at the main seed, and
   the whole share includes an online mechanism).
4. G's absolute register bands replaced by the uniform-spread check (no
   forbidden repair taken).
5. `test_econ_wiring`'s CPI band reads a mix-adjusted ticket ratio: raw
   2.292 (1.928 before) because the category mix became era-dependent
   (2019 online share 27%); weighted by 1991 shares 1.849 vs CPI 1.877, in
   band; raw printed.
6. C4 reads liveness at emission (the failure was a retry).
7. `test_counterparties` builds the key-clearance catalogue with outlets
   (58,891 records, max serial 58,891).
8. Added: J's in-file disarm, K8 volume print, K9 corpus predicate, the AML
   and card-fraud predicates in `test_pipeline_e2e`.
9. Outlets shrink only the head (largest grocery 54,438 → 37,449,
   restaurant 118,267 → 61,804, lower p90) and raise the median; the law
   then gives grocery and restaurant the head ranks (shipped maxima 65,472
   and 114,674). Retuning cannot reach a few hundred: at half the unit J4
   fails (0.757 / 0.487) and the restaurant median rises to 4,252; at a
   tenth (1,567 records per 10,000, past Nilson; 0.821 / 0.849) it is 1,164.
10. The fraud venue step was not in the design: with the category law alone
    biller fraud/legit lift
    went 0.70 / 0.75 → 1.27 / 1.71 on two pop-2,000 2019 seeds.

## Measured

`test_card_merchant_graph` (pop 500,000 unless stated):

| Quantity | Before | After |
|---|---:|---:|
| Records; chains; added outlets | 26,000; 0; 0 | 29,346; 1,068; 3,346 |
| Chain volume / establishment share (J4) | 0 / 0 | 0.652 / 0.279 |
| Largest chain: outlets, areas, most in one area | none | 51, 28, 10 |
| Pops 300 / 2,000 / 8,000: added outlets | 0 | 150 / 91 / 127 |
| Top-1 reach; hubs above 25% / 50% (G) | 0.0823; 0 / 0 | 0.0799; 0 / 0 |
| Mean home-to-favourite miles; P(within 50) (H) | 3.8; 0.9730 | 3.9; 0.9724 |
| Top physical outlet's home-area span | 1 | 8 (its brand 8) |
| Local pools; cutoff discard | 0.731 MiB; 2.74e-08 | 0.824 MiB; 2.22e-08 |
| Visit share 2022 / F = 30: grocery, restaurant, fuel, retailOther | 0.080, 0.086, 0.075, 0.119 | 0.166, 0.163, 0.078, 0.155 |
| Visit share: pharmacy; ecommerce; four billers | 0.087; 0.284; 0.270 | 0.039; 0.278; 0.121 |
| Within-card top-1 (K1) | 0.1500 | 0.1500 |
| Corpus legs: sub-gate D ratio; within-card top-1 | 2.669 / 2.637; 0.2454 / 0.2352 | 2.990 / 2.841; 0.2040 / 0.2166 |
| Corpus legs: grocery+restaurant over billers (K9) | 0.48 / 0.57 | 4.59 / 2.97 |
| 2019 overlap legs pooled: biller fraud/legit lift; category AUC | 0.737; 0.586 | 1.018; 0.613 |

`test_merchant_churn`: live ratio 0.842 / 0.961 (0.832 / 0.955 before),
incumbent survival 0.4674 / 0.7755, traversal 1.67x / 1.22x (floor 1.10),
out-of-tenure 0.197% / 0.223% (ceiling 1%). G' lifts 0.982 / 1.043 / 1.034
/ 0.925 (0.974 / 0.988 / 1.023 / 0.958 before the fraud venue step). `test_econ_wiring`
drift parity 1.071 (1.113 before), volume ratio 0.635, fraud-rides-L mean
0.915.

Corpus movement (`tests/golden_run.b2sum`, pop 2,000, 60 days): before
`b8b5cb01...` 164,830 rows; biller lane `8c343ee2...` 164,830; outlets
`3e821ce4...` 193,302 (+17.3%); category law
`c5b502f0c272cfa194fdc81f27952b8d4d2f6587ace0ba93e010bfe4e8fafcbc` 183,820
(−4.9%); fraud venues
`5b6ec79229b1f7fc05129d73c77756dd41308ee5bf117e712ffa854780c98336` 183,820
(fraud destinations and their chargebacks). With a category-free amount
law the outlet step still moves +16.5% (the core-floor regime: 330 base
records gain 91 outlets) and the category step +1.2% (so its move is the
amount mix). At pop 20,000 the steps give 2,039,577, 1,976,176 (−3.1%) and
2,238,916 (+13.3%). Re-pinned once at round end. `test_bank_ledger` A2 and
`test_remote_payees` B2 re-pinned with per-step values (B2 again at step
4, on 109 chargebacks); shared-stream pins hold. All three table goldens
move (outlet accounts): the owner re-pins and re-runs
`docs/card_fraud_postgres_acceptance.sql` and
`docs/card_fraud_device_ip_investigate.sql`. `kTableCount = 43` unmoved.

For MulePatternLearner: chain hubs split (largest chain 51 nodes); online
brands and billers stay single hubs above 2,048. Hubs grow in number: at
pop 500,000 in 2022 records above 2,048 a year go 8,974 of 26,000 (0.345)
→ 10,964 of 29,346 (0.374), the typical grocery and restaurant outlet near
2,300. Grocery and restaurant visits roughly double, biller card visits
halve (replacing a flat 4.7 payments per payer a year). Regenerate and
reload the 2024 corpus before rebuilding the hub registry.

# AMENDMENT: hub-realism-2026-09 round close

The run golden is re-pinned once for the round: `tests/golden_run.b2sum`
`a30c535d...` (231,731 rows) →
`5b6ec79229b1f7fc05129d73c77756dd41308ee5bf117e712ffa854780c98336`
(183,820, −20.7%). Chain (each link in its amendment):
institutional-providers `42c5c164...`, unknown-counterparty `dd0363aa...`,
bank-gl `754ab129...` and its pool fix `0642235c...` (231,731 rows each),
atm-spread (no byte), counterparty-sizes `a1824bb5...` (164,833) and its
schedule fix `b8b5cb01...` (164,830), outlets-frequency `8c343ee2...`
(164,830), `3e821ce4...` (193,302), `c5b502f0...` (183,820), `5b6ec792...`
(183,820). Row counts moved at cadence (−28.9%), outlets (+17.3%) and the
category law (−4.9%). A probe outside the repository replaying the
binary's construction in-process (`buildWorld`, `runWindowedTransfers`,
same CLI setup) reproduces both ends. Captured as
`tests/test_run_golden.cpp` documents; non-PostgreSQL suite 68 of 74 pass
(five PostgreSQL tests skip with code 77; `test_scale_soak` opt-in via
`PL_SOAK`).

## The authority row

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| `test_run_golden` checks the run's summary before comparing or capturing: pinned population (2,000), ≥1 account per person, summary rows = rows digested by the Golden sink, 0 < fraud < rows; a failure fails the capture too | A digest pins anything (cash-hub-defect-2026-08); a re-pin must not record an empty, fraud-free or inconsistent run | INVARIANT | - | ENFORCED: 2,000 people, 12,893 accounts, 183,820 rows, 502 fraud. Disarm fails before the baseline is read. Content predicates stay in their stages (K9, sub-gate D, the provider, remote-payee and bank-ledger legs) |

## Hub sizes at scale, measured without PostgreSQL

The same probe ran the mule-temporal 2024 configuration (pop 200,000 from
2024-01-01, 366 days, seed 42): 123,362,638 stream rows, 633 s, 18.2 GB peak
RSS. It counts rows naming each account as source or target (the stream,
not exported vertices; compare classes), × 365/366. Baseline: the pre-round
corpus in TigerGraph (research table).

| Counterparty class | Records (with a row) | Busiest five a year | Above 2,048 a year | Pre-round corpus, full year |
|---|---:|---|---:|---|
| External-unknown catch-all | retired | no row | 0 | 3,814,933 |
| Bank GL, card interest | 1 | 1,743,705 | 1 | card issuer 2,316,826 (interest and late fees) |
| Bank GL, card fees | 1 | 537,614 | 1 | (in the card issuer) |
| Bank GL, deposit fees | 1 | 368,463 | 1 | fee collection 361,991 |
| Bank GL, credit-line interest | 1 | 119,898 | 1 | overdraft line 113,386 |
| Auto insurers | 100 (100) | 339,498; 337,019; 213,195; 184,806; 111,098 | 74 | one insurer, 1,808,916 |
| Home insurers | 150 (150) | 4,350; 2,204; 1,365; 1,295; 1,096 | 2 | (in the insurer's key set) |
| Life insurers | 125 (125) | 92,864; 63,937; 59,334; 55,407; 44,847 | 88 | one insurer, 1,074,202 |
| Mortgage servicers | 300 (300) | 62,459; 57,474; 57,263; 54,384; 52,951 | 46 | on the student servicer |
| Student-loan servicers | 15 (15) | 157,498; 97,071; 90,653; 88,984; 77,282 | 8 | one servicer with every mortgage, 1,430,365 |
| Auto lenders | 200 (200) | 38,329; 37,946; 35,987; 32,481; 31,863 | 72 | one lender, 714,971 |
| ATM terminals | 270 (270) | 42,865; 42,018; 32,800; 32,788; 32,474 | 270 | busiest four ~250,000 each |
| Cash depositories | 50 (50) | 25,491; 24,372; 24,191; 23,978; 23,561 | 50 | (with ATMs, 227 cash-point hubs) |
| Check-capture points | 50 (50) | 3,701; 3,490; 3,406; 3,395; 3,394 | 25 | (with ATMs) |
| Card merchants, online | 1,423 (1,417) | 355,252; 322,638; 300,776; 268,225; 240,174 | 1,017 | busiest merchant 341,126 |
| Card merchants, chain outlets | 1,831 (1,831) | 35,830; 32,694; 31,327; 30,058; 26,743 | 1,721 | no outlets |
| Card merchants, independent physical | 4,735 (4,644) | 120,860; 70,624; 66,103; 62,814; 53,987 | 1,400 | |
| Card merchants, biller categories | 4,333 (4,313) | 230,067; 156,108; 149,633; 132,422; 130,396 | 1,258 | |
| Card merchant brands (outlets summed; 404 chains) | 10,778 active | 718,791 (53 outlets); 369,781 (21); 363,037 (26); 355,252 (1); 322,638 (1) | 4,078 | 4,384 card-merchant hubs |
| Check-payee banks | 1,000 (1,000) | 302,960; 286,521; 209,781; 111,916; 78,843 | 103 | on the catch-all |
| Funeral homes | 1,785 (948) | 5; 4; 3; 3; 3 | 0 | on the catch-all |
| P2P platforms | 2 (0) | no row | 0 | on the catch-all |
| Employers | 89,232 (63,762) | 60,360; 37,805; 21,854; 17,139; 13,454 | 76 | ~480 at 38,000-41,000 each |
| Landlords | 66,659 (44,379) | 1,202; 989; 965; 922; 856 | 0 | 240 at ~2,300 each |
| Subscription billers | 160 (160) | 47,777; 47,707; 47,121; 47,036; 46,979 | 160 | 160 at ~47,000 each |
| SSA | 1 | 207,023 | 1 | a hub |
| Disability payer | 1 | 73,780 | 1 | a hub |
| IRS | 1 | 262,063 | 1 | a hub |
| Every account | | | 6,443 (one a customer account at 2,084) | 5,602 |

The busiest external account falls from 3,814,933 (catch-all) to 355,252
(an online merchant); only three internal `gl` accounts exceed it. Accounts
above 2,048 rise 5,602 → 6,443: provider pools (290), check banks (103) and
every ATM (busiest ~43,000) are new hubs; employer hubs fall to 76,
landlord hubs to 0; card merchant records above the cap reach 5,396.
MulePatternLearner's hub registry and history-withheld stubs stay
necessary.

Instrument check, pre-round tree (60 days from 2024-01-01, annualized;
14.9 GB peak, so no full year): catch-all 4,182,444, auto insurer
1,902,210, student servicer 1,525,566, life insurer 1,108,858, auto lender
825,089, fee collection 302,542, busiest four ATMs 261,291-273,555, busiest
merchant 385,172, 5,825 accounts above 2,048: all within 17% of
TigerGraph. Exceptions: the card issuer reads 1,319,335 (card balances
build through the year; this tree's January-February annualize the card
GLs to 1,138,532 vs 2,281,319 for the year), and pre-round employers read
at most 15,683 (the registered unreconciled gap).

## Registered limitations

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| Every tuition installment paid one education merchant (`tuition::generate`, `transfers/legit/routines/family/tuition.cpp`, `run.education().pick(rng)` once per run) | Students attend many schools | MEASUREMENT | L-9 Tuition | CLOSED by tuition-payee-2026-09: 32,398 rows in 2024 at pop 200,000 on one merchant; now 45,571 rows over 950 schools, largest 191 (0.42%) |
| Hubs above 2,048 rise 5,602 → 6,443 | Card merchants, ATMs, providers, large employers, SSA and the IRS are real hubs at 200,000 customers | MEASUREMENT | research hub section | RECORDED: the round removes artifacts (catch-all, singleton providers, ATM tie-break, uniform rosters), not hubs |

## Owner must do

Re-pin `tests/golden_tables.md5`, `tests/golden_tables_aml.md5` and
`tests/golden_tables_card_fraud.md5` against PostgreSQL, then re-run
`docs/card_fraud_postgres_acceptance.sql` and
`docs/card_fraud_device_ip_investigate.sql`. Regenerate the mule-temporal
2024 corpus under a new dataset id, load a new TigerGraph snapshot, rebuild
MulePatternLearner's hub registry (dropping or separating the four GL
accounts' edges). `kTableCount = 43`.

# AMENDMENT: evolver-lanes-2026-09

Closes outlets-frequency's row-count coupling, in two measured steps:

1. Evolver lanes: `dynamics::monthly::evolveAll`
   (`activity/spending/dynamics/monthly/evolution.cpp`) used the session
   rng that `DayDriver::runDay` uses for day frames (`DaySource::build`: one
   Gamma(1.3, 1/1.3) shock per day on every spender's rate) and dynamics
   multipliers; its three passes retry (contact add on self/duplicate,
   `churnBillers` up to 8 per closed biller, favourite add on duplicate), so
   catalogue state moved every later day. Each pass now draws on
   `RngFactory{Market::laneSeed()}` lanes {"evolve-contacts" |
   "evolve-billers" | "evolve-favourites", PersonId, "YYYY-MM"}.
   `Market::laneSeed()` (new) is `buildMarket`'s base seed for the
   `payees`, `payee-billers`, `payee-behavior` lanes (run seed in both
   engines, `spec.seed` in the harness). `CommerceEvolver::evolveIfNeeded`
   takes no rng; lanes open only when a pass would draw.
2. Card routing lanes (found by sub-gate G after the evolver lanes): `CardCycleDriver`
   makes payment, fee, interest and dispute rows through the session
   factory, whose `Factory::make` routes device and IP on the session rng
   (endpoint-pool-dependent draws, only for cards with purchases). Each
   card session (`transfers/channels/credit_cards/detail/session.hpp`) now
   routes on `{"credit_cards", "routing", card}` beside
   `{"credit_cards", "lifecycle", card}`, in `CardCycleDriver` and
   `Lifecycle`.

The session rng now draws only day frames and dynamics multipliers. No law
or distribution changed; the shared entity stream is untouched.

## How the production engine couples (the question the finding left open)

The windowed engine (`SessionBundle` → `Session::advance` →
`DayDriver::runDay`) and batch `Simulator` share `DayDriver`. A counter in
`runDay` (probe outside the repository, removed) measured 10,372 evolver
session draws at 2025-02-01 and 10,434 at 2025-03-01 (10,238 and 10,201 in
the harness); one extra draw after the February evolver moved the binary
183,820 → 180,818 rows (−1.6%), January identical. It never flipped under a
biller lane because the production catalogue differs (the shared stream
reaches `buildMerchants` elsewhere) and no production customer holds a
biller closing before March (13 and 45 in the harness). Favourites,
contacts and card rows coupled it all along.

## The authority rows

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| Evolver draws on {"evolve-…", PersonId, "YYYY-MM"} off `Market::laneSeed()`, never the session rng | Data-dependent draws on their own lane (merchant-churn-2026-07) | INVARIANT | - | ENFORCED: `test_merchant_churn` sub-gate G |
| Card lifecycle rows route on {"credit_cards", "routing", card} | Same rule | INVARIANT | - | ENFORCED: G with the card lifecycle on; the evolver lanes alone red (`c31ce07ee3446b94` shipped vs `b3198d702fe6d39c` stressed), green with it off (`949f0a7db44bce44` both), locating the second coupling |
| Session rng position after the window ignores biller and favourite sets | A harness row move is a mechanism, not a reshuffled shock sequence | INVARIANT | - | ENFORCED: G1 with every customer given a biller and favourite closing in January (13 vs 2,000 closed slots at 2025-02-01) reads `949f0a7db44bce44` both ways; G2: corpora differ (150,379 vs 150,492 session rows), nobody keeps the closed merchants; G3 disarm (one session draw per closed slot) `59502f28dd8ebcb6` vs `47230c332333d628`, red |
| Lane key is the calendar month | Same draws per month whatever the window | INVARIANT | - | ENFORCED: `test_card_point_in_time`, `test_arch_equivalence` |
| One BLAKE2b derivation per pass per person per month | Evolver stays O(persons) | MEASUREMENT | - | RECORDED: 283 ns per lane (2,000,000-lane probe): ~24M lanes, 7 s serial at pop 500,000 over 24 months; ~0.7 s of 198 s at 50,000 × 730 days; card routing adds one per card per run |

## Registered limitations

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| Gate row counts ride one day-shock realization | One Gamma(1.3, 1/1.3) shock per day: a 29-day month's mean shock has SD 0.877 / sqrt(29) = 0.163 | CHOICE (pre-existing) | - | REGISTERED: read a 60-day row move against the shock sum (spending rows per unit shock 2,400 / 2,375 / 2,412 before and after each step) |

## Where the implementation deviates from the design

1. Card routing lanes were not designed; the gate showed the goal unmet, and doing it
   now costs one re-pin instead of two (cash-hub-defect-2026-08: re-pin once, before the model change).
2. Lanes derive from the market (the one function every engine and the
   harness call), not a driver-bound factory the harness could miss
   (`Threads::rngFactory` carries the same seed).
3. Lazy lanes (opening draws nothing).
4. Added: sub-gate G with stress, non-vacuity and in-file disarm.

## Measured

`tests/golden_run.b2sum` (pop 2,000, 60 days from 2025-01-01, seed
3405691582): `5b6ec792...` 183,820 rows (502 fraud) → evolver lanes
`0b43b7137f4b2d66ee707e90ee20f713e844e308482e686b9d50debb3a5f1df8` 203,933
(+10.9%, 548 fraud) → card routing
`9242daa97bd7dab3258e4c8bb5022521475e93b25d986d6565c19716b6f0ac57` 217,566
(+6.7%, 576 fraud), captured per `tests/test_run_golden.cpp`.

Both moves are the shock realization: the evolver lanes keep January identical
(shock sum 23.722 over days 0-30, 57,087 spending rows) and changes shocks
from 2025-02-01 (February mean 0.918 → 1.236, −0.5 → +1.45 SD); card routing from
day 1. Window shock sums 50.333 / 59.562 / 64.058 (expected 60, SD 6.79),
spending rows 120,815 / 141,479 / 154,523: flat 2,400 / 2,375 / 2,412 per
unit shock. The old pin sat at −1.42 SD, the new at +0.60.

Re-measured: two biller-lane re-randomizations now leave
`test_bank_ledger` leg A at 174,319 rows, leg B 175,639 (2,278 postings),
the binary 217,566; only digests move (before: 145,906 vs 180,226).

Pins: shared-stream pins hold at `9e0a89591a4d861f` (`test_bank_ledger` A1,
`test_remote_payees` B1, `test_product_providers` B5). Re-pinned:
`test_bank_ledger` A2 155,131 rows (`0d570247c6b3bdb4`; postings 1,463 /
467 / 251 / 34) → 179,735 (`d0e908064223de61`; 1,423 / 470 / 286 / 34) →
174,319 (`7ff1cbfc35d35b98`; 1,495 / 459 / 285 / 39); `test_remote_payees`
B2 155,065 legit / 6,117 retired (`530f92353c0e0cb7`) → 179,654 / 7,340
(`d2c16e608a7a91f0`) → 174,278 / 7,195 (`60d0fb24bcc79c28`).
`test_card_payment_timing` passes the routing argument (no router, lane
never drawn). Non-PostgreSQL suite 68 of 68; PostgreSQL tests and
`test_scale_soak` not run.

For MulePatternLearner: no law moved; regenerate the 2024 corpus after this
round if not yet done.

## Owner must do

Re-pin the three table goldens against PostgreSQL (spending rows from day
1, fraud rows, card lifecycle devices and IPs, and balances move), then
re-run `docs/card_fraud_postgres_acceptance.sql` and
`docs/card_fraud_device_ip_investigate.sql`. `kTableCount = 43`.

# AMENDMENT: tuition-payee-2026-09

Every tuition installment paid one education catalogue merchant:
`tuition::generate` drew the payee once per run with `EducationPayees::pick`
(liveness ignored), so at pop 200,000 in 2024 all 32,398 rows named one
account (hub-realism-2026-09). Each student now pays an education record
open on every installment date, in the home area if one exists, else any
open one, on `{"family", "tuition-school", PersonId}`. The retired draw is
still spent, so only the tuition target moves. Gate
`tests/test_tuition_payees.cpp`.

## The owner's decision, and why the record could not make it

Code paid a merchant; the L-9 row (C4 definition pass, commit `bb2bdf9`,
2026-07-18) defined a parent-to-student transfer, from the misreading
"`tuition.cpp pickPayer` draws a parent, the payee is the student" (the
code already paid `fhelp::pickEducationMerchant`, then
`EducationPayees::pick` after `c874d15`, May 2026). Neither was an owner
decision. For the code's reading: the shape (4-5 installments 30 days apart
from a term start) is a school plan (CFPB 2023), and a student-cash
transfer would feed the liquidity throttle into more student spending.
Owner decision 2026-09-26: a school payment; home area first, uniform over
education records open for the whole plan, else all open records; amount
wording follows the code; one plan per run registered.

## The design

1. Payee law (`transfers/legit/routines/family/schools.hpp`,
   `schools::Directory`): home area on the first installment date via
   `remote::homeAreaAt` (relocation-aware, as funeral homes); a school
   qualifies if its interval holds the first and last dates (intervals are
   contiguous); pool = open records in the home area, else all open (online
   included); uniform, one bounded draw, none if empty.
2. Draws: the retired `uniformInt` over all education records is spent
   first on `{"family", "tuition"}`; the school is drawn after the plan's
   dates on `{"family", "tuition-school", PersonId}`, alone on its lane
   (merchant-churn-2026-07).
3. Routing parity: family rows share the `{"family", "routing"}` lane, so
   a plan with no open school is still made, then dropped.
4. Carriers: `EducationPayees` (catalogue, home areas, relocation), bound by
   `relatives::makeEducation(plan, sources)` beside funeral homes; every
   engine and harness reaches the family pass only via
   `generateFamilyTxns(plan, ...)`.
5. Memory: catalogue indices only (34 at pop 2,000, 1,115 at 200,000), per
   family pass (`docs/ram_derive_dont_store.md`).

## The authority rows

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| Tuition is a school payment plan: one parent's local account pays every installment to the student's school (an education catalogue record) | Families pay schools, often by term installment plans | CHOICE (definition) | Owner decision 2026-09-26; CFPB, *Tuition Payment Plans in Higher Education* (2023-09-14): ~4 million students per term; 87% of ~450 institutions offer plans [Certain] | CONFORMS. Deviations: the school also takes card payments and is weighted by card spend, not enrollment; no aid, loan disbursement, 529 or refund flow |
| Home-area school when one is open for the whole plan, else any open one | Most students attend near home | CHOICE | Hillman, TICAS, *How Far Do Students Travel for College?* (Oct 2023; NPSAS:20, public and private non-profit, online-only excluded), Table 1: median 17 miles, 57% within 25, 69% within 50 [Certain]; NCES Digest 309.20 (fall 2022): 1,619,747 of 2,139,884 first-time students in-state [Certain], 75.7% [Derived] | DEVIATES-BY-CHOICE: local = same city row, and nobody with a local option leaves: 0.991 local at pop 200,000 (8,651 of 8,733) vs 0.69; 0.582 at pop 2,000 (53 of 91; 38 live where no school is open). A mobility share (one more draw) is the owner's call |
| Uniform over the pool | Enrollment concentrates | CHOICE | NCES Digest 317.40 (fall 2021): 196 of 3,777 institutions with 20,000+ students enroll 7,096,900 of 18,621,073 [Certain]: 5.2% of institutions, 38.1% of students [Derived] | DEVIATES-BY-CHOICE (no enrollment data; card weight would import a spending law): largest school 0.42% at pop 200,000 (950 of 1,115 paid) |
| School open on every installment date | No payment outside an operating interval | INVARIANT | merchant-churn-2026-07 | ENFORCED: B1 0 closed-school rows; pop 200,000 has 44 records opening or closing in the span, a liveness-blind pick would hit 659 plans, disarmed 2,686 rows; C2, C3 |
| No open school: nothing paid, rows still made | Dropping rows must not move other sessions | INVARIANT | - | ENFORCED: A6 (all closed: 0 tuition rows, other family rows identical; skipping lands on the tuition-off digest); binds on neither corpus leg |
| Retired draw on `{"family", "tuition"}`; school on `{"family", "tuition-school", PersonId}` | Only the target moves | INVARIANT | - | ENFORCED: A pins the pre-round shared stream `ddfd735e74a97cd2`, the other 2,613 family rows (`3380fc1f52318eb5`, device and IP included), 210 tuition rows target-masked (`c748e4816d8e5467`). Disarms: school on the tuition lane leaves 182 rows and moves later sessions; burn skipped leaves 192 |
| Largest school ≤ 0.25 of tuition rows | No single account takes all tuition | INVARIANT (anti-hub) | - | ENFORCED: B2 0.0667 / 0.0042; D (settled corpus, run-golden config) 180 rows over 32 schools, 0.0722; bound clears the busiest home area (0.1758, 0.1631); one-school disarm 1.0 |

## Registered limitations

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| One plan per run, from the window's first month start | A student is billed every term | MEASUREMENT | CFPB 2023 | NONCONFORMING, not fixed (owner, 2026-09-26; pre-existing): a year from January pays spring only, twenty years one term. The fix adds rows and must keep a student's school across plans (the pick reads each plan's dates) |
| `buildPlan` anchors on `monthStart(window.start)` | Rows fall inside the window | INVARIANT | - | REGISTERED, unmeasured: a start after the 10th can predate the window; all configs start on the 1st |
| Few education records on gate legs | Home cities have schools | CHOICE | outlets-frequency density limitation | REGISTERED: 34 at pop 2,000 (38 of 91 plans fall back); 82 of 8,733 at 200,000 |
| B3 attributes plans through the payer | Every plan checked | CHOICE | - | REGISTERED: parents with two or more students skipped (9 of 100 payers at 2,000; 1,119 of 9,852 at 200,000); B1, B2, D read all rows |

## Measured (`test_tuition_payees`)

All sub-gates pass, ~6 s at ~2.0 GB peak (pop 200,000). Measured on an
isolated export of the staged hub-realism tree (`git checkout-index`) while
evolver-lanes-2026-09 was written, then on the committed evolver-lanes tree
(`9ab2953`): A-C identical (the family pass and income-free world do not see
evolver lanes); D below.

A (run-golden world, income off): pins as above; with the target the
tuition digest moved `496b31f798be1121` → `3801cba73588366e` (A5); tuition
off moves other family rows to `6663293bdc946311` (A4); all schools closed
keeps the pin (A6).

| B, family pass | run-golden world | pop 200,000, 2024, seed 42 |
|---|---:|---:|
| Education records (opening or closing in the span) | 34 (0) | 1,115 (44) |
| Tuition rows, schools paid | 210, 32 | 45,571, 950 |
| Largest school: rows, share | 14, 0.0667 | 191, 0.0042 |
| Busiest home area's share of attributed plans | 0.1758 | 0.1631 |
| Attributed plans: home, national fallback, misplaced | 53, 38, 0 | 8,651, 82, 0 |
| Plans a national pick would misplace (expected) | 48.3 | 8,312.4 |
| Plans a liveness-blind pick sends to a closed school (expected) | 0.0 | 659.0 |

C (hand-built catalogue): 6 education records counted, the grocery not;
area 5 splits 2,038 / 1,962 over two open schools; a closing school, an
opening school and no home area fall back to the same three (1,377 / 1,332
/ 1,291); nothing open returns no school without touching the lane; one
bounded draw per pick.

D (settled run-golden corpus): 180 of 210 rows settle over 32 schools, none
off-catalogue or closed, largest 13 (0.0722); 184 and 14 (0.0761) on the
hub-realism tree (the funding screen moved with the evolver).

| Disarm (temporary production edit) | Red on |
|---|---|
| School drawn on the tuition lane | A2, A3 (182 rows) |
| Retired draw not spent | A2, A3, A6 (192 rows) |
| Liveness ignored | B1 at pop 200,000 (2,686 rows), A6, C1-C5 |
| National pick only | B3 (49 and 8,330 misplaced), C1 |
| No-school plan skipped | A6 (tuition-off digest `6663293bdc946311`) |
| One school for all | B2 and D (1.0), B3, C1, C2 |

Harness trap: `Router` keeps sticky device and IP positions that `make`
advances, so a second family pass on one router starts where the first
ended; A6 failed until each pass copied the pristine family router, as
production does.

## Re-pins

- `test_bank_ledger` and `test_remote_payees` also mask the tuition target;
  pre-round and this build agree on both trees. Landing tree (`9ab2953`):
  `42502a8f4aba68ee` over 174,319 rows; `e14294a2f362eda3` over 174,278
  legit / 7,195 retired (the pre-round build reproduced that tree's run
  golden). Hub-realism tree: `0188a8c6befb3903` over 155,131;
  `8f0b962248053292` over 155,065 / 6,117.
- `tests/golden_run.b2sum` `9242daa97bd7dab3...` →
  `b1f59ace045e6c6fdee6f8ef6038bfb801660760457786aba249885de6723e7b`, same
  217,566 rows (hub-realism tree: `5b6ec79229b1f7fc...` →
  `866adb10a8252751...`, 183,820), domain predicate held. Suite 69 of 75
  (five PostgreSQL skips with code 77; `test_scale_soak` opt-in).

## Owner must do

Decide the Tuition amount axis (published or net of aid, with or without
school-billed housing) and the mobility share. Re-pin the three table
goldens where they carry tuition rows and re-run
`docs/card_fraud_postgres_acceptance.sql` and
`docs/card_fraud_device_ip_investigate.sql` (in addition to the
hub-realism re-pin). The mule-temporal regeneration also retires the
education hub from MulePatternLearner's registry. `kTableCount = 43`.

# AMENDMENT: mule-label-contract-2026-10

The mule-temporal Account table lacked nine of the fifteen fields of
MulePatternLearner's (MPL's) Account label contract, so loads left them at
defaults and every `mule_ring_id` stayed -1. It now carries all fifteen in
MPL's load order (`load_accounts`, `ACCOUNT_LOAD_COLUMNS`): the six
existing columns, then `mule_label_known`, `is_mule_masked`, `pu_label`,
the effective and availability clocks, `mule_ring_id`,
`mule_label_source`. Exporter only; no draw, event, edge, rail, id or other
table moves; no golden moves. Contract: `docs/mule_temporal.md`,
"Account-level mule supervision". Gates: `tests/test_mule_temporal.cpp`
(`checkMuleLabels`, fixture) and `tests/test_mule_temporal_labels.cpp`
(real world, windowed engine).

## The reveal guard decides `mule_label_known`

MPL's one-time reveal `reveal_mule_labels` reads `is_mule`, `is_external`,
`first_seen_ts_ms`, `first_seen_seq`, Zelle verdicts and clocks, inter-mule
payments and scope partitions, not the new clocks, ring or source. With any
internal label known or revealed it answers `already_revealed` and writes
nothing unless forced (MPL's preparation does not force). Following MPL's
loading guidance (known labels, explicit 0 for non-mules) would leave every
mule masked and training without positives. So the export marks nothing
known, masks every label (`pu_label` 0), and carries truth, clocks, ring
and source; the reveal writes labels and keeps rings.
`validate_label_contract` counts one `invalid_unknown` per mule right after
load, zero after the reveal.

## The authority rows

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| `mule_label_known` False everywhere | MPL's reveal makes labels known and runs only when none is | CHOICE | MPL `gsql/queries/label_reveal.gsql` (`already_revealed`), `src/mule_pattern_learner/tigergraph/reveal.py` (`apply`, no `force`) | DEVIATES-BY-CHOICE from `docs/reference/labels.md`, "Loading accounts". Sub-gate E: `invalid_unknown` 27 as loaded, all 0 after; disarm red in both gates. If MPL ever separates source from revealed labels, True for internal accounts is the only change |
| `is_mule_masked` True, `pu_label` 0 everywhere | The reveal chooses positives | INVARIANT | MPL `docs/reference/labels.md` (`pu_label` 1 iff known, mule, unmasked) | ENFORCED: both gates |
| Mule effective clock = first exported payment on a ring laundering channel (`Fraud` group), else first observation | The contract wants first simulated mule activity | CHOICE | MPL `docs/reference/labels.md`, "Loading accounts" | ENFORCED: D finds the payment independently (27 of 27); channel only (flipping verdicts and ring ids moves no Account byte); verdict-based disarm reds the fixture |
| Other internal accounts: clocks = first observation | Account creation may be both clocks | CHOICE | same | ENFORCED: both gates |
| Availability = effectiveness | Simulator truth is complete at once, like the Zelle oracle | CHOICE | `docs/mule_temporal.md` (Zelle oracle) | REGISTERED: the reveal replaces availability with discovery and internal effective clocks with first observation |
| `mule_ring_id` = home ring (whose members hold the owner); -1 otherwise | One ring per account; a later ring's mule was recruited by its home ring | CHOICE | MPL `docs/reference/labels.md`; `synth/people/rings.hpp` (`injectMultiRingMules` adds entries, never members) | ENFORCED: C vs topology and laundering rows; 1 of 27 mules in two rings; first-ring disarm red |
| External accounts unknown: `is_mule` 0, zero clocks, empty source | No external mule role | INVARIANT | `docs/mule_temporal.md` | ENFORCED: 1,649 external accounts |
| Other 26 tables and the first six Account columns unchanged | Exporter-only supervision | INVARIANT | - | ENFORCED: A vs pre-round HEAD `dac2d64` (26 tables byte for byte; six columns = the old table, `954256dfa302dbf7`, 277,088 bytes); fixture checks targeted cell moves |
| A loader must map all fifteen Account columns, with MPL's label contract as the reference | Loading is how the owner gets the graph | INVARIANT | MPL's Account label contract | OPEN, outside this repository: the TigerGraph loader still takes six columns and refuses the fifteen-column table, so a regenerated corpus does not load until it changes |

## Registered limitations

| The value | The claim about the world | Class | Citation | Status |
|---|---|---|---|---|
| Account rows written at export end, in first-observation order | Metadata emitted at first observation | CHOICE | `docs/mule_temporal.md` | REGISTERED: label clocks are the only late-set cells; a window ending before a mule's first laundering payment shows first observation; 24 bytes per account |
| Booleans render `True`/`False` | MPL asks for lowercase | CHOICE | `docs/mule_temporal.md` | REGISTERED: the TigerGraph loader lowercases them (as `is_external`); a CSV copied straight to `load_accounts` needs the three flags lowercased (TigerGraph's reading unchecked) |
| An external account's empty source is NULL in PostgreSQL | `COPY ... FORMAT csv` reads an unquoted empty as NULL | CHOICE | - | REGISTERED: validation uses `coalesce`; a loader must accept a NULL `mule_label_source` |
| Three new checks in `docs/research/validate_temporal_dataset.sql`, two profile keys | Staged tables keep the contract | MEASUREMENT | - | NOT RUN (no PostgreSQL) |

## Measured (`test_mule_temporal_labels`)

Pop 600, 2019 (365 days), seed 20261004, scaled fraud profile: 304,259
payments, 10 rings, 27 mules (10 home rings, 1 in two rings, 27 with a
laundering payment), 1,649 external accounts. Suite 70 of 76 (five
PostgreSQL skips; `test_scale_soak` opt-in); `test_run_golden` passes
unmoved. A serverless binary run (`--usecase mule-temporal`, pop 500, 30
days from 2019) completes. Sub-gate B requires `schemas/mule_temporal.gsql`'s
Account vertex to store columns at `$0`-`$4`, `$6`-`$14`, then `$5` (MPL's
`load_accounts` mapping; the vertex already has the fifteen attributes, so
the schema is unchanged); moving `is_mule` to sixth reds it.

## Owner must do

1. Update the TigerGraph loader to map all fifteen Account columns (MPL's
   label contract is the reference) before the next load.
2. Regenerate the mule-temporal corpus; run
   `docs/research/validate_temporal_dataset.sql` and
   `docs/research/profile_temporal_dataset.sql`.
3. Clear the graph's data and load the regenerated corpus from a clean
   export.
4. Give MPL's run a new `scope.id` and train.

The mule-temporal table count (27) does not move.
