# Use-case exports

Every run streams the raw ledger into the shared `transactions` table during settlement (`SELECT * FROM transactions ORDER BY row_seq`), and the chosen `--usecase` writes its own tables during the run. All of it lands in the PostgreSQL database named by `PL_PG`; no files are written, and the same seed and config rewrite byte-identical content. How to run each use case is in the [README](../README.md); the world they all share is in [simulation.md](simulation.md).

Each use case writes its own schema, so same-seed runs in one database compose without collisions. IDs are canonical across use cases; `mule-temporal` pseudonymizes them.

| `--usecase` | Schema and tables |
|---|---|
| `standard` | `public` (unprefixed, beside the shared `transactions` stream) |
| `mule-ml` | `mule_ml.ml_ready_*` |
| `aml` | `aml.aml_*` |
| `aml-txn-edges` | `aml_txn_edges.aml_txn_edges_*` |
| `card-fraud` | `card_fraud.cf_*` |
| `mule-temporal` | `mule_temporal.mt_*` (opaque entity IDs, temporal schema) |

## `standard`

Vertices `person`, `accountnumber`, `phone`, `email`, `device`, `ipaddress`, `merchants`, `external_accounts`; edges `HAS_ACCOUNT`, `HAS_PHONE`, `HAS_EMAIL`, `HAS_USED`, `HAS_IP`, `HAS_PAID` (aggregated); the raw ledger is the shared `transactions` stream. `HAS_PAID` and the flow aggregates keep only rows inside both endpoint owners' `[joinTs, closeTs)`, as a bank's books would. The entity-resolution `customer.csv` carries `created_at` (join) and `closed_at` (closure if inside the window, else empty).

## `mule-ml`

For GraphSAGE and node-level mule detection: `Party` (one row per account with fraud label, phone, email and full deterministic identity: name, SSN, DOB, address, geo, country, canonical IP, canonical device), `Transfer_Transaction` (the raw ledger), and `Account_Device` and `Account_IP` (aggregated account-infra edges with counts and first and last seen). Ages come from the per-person birth date (persona-band draws, e.g. retired Beta-weighted over 65–99, student 16–34; joiners anchored at join). Addresses use `faker-cxx` with deterministic zip-code lookups to real US cities, plus a fallback list.

## `mule-temporal`

For TigerGraph Mule_Pattern_Learner: [schemas/mule_temporal.gsql](../schemas/mule_temporal.gsql) in schema `mule_temporal`, eight vertex tables, seven association tables with discriminated half-open tenures and twelve payment-participation tables, with payments and association changes in one chronological sequence. Zelle payments appear only in `Zelle_Transfer`, others only in `Payment_Transaction`. Account carries MulePatternLearner's fifteen-column label contract, every label masked and none marked known. Tables, Zelle model, sequencing, supervision and loading: [mule_temporal.md](mule_temporal.md).

## `aml`

For TigerGraph AML_Schema_V1.

- Vertices: Customer, Account, Counterparty, Name, Address, Country, Watchlist, Device, Transaction, SAR, Bank, MinHash buckets (Name, Address, Street_Line1, City, State), Connected_Component.
- Edges: customer_has_account / account_has_primary_customer; send/receive_transaction (customer side), counterparty_send/receive_transaction (counterparty side), sent/received_transaction_to/from_counterparty (aggregated); uses_device; logged_from; customer/account/counterparty/bank/address has_name / has_address / associated_with_country; customer_matches_watchlist; references (SAR → Customer with role); sar_covers (SAR → Account with activity amount); beneficiary_bank / originator_bank; resolves_to (counterparty → customer soft link); MinHash bucket edges.
- Customer: `customer_type` and its derived demographic and occupation attributes show the end-of-window persona, as a CRM would. Status flips `active → closed` once account closure precedes the corpus end. AML corpora are full-world, with no membership row filter (declared). The onboarding date is a synthetic backdated derivation, not joinTs (a declared inconsistency; aligning them is registered).
- SARs: one per ring, filed 30 days after its last illicit transaction (BSA), and one per solo fraudster. Violation type from the dominant fraud channel: structuring → `structuring`, invoice → `suspicious_activity`, else `money_laundering`. Per-account `activity_amount` is in + out throughput, not a share.
- MinHash: byte-for-byte compatible with TigerGraph's reference `TokenBank.cpp` (Austin Appleby's MurmurHash2 on byte shingles, k = 3, with the reference's exact 101-element c1/c2 coefficient tables). Bucket IDs include the band (`{PREFIX}_{band}_{hash}`) so LSH bands stay independent. The reference C source is embedded as a comment for migration and validation.

## `aml-txn-edges`

The AML schema with a transaction-edge view instead of the aggregated `HAS_PAID` projection, plus derived account features for models that use both ledger and topology: PageRank, Louvain community ID, weakly connected component ID and size, shortest path to a mule, IP and device collision counts, in/out mule ratio, multi-hop mule count, betweenness, in/out degree, clustering coefficient. Customer persona and `active`/`closed` status match the AML export. It cannot run under the serverless `PL_FILE_ONLY=1` test mode.

## `card-fraud`

