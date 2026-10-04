# Generated temporal mule dataset: 200,000 people, 2024

Generated 18 September 2026 into local PostgreSQL database `phantomledger_mule_temporal_2024_research`, schema `mule_temporal`, all 27 staging tables of the [graph schema](../../schemas/mule_temporal.gsql). Current runs use `phantomledger` (shared with every use case); this data was not moved there. Staging columns are text; convert `is_mule` to the graph's `INT` when loading.

| Item | Result |
|---|---:|
| People / calendar days (1 January to 31 December 2024) / seed | 200,000 / 366 / 42 |
| Exported account vertices | 788,283 |
| Mule accounts (`is_mule = 1`) | 233 |
| Other accounts (`is_mule = 0`) | 788,050 |
| Withheld account labels (`is_mule = -1`) | 0 |
| Mule accounts participating in Zelle | 191 |
| All payments | 115,741,341 |
| Zelle transfers | 1,225,864 (1.0591% of all payments) |
| Other payment vertices | 114,515,477 |
| Non-Zelle deposit-to-deposit payments | 24,423,034 |
| Zelle: internal → internal | 1,144,076 |
| Zelle: external → internal | 43,690 |
| Zelle: internal → external | 38,098 |
| External accounts participating in Zelle | 5,581 |
| Zelle payments involving a business/landlord | 14,924 |
| Zelle payments with fraud supervision | 5,036 |
| Zelle token registrations / closed bindings | 144,910 / 1,240 |
| Zelle total amount / mean / median | $143,691,534.58 / $117.22 / $66.11 |
| Database size after generation | 72 GB |

- Payments run from 2024-01-01 00:00:00 to 2024-12-31 23:59:59. All requested people are present.
- Account vertices are observed accounts; the simulator's registry can also hold never-observed ones.
- Other rails: 4,163,459 ach; 60,421,728 card; 6,977,290 cash; 183,319 check; 36,079,573 unknown; 6,690,108 internal. `unknown` means no narrower rail is known, not Zelle.

## Mule-label definition

`is_mule = 1` is the simulator's `entity::account::Flag::mule` role on the designated primary account, internal accounts only; everything else is 0, external Zelle users included, so external mule prevalence is uncalibrated. Static truth, not recruitment dates or adjudications; never a GNN feature or neighbor message. Zelle `fraud_label` is a separate payment target. Policy and temporal limits: [Account-level mule supervision](../mule_temporal.md#account-level-mule-supervision).

This run predates the label contract: its `mt_Account` has six columns (`id, account_type, is_external, first_seen_seq, first_seen_ts_ms, is_mule`), not the current fifteen (MulePatternLearner's nine label fields, `mule_label_known` through `mule_label_source`, appended). A graph loaded from it has `mule_ring_id` -1 for all 233 mules, though each is in a ring. The append changes nothing else: for the same world and payments, the other 26 tables and the six columns are byte-identical. A regeneration still differs, because generator rounds committed from 26 September 2026 change the payments. To get the rings, regenerate and reload ([Loading the corpus into TigerGraph](../mule_temporal.md#loading-the-corpus-into-tigergraph)). These counts describe this run only.

## Research profile

The [Zelle scenario](zelle_model.md): 31.5163% consumer sending-user propensity, 55.6225% conditional electronic-P2P rail choice, rolling sending limits; not targets for all account-to-account payments. Businesses use a documented consumer-rate proxy; eligible ownerless external counterparties are assumed domestic accounts at participating banks. Amounts are the synthetic ledger's, not fitted to national Zelle averages.

## Reproduce and validate

Generation rewrites the selected output tables and the shared `transactions` audit table in `phantomledger`.

```sh
PL_PG='dbname=phantomledger' ./build/phantomledger \
  --usecase mule-temporal --start 2024-01-01 --days 366 --population 200000 --seed 42
psql 'dbname=phantomledger' -f docs/research/validate_temporal_dataset.sql
psql 'dbname=phantomledger' -f docs/research/validate_zelle_dataset.sql
psql 'dbname=phantomledger' -f docs/research/profile_temporal_dataset.sql
```

- Both validation scripts passed: table counts, exact payment reconciliation, event exclusivity, identities, references, valid-time intervals, participation clocks, visible token bindings, rolling sender-profile dollar limits, requested population, binary mule labels.
- `test_mule_temporal`, `test_app_options`, `test_pipeline_e2e` and `test_window_invariance` (month versus quarter windows) passed. Current tests verify that changing account or payment labels changes no ID, feature attribute, rail, event, edge or clock.
- Monthly generation buffers bound memory. The run took about 63 minutes and peaked at about 13.7 GB RSS (generator-reported).
- The [manifest](mule_temporal_2024_dataset.json) records counts, run digest, source-file checksums and validation results; the [calibration snapshot](zelle_2024_calibration.json) and [reproduction script](zelle_2024.py) keep the public-data calculation.
- Keep the `transactions` audit table out of model features.
- Not done: TigerGraph loading or compilation, model training.
