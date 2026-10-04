# The card-fraud feature contract (point-in-time gate)

Status: in force, pinned by `tests/test_card_point_in_time.cpp` (online-GNN
minimum-realism gate 4 in the tests). Amended 2026-07-27 by the online-graph
integrity round (membership-filtered payments, role-neutral device IDs,
observed session endpoint vertices, timestamped transaction→device/IP edges)
and by use-chip-causal-2026-07 (round 8: `use_chip` became causal and moved to
feature-safe, pinned by `tests/test_card_use_chip.cpp`).

This says, for every column exported under `--usecase card-fraud`, whether a
model may read it. The rule:

> A feature may only depend on information that existed at its own row's
> timestamp.

A violating column is a leak, labelled or not: it carries the future in
training and cannot be reproduced at serving. The rule is necessary, not
sufficient: round 6 found a column observable at its timestamp that also read
out the generator's role assignment. Both tests must pass.

## How the contract is enforced

`tests/test_card_point_in_time.cpp` exports one world twice through the
production path: all settled rows, and only rows before a mid-window cutoff
`T` (what a model scoring at `T` could see). Every feature-safe value in the
score-time export must be byte-identical in the full export.

| Class | Tables | Requirement |
|---|---|---|
| Stream prefix | `Payment_Transaction`, `Card_Send_Transaction`, `Merchant_Receive_Transaction`, `Transaction_Uses_Device`, `Transaction_Uses_IP` | score-time lines are a byte-exact prefix of the full export |
| Identical | `Party`, static PII tables and associations other than the `Device`/`IP` vertex sets, `Has_Device`/`Has_IP`, `Has_Std_City`/`Has_Std_Postcode`/`Has_Std_State`, `Merchant_Category` | world-derived; independent of the transaction prefix |
| Growing set | `Card`, `Device`, `IP`, `Party_Has_Card`, `Merchant`, `Merchant_Assigned`, the geo chain | rows may be added; a row in both exports is identical |

It would have failed the pre-v2 `Card.is_fraud`, which flipped 0 → 1 when a
future flagged row arrived. It cannot catch the round-6 defect class: a
generator-role artifact (such as a fraud-device identifier) is stable in the
row. Only reading the render path finds those.

## Feature-safe

### `cf_Payment_Transaction` (the row under judgement)

| Column | Note |
|---|---|
| `id` | `T<row_seq>`, joins `public.transactions` 1:1. row_seq is monotone in time: never feed it as a number. |
| `transaction_time`, `unix_time` | the score-time anchor |
| `amount` | era-realized dollars (macro-history H1: a 1991 ticket is ~0.53× a 2019 one). Normalize within era, or the model learns the calendar. |
| `mer_cat` | the destination merchant's modelled category |
| `use_chip` | causal since round 8 (was a content hash). `Online Transaction` exactly when the destination is a geography-free acceptance endpoint (the catalog `Footprint::online` population both legitimate selection and the fraud rails draw card-not-present picks from, or a non-catalog remote biller). Physical outlets split `Chip`/`Swipe` by the dated US EMV terminal mix (zero before 2012, ~0.65 in 2019, frozen ~0.90 outside coverage). Carries the modelled CNP-majority fraud signal on purpose, like distance-from-home. Limits: chip/swipe is a presentation-layer mix, not per-card or per-terminal adoption; the legitimate CNP share is era-flat (registered debt, `payments.cpp`); entry mode only, not authorization outcome. |

### Identifiers, structure and geography

`card_number`, `Merchant.id`, `Party.id`, `Party_Has_Card`, `Is_Merchant`,
`Merchant_Assigned`, `Has_State`, `Has_City`, `Has_Zip`,
`Merchant_Location.*`, `Assigned_To`, `Located_In`, `City.*`, `State.id`,
`Zipcode.*`, `Merchant_Category.category`: identifiers and static world
geography, fixed before any transaction settles (merchant location is
`entity::merchant::Record.location`; City population is the catalogue value).

- `Is_Merchant` (populated since merchant-ownership-2026-07) is the merchant →
  proprietor register, ~45% coverage. Membership hashes the merchant key
  alone, so it cannot encode footprint, size or geography; fraud lift on
  "destination has an owner edge" is 0.95-1.12x across seeds.
- `card_number` is a categorical join key: leading `C`/`D` is an exporter type
  tag, trailing `-G<n>` a reissue generation.
