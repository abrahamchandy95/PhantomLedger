# Docs audit — TGN reorientation, 2026-08

**Scope.** Ten auditors read `docs/` (16 files), `README.md` and the four
executable SQL artifacts, each against an adversarial verifier. Raw output was
**262 findings**. The verifiers **REFUTED 13**; those are dropped or restated as
the verifier's correction and are marked below. The surviving 249 findings
**deduplicate to 84 distinct issues**: **21 blocking, 27 high, 26 medium, 10
low**. Nine further items are *load-bearing text that looks deletable* and are
in section 7, not counted as defects.

**Working-tree state, read before anything else.** Seven files already carry
UNCOMMITTED edits from an earlier pass of this same reorientation:
`README.md`, `docs/card_fraud_online_gnn.md`, `docs/card_fraud_v2_roadmap.md`,
`docs/debugging.md`, `docs/era_data_provenance.md`,
`docs/fraud_model_audit.md`, `docs/ram_derive_dont_store.md`
(`git diff --stat`: 115 insertions, 40 deletions). Those edits removed every
TabFormer/IBM token from `docs/` and `README.md`, fixed the README table count
37 → 43 (`README.md:1091`), corrected the README `Is_Merchant` and
`Has_Device`/`Has_IP` claims, replaced the calendar-date evaluation split in
`docs/card_fraud_online_gnn.md:106-132`, and added the 100k×3y memory
arithmetic at `docs/ram_derive_dont_store.md:96-118`. **Section 2's inventory
and every line number below are read from the CURRENT working tree**, so they
already account for that pass. Findings the pass resolved are marked
`ALREADY FIXED IN WORKING TREE` and are excluded from the counts above.

---

## 1. Verdict per document

| doc | last touched | rounds behind | verdict | one-line reason |
|---|---|---|---|---|
| `docs/card_fraud_online_gnn.md` | 2026-08-05 `466951e` (+ uncommitted) | 4 → 2 | **major-rewrite** | Now the primary TGN artifact; split and TabFormer already fixed, but the feature/gate table still cites gates that do not run and says nothing about what actually reaches TigerGraph. |
| `docs/card_fraud_feature_contract.md` | 2026-08-05 `466951e` | 4 | **major-rewrite** | Highest-value doc for a TGN feature pipeline and the only one the fix pass did not touch; two central rulings falsified and its own status block stops at 2026-07-27 (`:3`). |
| `docs/fraud_model_audit.md` | 2026-08-05 `ce42282` (+ uncommitted) | 4 (Part I) / 5 (Part II) | **major-rewrite** | The citation ledger. Part IV (`:565-610`) is wrong about the loader contract, the table count and the anti-shortcut levels; the amendment chain stops at `:1346` (2026-07-30). |
| `docs/card_fraud_v2_roadmap.md` | 2026-08-05 `466951e` (+ uncommitted) | 8 | **retire-or-archive** | ROUNDS 1-8 narrative with a live-sounding status line (`:3`, dated 2026-07-27). Its history is worth keeping; it must stop competing with CLAUDE.md. Lift rulings 2 and 6 (`:33`, `:39`) and the parity trap (`:608`) to a live page first. |
| `docs/card_fraud_victimization.md` | 2026-08-05 `466951e` | 4 | **keep-with-edits** | Mechanism and the F1/F2/D1/D2/D3 anchors still bind (13 code sites cite them); status block, code pointers and the expired crypto/era argument (`:281`) are stale. |
| `docs/card_fraud_postgres_acceptance.sql` | 2026-08-05 `466951e` | 1 | **keep-with-edits** | Correct gate; header still says 39 tables (`:35`) while its own assertions demand 43 (`:96`, `:177`), and it covers 2 of the loader's 7 empty-table aborts. |
| `docs/card_fraud_device_ip_investigate.sql` | 2026-08-05 `466951e` | 1 | **keep-with-edits** | Section-3 branch is dead code whose guard contradicts its own message (`:170`); the closing trigger (`:321`) can never fire. |
| `docs/tf_gnn_prep_session_endpoints.sql` | 2026-08-05 `466951e` | 1 | **major-rewrite** | The repair is still needed but its entire stated justification (`:10-22`, `:34`, `:296`) was inverted by `attacker-infra-2026-07`; as written it tells the operator to skip it. |
| `tf_gnn_prep_ddl.sql` (repo root) | 2026-08-05 `466951e` | 3 | **retire-or-archive** | A `pg_dump` snapshot behind the live schema and behind the loader repo; restoring it reverts the session-endpoint repair and drops the coordinate/party-geography datasets. Re-dump or banner it. |
| `README.md` | 2026-08-05 `5957c67` (+ uncommitted) | 6 | **major-rewrite** | Card-fraud section repaired by the fix pass; the mechanism sections (merchants `:250`, spending `:795-798`, LOC `:1022`, favourites `:714`) still describe three closed defects as current behaviour. |
| `docs/ram_derive_dont_store.md` | 2026-07-20 `d541d0e` (+ uncommitted) | 2 | **keep-with-edits** | Target arithmetic now present (`:96-118`); baseline table (`:15-23`) still tops out at 20k×730d and R2.4's "banked" ruling (`:32`) was made at 1/8th the target's fold cost. |
| `docs/debugging.md` | 2026-07-26 `137f581` (+ uncommitted) | 3 | **keep-with-edits** | Operator guide; card-fraud example fixed, but the soak knobs are undocumented (`:52`) and the `is_fraud` probe (`:186`) invites adaptation to a column that is always 0. |
| `docs/code_review_roadmap.md` | 2026-07-20 `61674c6` | 8 | **major-rewrite** | 14 stations, 230 lines, and the strings `card_fraud`, `TGN`, `GNN` appear nowhere. Routes a reviewer past the TGN deliverable entirely. |
| `docs/era_data_provenance.md` | 2026-07-26 `137f581` (+ uncommitted) | 2 | **keep-with-edits** | Calibration-anchor and coverage rules (`:21-45`, `:98-102`) are load-bearing and correct; the "UNREAD BY GENERATION" contract (`:16`) is flatly false. |
| `docs/h1_nominal_scale_wiring.md` | 2026-07-26 `137f581` | 2 | **keep-with-edits** | Mechanism sound; gate description superseded by H4 (`:160-165`), smoke figures dead (`:169-179`). |
| `docs/h2_persona_timeline.md` | 2026-07-26 `137f581` | 2 | **keep-with-edits** | "Owner verification pending" banner (`:3-12`) on settled work; the gate-design lesson at `:27` is the most reusable sentence in the arc. |
| `docs/h3_mortality_estate.md` | 2026-07-26 `137f581` | 2 | **keep-with-edits** | Same stale banner (`:3-8`) naming a merge script that does not exist; `:63-69` is load-bearing and must survive. |
| `docs/h4_macro_modulation.md` | 2026-07-26 `137f581` | 3 | **keep-with-edits** | Three of four headline gate readings superseded (`:15`, `:16`, `:17`); the EIP deferral's justification (`:131`) expires under a modern window. |

---

## 2. TabFormer / IBM inventory

**`docs/` and `README.md` are CLEAN.** `grep -rni 'tabformer\|IBM' README.md
docs/` returns zero hits on the current working tree — the fix pass removed
them all. Every reference below is outside those two locations and **none of it
is prose you can reword freely**.

### PROSE — reword freely (no gate, no consumer, no emitted value)

| path:line | current text | proposed replacement |
|---|---|---|
| `include/phantomledger/exporter/card_fraud/schema.hpp:5` | "a TabFormer-shaped, transaction-fraud-only corpus" | "a transaction-fraud-only corpus shaped for continuous-time temporal graph learning (TGN, arXiv:2006.10637)" |
| `include/phantomledger/exporter/card_fraud/schema.hpp:32-33` | "Interpreted as DEBIT-card transactions (CHOICE; TabFormer mixes credit and debit cards)." | "Interpreted as DEBIT-card transactions (CHOICE; mixing credit and debit in one card view is standard for transaction-fraud graphs)." |
| `include/phantomledger/exporter/card_fraud/schema.hpp:126` | "DEMO ONLY in TF_GNN_v3 and empty on real TabFormer; PhantomLedger populates ALL of it" | "DEMO ONLY in TF_GNN_v3; PhantomLedger populates ALL of it from its PII and access synthesis" |
| `include/phantomledger/exporter/card_fraud/schema.hpp:366` | "DEMO ONLY in TF_GNN_v3 (empty on TabFormer; excluded from the GNN)." | "DEMO ONLY in TF_GNN_v3 (excluded from the GNN)." |
| `src/exporter/card_fraud/export.cpp:166` | "exactly like TabFormer's Online rows (which carry no merchant geography)" | "an online merchant has no modelled physical location, so it gets no geography chain" |
| `tests/CMakeLists.txt:487-488` | "The aggregate rate carries IBM TabFormer's observed 0.11675% as a NAMED COMPARATOR" | "The aggregate rate is printed against a plausibility band; the external comparator was removed 2026-08 and the LEVEL is currently UNCALIBRATED." |
| `tests/test_schedule.cpp:107-108` | "intentionally not described as IBM's exact released-artifact interval" | Delete the sentence. The 10,592/348 assertions on `:110-115` are live gate constants — **do not touch them** (see §7). |
| `data/commerce/README.md:20-24` | "The IBM benchmark supports this hierarchy… released TabFormer corpus references 100,343 merchant identifiers" | Re-attribute to the underlying virtual-world generator paper (arXiv:1910.03033), already cited on `:22-23`, and drop the corpus reference. |

