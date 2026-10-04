# Temporal mule detection: Mule_Pattern_Learner

`--usecase mule-temporal` builds a synthetic temporal graph corpus for
account-level mule detection on the supplied TigerGraph 4.2.5 schema. Graph
data only: no feature query, training pipeline or model. The `mule-ml` export
and the simulation RNG are unchanged. Run size, label counts, Zelle coverage
and validation: [2024 research dataset](research/mule_temporal_2024_dataset.md).

```sh
PL_PG='dbname=phantomledger' make run ARGS="--usecase mule-temporal --start 2024-01-01 --days 366 --population 200000 --seed 42"
```

- Writes database `phantomledger` (shared with other use cases), schema
  `mule_temporal`, tables such as `"mt_Zelle_Transfer"`. No data files.
- Zelle activity, tokens and simulator labels come with the use case; no
  other switches.
- Columns are text; convert to the types in
  [`schemas/mule_temporal.gsql`](../schemas/mule_temporal.gsql). Booleans
  render `True`/`False`.
- The shared `transactions` table is a simulator audit product; never join it
  into model features.

## Tables and source mapping

27 tables: 8 vertex, 7 forward association and 12 forward participation
types. Headers follow the schema's attribute order, except Account's, which
follow the load order of MulePatternLearner's (MPL's) label contract
([Account-level mule supervision](#account-level-mule-supervision)). Edge
tables prepend `from_id,to_id`. TigerGraph creates the 19 reverse edge types
(`REVERSE_EDGE`); do not load reverse copies.

| Element | Source and meaning |
|---|---|
| `Party` | Account owners, separate from accounts. No fraud or mule flags. |
| `Account` | Registry accounts, external and ownerless included. Visible at bank enrollment if owned, else at first payment or the Zelle registration just before it. `account_type`: `deposit`, `credit`, `brokerage`, `gl` (bank income ledger: internal, ownerless, never Zelle), else `unknown`. Label columns are supervision, never features. |
| `Token` | One opaque `synthetic_handle` per participating deposit, family, business or landlord account (internal or external), registered at first observed Zelle use. Modeled, not a confirmed phone or email. |
| `Device`, `IP` | Enrolled or payment-observed endpoints. Shared, public and attacker endpoints share one opaque ID format. |
| `Address` | Tokenized normalized street, home area and country; no raw address. A relocation closes one tenure and opens the next. |
| `Payment_Transaction` | Every settled non-Zelle payment, simulator USD, no fraud labels. |
| `Zelle_Transfer` | One payment on the synthetic Zelle rail; never also a `Payment_Transaction`. |
| `Party_Owns_Account` | Enrollment to account closure. No reassignment. |
| `Party_Uses_Token`, `Token_Bound_To_Account` | First Zelle registration to owner closure; ownerless external bindings stay open. Recipient-only users can register. No token recycling or rebinding. |
| `Party_Uses_Device`, `Party_Uses_IP` | Institution-enrolled usages, clipped to membership. Exclusive end is source `lastSeen` (an inclusive day) plus one day. Session-only endpoints get no edge. |
| `Account_Uses_Device` | Inherited from the owner's enrolled device tenure (`source_system=synthetic_owner_device`), not from transaction counts. |
| `Party_Has_Address` | Initial and relocation tenures, clipped to membership. |
| `Transfer_*`, `Transaction_*` | Sender, receiver, observed device and IP per payment. Zelle token roles set when a binding exists; non-Zelle token roles empty. |

Other associations use `source_system=synthetic_registry`, confidence 1.0.
Duplicate, overlapping or adjacent evidence for a pair merges into one
tenure; a gap starts a new edge with a new discriminator. Missing endpoints
are omitted (no shared `unknown` vertex). An unregistered payment account is
an error.

## Zelle generation and simulator labels

The ledger has no Zelle field; the [researched 2024 scenario](research/zelle_model.md)
assigns it:

- 31.5163% sending-user propensity among banked consumer profiles.
- 55.6225% Zelle choice for an eligible electronic P2P payment by a sending-user profile.
- Rolling sending limits: consumer $3,500/24 hours and $20,000/30 days; business $15,000/24 hours and $60,000/30 days.
- External-bank consumers and eligible businesses send and receive;
  recipient-only enrollment is supported.
- A fixed behavioral scenario, not a historical reconstruction or a share of
  all payments. Denominators, checksums, proxies and the single-bank-policy
  limit are in the research note.
- Draws and rolling budgets are seeded apart from the simulation RNG and use
  no future events or fraud verdicts.

Rails: cash, checks, card, internal postings and tagged ACH keep their broad
rails; other non-Zelle rails are `unknown`. The channel is a broad access
category; typology names never enter the graph. Fee and interest postings
(card interest, card late fees, overdraft fees, overdraft line-of-credit
interest) run on the `internal`/`bank` rail from the charged account to the
bank income GL of their kind (`account_type` `gl`), with no device or IP.
Each GL touches a large share of charged accounts: drop or separate GL edges
rather than treat a GL as a neighbour.

Zelle fraud oracle: each Zelle payment's verdict arrives at the next sequence
value, same millisecond. `fraud_label` is 0 or 1, `label_known=True`, and the
availability sequence is strictly after the event's. It is an oracle, not
investigation latency or real adjudication, so it supports no
delayed-feedback claims.

All labels and their known and availability fields are supervision only.
Masking for sparse-supervision experiments belongs in training; the dataset
keeps ground truth.

## One chronological sequence

Association boundaries and settled payments share one monotonically
increasing positive 64-bit sequence (an ordering, not elapsed time). Within
one timestamp:

1. Association closures (first-use token closures, then scheduled), stable order.
2. Association openings, stable edge/pair order.
3. First-use Zelle token registrations, just before their payment.
4. Payments in settled-ledger order; each Zelle payment is followed by its
   reserved supervision-arrival slot.

- A new endpoint takes `first_seen_seq` from its opening, or from its payment
  if event-only.
- Timestamps are unsigned epoch milliseconds; the simulator resolves seconds.
- Entity metadata is written once, at first observation. No accumulated
  counts, whole-history endpoints, train/test flags or time encodings.
- Event IDs `T<row_seq>` link to the audit ledger. Entity IDs are
  domain-separated 128-bit BLAKE2b pseudonyms with no raw PII, IP or
  fraud-role prefix; not keyed production tokenization, not a security
  boundary.

Association traversal, reverse included:

```text
valid_from_seq <= seed_seq
AND (valid_to_seq == 0 OR seed_seq < valid_to_seq)
```

- Entities: `first_seen_seq <= seed_seq`. Historical events:
  `event_seq < seed_seq`; the seed's own participation edges are its
  declared context only. A historical event's message uses that event's
  cutoff. Never mask only the first hop of a whole-history neighborhood.
- The exclusive end is a visibility predicate, never a feature: it reveals
  the future.
- Tenures open at the export boundary have `valid_to_seq=0`; the window end
  detaches nothing. Scheduled changes in the quiet tail are processed through
  the window's exclusive end, with or without a payment.
- Rejected: payments outside the window, decreasing timestamps, invalid
  account references, negative or infinite amounts. A missing amount (NaN at
  the C++ adapter seam) becomes `0.0` with `amount_present=False`; a real
  zero stays present.
- Point edges repeat their event's sequence and timestamp.

## Account-level mule supervision

Account carries MPL's fifteen-column label contract in MPL's load order
(MPL's `docs/reference/labels.md`, "Loading accounts";
`contract.graph_schema.ACCOUNT_LOAD_COLUMNS`). The first six are the original
columns, unchanged; nine are appended:

```text
id,account_type,is_external,first_seen_seq,first_seen_ts_ms,is_mule,mule_label_known,is_mule_masked,pu_label,mule_label_effective_seq,mule_label_effective_ts_ms,mule_label_available_seq,mule_label_available_ts_ms,mule_ring_id,mule_label_source
```

| Column | Value |
|---|---|
| `is_mule` | 1 if assigned `entity::account::Flag::mule`, else 0 |
| `mule_label_known` | `False` everywhere ([why](#why-no-label-is-marked-known)) |
| `is_mule_masked` | `True` everywhere |
| `pu_label` | 0 everywhere |
| `mule_label_effective_seq`, `mule_label_effective_ts_ms` | Mule: `event_seq` and `event_ts_ms` of its first exported payment, sent or received, on a ring laundering channel (the `Fraud` channel group, used only by ring typologies), else its first observation. Other internal account: first observation (`first_seen_seq`, `first_seen_ts_ms`). External: 0, unspecified |
| `mule_label_available_seq`, `mule_label_available_ts_ms` | Equal to the effective clock: simulator truth is complete when it holds, like the Zelle oracle's |
| `mule_ring_id` | Mule: its home ring, the one that recruited it (whose members hold its owner); a mule another ring later added keeps its home ring, since the schema holds one ring per account. Otherwise -1, as for a mule in no ring (never generated) |
| `mule_label_source` | `phantomledger_role` for internal accounts; empty for external, which PostgreSQL's CSV `COPY` stores as NULL |

- Ring ids are the ring topology's index from 0, the same as other exporters'
  `ring_id` (transactions, chains, shell accounts, SARs); 0 is a ring. A
  laundering payment carries the ring that made it, possibly another of a
  multi-ring mule's rings.
- The effective clock reads a payment's channel, never its verdict or ring;
  the ring reads the topology, never a payment. Labels change no event, edge,
  rail, entity ID, temporal clock or other column; the first six columns are
  byte-identical to the six-column table's.
- Both clocks are positive for every internal account, never before its first
  observation, and availability is never before effectiveness, so they
  already meet the contract's rules for a known label.
- Label clocks are the only Account cells later activity sets, so Account rows
  are written when the export finishes, in first-observation order. A window
  ending before a mule's first laundering payment shows its first
  observation.
- These are simulator role labels, not adjudications; 0 means no mule role,
  not fraud-free. Only the designated primary account is positive: not the
  owner's other accounts, victims, non-mule organizers, shell accounts or
  counterparties of fraudulent payments.
- Mule roles are internal only. External accounts are 0 (closed world), so
  external mule prevalence is uncalibrated.
- The role is fixed at synthesis. Its effective clock is the first laundering
  payment exported, not recruitment; availability is the simulator's, not an
  investigation's. `first_seen_seq` is visibility, not discovery.
- Keep label columns out of features, neighbor messages and target-exposing
  sampling.

### Why no label is marked known

MPL's one-time reveal, `reveal_mule_labels` (MPL's
`gsql/queries/label_reveal.gsql`, `docs/explanation/label-reveal.md`),
decides which mules a bank would have found before each split's cutoff. It
reads `is_mule`, `is_external`, each mule's `first_seen_ts_ms` (start of its
monitoring hazard) and `first_seen_seq`, Zelle `fraud_label`, `label_known`
and `label_available_ts_ms`, the sequences and times of payments between
mules, and the scope's partitions. It reads no label clock, ring or source.

It guards on `mule_label_known` and `pu_label`: if any internal account is
known or has `pu_label` 1, it returns `already_revealed` and writes nothing
unless `force = TRUE`. MPL's preparation calls it without `force` and accepts
that, so known internal labels at load (as the contract's loading guidance
describes) would leave every mule masked and training with no positive. The
export therefore marks nothing known. The reveal then writes
`mule_label_known`, the mask, `pu_label`, both clocks (effective becomes the
first observation, a mule's availability its simulated discovery) and the
source of every internal account. It does not write `mule_ring_id`, so rings
survive.

- `validate_label_contract` straight after the load reports one
  `invalid_unknown` per mule (an unknown label must carry `is_mule` 0 and
  ring -1) and zero otherwise; after the reveal, when MPL checks it, all
  zero.
- If MPL's reveal ever separates source-supplied from revealed labels,
  setting `mule_label_known` true for internal accounts is the only change
  this export needs.

Temporal evaluation:

- Build each account seed's neighborhood strictly before the seed, use
  `is_mule` only as the target, and train only on labels available before
  the seed (in MPL, revealed positives and their discovery clocks).
- A production label feed needs its own validity and availability records in
  this contract.
- Report account-level precision, recall and PR-AUC apart from Zelle
  transfer-fraud metrics: different targets.
- Valid time is not known time: a correction backdated before a seed may
  arrive after it. Apart from label availability clocks, known-time fields
  are deferred; backtests on a corrected production snapshot need source
  arrival history or archived snapshots.

## Loading the corpus into TigerGraph

The Account vertex in [`schemas/mule_temporal.gsql`](../schemas/mule_temporal.gsql)
is MPL's (`gsql/schema/schema.gsql`) and stores `is_mule` last; the table has
it sixth. A loader must map all fifteen Account columns by position, `$0` to
`$4`, `$6` to `$14`, then `$5`, with MPL's label contract as the reference. A
graph loaded from the old six-column table has ring -1 for every mule (its
revealed labels kept) until reloaded.

After a regeneration:

1. Regenerate with `--usecase mule-temporal` and `PL_PG`; run
   `docs/research/validate_temporal_dataset.sql` and
   `docs/research/profile_temporal_dataset.sql`.
2. Clear the graph's data (keep the schema) and load all 27 tables from a
   fresh export with the TigerGraph loader.
3. Run MPL's `mule train` with a new `scope.id` (MPL's "Set up a graph"
   guide). Its first preparation creates the scope, runs the reveal, then
   `validate_label_contract`, which must report zero violations.

Account alone: MPL's `load_accounts` job (`gsql/schema/account_loading.gsql`)
reads a fifteen-column CSV by position under its `account_header`
(`HEADER="true"` skips the file's header row), so the column order must be
the table's. Export it and pass the file as `accounts` (REST++ streaming
takes rows without the header, per MPL's labels reference):

```text
\copy mule_temporal."mt_Account" TO 'Account.csv' WITH (FORMAT csv, HEADER true)
```

Lowercase the three flag columns: MPL asks for `true`/`false`, and
TigerGraph's reading of `True`/`False` is unchecked. The other 26 tables still
need the TigerGraph loader.

## DDL and verification

- The DDL is for a fresh `Mule_Pattern_Learner` graph; no graph was created
  or modified. No existing-graph migration: the old definition and data are
  not available here, and TigerGraph cannot alter an edge discriminator in
  place ([TigerGraph 4.2 schema changes](https://www.tigergraph.com/docs/gsql-ref/4.2/ddl-and-loading/modifying-a-graph-schema)).
  The supplied `migrations/temporal_valid_time.gsql` reference is not
  represented locally.
- `test_mule_temporal`: table shapes, disjoint event types, common clocks,
  endpoint integrity, repeated tenures, exact boundaries, first observation,
  label isolation, missing amounts, probability gates, sending-limit
  boundaries, external senders and recipients, rail exclusions, chunk
  independence, prefix-versus-full visibility. On a hand-built fixture, the
  Account contract: header, every label value, the home ring against a ring
  listing the mule first, ring 0, a mule in no ring, the required topology,
  the effective clock at the first laundering payment, and that flipping
  verdicts, moving the ring or relabeling a P2P payment as laundering moves
  only the expected cells.
- `test_mule_temporal_labels`, on a real world through the windowed engine
  (pop 600, 2019, ten rings): the other 26 tables and Account's first six
  columns pinned to the pre-change build; the schema's Account vertex storing
  columns at `$0` to `$4`, `$6` to `$14`, `$5`; each mule's home ring against
  the topology and its laundering payments' ring ids; every clock against an
  independent reading; MPL's reveal guard and contract counts before and
  after the reveal.
- The GSQL has not run on the target 4.2.5 instance; server compilation is
  unverified.