A transaction-fraud corpus for TigerGraph's TF_GNN_v3 GSQL schema, built for temporal graph networks (TGN) that score each transaction: payments carry timestamps and timestamped device and IP session edges, so the corpus replays as a continuous-time event stream. Its whole window must lie inside the 1990–2024 era ([era coverage](simulation.md#era-coverage)). 43 tables in schema `card_fraud` (prefix `cf_`):

- `Payment_Transaction`: `card_purchase` rows (credit-card purchases plus unauthorized-card and gift-card-scam fraud) and `merchant` rows (account-paid POS, read as debit card). Unauthorized rows use the victim's primary account and so derive a debit-card identity; treating them as credit-card activity needs fraud planning moved into the statement, payment and interest lifecycle, which closes first today. 8 loaded columns: `id` (`T<row_seq>`, 1:1 with `transactions`), timestamp, amount, `is_fraud`, unix time, merchant category, `use_chip`, `error`. Streamed during settlement with the `Card_Send_Transaction`, `Merchant_Receive_Transaction`, `Transaction_Uses_Device` and `Transaction_Uses_IP` edges.
- `Card` (registry credit cards, ≤1 per person; any other view source becomes the account's derived debit card), `Party` (canonical customer IDs; `created_at` is joinTs), `Party_Has_Card`. `Is_Merchant` links view-observed merchants (~42% of the catalogue, draw-free from world state; `merchant-ownership-2026-07`) to their beneficial owner; it asserts identity, not money flow.
- `Merchant`, `Merchant_Category`, `Merchant_Assigned` and a consistent `City`/`State`/`Zipcode` chain (`Has_City`/`Has_State`/`Has_Zip`, `Assigned_To`, `Located_In`) from the merchant's world location; the 71-US-city catalogue is a runnable placeholder, not Census-complete ZCTA or establishment data.
- PII vertices `Address`/`Phone`/`Email`/`IP`/`Device`/`ID`/`Full_Name`/`DOB` and non-infrastructure `Has_*` edges (TF_GNN_v3 marks the layer demo-only; PhantomLedger fills it). Since `attacker-infra-2026-07`, `Has_Device` and `Has_IP` are the institution's incomplete endpoint registry (~72% device, ~61% address coverage), not true ownership, so a missing edge is weak evidence ("not on file ⇒ fraud" precision 0.027 at 2.9× lift). Use them for structure and the timestamped edges for anything point-in-time.
- `Transaction_Uses_Device` and `Transaction_Uses_IP` link each payment to its session's device and IP with `edge_unix_time`, including exogenous attacker sessions. Score a transaction before adding its session edges to temporal memory.
- `Ground_Truth_Label` `(entity_type, entity_id, label)`: the investigative overlay, positives only, joinable 1:1 to vertex tables, one of the 43 tables but outside the feature graph (TF_GNN_v3 does not load it; no edge points to it).

Labels (card-fraud-realism-v2): `Card.is_fraud`, `Party.is_fraud`, `Device.is_blocked` and `IP.is_blocked` are full-window verdicts ("this card ever carried a flagged row") that would leak the answer, so they are written as 0; the columns stay because TF_GNN_v3 maps columns by position. The one supervised target is `Payment_Transaction.is_fraud`, observable at its own timestamp. Entity verdicts are in `cf_Ground_Truth_Label` for evaluation only.

Realism: a 10,000-person, 60-day audit found all 748 fraud rows at merchants with no legitimate card-view transaction, and every unauthorized-fraud row on a TEST-NET-2 attacker IP. Both are fixed: fraud card-rail destinations come from the legitimate acceptance catalogue through the same modality-conditioned distance-decay kernel, and attacker IPs from the shared sampler. Gates: `test_card_baselines` (merchant-ID-only baseline); [card_fraud_feature_contract.md](card_fraud_feature_contract.md) (readable columns, pinned by a truncation experiment in `test_card_point_in_time`); `test_card_prevalence` (per-year prevalence, channel, typology, amount and episode bands).

The corpus supports a point-in-time temporal GNN pipeline but is not yet a public calibrated online benchmark. Open: fraud-level calibration; unauthorized credit-card activity in the statement, payment and interest lifecycle; effective-dated card, device and residence lifecycles; era-varying fraud technology and rail mix; operationally delayed labels; an executable, tested TigerGraph GSQL, training and evaluation pipeline. Causal feature, split, metric and minimum-realism gates: [card_fraud_online_gnn.md](card_fraud_online_gnn.md).

Only loaded attributes are emitted, in TF_GNN_v3's loaded-attribute order. PageRank and community slots, engineered `Payment_Transaction` features and TF_GNN_v3's interaction, co-occurrence and community edges are in-graph TigerGraph query work; the production GSQL feature query and a full training and evaluation runner are not shipped yet. Export a table as CSV with `\copy card_fraud."cf_Payment_Transaction" TO 'Payment_Transaction.csv' WITH (FORMAT csv, HEADER true)`.

## References

- Rossi, Chamberlain, Frasca, Eynard, Monti & Bronstein 2020, "Temporal Graph Networks for Deep Learning on Dynamic Graphs," arXiv:2006.10637 (the continuous-time framing the card-fraud corpus feeds).
- Hashing: Austin Appleby, MurmurHash2 32-bit; TigerGraph DevLabs `TokenBank.cpp` MinHash reference (embedded as a comment in the AML MinHash module).