### VOCABULARY — the exporter EMITS these value sets

Removing the *name* is free; removing the *values* is a schema change that
moves `tests/golden_tables_card_fraud.md5` and any pushed TigerGraph load.

| path:line | what |
|---|---|
| `include/phantomledger/exporter/card_fraud/derive.hpp:145` | Block header `// TabFormer "Use Chip" value set` |
| `include/phantomledger/exporter/card_fraud/derive.hpp:178-180` | The emitted literals `"Online Transaction"` / `"Chip Transaction"` / `"Swipe Transaction"` |
| `include/phantomledger/exporter/card_fraud/derive.hpp:231` | Block header `// TabFormer "Errors?" value set`, and the seven emitted `error` strings below it |
| `tests/test_card_use_chip.cpp:210` | Assertion message "every derived value is in the TabFormer value set" — the *check* is real, the *name* is not |

**Disposition:** rename the two block headers to "entry-mode value set" and
"authorization-error value set" now; leave the strings alone. `docs/
fraud_model_audit.md:568` (already edited by the fix pass) states this rule
correctly and should be treated as the standing ruling. Note that
`docs/card_fraud_feature_contract.md:328` already rules `error` "treat as noise
or drop" — **dropping the column is the cheaper TabFormer exit than renaming
seven values**, and it is an owner decision (§6b).

### GATE-CONSTANT — a test's band or printout depends on the number

| path:line | what | does a band depend on it? |
|---|---|---|
| `tests/test_card_prevalence.cpp:54-55, :168, :170, :381-383` | `kTabFormerRate = 0.0011675`, printed as a ratio | **No.** Verified: the asserted band is `kRateFloor = 0.0002` / `kRateCeiling = 0.02` at `:173-174`, checked at `:384`. The constant is print-only, so removing it cannot turn a gate red. |

**Owner decision required anyway** (§6b): the prevalence LEVEL is now
**UNCALIBRATED**. `docs/fraud_model_audit.md:159` (fix pass) says so. The
measurement stands — the gate prints `CARD VIEW … rate 0.13050%` — but there is
no external anchor. A replacement must be an issuer-side rate **by NUMBER of
transactions**, not value-loss basis points.

### IDENTIFIER — lives in a database or an external consumer

| path:line | what |
|---|---|
| `tf_gnn_prep_ddl.sql:404` | `policy.policy_id = 'nvidia_tabformer_v1'` |

**Do not rename from this repository.** Traced to six live sites: the row is
`INSERT`ed as a primary key by `tf_gnn_loader_v2/sql/postgres/020_create_split_policy.sql:43`;
it is a join key in `030_create_train_seen_tables.sql:280,:300` and
`040_create_transaction_views.sql:150`; it is asserted in Python as
`_SPLIT_POLICY_ID` at `tf_gnn_loader_v2/src/tf_gnn_loader/postgres/verify.py:31`;
and it is recorded in `artifacts/tf_gnn_load/postgres_verification.json`. Here
it gates `transaction_manifest`, which every downstream view reads. Renaming it
in `tf_gnn_prep_ddl.sql` alone makes `transaction_manifest` return **zero rows
with no error** — the whole prep layer goes silently empty. The loader repo is
**not under version control**, so those edits have no revert path. Correct
order if the owner wants it gone: INSERT a new policy row alongside the old,
repoint all five consumers, verify a load, then DELETE the old row.

---

## 3. Legacy rules — the ones that actively mislead

Ranked. Blocking first.

### 3.1 BLOCKING — Seven gates are cited as enforcement and **none of them runs**

`tests/CMakeLists.txt` registers exactly **60** `pl_add_test` targets;
`ls tests/test_*.cpp` returns **67** files. The seven orphans are
`test_card_use_chip`, `test_card_endpoint_graph`, `test_card_payment_timing`,
`test_merchant_churn`, `test_relocation`, `test_card_churn`,
`test_session_point_in_time`. `grep -c` for each in `tests/CMakeLists.txt`
returns **0**, and `git log -S` shows none was ever registered.

Sites that cite them as live enforcement:

- `docs/card_fraud_feature_contract.md:9` — "CARE → FEATURE-SAFE because it
  became causal, **pinned by** `tests/test_card_use_chip.cpp`". The reclassification of
  `use_chip` to FEATURE-SAFE rests entirely on this.
- `docs/card_fraud_feature_contract.md:67` — same claim inline.
- `docs/card_fraud_feature_contract.md:116`, `:322`, `:73` — four separate
  citations of `test_card_endpoint_graph` (sub-gates G and H, the endpoint reuse
  figures, the not-on-file precision).
- `docs/card_fraud_online_gnn.md:52`, `:184` — `use_chip` marked Closed/DONE.
- `docs/fraud_model_audit.md:568`, `:341`, `:796`, `:875`, `:1178`, `:1391`.
- `docs/card_fraud_v2_roadmap.md:557`, `:758`, `:497`.