- Unauthorized positives are debit-backed until credit fraud joins statement
  servicing; report an instrument-type-only baseline until then.

#### Card reissue generations (`card-churn-2026-07`)

One `cf_Card` vertex and one `cf_Party_Has_Card` edge per observed generation;
`card_number` gains `-G<n>` after the first. Boundaries follow a draw-free
schedule keyed on card and window: 36-60-month validity, a 0.07/yr
loss/theft/damage hazard, and a 2015-2017 EMV migration wave. Each row carries
the generation live at its timestamp; unobserved generations are not written.

Cards per party is safe, with limits. Fraud-driven reissue (~27% of real
reissuance, the largest cause after expiry) is not modelled, because it is
downstream of the compromise and the only fraud signal at export time is the
withheld full-window verdict. So:

1. Exported reissue rates are below production; do not calibrate issuer
   reissuance on them.
2. Holding more than one card number carries no systematic fraud signal here
   (lift 1.012x / 0.990x across two legs, `test_card_endpoint_graph` sub-gate
   H), though in reality it carries a strong one. The residual correlation is
   activity: a heavily used card straddles more boundaries and draws more
   attacks. A model leaning on card count will underperform in production
   relative to this corpus, not overperform.

#### Coordinates (`merchant-coordinates-2026-07`)

`cf_Merchant_Location(merchant_id, lat, lon)` and `lat`/`lon` on `cf_Zipcode`
and `cf_City`: decimal degrees from catalogue integer microdegrees. Safe;
static world state fixed in G1c. Limits, the first binding hardest:

- Area centroids (`Record.location` is a `GeoAreaId`), so co-located merchants
  share a point: 149 merchants on 48 centroids in the e2e window. Features
  assuming distinct outlets (nearest-neighbour merchant, intra-ZIP
  clustering, "same building") read resolution the generator lacks.
- Row presence is the `has_coordinates` mask. Online merchants and non-catalog
  fraud billers are absent, not zeroed, so a `LEFT JOIN` gives NULL, not the
  Gulf of Guinea. Absence tracks card-not-present, the same split as
  `Has_City`/`Has_Zip` absence and `use_chip`.
- Distance-from-home is computable (next section).

Gate: the coordinate block in `tests/test_pipeline_e2e.cpp` checks the US
bounding box, byte-identity with the merchant's `cf_Zipcode` row, coverage
equal to `cf_Has_Zip`, and more than one distinct point. A lat/lon swap reds
149/149 on bounds and agreement; a constant point reds bounds and the
distinct-point floor.

#### Cardholder-to-merchant distance (`party-geography-2026-07`)

`cf_Has_Std_City(party_id, city_id, since_unix_time)`,
`cf_Has_Std_Postcode(party_id, zipcode_id, since_unix_time)` and
`cf_Has_Std_State(party_id, state_id, since_unix_time)` give each party's
home-area history on the same City/Zipcode/State vertices merchants use. Safe
and prefix-invariant (relocation is fixed before the fold).

One row per tenure since `relocation-2026-07`. Use the row with the greatest
`since_unix_time` ≤ `transaction.unix_time`. An undated join silently returns
a plausible distance to the wrong home for every mover. Non-movers have one
row stamped at window start. `Street_Address.lat/lon` in TigerGraph is the
current home only; never use it for point-in-time distance.

| End | Path |
|---|---|
| cardholder | `Party` → `Party_Has_Std_Postcode` → `Zipcode.lat/lon` |
| merchant | `Merchant` → `Merchant_Has_Location` → `Merchant_Location.lat/lon` |

The selection kernel is built on this axis
(`popularity * exp(-distanceMiles / scaleMiles(homeArea))`) and the fraud rails
read the same distance, so use it, in miles (`area.hpp`, owner directive
2026-07-21). Limits:

- Both ends are centroids: intra-area distance is exactly zero ("same postal
  area"), and co-located merchants are equidistant from everyone.
- Coresidents share a home area (household lane), so distance does not
  discriminate within a household.
- Foreign-domiciled parties (~4%; production mix `LocaleMix::usBankDefault`)
  really are thousands of miles from US merchants. Do not clip or winsorize
  them: an issuer
  most wants to reason about them, and dropping them turns "has geography"
  into a residency flag.
- Relocation runs at 0.1047 moves/person-year over a 20-year window (Census
  CPS ASEC), declining across the era. Registered limits: static household
  composition (nobody moves out); moves stay in the origin country; no age or
  tenure tilt; re-occupying a previous area collapses to one edge stamped at
  the earliest occupancy, since TigerGraph keys an edge by (from, to) with no
  discriminator.

Gate: the distance block in `tests/test_pipeline_e2e.cpp` checks distinct-party
coverage against the Party vertex count (row equality, dropped in
`relocation-2026-07`, passed vacuously on its seven-day window, where nobody
moves), equal row counts across the three tables, every `since_unix_time`
inside the window, referential integrity both ways (party areas the merchant
loop missed are unioned into the vertex tables), a
`Party → Zipcode → coordinate` walk with no unreachable party, more than one
home point, and at least one foreign home centroid. That last check
exists because every gate harness ran `LocaleMix::usOnly()` until this round;
`test_pipeline_e2e` now runs the production mix. Dropping foreign parties reds
coverage at 94/100 and the foreign check at 0.

### `cf_Party` attributes

| Column | Note |
|---|---|
| `gender` | content-keyed even split, no mechanism; noise |
| `dob`, `party_type`, `name` | static identity |
| `created_at` | safe since H3: the membership `joinTs`, written through the one membership path. The older audit prohibition predates H3 and is lifted. |

### The PII and investigative layer

`Address`, `Phone`, `Email`, `IP.id`, `Device.id`, `ID`, `Full_Name`, `DOB`,
and the timestamped `Transaction_Uses_Device` / `Transaction_Uses_IP` edges.
Static world facts, so point-in-time safe.

- Shared devices and IPs are intended signal: rings share infrastructure.
- Cross-victim endpoint reuse is the card-fraud graph signal (since
  attacker-infra-2026-07; before, one endpoint per compromise meant Device/IP
  passed no message between victims). Campaign-scoped infrastructure with a
  heavy-tailed case load: 74-82% of attacker devices seen by more than one
  victim, mean 5-9, max 37-40. Endpoint degree and history are first-class
  features.
- For online scoring use `Transaction_Uses_Device` / `Transaction_Uses_IP`
  (session endpoint plus `edge_unix_time`). Score first, then append the edge
  to memory.
- `Has_IP` / `Has_Device` are the institution's endpoint registry,
  deliberately incomplete (~72% device, ~61% address). They mean "on file",
  not "owns"; absence is weak evidence, as in production. Whole-window with no
  interval: use them for structure (Party from an endpoint), not as dated
  facts.
- `Device.id` is a vertex identity over the world roster plus endpoints seen
  in card-view rows, so attacker endpoints cannot be spotted by absence.
  `is_blocked` is written 0; the verdict is in the quarantined overlay.
- All device identities (personal, legitimate-shared, ring, attacker) share
  one fixed-width opaque `D…` namespace: a categorical key, never a number.
- `Email_Minhash` / `Has_Email_Minhash` (added 2026-07-27, owner request) are
  safe structure: LSH band buckets (shared `common/minhash`, `EMH` prefix,
  b=10 bands × r=1, 3-gram shingles) derived only from the already exported
  `Email`/`Has_Email`. No new channel, time axis or label content; only
  customers have emails (attacker rings have none), so buckets cannot encode
  role. Shared bucket means similar text, the intended signal. Opaque
  categorical keys.

### `public.transactions` (the raw ledger), per row

`src_acct`, `dst_acct`, `amount`, `ts`, `channel`, `ip_address`, `device_id`:
observable at the row's timestamp.

- `ip_address`: attacker addresses come from `network::randomIpv4`, the same
  generator as legitimate sessions, so no prefix separates them (they used to
  be TEST-NET-2).
- `device_id`: categorical identity for retrieving prior state only; never
  parse, bucket or use its magnitude. The old `FD…` role namespace was closed
  by `exporter/common/render.hpp`.

## The target (never an input)

`cf_Payment_Transaction.is_fraud` and its raw twin
`public.transactions.is_fraud`: the only supervised label in the feature
graph, a per-row fact observable at the row's timestamp.

### The label is censored on authorization attempts: exclude them from the loss

A non-empty `error` marks an authorization attempt, not a settled purchase. Its
`is_fraud` is 0 because no label exists; treating it as a negative poisons
training.

| `error` | Population | Share of payment table |
|---|---|---|
| empty / NULL | settled purchase, the only rows with a real label | ~94% |
| `Insufficient Balance` | the replay's own funding declines | ~0.1% |
| `Do Not Honor` | card-testing probes (`infra/enumeration.hpp`) | ~0.03% |
| anything else | non-funding declines (bad PIN, bad CVV, ...) | ~5.6% |

Production labels come from disputes and chargebacks; a declined authorization
never settles, so it is never labelled. This is censored feedback,
"endogenously missing for declined transactions" (Fundamental Limits of Fraud
Detection in Card Payment Networks, arXiv 2605.27557, accessed 2026-08-07).
Some funding declines are fraud attempts (the unauthorized rail drains a
victim, so its later charges cannot fund), but labelling them 1 is equally
wrong, since production never learns that either. No production pipeline has
ground truth for these rows, so neither does this corpus.

Keep the nodes and mask them from the loss: they carry real `Card_Send` /
`Merchant_Receive` / `Uses_Device` / `Uses_IP` edges.

The TigerGraph loader does not load `error` (correctly: it describes the
authorization response, and scoring happens at the request) but applies no
filter, so every attempt loads as an ordinary transaction with `is_fraud = 0`.
`error` is on every exported row; the fix belongs in the loader: filter these
rows, or expose a boolean derived from `error` for loss masking. Like
`cf_Is_Merchant` (CLAUDE.md `merchant-ownership-2026-07`), this export
decision is invisible to the repository that depends on it, so it is stated
here as contract.

## Prohibited (leaks, ground truth, or both)

| Column | Why |
|---|---|
| `cf_Card.is_fraud`, `cf_Party.is_fraud`, `cf_Device.is_blocked`, `cf_IP.is_blocked` | full-window entity verdicts. Written as 0 since round 1, kept only for the TigerGraph loader's positional column mapping. Restoring them reopens the leak. |
| `cf_Ground_Truth_Label` (whole table) | quarantined overlay: the four withheld verdicts, positives only, future-dependent (`test_card_point_in_time` prints how much it moves across the cutoff). Evaluation only; the TigerGraph loader does not load it and no edge points at it. |
| `public.transactions.ring_id`, `.fraud_type` | the generator's ground truth; that table is the corpus, not the feature graph. Slice evaluations with them, never fit. |

`cf_Has_Device` / `cf_Has_IP` left this list in attacker-infra-2026-07. They
had been header-only because every customer endpoint had a Party owner and no
attacker endpoint did, making missing adjacency a role label. The generator
changed both sides: registry coverage is partial (`infra::enrollment`), and a
declared share of unauthorized fraud runs from the victim's own endpoint or
exits through a residential proxy. Residual: "endpoint not on file ⇒ fraud"
precision 0.027, lift 2.9x over base rate, a real weak feature (see the PII
layer).

## Use with care (safe, but not what they look like)

| Column | Catch |
|---|---|
| `cf_Payment_Transaction.error` | marks an authorization attempt (see the censored-label section): mask the loss with it, never feed it. It began as a 2% export-time content hash with no cause ("treat as noise or drop"), the remaining hash half of online-GNN gate 4 once `use_chip` became causal. |
| `cf_Party.gender` | content-keyed split, no mechanism |
| `cf_City.population` | 71-US-city runnable placeholder, not Census-complete. Usable as a feature; draw no demographic conclusions. |

## Splitting and evaluation

- Split temporally: train before `T`, evaluate after. A random split leaks
  both ways: the same card sits on both sides, and later rows inform earlier
  ones.
- Do not compare fraud rates across eras naively. H4 makes volume era-varying
  (a 1991 window runs at ~0.67× 2019 real consumption) while the budget
  `F = pL/(1−p)` rides the realized candidate count, so the rate is
  era-stable but counts are not. Round 3's prevalence suite pins this per
  year.
- Beat a baseline. `tests/test_card_baselines.cpp` gates the merchant-ID-only
  classifier at recall@precision≥0.90 < 0.25. A model that does not clear
  trivial baselines by a wide margin has not learned the graph.

## Changing this contract

Classify every new exported column here in the same round, covered by
`test_card_point_in_time`. A column that fits no class does not ship; silently
exported values are how the original four labels got in.

Reclassification is normal. Device identity went safe → prohibited when the
`FD…` namespace was found, and back to categorical-safe only after the
renderer became role-neutral and the endpoint-universe and prefix tests
shipped. `use_chip` went use with care → feature-safe only when round 8
replaced the hash with the acceptance-environment mechanism and its gate. A
point-in-time gate cannot see a stable generator-role artifact, so passing it
does not make a column safe to feed.
