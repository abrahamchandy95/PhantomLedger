# Docs audit: TGN reorientation, 2026-08

Dated snapshot. Line numbers cite the audited tree and have drifted;
`docs/card_fraud_v2_roadmap.md` has since been retired. Bare doc names below
(`fraud_model_audit.md:569`) are under `docs/`.

## Scope

- Ten auditors read `docs/` (16 files), `README.md` and four executable SQL
  artifacts, each against an adversarial verifier. 262 findings; 13 refuted
  (dropped, or restated as the verifier corrected them, and marked). The other
  249 dedupe to 84 issues: 21 blocking, 27 high, 26 medium, 10 low. Nine
  load-bearing items that look deletable are under
  [Do not touch](#do-not-touch), not counted.
- An earlier uncommitted pass had edited seven files (`README.md`,
  `card_fraud_online_gnn.md`, `card_fraud_v2_roadmap.md`, `debugging.md`,
  `era_data_provenance.md`, `fraud_model_audit.md`, `ram_derive_dont_store.md`;
  115 insertions, 40 deletions). It removed every TabFormer/IBM token from
  `docs/` and `README.md`, fixed the README table count 37 → 43
  (`README.md:1091`) and its `Is_Merchant` and `Has_Device`/`Has_IP` claims,
  replaced the calendar-date split (`card_fraud_online_gnn.md:106-132`), and
  added the 100k × 3y memory arithmetic (`ram_derive_dont_store.md:96-118`).
  Line numbers read that tree; findings it fixed are marked and not counted.

## Verdict per document

`*` = plus uncommitted edits. Dates are 2026.

| doc | last touched | rounds behind | verdict | why |
|---|---|---|---|---|
| `card_fraud_online_gnn.md` | 08-05 `9a3017a`* | 4 → 2 | major rewrite | Primary TGN doc; cites gates that never run; omits what reaches TigerGraph |
| `card_fraud_feature_contract.md` | 08-05 `9a3017a` | 4 | major rewrite | Most valuable for TGN features, untouched by the fix pass; two central rulings falsified; status stops 07-27 (`:3`) |
| `fraud_model_audit.md` | 08-05 `f8b6c6c`* | 4 (Part I), 5 (Part II) | major rewrite | Part IV (`:565-610`) wrong on loader contract, table count, anti-shortcut levels; amendments stop at `:1346` (07-30) |
| `card_fraud_v2_roadmap.md` | 08-05 `9a3017a`* | 8 | retire or archive | Rounds 1-8 narrative; live-sounding status (`:3`, 07-27) competes with CLAUDE.md. Lift rulings 2, 6 (`:33`, `:39`) and the parity trap (`:608`) first |
| `card_fraud_victimization.md` | 08-05 `9a3017a` | 4 | keep with edits | Anchors F1/F2/D1/D2/D3 bind 13 code sites; status, code pointers, era argument (`:281`) stale |
| `card_fraud_postgres_acceptance.sql` | 08-05 `9a3017a` | 1 | keep with edits | Header 39 tables (`:35`), asserts 43 (`:96`, `:177`); covers 2 of the loader's 7 empty-table aborts |
| `card_fraud_device_ip_investigate.sql` | 08-05 `9a3017a` | 1 | keep with edits | Dead `load_devices` branch whose guard contradicts its message (`:170`); trigger (`:321`) never fires |
| `tf_gnn_prep_session_endpoints.sql` | 08-05 `9a3017a` | 1 | major rewrite | Repair needed, but `attacker-infra-2026-07` inverted its justification (`:10-22`, `:34`, `:296`): it says to skip it |
| `tf_gnn_prep_ddl.sql` (repo root) | 08-05 `9a3017a` | 3 | retire or archive | `pg_dump` behind the live schema and the loader; restoring it reverts the session-endpoint repair and drops the coordinate and party-geography datasets. Re-dump or banner it |
| `README.md` | 08-05 `74ac110`* | 6 | major rewrite | Merchants `:250`, spending `:795-798`, LOC `:1022`, favourites `:714` describe closed defects |
| `ram_derive_dont_store.md` | 07-20 `3b51af7`* | 2 | keep with edits | Baselines (`:15-23`) stop at 20k × 730d; "banked" R2.4 (`:32`) judged at 1/8 the target's fold cost |
| `debugging.md` | 07-26 `9c8df5e`* | 3 | keep with edits | Soak knobs missing (`:52`); `is_fraud` probe (`:186`) invites use on always-0 columns |
| `code_review_roadmap.md` | 07-20 `9c63d89` | 8 | major rewrite | 14 stations, 230 lines, no `card_fraud`, `TGN` or `GNN` |
| `era_data_provenance.md` | 07-26 `9c8df5e`* | 2 | keep with edits | Rules at `:21-45`, `:98-102` correct and load-bearing; "UNREAD BY GENERATION" (`:16`) false |
| `h1_nominal_scale_wiring.md` | 07-26 `9c8df5e` | 2 | keep with edits | Gate text superseded by H4 (`:160-165`); smoke figures dead (`:169-179`) |
| `h2_persona_timeline.md` | 07-26 `9c8df5e` | 2 | keep with edits | Stale pending banner (`:3-12`); `:27` is the arc's most reusable lesson |
| `h3_mortality_estate.md` | 07-26 `9c8df5e` | 2 | keep with edits | Same banner (`:3-8`), naming a nonexistent merge script; `:63-69` load-bearing |
| `h4_macro_modulation.md` | 07-26 `9c8df5e` | 3 | keep with edits | 3 of 4 headline readings superseded (`:15`-`:17`); EIP deferral reason (`:131`) expires in a modern window |

## TabFormer / IBM inventory

`grep -rni 'tabformer\|IBM' README.md docs/` is clean. What remains elsewhere:

### Prose: reword freely

| path:line | offending text | replacement |
|---|---|---|
| `include/phantomledger/exporter/card_fraud/schema.hpp:5` | "a TabFormer-shaped, transaction-fraud-only corpus" | "a transaction-fraud-only corpus shaped for continuous-time temporal graph learning (TGN, arXiv:2006.10637)" |
| `schema.hpp:32-33` | "(CHOICE; TabFormer mixes credit and debit cards)" | "(CHOICE; mixing credit and debit in one card view is standard for transaction-fraud graphs)" |
| `schema.hpp:126` | "and empty on real TabFormer" | drop; end "PhantomLedger populates ALL of it from its PII and access synthesis" |
| `schema.hpp:366` | "empty on TabFormer;" | drop; keep "(excluded from the GNN)" |
| `src/exporter/card_fraud/export.cpp:166` | "exactly like TabFormer's Online rows (which carry no merchant geography)" | "an online merchant has no modelled physical location, so it gets no geography chain" |
| `tests/CMakeLists.txt:487-488` | "carries IBM TabFormer's observed 0.11675% as a NAMED COMPARATOR" | "The aggregate rate is printed against a plausibility band; the external comparator was removed 2026-08 and the LEVEL is currently UNCALIBRATED." |
| `tests/test_schedule.cpp:107-108` | "intentionally not described as IBM's exact released-artifact interval" | delete; keep the 10,592/348 assertions at `:110-115` (live) |
| `data/commerce/README.md:20-24` | "The IBM benchmark… released TabFormer corpus references 100,343 merchant identifiers" | cite the generator paper already at `:22-23` (arXiv:1910.03033); drop the corpus reference |

### Vocabulary: emitted values

Renaming is free; changing the values moves `tests/golden_tables_card_fraud.md5`
and any pushed TigerGraph load.

- `include/phantomledger/exporter/card_fraud/derive.hpp:145` header
  `// TabFormer "Use Chip" value set`; `:178-180` emit `"Online Transaction"`,
  `"Chip Transaction"`, `"Swipe Transaction"`.
- `derive.hpp:231` header `// TabFormer "Errors?" value set`, over seven
  emitted `error` strings.
- `tests/test_card_use_chip.cpp:210` message "every derived value is in the
  TabFormer value set" (real check, wrong name).

Rename the headers "entry-mode value set" and "authorization-error value set";
keep the strings. `fraud_model_audit.md:568` is the standing ruling.
`card_fraud_feature_contract.md:328` already rules `error` "treat as noise or
drop"; dropping the column is the cheaper exit (owner decision B4).

### Gate constant

`kTabFormerRate = 0.0011675` (`tests/test_card_prevalence.cpp:54-55`, `:168`,
`:170`, `:381-383`) is print-only: the band is `kRateFloor = 0.0002` /
`kRateCeiling = 0.02` (`:173-174`, checked `:384`). The prevalence level is now
uncalibrated (`fraud_model_audit.md:159`); the gate prints `CARD VIEW … rate
0.13050%` with no anchor. A replacement must be an issuer-side rate by number of
transactions, not value-loss basis points (B3).

### Identifier in a live database

`policy.policy_id = 'nvidia_tabformer_v1'` (`tf_gnn_prep_ddl.sql:404`). Do not
rename it from here. The TigerGraph loader uses it at six live sites (primary
key insert, three joins, its verification code, its recorded verification
output). Here it gates `transaction_manifest`, which every downstream view
reads: renaming it in `tf_gnn_prep_ddl.sql` alone returns zero rows with no
error. The loader repo has no version control, so no revert. Safe order: insert
a new row beside the old, repoint all five consumers, verify a load, delete the
old row.

## Legacy rules that mislead

### Blocking: seven cited gates never ran

60 registered `pl_add_test` targets, 67 `tests/test_*.cpp`. Never registered
(`grep -c` 0, `git log -S`): `test_card_use_chip`, `test_card_endpoint_graph`,
`test_card_payment_timing`, `test_merchant_churn`, `test_relocation`,
`test_card_churn`, `test_session_point_in_time`. Stale binaries in
`build/tests/` look green when run by hand.

Cited as enforcement: `card_fraud_feature_contract.md:9`, `:67` (sole basis for
moving `use_chip` from CARE to FEATURE-SAFE), `:116`, `:322`, `:73` (four
`test_card_endpoint_graph` citations: sub-gates G and H, endpoint reuse,
not-on-file precision); `card_fraud_online_gnn.md:52`, `:184`;
`fraud_model_audit.md:568`, `:341`, `:796`, `:875`, `:1178`, `:1391`;
`card_fraud_v2_roadmap.md:557`, `:758`, `:497`.

Never true: a stronger form of `attacker-infra-2026-07` lesson 1 ("a count of
endpoints is not a measurement of the graph"). Fix: register and run them;
until then annotate each citation "test source exists, NOT REGISTERED" rather
than deleting it.

### Medium (retracted blocking): sub-gate G passes

Claimed: lift 1.356x (leg-long) and 1.213x (leg-wide) against
`ownedLift > 0.80 && ownedLift < 1.25` (`tests/test_card_endpoint_graph.cpp:933`),
so leg-long fails. Measured after registering (2026-08-05), twice, identical:

```
G merchant register: 139 of 343 owned; 304177 rows, fraud rate 0.009909, lift 1.084x
G merchant register: 142 of 354 owned; 298373 rows, fraud rate 0.011040, lift 1.209x
exit=0 (the gate passes)
```

Same legs (coverage matches), so the lift was misreported; `1.356` is not in
the output.

- Real error: realized coverage is 40.5% / 40.1%, not "~45%"
  (`card_fraud_feature_contract.md:73`). 0.45 is the declared
  `kBeneficialOwnerCoverage`
  (`include/phantomledger/entities/counterparties/merchant_ownership.hpp:132`).
  Write "declared coverage 0.45; realized ~40%".
- Watch: 1.209x is 3.3% under the ceiling, and `merchant-selection-2026-08` and
  `venue-reuse-2026-08` changed which merchants fraud reaches. The registered
  gate now catches drift.
- This and the orphan-gate finding came from one verifier pass. Re-run any
  number here before acting on it.

### Blocking: `Has_Device`/`Has_IP` "header-only", and the safety argument on it

Stale: `fraud_model_audit.md:569` (header-only), `:570` (Party ownership
withheld for all endpoints), `:576` (`Is_Merchant` unpopulated), `:587` (so
Party adjacency "cannot expose endpoint role"); `card_fraud_feature_contract.md:45`
(Identical class, against its own `:322`, FEATURE-SAFE). Closed by
`attacker-infra-2026-07` and `merchant-ownership-2026-07`:
`src/exporter/card_fraud/export.cpp:574` says "Has_Device / Has_IP ARE NO
LONGER HEADER-ONLY" (writers `:660`, `:702`); `:409-438` populates
`cf_Is_Merchant`. The audit supersedes the rows by name at `:847` and `:866`,
280 lines past the table.

Fix: rewrite the four rows with pointers to `:847`/`:866`. Replace `:587` with
the measured residual (endpoint not on file ⇒ fraud: precision 0.027, 2.9x
lift; intended and banded). Fix contract `:45` and add
`Has_Std_City`/`_Postcode`/`_State` (Identical class per
`tests/test_card_point_in_time.cpp:442`).

### Blocking: the repair script says to skip the repair

`tf_gnn_prep_session_endpoints.sql` re-anchors the live Device/IP vertex set on
session edges, but:

- `:10` calls the table header-only "BY DESIGN", citing `schema.hpp:283`, now
  party-geography prose.
- `:34` expects `audit_identity_fanout` to keep reporting 0 linked parties.
- `:296` expects the acceptance check that `cf_Has_Device` + `cf_Has_IP` carry
  0 rows; `card_fraud_postgres_acceptance.sql:600-608` now raises when the
  smaller is empty.
- `card_fraud_device_ip_investigate.sql:321` runs the repair only when
  `load_devices`/`load_ips` is zero, which no longer happens.

`attacker-infra-2026-07` rule 7 inverted both acceptance scripts; this third
was missed. The repair is still needed: at `kDeviceCoverage = 0.72` /
`kIpCoverage = 0.61` (`include/phantomledger/entities/infra/enrollment.hpp:78-79`)
the INNER JOIN silently under-loads the endpoint layer, dropping every
attacker endpoint not on file (the cross-victim signal). Trigger instead on
`load_devices` materially below `count(DISTINCT device_id)` in
`cf_Transaction_Uses_Device`, and fix `card_fraud_device_ip_investigate.sql:170`
(guard requires `ownership = 0`; message says POPULATED).

### Blocking: stale table count

`kTableCount = 43` (`include/phantomledger/exporter/card_fraud/schema.hpp:502`),
asserted exactly by `tests/test_table_golden.cpp:416`. Wrong:
`fraud_model_audit.md:567` (37, against its own `:1421`), `:769` (37), `:1128`
(40), `:1228` (44, never true); `card_fraud_v2_roadmap.md:176` (37);
`card_fraud_postgres_acceptance.sql:35` (39, while `:96`, `:177` assert 43).
Moved by `merchant-coordinates-2026-07` (37 → 40) and `party-geography-2026-07`
(40 → 43).

Fix, per `bls-citation-2026-07` rule 4 ("prefer one constant over four
copies"): date-stamp historical counts; cite `kTableCount`. 43 = 34 tables of
the original TigerGraph loader schema + 2 session edges + `Ground_Truth_Label`
+ the Email_Minhash pair + `Merchant_Location` + three `Has_Std_*`.

### Blocking: the merchant-ID baseline passes by reaching no floor

`card_fraud_feature_contract.md:343` cites "recall@precision≥0.90 < 0.25" as
no-shortcut evidence; `fraud_model_audit.md:606` says best precision 1.0000
(lift 295.65x). `./build/tests/test_card_baselines` at HEAD: pure-fraud-merchant
share 0.0000, recall @ precision ≥ 0.90 0.0000, best precision 0.7500 (lift
221.73x), 37,547 card rows, 262 merchants, base rate 0.00338 (changed by
`venue-reuse-2026-08`). Both clauses of `:606-608` are false: no fraud-only
merchant (0 of 262), and precision never reaches the floor, so the bound holds
on both axes. The same triple sits in `tests/test_card_baselines.cpp:71-77`,
under "DO NOT RESTATE A PRINTED NUMBER IN A COMMENT".

Fix: point at the gate's output; pin no new numbers. `merchant-selection-2026-08`
rule 5 names this: a band a leg cannot reach gives "a pass earned by having no
data".

### Blocking: "the era lock ends the window in 2020"

Stale at `fraud_model_audit.md:112` and `card_fraud_victimization.md:281`
(crypto declined), `h4_macro_modulation.md:131` (EIP deferral: window ends
2020-01-01), `tests/test_card_class_f.cpp:156-157` (ends 2021-01-01).
`src/app/cli.cpp:184-195` locks card-fraud to the macro series, which covers
1990-2024 (`include/phantomledger/synth/econ/era_data.hpp:101-136`) since
`macro-history-v1` (`9c8df5e`, 2026-07-26). Three prohibitions rest on the
expired reason: the crypto-rail decline, the EIP deferral, the short modern leg
of `test_card_class_f`.

Fix: state 1990-2024 and re-decide each. Legit crypto now has typed
`crypto_ramp_out`/`crypto_ramp_in` boundary flows; crypto as a scam rail is a
separate fraud-calibration decision. EIP matters: `--start 2020-01-01 --days
1096` reaches all three payments while `realPceLevel` carries the 2020 collapse
(0.9698) and 2021 rebound (1.0471), an inconsistent world.

### High: README describes closed defects as current

| line | claim | truth |
|---|---|---|
| `:714` | favourite add/drop is "a structural TODO" | `evolveFavorites` runs at `src/activity/spending/dynamics/monthly/evolution.cpp:297` (`:217` notes it was dead), `merchant-churn-2026-07`. Worst for a TGN: implies a frozen card → merchant graph |
| `:795` | "~8% of people get a 3-9 day high-spending burst" | `burst-rate-2026-07`: `burstsPerYear = 0.487` × `segmentSpan / 365.25` |
| `:1022` | LOC accrual sweeps "every enabled account"; billing "pre-generated (23:55 on each account's cycle day)" | `loc-accrual-perf-2026-08` closed that O(population²) sweep: lazy roll-forward on a due-time min-heap (`include/phantomledger/transactions/clearing/loc_accrual.hpp:119-167`). The 23:55 claim has no code referent and contradicts `:556` (rolling 30-day period) |
| `:795`, `:798` | "favK ∈ [8,30]… global merchant CDF"; "82%… from favorites" | `merchant-selection-2026-08`: REACH-law membership, geography-gated; `baseExploreP = 0.02` (`include/phantomledger/transfers/legit/routines/spending/behavior.hpp:42`) puts ~99.2% of picks on favourites |

### High: era-data provenance says the series are unread

`era_data_provenance.md:16-19` ("UNREAD BY GENERATION") contradicts
`fraud_model_audit.md:462`. 32 non-test files read
`priceScale`/`wageScale`/`realPceLevel`, and the refresh procedure (`:173-176`)
leads with the dead "moves ZERO goldens" branch. H1-H4 landed in the doc's own
commit. Fix: every value refresh is model-moving: re-pin `golden_run.b2sum`
and the three table goldens.

### High: gate numbers point at the wrong gates

`card_fraud_v2_roadmap.md:29`, `:50` say gates 1, 3, 4, 5 of
`card_fraud_online_gnn.md` have code; in its list (`:166-187`) 1, 3, 5 are open.
The renumbering (`## Minimum realism gates` 1-5 → `## Remaining benchmark
gates` 1-6) missed twelve sites: `tests/test_card_baselines.cpp:5`,
`tests/test_card_point_in_time.cpp:5`, `derive.hpp:238`,
`src/transfers/fraud/typologies/unauthorized.cpp:33`,
`include/phantomledger/transfers/fraud/injector_inputs.hpp:74`,
`schema.hpp:73`, `export.cpp:51`, `tests/test_table_golden.cpp:467`,
`card_fraud_postgres_acceptance.sql:557`, `tests/CMakeLists.txt:398,425,443,463`.
Fix: named anchors (restore an anchored "Minimum realism gates" 1-5), or
renumber once and update all twelve together.

### High: dangling `U-N` lineage

`harness-world-shape-2026-07` re-keyed `fraud_model_audit.md` to
`F-`/`L-`/`M-` codes: `U-4`, `U-5`, `U-7`, `U-8`, `U-9`, `U-11`, `U-12` have 0
hits (the one `U-6` is a superseded-claims row). Dangling:
`card_fraud_victimization.md:4`, `:271`, `:326`, `:351`, `:379`, `:381`, `:394`;
`era_data_provenance.md:167`; `h1_nominal_scale_wiring.md:4`, `:14`, `:183`;
`h2_persona_timeline.md:3`, `:120`, `:173`, `:179`, `:182`;
`h3_mortality_estate.md:4`, `:8`, `:66`, `:158`, `:159`, `:163`;
`h4_macro_modulation.md:4`, `:47`, `:74`, `:156`, `:221`; code at
`include/phantomledger/synth/econ/era_data.hpp:70` and
`include/phantomledger/transfers/fraud/typologies/unauthorized.hpp:88`. Fix: a
`U-N → M-N` table atop each doc, or re-point; never drop a `U-N` label without
fixing the two code sites.

### High: stale "owner verification pending" banners

`h3_mortality_estate.md:3-8` and `h2_persona_timeline.md:3-12` (2026-07-25)
tell the owner to run `merge_authority_*.py` (it does not exist) and recapture
all four goldens (done many times since). Their suite count (44) and the audit
chain's (56/56, 57/57, 62/62, 64/64, 65/65 at `fraud_model_audit.md:839`,
`:877`, `:951`, `:1037`, `:1237`, `:1342`) exceed the 60 registered targets:
files counted as targets, as with the orphan gates. Fix: mark both closed;
delete the `merge_authority_*.py` steps.

### High: golden row counts three re-pins old

`fraud_model_audit.md:1399` (the chain's last golden statement): "unmoved at
`2afaf188…`/188,477"; `tests/golden_run.b2sum` reads `07d4388c…  rows: 186144`.
Same: `card_fraud_v2_roadmap.md:180` (197,199 rows), `h4_macro_modulation.md:25-29`
(184,988 → 197,245), `h1_nominal_scale_wiring.md:169-179` (post-2b smoke
figures), `card_fraud_feature_contract.md:136` ("149 merchants across 48
distinct centroids"; the e2e gate prints 96 across 28 and asserts only more
than one). Fix: stop restating; point at `tests/golden_run.b2sum` and gate
output.

### High: `code_review_roadmap.md` skips the TGN deliverable

- No `card_fraud`, `TGN` or `GNN` in 230 lines. The Exporters station's read
  list (`:193-198`) never opens `exporter/card_fraud/` or `exporter/econ/`; its
  roster (`:203-205`) names ten tests and none of the eight registered
  card-fraud gates. Its own "read the tests last, as the spec" rule makes an
  unlisted gate invisible.
- Its byte-neutral rule names three goldens (`:21`; `golden_tables{,_aml}.md5`
  at `:221`), omitting `tests/golden_tables_card_fraud.md5`. A cleanup can move
  that golden and pass; in `merchant-coordinates-2026-07` and
  `merchant-ownership-2026-07` it was the only one that moved.
- Fix: add that golden at `:21` and `:221`, and the card-fraud read list and
  its eight gates to the Exporters station.

### Medium: entry mode is a distribution shift, not a 60-70x shortcut

The auditors' 60-70x `use_chip` shortcut was refuted. As corrected:

- Fraud CNP is era-flat, `kCardNotPresentShare = 0.70`
  (`src/transfers/fraud/typologies/unauthorized.cpp:39`); legit is dated,
  `kCnpShareByYear`
  (`include/phantomledger/activity/spending/market/commerce/local_pools.hpp:178-187`,
  0.010 in 1991 → 0.362 in 2022; read at `market/bootstrap.cpp:328`,
  `dynamics/monthly/evolution.cpp:92`).
- Measured (gate then unregistered): 1991 leg (pop 300, 730d) fraud 0.6760 vs
  legit 0.1698, 3.98x; 2019 leg (pop 300, 365d) 0.5294 vs 0.5802, 0.91x,
  inverted.
- The finder took `cnpShareForYear` (a membership-pool input) for the realized
  share; the per-row roll is the flat `kCardPresentShare = 0.89`
  (`src/activity/spending/routing/payments.cpp:42`). A draw-free probe: 1991
  online membership is 0.179 at pop 300 but 0.0154 at pop 100,000, so an
  early-era window at production scale sits near 15-45x.

The sign flips between an early-era train fold (~4x) and a modern test fold
(~0.9x), undisclosed. `card_fraud_feature_contract.md:67` ("the legitimate CNP
share is era-flat") and `card_fraud_online_gnn.md:182` are half false: only the
~0.8% explore-lane roll is flat. Register separately: date
`kCardNotPresentShare` like the legit series (`burst-rate-2026-07`'s error, on
the fraud side).

### Medium: `instrument == credit` is an uncaveated non-fraud rule

Every fraud rail sources the victim's primary deposit account
(`src/transfers/fraud/injector.cpp:927`,
`include/phantomledger/transfers/fraud/rings.hpp:52-69`, `unauthorized.cpp:602`);
`derive::cardId` renders `'C'` credit, `'D'` debit (`derive.hpp:120-133`);
`tests/test_card_prevalence.cpp:487` asserts `unauthorizedCredit == 0`
(measured "unauthorized credit 0, debit 218"). So the first character of
`card_number` nearly labels the unauthorized rail negative.
`card_fraud_online_gnn.md:73` and `card_fraud_feature_contract.md:87` permit
`instrument` with no caveat there.

Fix (the verifier's): keep it permitted (forbidding it dodges a documented
blocker instead of measuring it); add the caveat inline; make the
instrument-only baseline (`:124`) report credit-side coverage with its PR-AUC.
Do not touch `tests/test_card_prevalence.cpp:487`.

### Medium: other stale mechanism claims

| doc:line | stale | now |
|---|---|---|
| `card_fraud_online_gnn.md:150`, `:33` | card/device/residence lifecycles and merchant availability open | closed: `card-churn-2026-07`, `relocation-2026-07`, `merchant-churn-2026-07` |
| `card_fraud_feature_contract.md:184`, `:186` | "`popularity * exp(-distanceMiles/scaleMiles)`… Use it" | `merchant-selection-2026-08` step 2: `ReachModel` × decay + Zipf visits; legit favourites 1,206 mi → 3.8 mi, so the axis is weak and correctly signed |
| `fraud_model_audit.md:226`, `:228` | top-1 reach 0.129/0.107; ratio 3.65 vs 2.12-2.18 | `tests/test_card_merchant_graph.cpp:610` prints 0.359/0.293; `:646-647` armed 2.633/2.740 vs disarmed 1.791/1.828; `:654`: the old 2.90 floor "would have failed a correct build" |
| `fraud_model_audit.md:1067`, `:1055` | hazards 0.158/0.1145/0.0540; BLS "~84% at 1 year, ~58% at 5" | `bls-citation-2026-07`: 0.1720/0.1134/0.0494; published 82.8% / 57.7% (4 yr). The doc retracts these at `:1368-1371` but states them unmarked 300 lines earlier |
| `fraud_model_audit.md:1070`, `:893` | `GeographicMerchantPools`, `geo_pools.hpp` | retired into `commerce/local_pools.hpp` |
| `fraud_model_audit.md:277` | `paidFraction = .65` | `0.74` (`include/phantomledger/activity/income/salary.hpp:47`; `:40-46`: ".65 sat at the SEED-persona mean") |
| `fraud_model_audit.md:220` | "1,133 records ≈ 712 per 10,000 people" | 1,133 at pop 8,000 is 1,416 per 10k; 712 is the base catalogue. CLAUDE.md repeats the slip |
| `fraud_model_audit.md:225` | crossover "pop ≥ 20,875" | realized 20,792; `commerce/affinity.hpp:38` says ~20,833 |
| `card_fraud_v2_roadmap.md:95`, `:566` | "`giftCardScam` is always card-present" | `unauthorized.cpp:461-463` draws a dated `digitalGiftCardShare`, zero before 2005: true only on 1991-start legs |
| `card_fraud_v2_roadmap.md:101` | kernel reuse closed the distance shortcut | open at 7.4x until `merchant-selection-2026-08` step 2 geography-gated legit membership |
| `card_fraud_v2_roadmap.md:97`, `:520` | `cardPresentDecayScaleMiles`, `pickMerchantDestination` | `commerce::decayScaleMilesFor`, `buildMerchantPool`/`pickFromPool`/`campaignVenue`; the dead symbol is also at `derive.hpp:151` |
| `card_fraud_v2_roadmap.md:135` | "precision never approaches 0.90" | last copy of the false argument `stale-claims-2026-08` fixed in `tests/test_card_baselines.cpp:64-78` |
| `card_fraud_victimization.md:49` | "the IP is a random address" | `attacker-infra-2026-07`; `randomIpv4` survives only as a comment (`injector.cpp:799`) |
| `card_fraud_victimization.md:24`, `:45` | `injector.cpp:577`, `:348`, `:374` | now `:975-1000`, `:387`, `:420-430`; the exclusion list also omits `shellFraudAccounts` |
| `h4_macro_modulation.md:15`-`:17` | parity 0.9504, drift 0.927, ring-rail 2.012 | `harness-world-shape-2026-07` dropped the single-seed estimator (`tests/test_econ_wiring.cpp:116-123`); now ~0.9072 mean, 0.950 (after dipping to 0.797 against a 0.80 floor), 1.9104 |
| `h1_nominal_scale_wiring.md:160-165` | drift parity on deflated y/y | H4 `:150-157`; code divides by `priceScale × realPceLevel` (`tests/test_econ_wiring.cpp:230`) |
| `era_data_provenance.md:132` | H5 adoption series "pre-registered as PLANNED" | H5 never happened (two mentions, both in docs; `h4_macro_modulation.md:21` defers the README sweep to it); `ecommerce_share` shipped as `kCnpShareByYear` |
| `code_review_roadmap.md:200` | "the four exporters" | five use-case exporters plus `exporter/econ/` |
| `code_review_roadmap.md:91-99`, `:101-112`, `:120-126`, `:128-142`, `:160-171`, `:173-189` | station inventories | miss `clearing/loc_accrual.*` (and `test_loc_accrual`, whose sub-gate D CLAUDE.md forbids deleting), `infra/attackers.hpp`, `infra/enrollment.hpp`, `holdings/card_reissue.hpp`, `counterparties/merchant_ownership.hpp`, `parties/relocation.hpp`, `commerce/reach.hpp`, `commerce/affinity.hpp`, `fraud/exposure.hpp`, `fraud/susceptibility.hpp`, `pipeline/world_footprint.hpp` |
| `debugging.md:163-165` | "its merge-script protocol" | gone since `harness-world-shape-2026-07`; now THE AUTHORITY RULE (`fraud_model_audit.md:21`) |
| `card_fraud_device_ip_investigate.sql:48` | "~61% address coverage" | the constant is `kIpCoverage` (CLAUDE.md's open questions repeat the slip) |
| `README.md:1005` | "avoiding invalid NANP ranges" | NANP is phone numbering; `network::randomIpv4` avoids reserved IPv4 ranges |

## What the 100k × 3y target changes

### Memory: 100,000 × 1,095 days does not fit 32 GB, probably not 40

Anchors (CLAUDE.md `loc-accrual-perf-2026-08`; `ram_derive_dont_store.md:15-23`):
50,000 × 730d = 45.0M rows, 13.2 GB peak RSS, 198 s (largest real run);
20,000 × 730d = 4,494 MB run peak, 2,841 MB prologue (post-R2.4c.0). Premise,
`ram_derive_dont_store.md:37-38`: "Both parts scale with population × days
(rows), not with population alone."

Rate: 45.0e6 / (50,000 × 730) = 1.233 rows per person-day (CLAUDE.md's "500,000
× 730d is ~450M rows" agrees). Target: 109.5M person-days → 135.0M rows, 3.00x
the 50k anchor.

| method | rate | projection |
|---|---|---|
| 50k anchor | 13.2 GB / 45.0M = 293 B/row | 39.6 GB |
| 20k anchor | 4,494 MB / 18.0M = 250 B/row (a floor: cost rose 250 → 293 from 20k to 50k) | 33.7 GB |
| linear in pop × days | prologue 2,841 MB × 7.5; fold 1,653 MB × 7.5 | 33.7 GB |

33.7-39.6 GB, central ~39.6. At 293 B/row a 32 GB box holds ~109M rows: 80,800
people at 1,095 days, or 100,000 at ~885 days (2.42 yr). Assumes peak RSS
linear in rows; a config-independent 1.233 rate (the 60-day golden runs 1.55,
so this understates short windows); a negligible population-only term (post-R2
spine ≈ 400 B/person, 40 MB at 100k). Nothing ≥ 100,000 with a multi-year
horizon has run here; the only ≥ 100k point, a pre-R2 run at 200,000 (14.6 GB,
`ram_derive_dont_store.md:19`), is not comparable.

Options are under B1. R2.5, which `ram_derive_dont_store.md:88-94` names the
prerequisite, is not done; R2.5a (`9c8df5e`, `base_run_set.hpp:5`,
`stages/transfers/windowed_run.cpp:211`) bounds only replay-view staging, and
the RAM doc omits it.

Measure first with `tests/test_scale_soak.cpp`, undocumented at
`debugging.md:52-53`: `PL_SOAK=1` (`SKIP_RETURN_CODE 77`), knobs `PL_SOAK_POP`
/ `_DAYS` / `_SEED`, defaults 10,000 / 365 (30x under target person-days); the
14,400 s timeout (`tests/CMakeLists.txt:707-709`) will likely bind.

Re-open "R2.4 is banked" (`ram_derive_dont_store.md:32`): the fold's share was
1,653 MB at 20k × 730d and is ~12.4 GB at the target, and
`loc-accrual-perf-2026-08` changed its per-row work since the probe.

### Temporal split

Already fixed: `card_fraud_online_gnn.md:106-132` splits by window (train ~67%,
validation ~16%, test ~17%; worked for `--start 2022-01-01 --days 1096`). Still
unrecorded:

- The split is not in this repo. The TigerGraph loader sets a split-policy row,
  read by `transaction_manifest` (`tf_gnn_prep_ddl.sql:373`), to
  `train_end_epoch = 1514764800` (2018-01-01) and
  `validation_end_epoch = 1546300800` (2019-01-01). A 3-year window missing
  either instant puts 100% of rows in one split, and `audit_failures`
  (`tf_gnn_prep_ddl.sql:1043-1154`) never checks a split is non-empty, so a TGN
  trains and evaluates on one partition with nothing failing.
- Legal windows: `src/app/cli.cpp:183-195` exits on card-fraud outside
  1990-2024. `2017-01-01 +1095d`, `2021-01-01 +1096d`, `2022-01-01 +1096d`
  pass; `2023-01-01 +1096d` fails. The default `--start 2025-01-01`
  (`include/phantomledger/app/options.hpp:83`) is illegal for card-fraud by
  design (tripwire `tests/test_app_options.cpp:66-78`); `README.md:149` does not
  say so.

Action (live-row or loader changes, not code here): an `empty_split` branch in
`audit_failures` requiring `split_id` 0, 1, 2 non-empty; split epochs derived
from the corpus window.

### Era machinery a modern 3-year window stops exercising

- Cross-era gates need two eras: `test_card_class_f` (1991 vs 2019,
  `tests/test_card_class_f.cpp:378-380`), `test_card_use_chip` (1991, 2019).
- Recessions go inert (`recessionMonths = 0` for 2021-2024,
  `era_data.hpp:131-136`), mooting `era_data_provenance.md:63-66`'s monthly
  unemployment request, which H4 rejected (`h4_macro_modulation.md:116-124`).
- Card reissue nearly vanishes. Probe over `entity::card::reissue::generationsFor`
  (20,000 keys), 1,096 days from 2022-01-01: mean 1.248 generations, 78.8%
  single, vs 6.419 over 20 years; 1.750 if overlapping 2015-2017 (EMV). The
  guidance at `card_fraud_feature_contract.md:91-121` governs a minority case.
- Relocation thins: 0.1047 moves per person-year ≈ 0.31 in 3 years; ~73% of
  parties have one tenure row (`card_fraud_feature_contract.md:163-169`).
- Merchant decay drops from 23% to ~3.5%, hiding the birth-sizing defect
  (`include/phantomledger/synth/merchants/lifecycle.hpp:262` sizes births with
  `hazardMature = 0.0494` for a cohort in the 0.1720/0.1134 birth bands, ~1.45x
  short), which `fraud_model_audit.md:1069` calls CONFORMS.
- CPI drift: 2021 → 2023 carries 12.5%, the series' largest annual move. The
  `h1`-`h4` and `era_data_provenance` illustrations are 29-year ratios (2.87x
  PCE, 1.88x CPI), and `h4_macro_modulation.md:216-222` covers only
  below-calibration. Modern years run above it on every axis (`realPceLevel`
  1.047/1.058/1.073, `priceScale` 1.060/1.145/1.192 for 2021-2023), so budgets
  sized off the docs come out wrong.
- Joiners thin ~2.5x (BEA growth ~1.34%/yr in 1991-92 vs 0.20-0.82%/yr in
  2021-2023): ~1,600 at 100k × 3y vs ~4,000 from 1991. Short-tenure accounts
  matter most to a temporal model.
- Keep the 1991 legs (0.533 `priceScale`; modern legs sit 4-23% from unity).
  `h3_mortality_estate.md:63-69`: unscaled production subscriptions survived
  because a leg sat at the calibration year. Conversely,
  `h2_persona_timeline.md:27`: two legs on one side of the 2025 payroll anchor
  cancelled and hid a half-income defect, and every H1-H4 corpus gate runs at
  1991 (`test_econ_wiring` 1991 + 2019; `test_persona_wiring`,
  `test_membership`, `test_estates`, `test_lifespan` 1991 only). Add a modern or
  frozen leg.
- Attacker fan-out densifies. At HEAD, leg-long (900 × 1461d): shared 0.7380,
  mean 5.102, max 35; leg-wide (1800 × 731d): 0.8198, 8.712, 39 (so "max
  37-40", `card_fraud_feature_contract.md:256`, is stale). The concurrency floor
  sizes both (80 and 44 operators vs a population term of 10.8,
  `src/synth/infra/attackers.cpp:24-66`); at 100,000 the population term binds
  (900 vs a 61.8 floor), raising cases per operator and endpoint degree ~4-8x.
  Record which term bound in any re-quote.
- Anti-shortcut bands were all measured at pop 300-10,000. Reach concentration
  falls with merchants per area (`merchant-selection-2026-08` rule 8: top-1
  reach 0.359 / 0.293 / 0.163 / 0.082 at 300 / 2,000 / 8,000 / 500,000), and
  `coreFloor = 250` (`include/phantomledger/synth/merchants/make.hpp:21`)
  binds only below 20,792: no gate leg runs the target's merchant regime.
- Session-edge probes grow ~3.5x past their "~20M rows" warnings
  (`card_fraud_device_ip_investigate.sql:24`, `:27`, `:288`;
  `tf_gnn_prep_session_endpoints.sql:102`, `:244`; sized for the retired 6,000
  × 7,305d corpus). The unindexed fan-out self-join grows superlinearly, and
  `src/exporter/sinks/table_mirror.cpp:105` drops tables each run, so no
  indexes.
- Acceptance defaults (`expected_population 6000`, `expected_days 7305`,
  `card_fraud_postgres_acceptance.sql:7`, `:11`) abort the target at `:235`.

## What a TGN implementer still cannot find

1. Node/edge inventory: which of the 43 `cf_` tables are vertices, edges, memory-bearing.
   `card_fraud_feature_contract.md:11` promises every column but rules on none
   of `Has_Address`, `Has_Phone`, `Has_ID`, `Has_DOB`, `Has_Full_Name`, or the
   `Card_Send_Transaction` and `Merchant_Receive_Transaction` columns.
2. Event recipe: `cf_Payment_Transaction` joined to `cf_Card_Send_Transaction`
   + `cf_Merchant_Receive_Transaction` on `txn_id`, keyed on `edge_unix_time`
   ("drives the GNN temporal sampler", `schema.hpp:188-189`).
3. Clock: `unix_time`, `transaction_time`, `edge_unix_time` never
   disambiguated.
4. Memory updates and batching: `card_fraud_online_gnn.md:61` orders features
   (`< t`) before the event, then stops; TGN's message-store staleness trap at
   that step-2/step-4 boundary is unmentioned.
5. Negatives: the docs specify transaction classification; the TigerGraph
   loader's schema assumes self-supervised link prediction. Nothing reconciles
   them or says how negatives are drawn.
6. Inductive nodes: `card_fraud_online_gnn.md:122-123` wants a holdout but not
   what churns in 3 years: merchants (hazard 0.1720/0.1134), cards barely
   (1.248 generations), parties barely (~0.31 moves), attacker endpoints
   continuously.
7. Card identity: `card_number` changes per reissue (`streaming.hpp:261-263`
   resolves the generation at row time; `derive::cardId` appends `-G<n>`,
   n > 0), splitting one account into up to ~6.7 nodes and dropping memory at
   each. Key on the account / `cf_Party_Has_Card`. `-G<n>` is a suffix, so the
   "identifier prefixes" ban (`card_fraud_online_gnn.md:86`) may miss it.
8. What reaches TigerGraph: the loader never references
   `Transaction_Uses_Device`, `Transaction_Uses_IP` or `Ground_Truth_Label` and
   has no transaction → endpoint edge, so the two timestamped session edges
   (the only continuous-time transaction → entity evidence) stay in Postgres,
   and labels arrive only via `cf_Payment_Transaction.is_fraud`. CLAUDE.md
   lists this under "Still open downstream"; the TGN contract does not.
   `tf_gnn_prep_ddl.sql` has no session-edge view (`audit_load_counts`,
   `:1749-1864`, lists 29 datasets), and `load_devices`/`load_ips` (`:1284`,
   `:1361`) set every `first_seen_unix_time` to 0.
9. Geography: `Merchant_Location` and three `Has_Std_*` are on no list.
   Distance is now safe (legit favourites mean 3.8 mi, 97.3% within 50 mi,
   closing a 7.4x wrong-signed shortcut), endpoints are area centroids
   (intra-area distance 0; coresidents share one), and a `cf_Merchant_Location`
   row is the `has_coordinates` mask. No doc says any of this.
10. Merchant signal: `card_fraud_online_gnn.md:42` marks fraud-only merchant
    identity "Closed". Per CLAUDE.md `venue-reuse-2026-08`, true
    common-point-of-purchase is unreachable here; cross-victim excess is ~0
    (armed 1.615x vs disarmed 1.364x, overlapping, one seed inverted);
    campaigns meet merchants only on `Rail::card` (~39% of unauthorized cases);
    what ships is an online cash-out analogue that "must never be described as
    physical POS-breach CPP".
11. A review path (see
    [`code_review_roadmap.md` skips the TGN deliverable](#high-code_review_roadmapmd-skips-the-tgn-deliverable)).

## Recommended edit plan

### Safe now (prose only)

Every prose fix above, preferring symbol names to line numbers (stale twice
now). Also: `PL_SOAK=1` and its knobs at `debugging.md:52`, and at `:186` a
note that `is_fraud` means something only on `cf_Payment_Transaction`;
archiving `card_fraud_v2_roadmap.md` after lifting rulings 2 and 6 (`:33`,
`:39`) and the parity trap (`:608`); and writing the six decision-free TGN gaps
(inventory, join recipe, clock, memory ordering, card identity, geography) into
`card_fraud_online_gnn.md`.

### Needs an owner decision

| # | decision | options |
|---|---|---|
| B1 | Ship 100k × 1,095d? Projects 33.7-39.6 GB. | R2.5 first; ~80,000 people or ~885 days; or ≥ 48 GB. Measure with `PL_SOAK` at 50k × 1096d and confirm the slope first. |
| B2 | Which 3-year window? | 2022-01-01 +1096d (modern, ends at the 2024-12-31 era edge, no headroom); 2021-01-01 +1096d (crosses the largest CPI jump); 2017-01-01 +1095d (pre-COVID, entry-mode lift ~4x not ~1x). Each changes which era machinery fires and which gate legs stay representative. |
| B3 | Prevalence anchor (now uncalibrated) | An issuer-side by-number rate, or declare it unanchored. Tuning toward one is a `targetEvents`/rail-mix change with re-pins; use the fraud-budget procedure. |
| B4 | `error` column: rename values or drop? | Contract rules it "noise or drop" (`:328`); dropping is cheaper but a schema change. |
| B5 | `nvidia_tabformer_v1` | Leave it (free; one token in a live DB) or rename at all six loader sites in lockstep; the loader has no revert path. |
| B6 | Register the seven orphans? | Register and fix; register and re-derive the band with a disarm; or annotate every citation. Not silently orphaned. (The predicted G red was retracted.) |
| B7 | Re-open the EIP and crypto-rail deferrals? | Declare 2020-2021-crossing windows out of scope, or promote the class-S EIP table. |
| B8 | Retire `card_fraud_v2_roadmap.md` and `tf_gnn_prep_ddl.sql`? | Archive the roadmap (recommended); re-dump the DDL from a repaired DB, or banner it stale. |

### Needs a lockstep code change

| # | change | where | gate |
|---|---|---|---|
| C1 | Register the seven orphans | `tests/CMakeLists.txt` | all seven |
| C2 | Split epochs from the window; `empty_split` check | the loader's split policy; `tf_gnn_prep_ddl.sql:1043-1154` | `audit_failures`, loader verification |
| C3 | Non-empty `cf_Is_Merchant`, `cf_Merchant_Location` (the gate covers 2 of the loader's 7 empty-table aborts) | `card_fraud_postgres_acceptance.sql:600-608` | the acceptance run |
| C4 | Re-target defaults from 6,000 × 7,305d | `card_fraud_postgres_acceptance.sql:5-31`, `:7`, `:11` | `:235` |
| C5 | Classify `Email_Minhash`/`Has_Email_Minhash`, the only 2 of 43 tables on no list (breaks the contract's rule at `:350`) | `tests/test_card_point_in_time.cpp` | same |
| C6 | Modern or frozen leg; keep 1991 | `tests/test_econ_wiring.cpp:240-241` | `test_econ_wiring` |
| C7 | 3-year leg (legs are 15y and 5y; the horizon is missing, not the band) | `tests/test_merchant_churn.cpp` | `test_merchant_churn` (after C1) |
| C8 | Fix the dead branch and dead trigger | `card_fraud_device_ip_investigate.sql:160-186`, `:321` | none (diagnostic) |
| C9 | R2.5, if B1 picks it | `pipeline/stages/transfers/` | `test_arch_equivalence`, `test_scale_soak` |
| C10 | Date `kCardNotPresentShare` on the legit side's Fed series | `unauthorized.cpp:39` | `test_card_use_chip` (after C1) |

## Do not touch

1. Zeroed-not-dropped label columns (`card_fraud_v2_roadmap.md:39`): the
   TigerGraph loader maps columns by position, so dropping one shifts every
   later attribute in a live load. Live as `kLabelWithheld = 0`
   (`src/exporter/card_fraud/export.cpp:58`; writers `:141`, `:395`, `:642`,
   `:702`; columns `schema.hpp:139`, `:155`). Same for the
   `cf_Ground_Truth_Label` quarantine (`card_fraud_feature_contract.md:319-320`)
   and roadmap ruling 2 (`:33`).
2. Anchors F1 (`:22`), F2 (`:40`), D1 (`:178`), D2 (`:197`), D3 (`:219`) of
   `card_fraud_victimization.md`, cited at 13 code sites: `exposure.hpp:5`,
   `:49`; `susceptibility.hpp:5`; `injector_inputs.hpp:117`, `:141`;
   `typologies/unauthorized.hpp:71`; `taxonomies/fraud/types.hpp:55`;
   `injector.cpp:398`, `:402`, `:975`; `test_fraud_low_population.cpp:5`;
   `test_card_class_f.cpp:147`; `test_card_victim_baselines.cpp:5`;
   `tests/CMakeLists.txt:535`, `:565`. Renumber only with all thirteen in one
   commit.
3. The debit-backed-positives warning (`card_fraud_feature_contract.md:87`),
   guarded by `tests/test_card_prevalence.cpp:483-489`. Strengthen, never trim.
4. `h3_mortality_estate.md:63-69` ("pins 2019 rows, where scale == 1.0 hides
   the difference"): promote it; it is the case against retiring 1991 legs.
5. `h2_persona_timeline.md:27-29` ("both of their legs (1991, 2019) sat before
   the anchor and were equally starved, so every RATIO held"): keep verbatim,
   though the smoke figures at `:24-25` are dead.
6. `ram_derive_dont_store.md:37-38` ("Both parts scale with population × days
   (rows), not with population alone."): why R2.5 (rows), not R3 (population),
   is the lever. Lead the sizing section with it.
7. `era_data_provenance.md:21-45` (`kCalibrationYear = 2019`, plus the rejected
   alternatives that stop a re-anchor to 2024) and `:98-102` (coverage
   `first ≤ 1990 && last ≥ 2020`), enforced at `era_data.hpp:153`,
   `src/synth/econ/catalog.cpp:60-69`, `src/app/cli.cpp:184-196`. Strip only
   the canonical-window clause (`:28-29`).
8. The governing directive (`card_fraud_v2_roadmap.md:25-28`): "No exported
   feature labels the corpus without behavior, plus a stated contract for which
   columns a model may read." Move it to a live page on archiving.
9. The four-call-site parity trap (`card_fraud_v2_roadmap.md:608`,
   `FraudEmission::legitCounterparties`; sites now `simulate.cpp:102`,
   `windowed_run.cpp:295`, `window_leg_support.hpp:416`,
   `test_membership.cpp:650`) and "do not add indexes"
   (`tf_gnn_prep_session_endpoints.sql:42-44`).
10. Live constants near edited text: `tests/test_schedule.cpp:110-115` (10592,
    348), `tests/test_card_prevalence.cpp:487` (`unauthorizedCredit == 0`),
    `tests/test_loc_accrual.cpp:365-377` (sub-gate D, a cost gate CLAUDE.md
    forbids deleting as flaky).
11. Never withhold or drop the always-0 label columns, `cf_Ground_Truth_Label`,
    `cf_Transaction_Uses_Device`/`_IP`, `cf_Has_Device`/`_IP`,
    `cf_Is_Merchant`, `cf_Merchant_Location` or `cf_Has_Std_*`.
    `merchant-ownership-2026-07` surfaced as a downstream hard abort: the
    TigerGraph loader refuses the push on seven empty-table conditions. Absent
    and asserted-empty tables look the same from here; grep the consumer for
    both first.