**Which round falsified it:** none — it was never true. This is
`attacker-infra-2026-07` lesson 1 ("a count of endpoints is not a measurement of
the graph") in a stronger form: the greens do not exist either. Stale binaries
sit in `build/tests/` from an unconfigured state, so running one by hand looks
green.

**Correction:** do NOT delete the citations — that converts a visible gap into
an invisible one. Register the seven targets in `tests/CMakeLists.txt` and run
them. Until then annotate every citation "test source exists, NOT REGISTERED".
**Expect at least one immediate red — see 3.2.**

### 3.2 ~~BLOCKING — sub-gate G is RED on the long leg~~ **RETRACTED. The RED claim was FALSE; the coverage half stands (MEDIUM).**

**This finding was wrong and is corrected here rather than deleted, per the
same rule that governs the rest of this document.** The original text claimed
`lift 1.356x` (leg-long) and `1.213x` (leg-wide) against the band
`ownedLift > 0.80 && ownedLift < 1.25` at
`tests/test_card_endpoint_graph.cpp:933`, and concluded leg-long FAILS.

**MEASURED at HEAD after registering the gate (2026-08-05), twice, identical
both times:**

```
G merchant register: 139 of 343 owned; 304177 rows, fraud rate 0.009909, lift 1.084x
G merchant register: 142 of 354 owned; 298373 rows, fraud rate 0.011040, lift 1.209x
exit=0 — the gate PASSES
```

**Both legs sit inside the band and the binary exits 0.** The string `1.356`
does not occur anywhere in the gate's output. The reported coverage figures
(139/343, 142/354) match exactly, so the same legs were run — the lift value
was misreported, not measured on a different configuration.

**What survives, and it is a real doc error:** realized coverage is
**40.5% / 40.1%**, not the "~45%" quoted at
`docs/card_fraud_feature_contract.md:73`. 45% is the DECLARED constant
`kBeneficialOwnerCoverage = 0.45`
(`include/phantomledger/entities/counterparties/merchant_ownership.hpp:132`),
not the realized rate. Fix `:73` to "declared coverage 0.45; realized ~40%".

**Worth watching, not fixing:** the leg-wide 1.209x sits within 3.3% of the
1.25 ceiling. That is thin margin, and `merchant-selection-2026-08` /
`venue-reuse-2026-08` both changed which merchants fraud reaches. The gate is
now registered, so a future drift past the ceiling will be caught — which it
would not have been before today.

**Method note for anyone reading this document:** finding 3.1 (the orphan
gates) and this finding came from the same verifier pass. One was correct and
consequential; one was a fabricated number inside a confident BLOCKING claim.
Re-run the measurement before acting on any numeric claim here.

### 3.3 BLOCKING — "`Has_Device`/`Has_IP` are header-only" and the anti-shortcut argument built on it

Four sites in the authority ledger and one in the feature contract still say the
endpoint ownership tables are empty, and one of them uses that emptiness as a
**safety argument**:

- `docs/fraud_model_audit.md:569` — "`Has_Device` / `Has_IP` are header-only
  loader-compatibility tables"
- `docs/fraud_model_audit.md:570` — "Static Party ownership is withheld for all
  endpoints because exogenous attackers have no truthful Party owner"
- `docs/fraud_model_audit.md:576` — "| Is_Merchant | UNPOPULATED (header-only):
  the world has no merchant-owning-party link |"
- `docs/fraud_model_audit.md:587` — "Static `Has_Device`/`Has_IP` tables are
  header-only, so Party adjacency cannot expose endpoint role."
- `docs/card_fraud_feature_contract.md:45` — lists "header-only
  `Has_Device`/`Has_IP`" in the Identical enforcement class, contradicting the
  same doc's `:322`, which moves both to FEATURE-SAFE.

**Which round falsified it:** `attacker-infra-2026-07` (Has_Device/Has_IP) and
`merchant-ownership-2026-07` (Is_Merchant).
`src/exporter/card_fraud/export.cpp:574` comments verbatim "Has_Device / Has_IP
ARE NO LONGER HEADER-ONLY", with writers at `:660` and `:702`; `:409-438`
populates `cf_Is_Merchant`.

**Mitigating, and it matters for how you fix this:** the audit *does* record the
inversion — `docs/fraud_model_audit.md:847` supersedes all three Has_* clauses
**by name**, and `:866` supersedes the Is_Merchant row by name. A reader of the
PART IV table 280 lines earlier never sees it.

**Correction:** rewrite the four rows in place with a forward pointer to `:847`
and `:866`. `:587`'s safety argument must be replaced with the measured residual
("endpoint not on file ⇒ fraud" precision 0.027 at 2.9x lift), not deleted — the
residual is intended and banded, not zero. Fix
`docs/card_fraud_feature_contract.md:45` in the same pass and add
`Has_Std_City`/`Has_Std_Postcode`/`Has_Std_State`, which
`tests/test_card_point_in_time.cpp:442` does place in the Identical class.

### 3.4 BLOCKING — the repair script tells the operator to skip the repair

`docs/tf_gnn_prep_session_endpoints.sql` is the live-DB fix that re-anchors the
Device/IP vertex set on session edges. Three of its statements are inverted:

- `:10` — "That table is header-only BY DESIGN
  (`exporter/card_fraud/schema.hpp:283`)". False, and the line anchor is also
  dead — `schema.hpp:283` is now party-geography prose.
- `:34` — "`audit_identity_fanout` should keep reporting 0 linked parties for
  device and ip, because that is the true state of the corpus."
- `:296` — instructs the operator that the acceptance script must still PASS
  "including the check that cf_Has_Device + cf_Has_IP carry 0 rows".
  `docs/card_fraud_postgres_acceptance.sql:600-608` now RAISEs an EXCEPTION when
  the smaller of the two is **empty** — a 0 is the failure.
- `docs/card_fraud_device_ip_investigate.sql:321` — gates applying the repair on
  `load_devices`/`load_ips` being **zero**. Since ownership is populated that
  count is never zero, so the stated trigger never fires.

**Which round falsified it:** `attacker-infra-2026-07`, whose rule 7 says both
acceptance scripts had their `Has_*` expectations inverted. This third script was
missed.

**Correction:** the repair is still needed and the reason has changed. With
registry coverage at `kDeviceCoverage = 0.72` / `kIpCoverage = 0.61`
(`include/phantomledger/entities/infra/enrollment.hpp:78-79`) the INNER JOIN no
longer zeroes the endpoint layer — it **silently under-loads** it, dropping every
attacker endpoint not on file, which is precisely the cross-victim signal. Change
the trigger from "is it 0?" to "is `load_devices` materially below
`count(DISTINCT device_id)` in `cf_Transaction_Uses_Device`?", and fix
`docs/card_fraud_device_ip_investigate.sql:170`, whose branch guard requires
`ownership = 0` while its own message says the table is POPULATED.

### 3.5 BLOCKING — the stale table count, in its last three homes

`kTableCount = 43` at
`include/phantomledger/exporter/card_fraud/schema.hpp:502`, asserted **exactly**
by `tests/test_table_golden.cpp:416`. Remaining wrong copies:

- `docs/fraud_model_audit.md:567` — "The current export contains **37 tables**"
  (contradicted by `:1421` in the same file, which says 43)
- `docs/fraud_model_audit.md:769` (37), `:1128` (40), `:1228` (**44** — a count
  the export has never had)
- `docs/card_fraud_v2_roadmap.md:176` — "ROUND 7's two event-time session edges
  move **the current contract** to 37"
- `docs/card_fraud_postgres_acceptance.sql:35` — header prose "39 tables",
  enumerating a composition that sums to 39, while `:96` and `:177` assert 43

**Which rounds falsified it:** `merchant-coordinates-2026-07` (37→40) and
`party-geography-2026-07` (40→43). `README.md:1091` is **already fixed** to 43.

**Correction:** `bls-citation-2026-07` rule 4 already ruled on this — "A LOWER
BOUND IS NOT A COUNT… prefer one constant over four copies". Date-stamp the
historical counts, and state the current one by reference to `kTableCount`
rather than adding a seventh copy. Correct composition: 34 original TF_GNN_v3
tables + 2 session edges + `Ground_Truth_Label` + the Email_Minhash pair +
`Merchant_Location` + three `Has_Std_*` = 43.

### 3.6 BLOCKING — the merchant-ID baseline is quoted as evidence of safety, and it is a floor-not-reached pass

`docs/card_fraud_feature_contract.md:343` cites "the merchant-ID-only classifier
at recall@precision≥0.90 < 0.25" as evidence no merchant shortcut exists.
`docs/fraud_model_audit.md:606` asserts "Best precision at any threshold is
**1.0000** (lift 295.65x)".

The verifier ran `./build/tests/test_card_baselines` at HEAD:
`pure-fraud-merchant share 0.0000`, `RECALL @ precision>=0.90 0.0000`,
`best precision (any threshold) 0.7500 (lift 221.73x)`, over
`37547 card rows (262 merchants)`, base rate `0.00338`.

**Which round falsified it:** `venue-reuse-2026-08` (best precision
1.0000 → 0.7500, "back under the 0.90 floor"). The mechanism sentence at
`docs/fraud_model_audit.md:606-608` is now false in **both** clauses: no
fraud-only merchant exists (0 of 262 touched), and precision never reaches the
floor, so the bound is held on both axes rather than by recall alone. The same
stale triple sits in `tests/test_card_baselines.cpp:71-77`, inside a comment
whose own closing line reads "DO NOT RESTATE A PRINTED NUMBER IN A COMMENT".

**Correction:** replace the pinned numbers with a pointer to the gate's printed
output. Do not add a third generation of pinned numbers. `merchant-selection-2026-08`
rule 5 already names this failure — "asserting a band a leg cannot reach is how
`test_card_baselines` came to score 0.0000 on merchant-shortcut recall, a pass
earned by having no data".

### 3.7 BLOCKING — "the era lock ends the window in 2020"

- `docs/fraud_model_audit.md:112` — "Crypto DECLINED — the era lock ends the
  window in 2020"
- `docs/card_fraud_victimization.md:281` — "Crypto stays DECLINED — the era lock
  ends the window in 2020."
- `docs/h4_macro_modulation.md:131` — the EIP deferral: "the canonical
  card-fraud window ends 2020-01-01, so no current probe config reaches the EIP
  dates"
- `tests/test_card_class_f.cpp:156-157` — "the era lock ends the corpus window
  at 2021-01-01"

`src/app/cli.cpp:184-195` locks `--usecase card-fraud` to
`[macroSeries().firstYear(), macroSeries().lastYear()]`, and
`include/phantomledger/synth/econ/era_data.hpp:101-136` covers **1990-2024**.

**Which round falsified it:** `macro-history-v1` (`137f581`, 2026-07-26), which
extended coverage to 2024. Three prohibitions now rest on an expired
justification: the crypto-rail decline, the EIP deferral, and the shortened
modern leg of `test_card_class_f`.

**Correction:** state the real bound (1990-2024) and re-decide each on its
merits. The EIP one is not academic — a `--start 2020-01-01 --days 1096` window
reaches all three payments while `realPceLevel` already carries the 2020 collapse
(0.9698) and 2021 rebound (1.0471) around them, i.e. an internally inconsistent
world rather than a neutral omission.

### 3.8 HIGH — the README describes three closed defects as current behaviour

- `README.md:714` — "Merchant favorite add/drop remains a structural TODO…the
  two merchant probabilities above must not be interpreted as realized monthly
  turnover." **False.** `math::evolution::evolveFavorites` is called at
  `src/activity/spending/dynamics/monthly/evolution.cpp:297`; `:217` in the same
  file records that it *was* dead code. Falsified by `merchant-churn-2026-07`.
  For a TGN this is the worst one on the page: it says the card→merchant graph is
  frozen for the whole window.
- `README.md:795` — "~8% of people get a 3-9 day high-spending burst at a random
  point in the window." That is the per-run coin `burst-rate-2026-07` closed;
  it is now `burstsPerYear = 0.487` scaled by `segmentSpan / 365.25`.
- `README.md:1022` — describes the LOC accrual tracker as sweeping "every
  enabled account". That is the O(population²) defect `loc-accrual-perf-2026-08`
  closed; the integral is now rolled forward lazily with a due-time min-heap
  (`include/phantomledger/transactions/clearing/loc_accrual.hpp:119-167`). The
  same sentence's "billing events are pre-generated (23:55 on each account's
  cycle day)" has no referent in the code and contradicts `README.md:556`, which
  correctly says billing is a rolling 30-day period.
