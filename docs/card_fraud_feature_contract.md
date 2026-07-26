# The card-fraud feature contract (gate 4)

**Status: IN FORCE as of card-fraud-realism-v2 round 2. Pinned by
`tests/test_card_point_in_time.cpp`.**

This document answers one question for every column PhantomLedger
exports under `--usecase card-fraud`: **may a model read it as an
input?**

The rule behind every entry is a single one:

> A feature may only depend on information that existed at its own
> row's timestamp.

A column that violates it is a leak whether or not it is labelled as
one. At training time it silently carries the future; at serving time it
cannot be reproduced, so a model that learned from it has no deployable
counterpart. That is the difference between a corpus you can benchmark
on and a corpus that merely loads.

## How the contract is enforced

`tests/test_card_point_in_time.cpp` runs a **truncation experiment**:
one world, exported twice through the production code path — once over
every settled row, once over only the rows before a mid-window cutoff
`T`. The second export is exactly the information a model scoring at
`T` could have had. Every feature-safe value present in the score-time
export must be **byte-identical** in the full-window export.

Three enforcement classes, because the tables differ in kind:

| Class | Tables | Requirement |
|---|---|---|
| **Stream prefix** | `Payment_Transaction`, `Card_Send_Transaction`, `Merchant_Receive_Transaction` | the score-time export's lines must be a byte-exact PREFIX of the full-window export's |
| **Identical** | `Party`, the whole PII layer, `Merchant_Category` | world-derived; cannot depend on the transaction prefix at all |
| **Growing set** | `Card`, `Party_Has_Card`, `Merchant`, `Merchant_Assigned`, the geo chain | the row SET may grow as rows arrive; a row present in both must be identical |

The gate is not a tautology: it would have **failed** on the pre-v2
`Card.is_fraud` column, whose value flipped 0 → 1 as soon as a future
flagged row arrived.

## FEATURE-SAFE (a model may read these)

### `cf_Payment_Transaction` — the row under judgement

| Column | Note |
|---|---|
| `id` | `T<row_seq>`; joins `public.transactions` 1:1. An index, not a feature — row_seq is monotone in time, so **do not feed it as a number**. |
| `transaction_time`, `unix_time` | the score-time anchor itself |
| `amount` | era-realized dollars (macro-history H1: a 1991 ticket is ~0.53× a 2019 one). If you normalize amounts, normalize **within era**, or the model learns the calendar. |
| `mer_cat` | the destination merchant's modelled category |

### `cf_Card`, `cf_Merchant`, `cf_Party` and the structural edges

`card_number`, `Merchant.id`, `Party.id`, `Party_Has_Card`,
`Is_Merchant` (header-only), `Merchant_Assigned`, `Has_State`,
`Has_City`, `Has_Zip`, `Assigned_To`, `Located_In`, `City.*`,
`State.id`, `Zipcode.id`, `Merchant_Category.category`.

Identifiers and static world geography. Merchant location is
world-modelled (`entity::merchant::Record.location`), and City
population is the catalogue value — both fixed before any transaction
settles.

### `cf_Party` attributes

| Column | Note |
|---|---|
| `gender` | a content-keyed even split; **not a modelled attribute** — it carries no mechanism and should be treated as noise |
| `dob`, `party_type`, `name` | static identity |
| `created_at` | **SAFE since H3.** It is the membership `joinTs` — when the customer relationship began — written through the one membership construction path. The audit's old prohibition on this column is hereby **lifted**; it predates H3 and no longer describes the code. |

### The PII / investigative layer

`Address`, `Phone`, `Email`, `IP.id`, `Device.id`, `ID`, `Full_Name`,
`DOB` and their `Has_*` edges.

Static world facts, so point-in-time safe. Two things to understand
before using them:

- **Shared infrastructure is real signal.** Fraud rings deliberately
  share devices and IPs. A model that learns "these parties transact
  from one device" is learning modelled behavior, not an artifact —
  this is the intended graph signal.
- **They are NOT transaction-time edges.** `Has_IP` / `Has_Device`
  associate a party with infrastructure over the whole window; they do
  not say which device sent a given transaction. Per-row device and IP
  DO exist in `public.transactions` (`device_id`, `ip_address`) and are
  feature-safe there. Promoting them into the card graph as
  transaction-time edges is registered work, not done.

### `public.transactions` (the raw ledger), row-level

`src_acct`, `dst_acct`, `amount`, `ts`, `channel`, `device_id`,
`ip_address` — all safe, all observable at the row's own timestamp.

## THE TARGET (never an input)

`cf_Payment_Transaction.is_fraud` — and its raw twin
`public.transactions.is_fraud`.

This is the **only** supervised label inside the feature graph. It is a
per-row fact observable at that row's timestamp, which is exactly what
makes it a legitimate target.

## PROHIBITED (leaks, ground truth, or both)

| Column | Why |
|---|---|
| `cf_Card.is_fraud`, `cf_Party.is_fraud`, `cf_Device.is_blocked`, `cf_IP.is_blocked` | full-window entity verdicts. **Written as 0** since round 1 — the columns are retained only so TF_GNN_v3 loading jobs map positionally. Restoring their content re-opens the leak the arc exists to close. |
| `cf_Ground_Truth_Label` (whole table) | the quarantined investigative overlay: the four withheld verdicts, positives only. Future-dependent by construction — `test_card_point_in_time` prints how much it moves across the cutoff as standing evidence. **Evaluation only.** No TF_GNN_v3 job loads it and no edge points at it. |
| `public.transactions.ring_id`, `.fraud_type` | the generator's own ground truth. That table is the corpus, not the feature graph. Use them to slice an evaluation, never to fit. |

## USE WITH CARE (safe, but not what they look like)

| Column | The catch |
|---|---|
| `cf_Payment_Transaction.use_chip` | an **export-time content hash** of the row (Swipe .63 / Chip .26 / Online .11), for fraud and legitimate rows alike. It is deterministic in the row and carries no future, so it passes this contract — but it is **not causal**. The card-present/card-not-present modality that b-2 introduced drives DESTINATION SELECTION in generation and is *not* reflected in this column. Treat as noise or drop. |
| `cf_Payment_Transaction.error` | same mechanism, 2% incidence. Authorization attempts are not modelled, so this column has no cause behind it. |
| `cf_Party.gender` | content-keyed split, no modelled mechanism. |
| `cf_City.population` | 71-US-city runnable placeholder, not Census-complete. Fine as a feature; do not read demographic conclusions out of it. |

## Splitting and evaluation

- **Split temporally.** Train on rows before `T`, evaluate after `T`.
  A random split leaks in two directions at once: the same card appears
  on both sides, and later rows inform earlier ones.
- **Do not compare fraud rates across eras naively.** H4 makes activity
  volume era-varying (a 1991 window runs at ~0.67× the 2019 real
  consumption level) while the fraud budget `F = pL/(1−p)` rides the
  realized candidate count, so the RATE is era-stable but the COUNTS
  are not. Round 3's prevalence suite is what pins this per year.
- **Report against a baseline.** `tests/test_card_baselines.cpp` gates
  the merchant-ID-only classifier at recall@precision≥0.90 < 0.25. A
  model that does not clear a trivial baseline by a wide margin has not
  learned the graph.

## Changing this contract

Adding an exported column means classifying it here in the same round,
and `test_card_point_in_time` must cover it. A column that cannot be
placed in one of the classes above does not ship — silently exporting a
value that a model *might* read is how the original four labels got in.
