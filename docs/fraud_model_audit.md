# PhantomLedger Fraud-Model Research Audit

Status: **STEP 4 SHIPPED + MEASURED (2026-07-18)** — the approved ADJUST
batch ("fraud-audit-2026-07") is implemented, verified live in the
binary (probe evidence below, including the realized F1 fraction), and
both golden baselines were recaptured. The table-golden PROMOTION TRIAL
came back VACUOUS (the goldens are structurally blind to this batch —
see the coverage finding); the FRAUD-VISIBLE PIN closing that gap is
DELIVERED (test_table_golden "fraud" section; baseline capture pending
owner's next `make test`). Remaining in this cycle: the named commit,
the pin's baseline capture + commit, and the step-5 audit-closure items
(source editions; SAR funnel calibration).

## Purpose and rules

This audit validates every research-sensitive fraud parameter against
primary sources BEFORE any of them may change:

1. Classification: **MEASUREMENT** / **TYPOLOGY** (validate shape) /
   **CHOICE** (document, don't "validate").
2. Never conflate prevalence AXES; cross-axis mapping is labeled inference.
3. Labels are not interchangeable: `is_fraud`, SARs, alerts/CTRs,
   chain/shell labels are calibrated separately.
4. Any change is a **model-version change** + deterministic re-pin
   (EVERY golden baseline the change touches, one commit).

## SHIPPED ADJUST BATCH — model version "fraud-audit-2026-07"

Owner decisions (2026-07-17): F1 = widen band; F2 = derive real score;
F3 = add filing probability. Implemented 2026-07-18.

### F1 — Structuring band widened (SHIPPED, MEASURED)

* `typologies/structuring.hpp Rules`: `epsilonMax` **400 → 1500**
  (ε ∈ [50, 1500] ⇒ threshold-profile splits land $8,500–$9,950;
  threshold and splitsMin/Max 3–12 unchanged).
* Effect: a documented fraction of structuring splits falls BELOW the
  [9,000, 10,000) alert band — the alert label becomes realistically
  incomplete. Analytic expectation ≈ (1500−1000)/1450 ≈ 34.5% of
  threshold-profile splits under $9,000.
* **Realized measurement (probe: aml, pop 10000, 60 d, seed 7; posted
  corpus):** 53 structuring rows; 8 threshold-profile splits (≥ $8,500),
  of which 3 below the alert band ⇒ realized share **0.375** (n=8;
  consistent with 0.345). **Liveness proof:** 7 of the 8 sit below
  $9,600 — the OLD band's floor (ε ≤ 400 ⇒ splits ≥ $9,600) — values
  impossible pre-batch.
* **Posted-mix observation (MEASUREMENT, logged):** posted profile mix
  was 8 threshold / 17 medium / 28 small (15%/32%/53%) vs the sampler's
  60/25/15 — clearing rejects unfunded victim debits and rejection
  probability rises with amount, so the POSTED corpus under-represents
  large splits. Emergent, realistic, not a defect; revisit only if a
  future audit wants the posted mix calibrated directly.
* Row-count neutrality BY CONSTRUCTION: every branch of
  `sampleSmurfAmount` consumes exactly two RNG draws, so ε changes
  values only — split counts, victim coins, timestamps, targets, chain
  ids and the fraud denominator L are untouched (invariant 5).

### F2 — Shell score derived from structure (SHIPPED)

* `labels.cpp` keeps the candidate set (ring members' shell-flagged
  primary accounts — fraud-scale, bounded) but `shellScore` is now a
  derived statistic instead of the constant 1.0.
* Mechanism (`labels::ShellStats`): `initShellStats` seeds the
  candidate rows + aggregate slots from static topology BEFORE the
  fold; ONE shared `accumulateShellTxn` runs per row inside BOTH aml
  sinks (and therefore both engines — the corpus paths run the same
  sinks); `finalizeShells` scores in the epilogues. Aggregates per
  candidate: inflow, outflow, first/last txn ts (retained for
  calibration), fraud txn count, total txn count.
* Score formula (CHOICE, documented):
  `passThrough = min(in,out) / max(max(in,out), 1e-9)`;
  `organicShare = (total − fraudCount) / max(total, 1)`;
  `shellScore = round2(passThrough × (1 − organicShare))` — the round2
  is the shared money rounding in `writeShellAccountRows` (no new float
  formatting). Dormant pass-through ring accounts score near 1;
  camouflaged or organically active accounts score lower; zero-activity
  candidates score 0.
* Determinism: the formula consumes only order-insensitive aggregates
  (sums/counts/min/max ts) — window/thread invariant by construction;
  output row order comes from the deterministic candidate vector, never
  from map iteration.

### F3 — SAR filing probability + monetary floor (SHIPPED)

* `aml/sar.cpp filesSar` (applied inside `generateSars`, so every call
  site — both exporters and the windowed epilogue — is gated): a group
  files iff BOTH (a) `stableU64({"SAR_FILE", <SAR id>}) % 100 < 70`
  (content-keyed on the SAR id — deterministic, batching-independent,
  the same device as the 1-in-8 alert escalation), and (b) rounded
  `amountInvolved` ≥ **$5,000** (31 CFR §1020.320).
* Effect: SAR presence is an incomplete institutional-response label,
  distinct from `is_fraud` ground truth. Subset-safety of every
  consumer verified during implementation: case→SAR indices are
  computed by `buildBundle` against the filtered span it receives, and
  all writers iterate the span (`writeSarRows`, REFERENCES, SAR_COVERS,
  SUBJECT_OF_SAR, ESCALATED_TO, RESULTED_IN, summaries).
* Calibration note: 0.70 is the initial CHOICE pending verification of
  current FinCEN SAR Stats against the simulated detection funnel
  (audit-closure item). The joint gate (draw × floor) bites hard at
  small N — probes observed 2/9 groups filing (pop 10000) and 0/2 (pop
  2000). Deterministic and content-keyed; revisit only via a new model
  version.

### Shipping procedure — EXECUTED (outcome per step, CORRECTED)

1. Implemented in one round (structuring.hpp; labels.hpp/.cpp; both aml
   streaming sinks + both exportFromArtifacts; sar.hpp/.cpp). `make
   test`: ALL GREEN — including both goldens, see step 2 for why.
   `test_fraud_amounts` was examined deliberately: it pins the
   UNAUTHORIZED samplers (cardTestCharge / cardFraudSpend /
   atoDrainAmount), not structuring — green through the batch, no
   update needed.
2. TRIAL SCORING — **VACUOUS (corrected 2026-07-18).** The pinned
   config's corpus (STANDARD use case, pop 2000/60 d/seed 3405691582;
   173,986 rows, reproduced by probe) contains ZERO structuring rows —
   its single ring drew a non-structuring playbook (P ≈ 0.64 per ring)
   — so F1 changed no pinned byte; and F2/F3 write tables that exist
   only in the aml use cases, which NO golden pinned (**COVERAGE
   FINDING: both goldens pin a STANDARD-use-case-only, fraud-sparse
   run**). The batch was therefore invisible to both goldens; they
   passed unchanged, correctly.
   The 3-line `golden_run.b2sum` delta observed against the last
   commit ({transactions, HAS_PAID, ACCOUNT_FLOW_AGG} — exactly the
   corpus-order/amount-sensitive files) PREDATES the batch: it
   accumulated from the earlier UNCOMMITTED session arc (most plausibly
   the S10 ordering re-pin), and with no commits in between the exact
   attribution is unrecoverable. An earlier revision of this document
   wrongly attributed that delta to F1 — RETRACTED here.
   **PROMOTION: NOT DECIDED by this batch.** The fraud-visible pin is
   the promotion vehicle (DELIVERED — see below); CSV arc step 5 stays
   gated behind it. Process lessons: commit baselines at capture;
   commit every round.
3. Both baselines recaptured (rm → `make test` twice → all 42 green).
4. Commit: owner action, one commit for the model batch + the
   uncommitted arc; body states the batch is golden-invisible and the
   baseline delta comes from the earlier arc (exact command in the
   session log).
5. This document update; dataset parameter statements below.

### FRAUD-VISIBLE PIN (delivered 2026-07-18; capture pending)

`test_table_golden` gained a second section: aml-txn-edges at the
probe-verified fraud-dense config (pop 10000, 60 d, seed 7 — 5 rings,
structuring spanning the F1 band, derived shell scores, SARs, alerts,
CTRs, cases), baselined in `tests/golden_tables_aml.md5`. The gate
HARD-REQUIRES ShellAccount / Sar / Alert / Ctr / InvestigationCase in
the pin and also pins the fraud-dense corpus (row_seq). First `make
test` captures the baseline (SKIP 77) — commit it in the capture round.
This section's first intentional-model-change round is the table
golden's REAL promotion trial.

### Live-binary evidence (probes, 2026-07-18, file-only)

Probe A — aml, pop 10000, 60 d, seed 7:
* Invariant 5 intact: candidates L=909,116; fraud rows 1,769 (0.1944%).
* **F1 live + measured:** see F1 section (share 0.375, 7/8 below the
  old band's floor).
* **F2 live:** ShellAccount scores are a derived spread
  {0.00 ×5, 0.04, 0.10, 0.21} — the constant-1.0 column is gone.
* **F3 live:** 2 SARs filed of 9 fraud groups (5 rings + 4 solos).

Probe B — aml, pop 2000, 60 d, seed 3405691582 (the pinned corpus;
use-case-independent, rowcount-matched 173,986):
* Zero structuring rows (grep over the full ledger dump) — basis of the
  trial-scoring correction above. 1 ring + 1 solo; SARs filed: 0
  (joint F3 gate at N=2).

**Probe pitfall (recorded):** ledger CSVs end rows with CRLF (RFC 4180,
`csv.cpp endRow`). awk field equality on the LAST column silently fails
unless the `\r` is stripped: use `sub(/\r$/,"",$10)` (or match with
`index()`); grep is substring-based and unaffected. Two probe awk runs
returned false zeroes this way before the CRLF was spotted.

### Dataset parameter statements (for user-facing dataset docs)

* Structuring splits: 60% threshold profile at `threshold − ε`,
  ε ~ U[50, 1500] ⇒ $8,500–$9,950; ≈34.5% of threshold-profile splits
  (realized 0.375 at probe scale) fall below the $9,000 alert band, so
  per-transaction alerts under-cover structuring BY DESIGN. (25% medium
  [3k,7k); ~15% small [300,1500); POSTED mix skews smaller because
  unfunded victim debits bounce at clearing.)
* `shell_score` = round2(passThrough × (1 − organicShare)) over each
  candidate account's full-history flows; near 1.0 ⇒ dormant pure
  pass-through; low ⇒ camouflaged/organic; 0 ⇒ no activity. Candidates
  remain ring members' shell-flagged primary accounts.
* SAR filing: a fraud group files with probability 0.70 (content-keyed,
  deterministic) AND only if activity total ≥ $5,000 (31 CFR
  §1020.320). SAR is an institutional-response label; `is_fraud`
  remains ground truth.

## Appendix A — PRE-BATCH CODE VALUES (extracted 2026-07-17)

Historical reference for the shipped batch (F1/F2/F3 rows superseded by
the values above). A.1 prevalence (6 rings/10k σ0.4; 4 solos/10k; caps
0.06/0.005; budget p = 0.0012); A.2 rings (size lognormal μ2 σ0.7
[3,150]; mules Beta(2,4) [0.10,0.70], reuse 0.06; victims lognormal μ3
σ0.8 [3,500], repeat 0.10); A.3 alerts (below-CTR [9000,10000) sev2;
CTR ≥10000 sev3 — statutory CONFIRMED; velocity ≥5/day sev2; escalation
⅛; 30/90d windows); A.4 playbooks (17, weights sum 1.00; composites
mirror placement→layering→integration; structuring-phase mass 0.36 ⇒
a ring lacks structuring with P ≈ 0.64); A.5 structure (layering 3–8
hops; structuring ε [50,400] × 3–12 splits — F1 SHIPPED ε [50,1500]);
A.6 camouflage (p2p 0.03/day, bill 0.35, salary 0.12); A.7 labels (SAR
one-per-group — F3 SHIPPED filing gate; shellScore ≡ 1.0 — F2 SHIPPED
derived score).

## Evidence dossier (step 2, drafted 2026-07-17)

PROVENANCE: knowledge-based citations of named public sources — NOT
live web verification; verify current editions before final calibration
claims (statutes excepted). Web verification is the audit-closure step.

* F1: FinCEN structuring guidance / FFIEC BSA-AML material — real
  structuring spreads wider below the threshold; per-transaction
  detection is not guaranteed. → ADJUST shipped.
* F2: FATF beneficial-ownership/shell typologies — shells are defined
  by dormancy, pass-through, opacity; constant-score ring-membership
  labeling overclaims. → ADJUST shipped.
* F3: FinCEN SAR Stats (millions of SARs/year; institutional detection
  incomplete; low TM conversion) — 100% group filing makes SAR ≡ ground
  truth. → ADJUST shipped. Floor: 31 CFR §1020.320.
* Non-flagged rows: layering 3–8 hops CONFIRMED (FATF Professional ML
  2018); ring size/mule reuse PLAUSIBLE (Europol EMMA — verify current
  cycle); repeat victimization PLAUSIBLE (FTC/IC3 — verify); budget
  p = 0.0012 CHOICE-DOCUMENTED (intentional oversample vs Fed Payments
  Study basis-point rates); playbook weights CHOICE-DOCUMENTED;
  camouflage CHOICE-DOCUMENTED (+ planned separability measurement).

## Findings log

| Date | Parameter | Verdict | Evidence | Action |
|---|---|---|---|---|
| 2026-07-17 | CTR ≥ $10,000 | CONFIRMED (statutory) | 31 CFR §1010.311 | none |
| 2026-07-17 | layering 3–8 hops | CONFIRMED (shape) | FATF Professional ML 2018 | none |
| 2026-07-18 | F1 structuring band | **SHIPPED + MEASURED** (ε max 400→1500) | probe: share 0.375 below alert band; 7/8 splits below the old band's floor | none |
| 2026-07-18 | F2 shell score | **SHIPPED** (derived score) | probe spread {0…0.21} | none |
| 2026-07-18 | F3 SAR filing | **SHIPPED** (p=0.70 + $5k floor) | probes 2/9 and 0/2 filing | funnel calibration at audit closure |
| 2026-07-18 | golden coverage | **FINDING → FIX DELIVERED** | both goldens pinned a standard-only, fraud-sparse run; batch was golden-invisible; promotion vacuous | fraud pin delivered (test_table_golden fraud section); capture + commit pending |
| 2026-07-18 | posted structuring mix | **MEASUREMENT (observation)** | posted 15/32/53 vs sampler 60/25/15 — unfunded rejections rise with amount | none (emergent realism; revisit only on demand) |
| 2026-07-17 | budget p = 0.0012 | CHOICE-DOCUMENTED | Fed Payments Study [verify] | dataset docs |
| 2026-07-17 | playbooks / camouflage | CHOICE-DOCUMENTED | — | dataset docs |
| 2026-07-17 | ring size, reuse, repeat victimization | PLAUSIBLE | EMMA / FTC / IC3 [verify editions] | finalize after verification |