- `README.md:795` — "favK ∈ [8,30] favorite merchants (weighted by global
  merchant CDF)" and `:798` — "82% of the time pick from favorites". Both
  falsified by `merchant-selection-2026-08`: membership is drawn from the REACH
  law and geography-gated, and `baseExploreP = 0.02`
  (`include/phantomledger/transfers/legit/routines/spending/behavior.hpp:42`)
  makes the favourites branch carry **~99.2%** of picks, not 82%.

### 3.9 HIGH — the era-data provenance contract says the series are unread by generation

`docs/era_data_provenance.md:16-19` — "**UNREAD BY GENERATION** until the named
macro-history H1+ model rounds." `docs/fraud_model_audit.md:462` says the
opposite in bold: "The series are READ by generation, so every refresh is now
MODEL-MOVING." 32 non-test files read `priceScale`/`wageScale`/`realPceLevel`.
The refresh procedure at `:173-176` puts the dead branch first, so an operator
reads "moves ZERO goldens" as the current rule.

**Which round falsified it:** the H1-H4 rounds landed in the same commit that
added this doc.

**Correction:** state unconditionally that a value refresh is MODEL-MOVING and
requires one re-pin of `golden_run.b2sum` plus the three table goldens.

### 3.10 HIGH — the gate-number cross-references resolve to the wrong gates

`docs/card_fraud_v2_roadmap.md:29` and `:50` cite "Gates 1, 3, 4 and 5 of
`docs/card_fraud_online_gnn.md`" and claim they "now have code behind them".
Against the current list at `docs/card_fraud_online_gnn.md:166-187`, gates 1, 3
and 5 are the **OPEN** ones. The renumbering (`## Minimum realism gates` 1-5 →
`## Remaining benchmark gates` 1-6) was never propagated to twelve citation
sites: `tests/test_card_baselines.cpp:5`, `tests/test_card_point_in_time.cpp:5`,
`include/phantomledger/exporter/card_fraud/derive.hpp:238`,
`src/transfers/fraud/typologies/unauthorized.cpp:33`,
`include/phantomledger/transfers/fraud/injector_inputs.hpp:74`,
`include/phantomledger/exporter/card_fraud/schema.hpp:73`,
`src/exporter/card_fraud/export.cpp:51`, `tests/test_table_golden.cpp:467`,
`docs/card_fraud_postgres_acceptance.sql:557`, `tests/CMakeLists.txt:398,425,443,463`.

**Correction:** prefer named anchors over numbers. Either restore a separately
anchored "Minimum realism gates" 1-5 section, or renumber once and update all
twelve sites atomically.

### 3.11 HIGH — dangling authority lineage

Every `U-N` cross-reference in the macro-history and victimization docs is
unresolvable. `harness-world-shape-2026-07` rebuilt `docs/fraud_model_audit.md`
as a citation-only record organised by `F-`/`L-`/`M-` codes. Grep counts in the
current audit: `U-4` 0, `U-5` 0, `U-7` 0, `U-8` 0, `U-9` 0, `U-11` 0, `U-12` 0
(the sole `U-6` hit is inside a SUPERSEDED-CLAIMS row). Dangling sites:
`docs/card_fraud_victimization.md:4`, `:271`, `:326`, `:351`, `:379`, `:381`,
`:394`; `docs/era_data_provenance.md:167`; `docs/h1_nominal_scale_wiring.md:4`,
`:14`, `:183`; `docs/h2_persona_timeline.md:3`, `:120`, `:173`, `:179`, `:182`;
`docs/h3_mortality_estate.md:4`, `:8`, `:66`, `:158`, `:159`, `:163`;
`docs/h4_macro_modulation.md:4`, `:47`, `:74`, `:156`, `:221`. One is baked into
code at `include/phantomledger/synth/econ/era_data.hpp:70` and one at
`include/phantomledger/transfers/fraud/typologies/unauthorized.hpp:88`.

**Correction:** add a one-line `U-N → M-N` mapping table at the top of each
affected doc, or re-point each citation. Do not delete a `U-N` label without
also fixing the two code sites.

### 3.12 HIGH — "Owner verification pending" banners on settled rounds

`docs/h3_mortality_estate.md:3-8` and `docs/h2_persona_timeline.md:3-12` open
with pending-action banners from 2026-07-25 instructing the owner to run
`merge_authority_*.py` scripts and "delete and recapture all four goldens".
`find . -name 'merge_authority*'` returns nothing; the goldens have been
recaptured repeatedly since. The suite counts in those banners (44) and across
the audit chain (56/56, 57/57, 62/62, 64/64, 65/65 at
`docs/fraud_model_audit.md:839`, `:877`, `:951`, `:1037`, `:1237`, `:1342`) all
exceed the 60 registered targets — the signature of counting source files rather
than targets, which is the same defect as 3.1.

### 3.13 HIGH — the golden row count, three re-pins out of date

`docs/fraud_model_audit.md:1399` is the last golden statement in the amendment
chain and reads "GOLDEN IMPACT: **NONE.** The corpus digest is unmoved at
`2afaf188…`/188,477". `tests/golden_run.b2sum` today reads
`07d4388c…  rows: 186144`. Same class:
`docs/card_fraud_v2_roadmap.md:180` ("the stream golden held at 197,199 rows"),
`docs/h4_macro_modulation.md:25-29` ("184,988 → 197,245 rows"),
`docs/h1_nominal_scale_wiring.md:169-179` (the "post-2b smoke reference
figures"), `docs/card_fraud_feature_contract.md:136` ("149 merchants across 48
distinct centroids" — the e2e gate now prints **96 across 28**, and asserts only
that more than one distinct point exists).

**Correction:** stop restating digests and row counts; point at
`tests/golden_run.b2sum` and at each gate's printed output.

### 3.14 HIGH — `docs/code_review_roadmap.md` does not route a reviewer to the TGN deliverable

The strings `card_fraud`, `TGN`, `GNN` and `TF_GNN` appear **nowhere** in its 230
lines. Station 12's read list (`:193-198`) names `standard/`, `mule_ml/`, `aml/`,
`aml_txn_edges/` and stops — `exporter/card_fraud/` and `exporter/econ/` are never
opened. Its gate roster (`:203-205`) names ten tests, none of them a card-fraud
gate; eight registered card-fraud gates appear nowhere in the document. The
doc's own method item 3 says "Read the station's tests LAST, as the spec", so an
unlisted gate is by its own rule an invisible property.

Compounding, and load-bearing: method item 4 defines "byte-neutral" — the licence
for fix-while-reading cleanup — against **three** golden baselines (`:21`), and
`:221` uses the brace form `golden_tables{,_aml}.md5`. There are **four**;
`tests/golden_tables_card_fraud.md5` is omitted from both. A cleanup validated
against the three named files can move the card-fraud golden and pass this doc's
stated test — and `merchant-coordinates-2026-07` and `merchant-ownership-2026-07`
were both rounds where `golden_run.b2sum` was UNMOVED and only
`golden_tables_card_fraud.md5` moved.

### 3.15 MEDIUM — the entry-mode asymmetry, restated correctly

*The auditors claimed a 60-70x `use_chip` shortcut. **REFUTED** — restated here
as the verifiers corrected it.*

The structural half is real: `src/transfers/fraud/typologies/unauthorized.cpp:39`
holds `kCardNotPresentShare = 0.70`, era-flat, while the legitimate side is dated
(`kCnpShareByYear`,
`include/phantomledger/activity/spending/market/commerce/local_pools.hpp:178-187`,
0.010 at 1991 → 0.362 at 2022, read at `market/bootstrap.cpp:328` and
`dynamics/monthly/evolution.cpp:92`). But the realized lift is **not** 60-70x.
Measured by running the (unregistered) gate at HEAD:

- 1991 leg (pop 300, 730d): `CNP share: fraud 0.6760 vs legit 0.1698` — **3.98x**
- 2019 leg (pop 300, 365d): `fraud 0.5294 vs legit 0.5802` — **0.91x, INVERTED**

The finder equated `cnpShareForYear` (a membership-pool input) with the realized
exported `use_chip` share; the per-row mode roll is still the flat
`kCardPresentShare = 0.89` at `src/activity/spending/routing/payments.cpp:42`.
A draw-free probe over the real sampler does show population sensitivity —
realized online membership at 1991 is 0.179 at pop 300 but **0.0154 at pop
100,000** — so at production scale an early-era window would sit near 15-45x.

**What to write instead:** the real defect is a **distribution shift**, not a
shortcut. The feature's sign flips between an early-era train fold (~4x) and a
modern test fold (~0.9x), and the doc has no disclosure of that.
`docs/card_fraud_feature_contract.md:67`'s clause "the legitimate CNP share is
era-flat (registered debt, payments.cpp)" is **half false** and must be split:
the membership pool is dated, only the ~0.8% explore-lane roll is flat.
`docs/card_fraud_online_gnn.md:182` has the same half-false clause. Register the
underlying gap separately: `kCardNotPresentShare` should be dated like the
legitimate series — that is `burst-rate-2026-07`'s error in another costume, on
the fraud side.

### 3.16 MEDIUM — `instrument == credit` is a deterministic non-fraud rule and the contract permits it uncaveated

