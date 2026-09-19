# Generated temporal mule dataset — 200,000 people, 2024

This historical run used the local PostgreSQL database **`phantomledger_mule_temporal_2024_research`**. Current runs use **`phantomledger`**, shared with every other use case; the counts below describe the earlier run and do not imply that its data has been moved. Generation completed on **18 September 2026**. That dataset contains **200,000 person vertices**, the full calendar year **1 January–31 December 2024 (366 days)**, and integer-valued **`Account.is_mule`** supervision.

The destination schema is **`mule_temporal`**, containing all **27 staging tables** for the [updated graph schema](../../schemas/mule_temporal.gsql). PostgreSQL staging columns are text, as in the existing exporter; convert `is_mule` to the graph's `INT` when loading TigerGraph.

```sh
psql 'dbname=phantomledger'
```

| Item | Result |
|---|---:|
| People / calendar days / seed | 200,000 / 366 / 42 |
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

The first payment is **2024-01-01 00:00:00** and the last is **2024-12-31 23:59:59**. Account vertices include accounts observed in the graph; the simulator's complete registry can also contain accounts that are never observed. All requested people are present.

Other rails: **4,163,459 ach; 60,421,728 card; 6,977,290 cash; 183,319 check; 36,079,573 unknown; 6,690,108 internal**. `unknown` means the source ledger does not identify a narrower rail; it does not imply Zelle.

## Mule-label definition

`is_mule = 1` comes from the explicit simulator `entity::account::Flag::mule` role on the designated primary account. Other accounts are `0`; secondary accounts of the same owner, victims, non-mule organizers and counterparties are not made positive merely because of ownership or a fraudulent payment. Zelle `fraud_label` is a separate payment-level target.

These account labels are static simulation truth, not recruitment dates or historical adjudications. Exclude `is_mule` from GNN features and neighbor messages. The current simulator generates internal mule-account roles; external accounts can use Zelle but have no separately generated mule roles and are `0` under this closed-world definition. This dataset does not calibrate external mule prevalence. See the [label policy and temporal limitations](../mule_temporal.md#account-level-mule-supervision).

## Research profile

The [research-based Zelle scenario](zelle_model.md) retains the **31.5163% consumer sending-user propensity**, **55.6225% conditional electronic-P2P rail choice**, and rolling sending limits. These probabilities are not targets for all account-to-account payments. Business adoption uses a documented consumer-rate proxy; eligible ownerless external counterparties are assumed to be domestic accounts at participating banks. Amounts come from the underlying synthetic ledger, not a fit to national Zelle averages.

## Reproduce and validate

Use the existing `phantomledger` database. PhantomLedger rewrites the selected output tables and the shared `transactions` audit table when generating a new dataset.

```sh
PL_PG='dbname=phantomledger' ./build/phantomledger \
  --usecase mule-temporal --start 2024-01-01 --days 366 --population 200000 --seed 42
psql 'dbname=phantomledger' -f docs/research/validate_temporal_dataset.sql
psql 'dbname=phantomledger' -f docs/research/validate_zelle_dataset.sql
psql 'dbname=phantomledger' -f docs/research/profile_temporal_dataset.sql
```

Both database validation scripts passed on the delivered dataset. They checked table counts, exact payment reconciliation, event exclusivity, identities, references, valid-time intervals, participation clocks, visible token bindings, rolling sender-profile dollar limits, the requested population, and binary mule labels. Tests **`test_mule_temporal`**, **`test_app_options`**, **`test_pipeline_e2e`**, and **`test_window_invariance`** passed. Current tests verify that changing account or payment labels does not change IDs, feature attributes, rails, events, edges, or clocks.

Large temporal runs use monthly generation buffers to bound memory while preserving the full-year output. The completed run took about 63 minutes and peaked at approximately 13.7 GB RSS as reported by the generator. The month-versus-quarter window-invariance test passed.

The [machine-readable manifest](mule_temporal_2024_dataset.json) records counts, the run digest, source-file checksums, and validation results. The [calibration snapshot](zelle_2024_calibration.json) and [reproduction script](zelle_2024.py) preserve the public-data calculation. The `transactions` audit table must remain outside model features. TigerGraph loading/compilation and model training have not been performed.
