# Temporal mule detection: Mule_Pattern_Learner

`--usecase mule-temporal` creates a synthetic temporal graph corpus for
account-level mule-detection research. It supports the supplied TigerGraph
4.2.5 schema without changing the existing `mule-ml` export or simulation RNG.
It supplies graph data, not a feature query, training pipeline or model.

See the [2024 research dataset](research/mule_temporal_2024_dataset.md) for the run size, label counts, external-bank Zelle coverage and validation results.

```sh
PL_PG='dbname=phantomledger' make run ARGS="--usecase mule-temporal --start 2024-01-01 --days 366 --population 200000 --seed 42"
```

The PostgreSQL database is `phantomledger`, shared with the other use cases.
The temporal exporter writes to its `mule_temporal` schema, with table names
such as `"mt_Zelle_Transfer"`. Selecting the use case automatically includes
Zelle activity, tokens and simulator labels; no additional switches are needed. As with other exporters, columns are text staging
columns; consumers must convert them to the types in
[`schemas/mule_temporal.gsql`](../schemas/mule_temporal.gsql). Booleans render
as `True`/`False`. No data files are written. The shared `transactions` table
remains a simulator audit product and must not be joined into model features.

## Tables and source mapping

The export contains **27 tables**: eight vertex types, seven forward
association types and twelve forward participation types. Headers follow
the supplied schema's attribute order, except Account's, which follows the
load order of MulePatternLearner's label contract (see
[Account-level mule supervision](#account-level-mule-supervision)). Edge
tables prepend `from_id,to_id`.
TigerGraph creates the 19 reverse edge types through `REVERSE_EDGE`; do not
load additional reverse copies.

| Schema element | Source and interpretation |
|---|---|
| `Party` | Modeled account owners, separate from accounts; no fraud or mule flags. |
| `Account` | Registry accounts, including external or ownerless counterparties. Owned accounts become visible on bank enrollment; other accounts on their first payment or immediately preceding Zelle registration. Account type uses only stable product roles: `deposit`, `credit`, `brokerage`, `gl` for the bank's own income ledgers, otherwise `unknown`. A `gl` account is internal (`is_external` False), ownerless and never a Zelle endpoint. The fifteen columns are MulePatternLearner's Account label contract in its load order: the six feature and clock columns, `is_mule` (integer supervision from the explicit simulator account role), then the mask, PU label, effective and availability clocks, ring and source of that label. Every label column is supervision, never a feature. |
| `Token` | One opaque `synthetic_handle` per participating internal or external deposit/family/business/landlord account, registered at first observed Zelle use. This is a modeled network handle, not a source-confirmed phone/email registration. |
| `Device`, `IP` | Enrolled endpoints or endpoints actually observed in payments. Shared, public and attacker endpoints use the same opaque ID format. |
| `Address` | Tokenized normalized street, home area and country. The existing relocation schedule closes the previous tenure and opens a new one. No raw address is exported. |
| `Payment_Transaction` | Every settled payment not assigned to Zelle. Amounts are simulator USD. No fraud labels are copied into this vertex. |
| `Zelle_Transfer` | One individual payment assigned to the synthetic Zelle rail. Never also a `Payment_Transaction`. |
| `Party_Owns_Account` | Owner enrollment through modeled account closure. No ownership reassignment feed exists. |
| `Party_Uses_Token`, `Token_Bound_To_Account` | First observed Zelle registration through owner closure; ownerless external bindings remain open. Recipient-only users can register. Token recycling/rebinding is not currently synthesized. |
| `Party_Uses_Device`, `Party_Uses_IP` | Only institution-enrolled usages, clipped to membership. Source `lastSeen` is an inclusive day; adding one day gives the exclusive end. Session-only endpoints do not gain enrollment edges. |
| `Account_Uses_Device` | Modeled authorization inherited from the owner's enrolled device tenure, identified by `source_system=synthetic_owner_device`. It is not inferred from transaction counts. |
| `Party_Has_Address` | Initial address and subsequent relocation tenures, clipped to membership. |
| `Transfer_*`, `Transaction_*` | Explicit sender, receiver, observed device and observed IP for each individual payment. Zelle token roles are populated when a modeled binding exists. Non-Zelle token roles are empty because their source has no token participation. |

Other association rows use `source_system=synthetic_registry` and confidence
1.0 for the modeled fact. Exact duplicates and overlapping/adjacent evidence
for a pair are coalesced into a continuous tenure; a gap creates a distinct
edge with a new discriminator. Missing endpoints are omitted, not represented
by shared `unknown` vertices. An unregistered payment account is an error.

## Zelle generation and simulator labels

The underlying ledger has semantic channels, not an explicit Zelle field.
The temporal use case uses the [researched 2024 scenario](research/zelle_model.md):

- 31.5163% sending-user propensity among banked consumer profiles.
- 55.6225% conditional Zelle choice for an eligible electronic P2P payment by a sending-user profile.
- Published rolling bank sending limits: consumer $3,500/24 hours and $20,000/30 days; business $15,000/24 hours and $60,000/30 days.

External-bank consumers and eligible businesses can send and receive Zelle.
Recipient-only enrollment is supported; external status never disqualifies
an account by itself. The research document records exact denominators,
source checksums, business/domestic-account proxies and the limitations of
using a single bank-policy profile. This is a fixed behavioral scenario,
not a historical reconstruction or a global percentage of all payments.

Cash, checks, card activity, internal postings and explicitly tagged ACH
retain their broad rails. Fee and interest postings (card interest, card late
fees, overdraft fees, overdraft line-of-credit interest) are payments on the
`internal`/`bank` rail from the charged account to the bank's income GL of
their kind, an `Account` with `account_type` `gl`, and carry no device or IP.
Each GL collects postings from a large share of the charged accounts, so a
consumer modelling customer relationships should drop or separate those edges
by account type rather than treat a GL as a neighbour. Other non-Zelle rails remain `unknown`. The
exported channel is a broad access category; raw fraud/camouflage typology
names never enter the feature graph. Seeded draws and rolling budgets are
isolated from the simulation RNG and use no future events or fraud verdicts.

The temporal exporter exposes the simulator's
per-payment fraud verdict immediately **after** each Zelle seed. The oracle
arrival consumes the next sequence value at the same millisecond timestamp.
`fraud_label` is 0 or 1, `label_known=True`, and the availability sequence is
strictly greater than the event sequence. This deliberately models an oracle,
not investigation latency or a real fraud adjudication feed. It cannot
support claims about delayed-feedback model performance.

All labels and their known/availability fields are supervision-only. Label
masking for sparse-supervision experiments belongs in the training pipeline;
it does not remove ground truth from the generated dataset.

## One chronological sequence

The exporter merges scheduled association boundaries with the chronological
settled payment stream and allocates monotonically increasing 64-bit positive
sequence numbers. Sequence is an ordering coordinate, not elapsed time.

At the same timestamp the order is:

1. Association closures (first-use token closures first, then scheduled closures), in stable order.
2. Association openings, in stable edge/pair order.
3. First-use Zelle token registrations, immediately before the corresponding payment.
4. Payments, in the supplied settled-ledger order; a Zelle payment is followed
   by its reserved supervision-arrival slot.

An opening's endpoints get `first_seen_seq` equal to the opening sequence if
they are new. Event-only endpoints get the payment's sequence. Millisecond
timestamps use unsigned epoch milliseconds; the simulator has second-level
time resolution. Entity metadata is emitted once, on first observation,
and is never rewritten from later activity. There are no accumulated counts,
canonical whole-history endpoints, train/test flags or time encodings.

`T<row_seq>` event IDs retain a link to the audit ledger. Entity IDs are
domain-separated 128-bit BLAKE2b pseudonyms of canonical synthetic identities;
their strings contain no raw PII, IP address or generator fraud-role prefix.
These pseudonyms are not keyed production tokenization and should not be used
as a security boundary for real customer identifiers.

All association traversal, including reverse traversal, uses exactly:

```text
valid_from_seq <= seed_seq
AND (valid_to_seq == 0 OR seed_seq < valid_to_seq)
```

Filter entity existence by `first_seen_seq <= seed_seq`. Historical event
context requires `event_seq < seed_seq`; the seed's participation edges are
available only as the current payment's declared context. If computing a
message for a historical event, use that historical event's own cutoff.
Never take a whole-history neighborhood and mask only the first hop.

The exclusive end is consumed as a visibility predicate, not as a feature:
an old tenure's eventual end reveals the future if embedded directly.
Tenures still active at the export boundary have `valid_to_seq=0`. Ending
the observation window does not create an artificial detachment. Scheduled
changes in the quiet tail are processed through the window's exclusive end;
they do not need a payment at their timestamp to exist.

Payments outside the window, decreasing timestamps, invalid account references,
negative or infinite amounts are rejected. A missing amount (NaN at the C++
adapter seam) becomes `0.0` with `amount_present=False`; a real zero remains
present. Point edges repeat their parent event's exact sequence and timestamp.

## Account-level mule supervision

The Account table carries the fifteen columns of MulePatternLearner's
(MPL's) Account label contract, in the order its loader reads them (MPL's
`docs/reference/labels.md`, "Loading accounts", and
`contract.graph_schema.ACCOUNT_LOAD_COLUMNS`):

```text
id,account_type,is_external,first_seen_seq,first_seen_ts_ms,is_mule,mule_label_known,is_mule_masked,pu_label,mule_label_effective_seq,mule_label_effective_ts_ms,mule_label_available_seq,mule_label_available_ts_ms,mule_ring_id,mule_label_source
```

The first six are the columns the table always had, unchanged; the nine
after them are appended. The exporter writes:

| Column | Value |
|---|---|
| `is_mule` | **1** for an account explicitly assigned `entity::account::Flag::mule`, **0** for every other account |
| `mule_label_known` | `False` for every account (see [why no label is known](#why-no-label-is-marked-known)) |
| `is_mule_masked` | `True` for every account |
| `pu_label` | 0 for every account |
| `mule_label_effective_seq`, `mule_label_effective_ts_ms` | For a mule, its first simulated mule activity: the first exported payment it sends or receives on a ring laundering channel (the `Fraud` channel group, which only the ring typologies use), in that payment's `event_seq` and `event_ts_ms`. A mule with no such payment in the export gets its first observation. Any other internal account gets its first observation (`first_seen_seq`, `first_seen_ts_ms`), its creation as far as the export can see. An external account gets 0, unspecified |
| `mule_label_available_seq`, `mule_label_available_ts_ms` | The effective clock: the simulator's truth is complete the moment it holds, as the Zelle oracle's is at its payment |
| `mule_ring_id` | For a mule, its home ring: the ring that recruited it, the one whose members hold its owner. A mule that another ring later took on as an extra mule keeps its home ring, because the schema holds one ring per account. -1 for every other account, and for a mule in no ring, which the generator never makes |
| `mule_label_source` | `phantomledger_role` for every internal account; empty for an external account, which PostgreSQL's CSV `COPY` stores as NULL |

Ring ids are the ring topology's index from 0, the same ids the other
exporters write as `ring_id` (transactions, chains, shell accounts, SARs),
so `mule_ring_id = 0` is a ring. A laundering payment carries the ring
that made it, which for a mule in several rings may be one of its other
rings. The effective clock reads the payment's channel, never its fraud
verdict or ring id, and the ring reads the topology, never a payment: the
label columns change no event, edge, rail, entity ID or other column, and
the six columns before them are byte-identical to the six-column table's.
Both clocks are positive for every internal account, never precede its
first observation, and availability is never before effectiveness, so
they already meet the contract's rules for a known label.

`is_mule` and the ring are simulator role labels, not real-world
adjudications. Zero means not assigned the modeled mule-account role; it
does not mean free of every kind of fraud.

The generator assigns the mule role to the designated primary account.
Other accounts belonging to the same person are not automatically relabeled.
Victims, non-mule fraud organizers, shell accounts and innocent counterparties
are not made positive just because a payment has a fraud verdict. The current
simulation assigns mule roles to internal customer accounts; external accounts
can use Zelle but have no separately generated external mule-role truth and
are zero under this closed-world role definition. External mule prevalence
is therefore not calibrated by this dataset.

The role is fixed at synthesis time. Its effective clock is the first
laundering payment the export holds, not the date of recruitment, and its
availability is the simulator's, not a historical investigation's
discovery time. The label columns must be excluded from graph features,
neighbor messages and sampling criteria that would expose targets.
`first_seen_seq` is account visibility, not label discovery time.
Changing mule-role labels does not change any event, edge, rail
assignment, entity ID or temporal clock.

The label clocks are the only Account cells that later activity can set.
Every Account row is written when the export finishes, in first-observation
order, because a mule's first laundering payment can come after its first
observation. An export of a shorter window that ends before that payment
shows the mule's first observation instead.

### Why no label is marked known

MPL's training labels come from its one-time label reveal,
`reveal_mule_labels` (MPL's `gsql/queries/label_reveal.gsql` and
`docs/explanation/label-reveal.md`). It decides which mules a bank would
have discovered before each split's cutoff from `is_mule`, `is_external`,
each mule's `first_seen_ts_ms` (the start of its monitoring hazard) and
`first_seen_seq`, the Zelle transfers' `fraud_label`, `label_known` and
`label_available_ts_ms`, the sequences and times of the payments between
mules, and the scope's partitions. It reads none of the label clocks, the
ring or the source, so those cannot change which mules it reveals.

It does read `mule_label_known` and `pu_label`, as a guard: if any internal
account has a known label or `pu_label` 1, it returns `already_revealed`
and writes nothing unless called with `force = TRUE`, and MPL's preparation
calls it without `force` and accepts that answer. A load whose internal
labels were known, as the contract's loading guidance describes, would
therefore leave every mule masked and training with no revealed positive.
So the export marks no label known and reveals none: the graph loads as
MPL's reveal expects a fresh PhantomLedger load to, and the reveal then
writes `mule_label_known`, the mask, `pu_label`, both clocks and the source
of every internal account. It does not write `mule_ring_id`, so the rings
survive it. It replaces the effective clocks with the first
observation and a mule's availability with its simulated discovery.

Two consequences follow. MPL's `validate_label_contract` run straight after
the load counts one `invalid_unknown` per mule, because an unknown label
must carry `is_mule` 0 and ring -1; every other count is zero, and after the
reveal every count is zero, which is when MPL's preparation checks it. And
if MPL's reveal ever tells a source-supplied label from a revealed one,
`mule_label_known` true for internal accounts is the only change this
export needs, since its clocks already meet the contract's rules for a
known label.

For temporal evaluation, choose account seed times and compute neighborhoods
strictly before each seed; use `is_mule` only as the account classification
target, and train only on labels available before the seed (in MPL, the
revealed positives and their discovery clocks). A production label feed
needs its own validity and availability records in the same contract.
Report account-level precision/recall and PR-AUC separately from the existing
Zelle transfer-fraud metrics. Those are different prediction targets.

Valid time is not known time: a correction backdated to before a seed may
have arrived afterward. Apart from the supervision labels' availability
clocks, the schema defers known-time fields. Historical backtests on a
corrected production snapshot require source arrival history or archived
snapshots before they can claim freedom from that form of leakage.

## Loading the Account table into MulePatternLearner

The Account vertex in [`schemas/mule_temporal.gsql`](../schemas/mule_temporal.gsql)
is MPL's (its `gsql/schema/schema.gsql`), which stores `is_mule` last; the
table keeps it sixth, in the contract's load order. Load the table with
MPL's `load_accounts` job, not a positional one:

1. Create a fresh graph with MPL's `gsql/schema/schema.gsql`. It is this
   repository's DDL plus attributes MPL's own queries write on the payment
   vertices; the Account vertex is identical.
2. Create the job: `gsql gsql/schema/account_loading.gsql` in MPL. Its
   `account_header` names the fifteen columns in the table's order and maps
   each to its attribute by name.
3. Copy `mule_temporal."mt_Account"` to a CSV with its header, for example
   `\copy mule_temporal."mt_Account" TO 'Account.csv' WITH (FORMAT csv, HEADER true)`,
   and run `load_accounts` with it as the `accounts` file. The REST++
   streaming interface takes the data rows without the header. The table
   renders booleans `True` and `False`, as it renders `is_external`; MPL's
   labels reference asks for lowercase `true` and `false`, so convert the
   three flag columns the way your push converts `is_external` if your
   loader needs it.
4. Load the other 26 tables with your existing jobs. They are unchanged.
5. Run MPL's `mule train`. Its first preparation creates the scope, runs
   the reveal, which finds no known label and writes the internal labels,
   then runs `validate_label_contract`, which must report zero violations.
   Give the run a new `scope.id` after a reload, as MPL's "Set up a graph"
   guide says, so it prepares a new dataset on the new data.

The owner's run order after a regeneration: regenerate the data, clear the
graph, reload every table with the Account table through `load_accounts`,
then prepare and train. A graph loaded before this change carries ring -1
for every mule and keeps its revealed labels; reloading is what brings the
rings in.

## DDL and verification

The DDL is for a fresh `Mule_Pattern_Learner` graph only. No graph has been
created or modified by this implementation. No existing-graph migration is
provided: the old graph definition and its data are not available here, and
TigerGraph does not support altering an edge discriminator in place. See
[TigerGraph 4.2 schema changes](https://www.tigergraph.com/docs/gsql-ref/4.2/ddl-and-loading/modifying-a-graph-schema).
The supplied reference to `migrations/temporal_valid_time.gsql` has therefore
not been represented as an existing local migration.

`test_mule_temporal` checks table shapes, disjoint event types, common clocks,
endpoint integrity, repeated tenures, exact boundaries, first observation,
label isolation, missing amounts, researched probability gates, sending-limit boundaries, external senders/recipients, rail exclusions, chunk independence and prefix-vs-full
historical visibility. It also checks the Account label contract on a
hand-built fixture: the fifteen-column header, the values of every label
column, the home ring against a ring that lists the mule first, ring 0, a
mule in no ring, the required topology, the effective clock at the first
laundering payment, and that flipping fraud verdicts, moving the ring or
relabeling a P2P payment as laundering moves only the Account cells it
should. `test_mule_temporal_labels` checks the same contract on a real
world through the windowed engine (pop 600, 2019, ten rings): the other 26
tables and the Account table's first six columns pinned on the pre-change
build, the home ring of every mule against the topology and the ring ids
of its laundering payments, every clock against an independent reading of
the rows, and MPL's reveal guard and contract counts replayed before and
after the reveal. The GSQL still needs execution on the target 4.2.5
instance before claiming server compilation compatibility.