**CONFIRMED by verifier, three ways.** Every fraud rail sources the victim's
primary deposit account (`src/transfers/fraud/injector.cpp:927`,
`include/phantomledger/transfers/fraud/rings.hpp:52-69`,
`unauthorized.cpp:602`); `derive::cardId` renders `'C'` for credit and `'D'` for
debit (`derive.hpp:120-133`); and it is a hard gate —
`tests/test_card_prevalence.cpp:487` asserts `unauthorizedCredit == 0`, measured
"unauthorized credit 0, debit 218". So the **first character of `card_number`**
is a near-perfect negative label for the unauthorized rail.

`docs/card_fraud_online_gnn.md:73` and `docs/card_fraud_feature_contract.md:87`
permit `instrument` as request context. The doc discloses the debit-only backing
three times elsewhere but never at the two places a modeller actually reads.

**Correction (verifier's, not the auditor's):** keep `instrument` permitted —
moving it to the forbidden list would route around a documented blocker instead
of measuring it — but append the caveat inline at the point of permission, and
require the instrument-only baseline (`:124`) to report the credit-side coverage
alongside its PR-AUC. **Do not touch `tests/test_card_prevalence.cpp:487`.**

### 3.17 MEDIUM — remaining stale mechanism claims, rolled up

| doc:line | quote | superseded by |
|---|---|---|
| `docs/card_fraud_online_gnn.md:150`, `:33` | card/device/residence lifecycles and merchant availability listed as open | `card-churn-2026-07`, `relocation-2026-07`, `merchant-churn-2026-07` |
| `docs/card_fraud_feature_contract.md:184`, `:186` | "`popularity * exp(-distanceMiles/scaleMiles)`… **Use it**" | `merchant-selection-2026-08` step 2 — membership is now `ReachModel` × decay + Zipf visit law; legit favourites moved 1,206 mi → 3.8 mi, so the axis is now weak and correctly signed |
| `docs/fraud_model_audit.md:226`, `:228` | top-1 reach `0.129`/`0.107`; ratio `3.65` vs `2.12-2.18` | step 2 of the same round: `tests/test_card_merchant_graph.cpp:610` prints 0.359/0.293; `:646-647` prints armed 2.633/2.740 vs disarmed 1.791/1.828, and `:654` says the old 2.90 floor "would have failed a correct build" |
| `docs/fraud_model_audit.md:1067`, `:1055` | hazards `0.158/0.1145/0.0540`; BLS "~84% at 1 year, ~58% at 5" | `bls-citation-2026-07` — recalibrated to `0.1720/0.1134/0.0494`, published values 82.8%/57.7%(4yr). The doc declares these wrong at `:1368-1371` and restates them unmarked 300 lines earlier |
| `docs/fraud_model_audit.md:1070`, `:893` | `GeographicMerchantPools`, `geo_pools.hpp` | both retired into `commerce/local_pools.hpp`; `find . -name geo_pools.hpp` returns nothing |
| `docs/fraud_model_audit.md:277` | `paidFraction = .65` | `include/phantomledger/activity/income/salary.hpp:47` is `0.74`; `:40-46` says ".65 sat at the SEED-persona mean" |
| `docs/fraud_model_audit.md:220` | "1,133 records ≈ 712 per 10,000 people" | arithmetic error: 1,133 at pop 8,000 is 1,416/10k; 712 is the base catalogue alone. CLAUDE.md carries the same slip |
| `docs/fraud_model_audit.md:225` | crossover "pop ≥ 20,875" | realized threshold is 20,792; `commerce/affinity.hpp:38` says ~20,833 |
| `docs/card_fraud_v2_roadmap.md:95`, `:566` | "`giftCardScam` is always card-present" | `unauthorized.cpp:461-463` draws a dated `digitalGiftCardShare`; zero before 2005, so it *looks* true on every 1991-start leg — a horizon-dependent falsehood |
| `docs/card_fraud_v2_roadmap.md:101` | kernel reuse closed the distance shortcut | it did not; the shortcut stayed open at 7.4x lift until `merchant-selection-2026-08` step 2 geography-gated *legitimate* membership |
| `docs/card_fraud_v2_roadmap.md:97`, `:520` | `cardPresentDecayScaleMiles`, `pickMerchantDestination` | now `commerce::decayScaleMilesFor` and `buildMerchantPool`/`pickFromPool`/`campaignVenue`. The dead symbol is also quoted at `derive.hpp:151` |
| `docs/card_fraud_v2_roadmap.md:135` | "precision never approaches 0.90" | the false safety argument `stale-claims-2026-08` corrected in `tests/test_card_baselines.cpp:64-78`; this is the last surviving copy |
| `docs/card_fraud_victimization.md:49` | "the IP is a random address" | `attacker-infra-2026-07`; the `randomIpv4` line survives only as a comment at `injector.cpp:799` |
| `docs/card_fraud_victimization.md:24`, `:45` | `injector.cpp:577`, `:348`, `:374` | now `:975-1000`, `:387`, `:420-430`; the exclusion list also omits `shellFraudAccounts` |
| `docs/h4_macro_modulation.md:15`, `:16`, `:17` | parity `0.9504`, drift `0.927`, ring-rail `2.012` | `harness-world-shape-2026-07` replaced the single-seed estimator (`tests/test_econ_wiring.cpp:116-123` says one seed cannot separate healthy from defective); current values `~0.9072` mean, `0.950` (after transiting **0.797** against a 0.80 floor), `1.9104` |
| `docs/h1_nominal_scale_wiring.md:160-165` | drift parity on deflated y/y | superseded by H4 `:150-157`; the code divides by `priceScale × realPceLevel` at `tests/test_econ_wiring.cpp:230` |
| `docs/era_data_provenance.md:132` | "H5's future adoption series are pre-registered as PLANNED" | `ecommerce_share` shipped **outside** H5 as `kCnpShareByYear`. **H5 never happened** — the string appears exactly twice in the repo, both in these docs, and `docs/h4_macro_modulation.md:21` defers the README sweep to it |
| `docs/code_review_roadmap.md:200` | "the four exporters" | five use-case exporters plus `exporter/econ/` |
| `docs/code_review_roadmap.md:120-126`, `:91-99`, `:101-112`, `:128-142`, `:160-171`, `:173-189` | station inventories | miss `clearing/loc_accrual.*` (and `test_loc_accrual`, whose sub-gate D CLAUDE.md forbids deleting), `infra/attackers.hpp`, `infra/enrollment.hpp`, `holdings/card_reissue.hpp`, `counterparties/merchant_ownership.hpp`, `parties/relocation.hpp`, `commerce/reach.hpp`, `commerce/affinity.hpp`, `fraud/exposure.hpp`, `fraud/susceptibility.hpp`, `pipeline/world_footprint.hpp` |
| `docs/debugging.md:163-165` | "its merge-script protocol" | `harness-world-shape-2026-07`; grep for `merge-script` in the audit returns nothing. The rule is now THE AUTHORITY RULE at `docs/fraud_model_audit.md:21` |
| `docs/card_fraud_device_ip_investigate.sql:48` | "~61% address coverage" | the constant is `kIpCoverage`. CLAUDE.md's open-questions block carries the same slip |
| `README.md:1005` | "avoiding invalid NANP ranges" | NANP is telephone numbering; `network::randomIpv4` avoids reserved IPv4 octet ranges |

---

## 4. What the 100k × 3y target changes

### 4.1 Does 100,000 people × 1,095 days fit? **Not in 32 GB. Probably not in 40.**

Anchors, both from CLAUDE.md `loc-accrual-perf-2026-08` and
`docs/ram_derive_dont_store.md:15-23`:

- **50,000 × 730 days = 45.0M rows, 13.2 GB peak RSS, 198 s.** (the newest and
  largest real measurement)
- 20,000 × 730 days = 4,494 MB run peak / 2,841 MB prologue (post-R2.4c.0)
- `docs/ram_derive_dont_store.md:37-38`: "Both parts scale with population ×
  days (rows), not with population alone." **That sentence is what licenses the
  arithmetic below and must not be edited away.**

Derived row rate: `45.0e6 / (50,000 × 730)` = **1.233 rows per person-day**,
cross-checked against CLAUDE.md's independent "500,000 × 730d is ~450M rows"
(1.233 exactly).

Target: `100,000 × 1,095` = **109.5M person-days → 135.0M rows**, i.e. **3.00x**
the 50k/730d anchor.

| method | rate | projection |
|---|---|---|
| bytes/row from the 50k anchor | 13.2 GB / 45.0M = **293 B/row** | **39.6 GB** |
| bytes/row from the 20k anchor | 4,494 MB / 18.0M = **250 B/row** (a floor — per-row cost already drifted 250→293 between 20k and 50k) | **33.7 GB** |
| this doc's own linear-in-(pop×days) method | prologue 2,841 MB × 7.5; fold 1,653 MB × 7.5 | **33.7 GB** |

**Verdict: 33.7-39.6 GB, central ~39.6 GB. It does not fit 32 GB under any of
the three methods.** At 293 B/row a 32 GB box tops out at ~109M rows =
**80,800 people at 1,095 days**, or **100,000 people at ~885 days (2.42 yr)**.

