# PhantomLedger Fraud-Model Research Audit

Status: **STEP 4 SHIPPED (2026-07-18)** — the approved ADJUST batch
("fraud-audit-2026-07") is implemented, verified live in the binary,
and BOTH goldens are re-pinned. Remaining in this cycle: the named
commit (owner runs git; command in the session log), the realized-F1
fraction measurement at the pinned seed (one command, below), and the
step-5 audit-closure items (source editions; SAR funnel calibration).

## Purpose and rules

This audit validates every research-sensitive fraud parameter against
primary sources BEFORE any of them may change:

1. Classification: **MEASUREMENT** / **TYPOLOGY** (validate shape) /
   **CHOICE** (document, don't "validate").
2. Never conflate prevalence AXES; cross-axis mapping is labeled inference.
3. Labels are not interchangeable: `is_fraud`, SARs, alerts/CTRs,
   chain/shell labels are calibrated separately.
4. Any change is a **model-version change** + deterministic re-pin
   (BOTH goldens, one commit). This batch was the table-digest golden's
   promotion trial (verdict below).

## SHIPPED ADJUST BATCH — model version "fraud-audit-2026-07"

Owner decisions (2026-07-17): F1 = widen band; F2 = derive real score;
F3 = add filing probability. Implemented 2026-07-18.

### F1 — Structuring band widened (SHIPPED)

* `typologies/structuring.hpp Rules`: `epsilonMax` **400 → 1500**
  (ε ∈ [50, 1500] ⇒ threshold-profile splits land $8,500–$9,950;
  threshold and splitsMin/Max 3–12 unchanged).
* Effect: a documented fraction of structuring splits falls BELOW the
  [9,000, 10,000) alert band — the alert label becomes realistically
  incomplete. With ε uniform, ≈ (1500−1000)/1450 ≈ 34.5% of
  threshold-profile splits land under $9,000 (realized fraction: see
  the pinned-seed measurement below).
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
  (audit-closure item). The probe below shows the JOINT gate (draw ×
  floor) bites hard at small N — revisit only via a new model version.

### Shipping procedure — EXECUTED (outcome per step)

1. Implemented in one round (structuring.hpp; labels.hpp/.cpp; both aml
   streaming sinks + both exportFromArtifacts; sar.hpp/.cpp).
   `make test`: the goldens failed as designed and everything else
   stayed green. `test_fraud_amounts` was examined deliberately: it
   pins the UNAUTHORIZED samplers (cardTestCharge / cardFraudSpend /
   atoDrainAmount), not structuring — green through the batch, no
   update needed.
2. TRIAL SCORING (recovered from git — the committed pre-batch baseline
   was still live because every intervening round was output-neutral):
   the CSV golden flagged EXACTLY {transactions.csv, HAS_PAID.csv,
   ACCOUNT_FLOW_AGG.csv} — the F1 corpus fingerprint, matching the
   blast-radius map, nothing else.
   **COVERAGE FINDING (2026-07-18): both goldens pin a MULE_ML-ONLY
   run** — F2's shell tables and F3's SAR tables are not under ANY
   golden pin, so their blast radius was invisible to the trial.
   Additionally the pre-batch `golden_tables.md5` had never been
   committed (untracked), so the table golden's catch list is
   established structurally (direct tables tee the SAME rendered bytes;
   all four direct gates stayed green) rather than by an observed diff.
   **PROMOTION: PASSED, with those two caveats.** Follow-ups recorded
   in the systemprompt: (a) commit baselines at capture, (b) add a
   FRAUD-VISIBLE golden pin (an aml section in the table golden) before
   relying on goldens to catch label-model drift.
3. Both baselines re-pinned (rm → `make test` twice → all 42 green).
4. Commit: owner action, one commit named
   "model: fraud-audit-2026-07 (F1 structuring band, F2 shell score,
   F3 SAR filing) — golden re-pin #7" (also carries the uncommitted
   pg-native arc; re-pin #7 in the commit-naming sequence — unrelated
   to the CLOSED accrual candidate formerly tracked as #7).
5. This document update; dataset parameter statements below.