Assumptions stated: peak RSS linear in rows; row rate config-independent at
1.233 (the 60-day golden runs 1.55, so short windows are *denser* and this
understates); the population-only term is negligible (post-R2 spine ≈ 400 B/person
= 40 MB at 100k), which is exactly why linear-in-rows is the right model rather
than linear-in-population.

**Nothing at population ≥ 100,000 with a multi-year horizon has ever been run in
this repository.** The only ≥100k datapoint anywhere is a pre-R2 owner run at
200,000 (`docs/ram_derive_dont_store.md:19`, 14.6 GB) that predates every R2
delivery and is not comparable.

**What the owner must decide** (also §6b):

1. **Do R2.5 — prologue windowing.** `docs/ram_derive_dont_store.md:88-94`
   already names it as the prerequisite for exactly this class of run, and it is
   **not done**. R2.5a landed (`base_run_set.hpp:5`,
   `stages/transfers/windowed_run.cpp:211`, commit `137f581`) and bounds the
   replay-view staging only, not the prologue's residency; the doc does not
   record it at all.
2. **Cut the target** to ~80,000 people, or to ~885 days.
3. **Provision ≥ 48 GB.**

Measure before committing. The in-repo instrument is
`tests/test_scale_soak.cpp`, and `docs/debugging.md:52-53` never says how to
enable it: `PL_SOAK=1` (`SKIP_RETURN_CODE 77`), knobs `PL_SOAK_POP` /
`PL_SOAK_DAYS` / `PL_SOAK_SEED`, defaults 10,000 / 365 — **30x under target in
person-days**, and `tests/CMakeLists.txt:707-709` sets a 14,400 s timeout that
will likely bind.

**Also re-open `docs/ram_derive_dont_store.md:32`** — "no retention lever left in
the fold worth a round; **R2.4 is banked**." That ruling was made when the fold's
share of peak was 1,653 MB at 20k×730d. At the target that share is **~12.4 GB**.
"Not worth a round" at 1.6 GB is not the same ruling at 12.4 GB, and
`loc-accrual-perf-2026-08` changed what the fold does per row since the probe was
taken.

### 4.2 A correct temporal split for a 3-year window

**ALREADY FIXED IN WORKING TREE.** `docs/card_fraud_online_gnn.md:106-132` now
carries a window-relative split (train ~67% / validation ~16% / test ~17%) with a
worked instance for `--start 2022-01-01 --days 1096`.

Two things that pass must still record:

- **The split does not live in this repo.** It is a row in
  `tf_gnn_prep.split_policy` read by `transaction_manifest`
  (`tf_gnn_prep_ddl.sql:373`), set by
  `tf_gnn_loader_v2/sql/postgres/020_create_split_policy.sql:43-54` to
  `train_end_epoch = 1514764800` (2018-01-01) and
  `validation_end_epoch = 1546300800` (2019-01-01). **Any 3-year window not
  containing both those instants puts 100% of rows into a single split**, and
  `audit_failures` (`tf_gnn_prep_ddl.sql:1043-1154`) has **no check that any
  split is non-empty** — a TGN would train and evaluate on the same partition
  with nothing failing.
- **Which 3-year windows are legal at all.** `src/app/cli.cpp:183-195` hard-exits
  card-fraud runs outside 1990-2024. `--start 2017-01-01 +1095d`,
  `--start 2021-01-01 +1096d` and `--start 2022-01-01 +1096d` all pass;
  `--start 2023-01-01 +1096d` fails; the CLI default `--start 2025-01-01`
  (`include/phantomledger/app/options.hpp:83`) is illegal for card-fraud **by
  design** (tripwire pinned at `tests/test_app_options.cpp:66-78`), and
  `README.md:149`'s options table does not say so.

**Action:** add an `empty_split` branch to `audit_failures` asserting all three
of `split_id` 0/1/2 are non-empty, and re-derive the split epochs from the actual
corpus window. Both are live-row / loader-repo changes, not code changes here.

### 4.3 Era machinery a 3-year modern window stops exercising

- **Cross-era gates cannot run inside one 3-year corpus.** `test_card_class_f`
  (1991 vs 2019, `tests/test_card_class_f.cpp:378-380`) and `test_card_use_chip`
  (1991 and 2019 legs) are explicitly two-era comparisons.
- **Recession machinery goes inert.** `recessionMonths = 0` for every year
  2021-2024 (`era_data.hpp:131-136`), so `docs/era_data_provenance.md:63-66`'s
  request for a monthly unemployment series is moot at this horizon — and H4
  already ruled against it (`docs/h4_macro_modulation.md:116-124`).
- **Card reissue nearly vanishes.** Measured by draw-free probe over
  `entity::card::reissue::generationsFor` across 20,000 keys: a 1,096-day window
  from 2022-01-01 gives **mean 1.248 generations/card, 78.8% single-generation**,
  against 6.419 over 20 years. A window overlapping 2015-2017 rises to 1.750
  because the EMV wave fires. So `docs/card_fraud_feature_contract.md:91-121`'s
  card-cardinality guidance is correct but governs a minority case.
- **Relocation thins.** 0.1047 moves/person-year → ~0.31 moves/person over 3
  years, so ~73% of parties hold exactly one tenure row and the emphatic
  one-row-per-tenure warning at `docs/card_fraud_feature_contract.md:163-169`
  governs a minority join.
- **Merchant decay shrinks from 23% to ~3.5%.** The birth-sizing defect
  (`include/phantomledger/synth/merchants/lifecycle.hpp:262` uses
  `hazardMature = 0.0494` to size births for a cohort walking the birth bands
  0.1720/0.1134, undersized ~1.45x) is real at 20 years and near-invisible at 3.
  `docs/fraud_model_audit.md:1069` still records this construction as CONFORMS.
- **CPI drift is not negligible, and the docs imply it is.** A 2021→2023 window
  carries **12.5%** price drift because it straddles the largest annual CPI move
  in the series. Meanwhile every era illustration in `h1`-`h4` and
  `era_data_provenance` is a 29-year span ratio (2.87x PCE, 1.88x CPI) that a
  3-year corpus never traverses, and `docs/h4_macro_modulation.md:216-222`
  documents only the *below-calibration* direction. A modern window runs
  **above** calibration on every axis: `realPceLevel` 1.047/1.058/1.073 and
  `priceScale` 1.060/1.145/1.192 for 2021/2022/2023. A reader sizing budgets off
  these docs will size them the wrong way.
- **The join cohort thins ~2.5x.** BEA growth is ~1.34%/yr in 1991-92 against
  0.20-0.82%/yr in 2021-2023, so a 100k×3y modern window mints ~1,600 joiners
  where a 1991-start window of the same length mints ~4,000. Short-tenure
  accounts are disproportionately important to a temporal model.
- **Do NOT drop the 1991 gate legs.** `docs/h3_mortality_estate.md:63-69` records
  a real defect (production subscriptions never CPI-scaled) that survived because
  a gate leg sat at the calibration year where `priceScale == 1.0`. A modern leg
  sits within 4-23% of unity; the 1991 leg is at 0.533. It is the only contrast in
  the suite that separates an unscaled amount from a correctly-scaled one. The
  gap is the **opposite**: `docs/h2_persona_timeline.md:27` records that *two*
  legs on the same side of the 2025 payroll anchor cancelled in a ratio and hid a
  half-income defect — and today **every** H1-H4 corpus gate runs at 1991
  (`test_econ_wiring` 1991+2019; `test_persona_wiring`, `test_membership`,
  `test_estates`, `test_lifespan` 1991 only). **Add a modern/frozen leg; keep the
  ancient one.**
- **Attacker fan-out will be denser, not sparser.** Re-run at HEAD: leg-long
  (900×1461d) `shared 0.7380, mean 5.102, max 35`; leg-wide (1800×731d)
  `shared 0.8198, mean 8.712, max 39` — so
  `docs/card_fraud_feature_contract.md:256`'s "max 37-40" is already stale at its
  own legs. More important, at both legs the **concurrency floor** sizes the
  attacker pool (80 and 44 operators against a population term of 10.8,
  `src/synth/infra/attackers.cpp:24-66`). At pop 100,000 the population term
  binds at 900 while the floor is only 61.8, so cases per operator rise ~4-8x and
  per-endpoint degree rises with them. Any re-quote must record **which sizing
  term bound**.
- **Every anti-shortcut band in the repo was measured at pop 300-10,000.**
  `merchant-selection-2026-08` rule 8 already established that reach
  concentration is emergent from merchants-per-area and cannot be bounded at gate
  legs: measured top-1 reach 0.359 (300) / 0.293 (2,000) / 0.163 (8,000) / 0.082
  (500,000). At 100,000 people `coreFloor = 250`
  (`include/phantomledger/synth/merchants/make.hpp:21`) stops binding — it binds
  below 20,792 — so **every gate leg in the repo runs a merchant regime the
  target corpus does not occupy**, and no doc says so.
- **Session-edge probes will be ~3.5x their documented size.**
  `docs/card_fraud_device_ip_investigate.sql:24,:27,:288` and
  `docs/tf_gnn_prep_session_endpoints.sql:102,:244` size their runtime warnings
  at "~20M rows" against the retired 6,000×7,305d corpus. Section 6's unindexed
  self-join grows superlinearly, and indexes cannot be added
  (`src/exporter/sinks/table_mirror.cpp:105` DROPs tables each run).
- **The acceptance gate's defaults hard-abort the target.**
  `docs/card_fraud_postgres_acceptance.sql:7,:11` default to
  `expected_population 6000` / `expected_days 7305`, and `:235` RAISEs on
  population mismatch.

---

## 5. What a TGN implementer still cannot find in these docs

Gaps, not errors. Each is concrete and currently unanswerable from `docs/`.

1. **No node/edge type inventory.** Nothing states which of the 43 `cf_` tables
   are vertices and which are edges, or which vertex types a TGN should carry
   memory for. `docs/card_fraud_feature_contract.md:11` claims to answer one
   question "for every column PhantomLedger exports" and gives **no verdict at
   all** for `Has_Address`, `Has_Phone`, `Has_ID`, `Has_DOB`, `Has_Full_Name`,
   and no column-level ruling for `Card_Send_Transaction` or
   `Merchant_Receive_Transaction`.

2. **No `cf_` → `(src, dst, t, msg)` recipe.** The answer is
   `cf_Payment_Transaction` joined to `cf_Card_Send_Transaction` +
   `cf_Merchant_Receive_Transaction` on `txn_id`, keyed on `edge_unix_time` —
   which `include/phantomledger/exporter/card_fraud/schema.hpp:188-189` describes
   as "edge_unix_time drives the GNN temporal sampler" and **no doc repeats**.

3. **No statement of which column is the event clock.** `unix_time` vs
   `transaction_time` vs `edge_unix_time` are never disambiguated anywhere in
   `docs/`.

4. **No memory-update semantics or batching guidance.**
   `docs/card_fraud_online_gnn.md:61` gives the right ordering discipline
   (construct features from `< t`, then append the event) and stops. TGN's
   raw-message-store / memory-staleness trap sits precisely on the
   step-2-vs-step-4 boundary and is not mentioned.

5. **No negative sampling, and an unreconciled task mismatch.** The docs specify
   transaction-level binary classification; the downstream schema assumes
   **self-supervised link prediction**
   (`tf_gnn_loader_v2/gsql/schema_tf_gnn.gsql:346`). Nothing anywhere
   reconciles the two, and no doc says how negatives are drawn.

6. **No inductive-node handling.** `docs/card_fraud_online_gnn.md:122-123`
   requires an inductive holdout and never says which entity types actually churn
   inside a 3-year window. From this audit: merchants do (birth-band hazard
   0.1720/0.1134), cards barely (mean 1.248 generations), parties barely (~0.31
   moves), and attacker endpoints do continuously.

7. **Card node identity is a trap and no doc warns about it.** The exported
   `card_number` **changes at each reissue** — `streaming.hpp:261-263` resolves
   the generation live at the row's timestamp and `derive::cardId` appends
   `-G<n>` for n > 0. A TGN keying card nodes on `card_number` splits one account
   into up to ~6.7 disconnected nodes over a long window and loses memory state
   at each boundary. The stable id is the account key / `cf_Party_Has_Card`.
   `-G<n>` is also a **suffix**, so it is not obviously covered by
   `docs/card_fraud_online_gnn.md:86`'s ban on "identifier prefixes".

8. **What actually reaches TigerGraph is undocumented, and it is less than the
   docs imply.** `grep -rn 'Transaction_Uses_Device\|Transaction_Uses_IP\|
   Ground_Truth_Label'` across `tf_gnn_loader_v2` returns **zero hits**, and
   `gsql/schema_tf_gnn.gsql` declares no transaction→endpoint edge type at all.
   So the two timestamped session edges — the only continuous-time
   transaction→entity evidence in the corpus — are **Postgres-only**, and labels
   reach the graph only via `cf_Payment_Transaction.is_fraud` (loaded at
   `loading_jobs.gsql:365,:395`), never via `cf_Ground_Truth_Label`. CLAUDE.md
   records this under "Still open downstream"; the TGN contract does not.
   `tf_gnn_prep_ddl.sql` has no view of any kind over either table
   (`audit_load_counts` at `:1749-1864` enumerates 29 datasets, none of them a
   session edge), and `load_devices`/`load_ips` (`:1284`, `:1361`) emit a
   **constant 0** for `first_seen_unix_time` — every endpoint claims to have
   existed since the epoch.

9. **Geography features are unruled.** Five exported tables
   (`Merchant_Location`, three `Has_Std_*`) get no verdict in the feature
   contract's permitted/forbidden lists. Distance **is** now safe — step 2 moved
   legit favourites to mean 3.8 mi with P(within 50 mi) 97.3%, closing a 7.4x
   wrong-signed shortcut — but a reader cannot tell that from the doc, nor that
   both endpoints are **area centroids** (intra-area distance is exactly zero,
   coresidents share a home area) and that row **presence** in
   `cf_Merchant_Location` is the `has_coordinates` mask.

10. **Merchant-side graph signal is over-promised.**
    `docs/card_fraud_online_gnn.md:42` marks fraud-only merchant identity
    "Closed", which silently reads as a claim about merchant structure. CLAUDE.md
    `venue-reuse-2026-08` records that true common-point-of-purchase is
    **structurally unreachable** in this generator, that the cross-victim
    **excess** is ~0 (armed 1.615x vs disarmed 1.364x, distributions overlapping,
    one seed inverted), that campaigns and merchants intersect on `Rail::card`
    alone (~39% of unauthorized cases), and that what ships is an **online
    cash-out analogue** that "must never be described as physical POS-breach
    CPP". None of that appears in the TGN contract.

11. **`docs/code_review_roadmap.md` has no path to any of this** (see 3.14).

---

## 6. Recommended edit plan

### (a) Safe now — pure prose, no gate and no consumer depends on it

1. Finish the TabFormer sweep in code comments: the eight PROSE sites in §2.
2. Fix the table count at `docs/fraud_model_audit.md:567,:769,:1128,:1228`,
   `docs/card_fraud_v2_roadmap.md:176` and
   `docs/card_fraud_postgres_acceptance.sql:35` — by reference to `kTableCount`,
   not by adding a copy (3.5).
3. Rewrite the four `Has_*`/`Is_Merchant` header-only rows with forward pointers
   to `docs/fraud_model_audit.md:847` and `:866`, and fix
   `docs/card_fraud_feature_contract.md:45` (3.3).
4. Correct the three era-lock statements to 1990-2024 (3.7).
5. Fix `docs/era_data_provenance.md:16-19` and `:173-176` — the series ARE read
   by generation (3.9).
6. Flip the two "Owner verification pending" banners to CLOSED and delete the
   three `merge_authority_*.py` instructions (3.12).
7. Delete or date-stamp every restated golden digest, row count and smoke figure
   (3.13). Replace with pointers to `tests/golden_run.b2sum` and to each gate's
   printed output.
8. Fix the dead symbols and moved line numbers in 3.17 — prefer symbol names
   over line numbers, which have now gone stale twice.
9. Add the `U-N → M-N` mapping tables (3.11).
10. Add `golden_tables_card_fraud.md5` to `docs/code_review_roadmap.md:21` and
    `:221`, and add the card-fraud read list and its eight gates to station 12
    (3.14).
11. Document `PL_SOAK=1` and its knobs in `docs/debugging.md:52`, and add the
    `is_fraud`-is-only-meaningful-on-`cf_Payment_Transaction` note at `:186`.
12. Retitle `docs/card_fraud_v2_roadmap.md` as an archive — **after** lifting
    rulings 2 and 6 (`:33`, `:39`) and the parity trap (`:608`) to a live page.
13. Write the six §5 gaps that need no decision (node/edge inventory, the join
    recipe, the clock column, memory-update ordering, the card-node-identity
    warning, the geography paragraph) into
    `docs/card_fraud_online_gnn.md`.

### (b) Needs an owner decision