### Live-binary evidence (probe: aml, pop 10000, 60 d, seed 7, file-only)

* Invariant 5 intact: candidates L=909,116; fraud rows 1,769 (0.1944%).
* **F2 live:** ShellAccount scores are a derived spread
  {0.00 ×5, 0.04, 0.10, 0.21} — the constant-1.0 column is gone.
* **F3 live:** 2 SARs filed of 9 fraud groups (5 rings + 4 solos); the
  joint gate is doing real work (solo unauthorized totals often sit
  under the $5,000 floor). Deterministic and content-keyed; funnel
  calibration deferred to audit closure as specced.
* **F1 at this seed:** zero structuring rows (≈11% likelihood — 5 rings
  at 0.36 structuring-phase playbook mass), so the realized-fraction
  measurement moves to the pinned seed, where the baseline diff proves
  structuring rows exist.

### F1 realized-fraction measurement (pinned config — PENDING one run)

The corpus is use-case-independent, so an aml run at the CSV golden's
config reproduces the pinned corpus with the full ledger dump:

```
PL_FILE_ONLY=1 ./build/phantomledger --usecase aml --population 2000 \
  --days 60 --seed 3405691582 --show-transactions --out /tmp/pl_f1_probe

grep -c fraud_structuring /tmp/pl_f1_probe/transactions.csv

awk -F, '$10=="fraud_structuring" && $3+0>=8500 {n++; if($3+0<9000)b++} \
  END{printf "threshold splits=%d below-alert-band=%d share=%.3f\n", \
  n, b, (n?b/n:0)}' /tmp/pl_f1_probe/transactions.csv
```

Record the share here (expected ≈ 0.345; pre-batch: exactly 0). If the
grep is positive but the awk n=0, the column indexing is wrong — stop
and re-derive from the header (amount=$3, channel=$10 per kLedgerHeader).

### Dataset parameter statements (for user-facing dataset docs)

* Structuring splits: 60% threshold profile at `threshold − ε`,
  ε ~ U[50, 1500] ⇒ $8,500–$9,950; ≈34.5% of threshold-profile splits
  fall below the $9,000 alert band, so per-transaction alerts
  under-cover structuring BY DESIGN. (25% medium [3k,7k); ~15% small
  [300,1500).)
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
mirror placement→layering→integration); A.5 structure (layering 3–8
hops; structuring ε [50,400] × 3–12 splits — F1 SHIPPED ε [50,1500]);
A.6 camouflage (p2p 0.03/day, bill 0.35, salary 0.12); A.7 labels (SAR
one-per-group — F3 SHIPPED filing gate; shellScore ≡ 1.0 — F2 SHIPPED
derived score).

## Evidence dossier (step 2, drafted 2026-07-17)

PROVENANCE: knowledge-based citations of named public sources; verify
current editions before final calibration claims (statutes excepted).

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
| 2026-07-18 | F1 structuring band | **SHIPPED** (ε max 400→1500) | FinCEN/FFIEC; baseline diff = {transactions, HAS_PAID, ACCOUNT_FLOW_AGG} | measure realized fraction (pending) |
| 2026-07-18 | F2 shell score | **SHIPPED** (derived score) | FATF shell typologies; probe spread {0…0.21} | none |
| 2026-07-18 | F3 SAR filing | **SHIPPED** (p=0.70 + $5k floor) | FinCEN SAR Stats; 31 CFR §1020.320; probe 2/9 filed | funnel calibration at audit closure |
| 2026-07-18 | golden coverage | **FINDING** | both goldens pin a mule_ml-only run; fraud-label tables unpinned | add fraud-visible pin (aml table-golden section) |
| 2026-07-17 | budget p = 0.0012 | CHOICE-DOCUMENTED | Fed Payments Study [verify] | dataset docs |
| 2026-07-17 | playbooks / camouflage | CHOICE-DOCUMENTED | — | dataset docs |
| 2026-07-17 | ring size, reuse, repeat victimization | PLAUSIBLE | EMMA / FTC / IC3 [verify editions] | finalize after verification |