| # | decision | options |
|---|---|---|
| B1 | **Does 100k × 1,095d ship?** It projects to 33.7-39.6 GB and does not fit 32 GB (§4.1). | (i) do R2.5 prologue windowing first; (ii) cut to ~80,000 people or ~885 days; (iii) provision ≥48 GB. Measure with `PL_SOAK` at 50k×1096d and confirm the slope before committing. |
| B2 | **Which 3-year window?** | 2022-01-01 +1096d (modern, ends at the 2024-12-31 era edge with zero headroom); 2021-01-01 +1096d (crosses the largest CPI jump in the series); 2017-01-01 +1095d (pre-COVID, but entry-mode lift ~4x rather than ~1x). Each changes which era machinery fires and which gate legs remain representative (§4.3). |
| B3 | **Replacement prevalence anchor.** The level is currently UNCALIBRATED (§2, GATE-CONSTANT). | Name an issuer-side rate **by NUMBER of transactions**, or declare the level explicitly UNANCHORED. Do not tune toward a new anchor without the fraud-budget procedure — that is a `targetEvents`/rail-mix change with golden re-pins. |
| B4 | **`error` column: rename the vocabulary, or drop the column?** | The feature contract already rules it "treat as noise or drop" (`:328`). Dropping is the cheaper TabFormer exit than renaming seven emitted values, but it is a schema change. |
| B5 | **`nvidia_tabformer_v1` split-policy id.** | Leave it (zero cost, one surviving token in a live DB) or run the six-site lockstep rename in the loader repo first (§2, IDENTIFIER). The loader repo has no revert path. |
| B6 | **Register the seven orphan tests?** Expect sub-gate G to go red immediately (3.1, 3.2). | Register and fix; or register and re-derive the band with a disarm; or leave unregistered and annotate every citation. Leaving them silently orphaned is not an option. |
| B7 | **Re-open the EIP and crypto-rail deferrals** now that their "window ends 2020" justification has expired (3.7). | Declare 2020-2021-crossing windows out of scope, or promote the class-S EIP table. |
| B8 | **Retire `docs/card_fraud_v2_roadmap.md` and `tf_gnn_prep_ddl.sql`?** | Archive the roadmap (recommended). Re-dump `tf_gnn_prep_ddl.sql` from a repaired live DB, or banner it as a stale snapshot. |

### (c) Needs a code change in lockstep

| # | change | file | gate |
|---|---|---|---|
| C1 | Register the seven orphan targets | `tests/CMakeLists.txt` | all seven; expect `test_card_endpoint_graph` sub-gate G red at 1.356x |
| C2 | Split-policy epochs re-derived from the window; add an `empty_split` check | `tf_gnn_loader_v2/sql/postgres/020_create_split_policy.sql`, `tf_gnn_prep_ddl.sql:1043-1154` | `audit_failures`, `verify.py` |
| C3 | Add `cf_Is_Merchant` and `cf_Merchant_Location` non-emptiness to the acceptance gate — it covers 2 of the loader's **7** empty-table aborts (`tf_gnn_loader_v2/sql/postgres/001_validate_sources.sql:1439,:1444,:1449,:1458,:1465,:1471,:1478`) | `docs/card_fraud_postgres_acceptance.sql:600-608` | the acceptance run itself |
| C4 | Re-target the acceptance defaults from 6,000 × 7,305d | `docs/card_fraud_postgres_acceptance.sql:7,:11,:5-31` | `:235` population equality |
| C5 | Add `Email_Minhash` / `Has_Email_Minhash` to an enforcement class — they are the only 2 of 43 tables absent from all four lists, violating the contract's own closing rule at `:350` | `tests/test_card_point_in_time.cpp` | `test_card_point_in_time` |
| C6 | Add a **modern/frozen era leg** to `test_econ_wiring`; keep the 1991 leg (§4.3) | `tests/test_econ_wiring.cpp:240-241` | `test_econ_wiring` |
| C7 | Add a 3-year leg to the merchant churn gate; its legs are 15y and 5y and the horizon is missing, not the band | `tests/test_merchant_churn.cpp` | `test_merchant_churn` (unregistered — C1 first) |
| C8 | Repair the dead branch and the never-firing trigger in the endpoint census | `docs/card_fraud_device_ip_investigate.sql:160-186`, `:321` | none — it is a diagnostic |
| C9 | R2.5 prologue windowing, if B1 chooses it | `pipeline/stages/transfers/` | `test_arch_equivalence`, `test_scale_soak` |
| C10 | Date `kCardNotPresentShare` against the same Fed series as the legitimate side (3.15) | `src/transfers/fraud/typologies/unauthorized.cpp:39` | `test_card_use_chip` (unregistered — C1 first) |

---

## 7. Do not touch

Load-bearing text that a cleanup pass would reasonably delete, and what breaks.

1. **`docs/card_fraud_v2_roadmap.md:39` — "Zeroed-not-dropped for the leaking
   label columns: TF_GNN_v3 loading jobs map POSITIONALLY."** Live:
   `kLabelWithheld = 0` at `src/exporter/card_fraud/export.cpp:58` with writers at
   `:141`, `:395`, `:642`, `:702`; columns declared at
   `include/phantomledger/exporter/card_fraud/schema.hpp:139,:155`. **Dropping
   these always-0 columns shifts every downstream attribute by one in a live
   TigerGraph load.** Same for the `cf_Ground_Truth_Label` quarantine ruling at
   `docs/card_fraud_feature_contract.md:319-320` and roadmap ruling 2 at `:33`.
   If the roadmap is archived, lift these first.

2. **`docs/card_fraud_victimization.md` section anchors F1 (`:22`), F2 (`:40`),
   D1 (`:178`), D2 (`:197`), D3 (`:219`).** Thirteen code sites cite them by
   name: `exposure.hpp:5,:49`, `susceptibility.hpp:5`,
   `injector_inputs.hpp:117,:141`, `typologies/unauthorized.hpp:71`,
   `taxonomies/fraud/types.hpp:55`, `injector.cpp:398,:402,:975`,
   `test_fraud_low_population.cpp:5`, `test_card_class_f.cpp:147`,
   `test_card_victim_baselines.cpp:5`, `tests/CMakeLists.txt:535,:565`. Correct
   content in place; never renumber without moving all thirteen in the same
   commit.

3. **`docs/card_fraud_feature_contract.md:87` — the debit-backed-positives
   warning.** The single most important leak warning in the doc, guarded by a
   registered gate (`tests/test_card_prevalence.cpp:483-489`). Strengthen it if
   anything (3.16); do not trim it while cleaning the stale text around it.

4. **`docs/h3_mortality_estate.md:63-69` — "test_econ_wiring's calibration gate
   pins 2019 rows, where scale == 1.0 hides the difference."** The measured
   counter-evidence to any proposal to retire the 1991 gate legs (§4.3). Promote
   it; do not delete it as historical.

5. **`docs/h2_persona_timeline.md:27-29` — "both of their legs (1991, 2019) sat
   before the anchor and were equally starved, so every RATIO held."** The
   companion lesson: two legs on the same side of an anchor cancel. This is the
   argument for **adding** a modern leg. Keep verbatim even though the two smoke
   figures beside it (`:24-25`) are dead numbers.

6. **`docs/ram_derive_dont_store.md:37-38` — "Both parts scale with population ×
   days (rows), not with population alone."** The premise the entire §4.1
   arithmetic rests on, and the reason R2.5 (rows) rather than R3 (population) is
   the right lever for this target. Promote it to the opening of the sizing
   section.

7. **`docs/era_data_provenance.md:21-45` (`kCalibrationYear = 2019`) and
   `:98-102` (coverage `first ≤ 1990 && last ≥ 2020`).** Both enforced in code —
   `era_data.hpp:153`, `src/synth/econ/catalog.cpp:60-69`, `src/app/cli.cpp:184-196`.
   `:21-45` includes the rejected alternatives, which is what stops someone
   "re-anchoring to 2024 since the corpus is modern now" and silently
   re-denominating every calibrated constant. Strip only the canonical-window
   clause at `:28-29`.

8. **`docs/card_fraud_v2_roadmap.md:25-28` — the GOVERNING DIRECTIVE.** "No
   exported feature labels the corpus without behavior, plus a stated contract
   for which columns a model may read." It survives the TGN reframe intact and is
   strengthened by it. Move it to the live page when the roadmap is archived.

9. **`docs/card_fraud_v2_roadmap.md:608` — the four-call-site parity trap
   (`FraudEmission::legitCounterparties`), and `docs/tf_gnn_prep_session_endpoints.sql:42-44`
   — "do not add indexes"** (`src/exporter/sinks/table_mirror.cpp:105` DROPs
   tables every run). Both rules are still correct; only the roadmap's line
   numbers have drifted (`simulate.cpp:102`, `windowed_run.cpp:295`,
   `window_leg_support.hpp:416`, `test_membership.cpp:650`).

10. **`tests/test_schedule.cpp:110-115`** (`10592` / `348` assertions) and
    **`tests/test_card_prevalence.cpp:487`** (`unauthorizedCredit == 0`) and
    **`tests/test_loc_accrual.cpp:365-377`** (sub-gate D, the COST gate CLAUDE.md
    forbids deleting as a flaky timing test). All three are live constants sitting
    next to text this cleanup touches.

11. **The always-0 label columns, `cf_Ground_Truth_Label`,
    `cf_Transaction_Uses_Device`/`_IP`, `cf_Has_Device`/`_IP`, `cf_Is_Merchant`,
    `cf_Merchant_Location`, `cf_Has_Std_*` — do not withhold or drop any of
    them**, however unreachable they look from inside this repository.
    `merchant-ownership-2026-07` was found by a downstream **hard abort**, not by
    a gate: `tf_gnn_loader_v2/sql/postgres/001_validate_sources.sql` refuses the
    entire push on seven distinct empty-table conditions. From inside this repo an
    absent table and an asserted-empty table look identical — grep the consumer
    for **both** before adding or withholding anything.
