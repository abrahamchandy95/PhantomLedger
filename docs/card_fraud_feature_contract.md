# The card-fraud feature contract (gate 4)

**Status: IN FORCE. Pinned by `tests/test_card_point_in_time.cpp`.
Amended 2026-07-27 by the online-graph integrity round: membership-filtered
payments, role-neutral device IDs, observed session endpoint vertices, and
timestamped transaction→device/IP edges. Amended again 2026-07-27 by
use-chip-causal-2026-07 (ROUND 8): `use_chip` is reclassified USE WITH
CARE → FEATURE-SAFE because it became causal, pinned by
`tests/test_card_use_chip.cpp`.**

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

**Point-in-time honesty is necessary, not sufficient.** Round 6 found a
column that satisfies the rule above and is still unusable: a value can
be perfectly observable at its own timestamp and *also* be a direct
readout of the generator's role assignment. Both tests have to pass.

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
| **Stream prefix** | `Payment_Transaction`, `Card_Send_Transaction`, `Merchant_Receive_Transaction`, `Transaction_Uses_Device`, `Transaction_Uses_IP` | the score-time export's lines must be a byte-exact PREFIX of the full-window export's |
| **Identical** | `Party`, static PII tables/associations other than the `Device`/`IP` vertex sets, header-only `Has_Device`/`Has_IP`, `Merchant_Category` | world-derived; cannot depend on the transaction prefix at all |
| **Growing set** | `Card`, `Device`, `IP`, `Party_Has_Card`, `Merchant`, `Merchant_Assigned`, the geo chain | the row SET may grow as rows arrive; a row present in both must be identical |

The gate is not a tautology: it would have **failed** on the pre-v2
`Card.is_fraud` column, whose value flipped 0 → 1 as soon as a future
flagged row arrived.

**What it does NOT catch** is the round-6 class of defect. A
generator-role artifact like a fraud-device identifier is stable in the
row and therefore sails through a truncation experiment. Nothing but
reading the render path finds it.

## FEATURE-SAFE (a model may read these)

### `cf_Payment_Transaction` — the row under judgement

| Column | Note |
|---|---|
| `id` | `T<row_seq>`; joins `public.transactions` 1:1. An index, not a feature — row_seq is monotone in time, so **do not feed it as a number**. |
| `transaction_time`, `unix_time` | the score-time anchor itself |
| `amount` | era-realized dollars (macro-history H1: a 1991 ticket is ~0.53× a 2019 one). If you normalize amounts, normalize **within era**, or the model learns the calendar. |
| `mer_cat` | the destination merchant's modelled category |
| `use_chip` | **CAUSAL since ROUND 8 (use-chip-causal-2026-07)**; previously a content hash classed USE WITH CARE. `Online Transaction` ⟺ the destination is a geography-free acceptance endpoint — the catalog `Footprint::online` population that BOTH legitimate selection and the fraud rails draw their card-not-present picks from, or a non-catalog remote biller. Physical outlets split `Chip`/`Swipe` by the dated US EMV terminal mix (zero before 2012, ~0.65 in 2019, frozen ~0.90 outside coverage). It therefore carries the REAL modeled CNP-majority fraud signal, on purpose — the same class of honest correlation as distance-from-home. Three declared limits: the chip/swipe split is a presentation-layer technology mix, not per-card/terminal adoption state; the legitimate CNP share is era-flat (registered debt, payments.cpp); and it is entry mode only — authorization outcomes remain unmodelled (`error`, below). Pinned by `tests/test_card_use_chip.cpp`. |

### `cf_Card`, `cf_Merchant`, `cf_Party` and the structural edges

`card_number`, `Merchant.id`, `Party.id`, `Party_Has_Card`,
`Is_Merchant` (**populated since merchant-ownership-2026-07** — the
merchant → proprietor register, ~45% coverage; membership is a hash of the
merchant key alone so it cannot encode footprint, size or geography, and
the measured fraud lift on "destination has an owner edge" straddles 1.0
at 0.95–1.12x across seeds), `Merchant_Assigned`, `Has_State`,
`Has_City`, `Has_Zip`, `Merchant_Location.*`, `Assigned_To`,
`Located_In`, `City.*`, `State.id`, `Zipcode.*`,
`Merchant_Category.category`.

Identifiers and static world geography. Merchant location is
world-modelled (`entity::merchant::Record.location`), and City
population is the catalogue value — both fixed before any transaction
settles. Treat `card_number` only as a categorical join key: its leading
`C`/`D` is an exporter type tag, not a model feature, and its trailing
`-G<n>` is a reissue generation (see below). Unauthorized
positives are currently debit-backed because credit fraud has not yet
been integrated into statement lifecycle servicing; an instrument-type-only
baseline must be reported until that blocker is closed.

#### Card reissue generations (`card-churn-2026-07`)

**One `cf_Card` vertex and one `cf_Party_Has_Card` edge per OBSERVED card
generation.** `card_number` renders as `C`/`D` + account key for the first
generation and gains a `-G<n>` suffix thereafter, so a party whose card was
replaced mid-window holds more than one card vertex. Generation boundaries
come from a draw-free schedule keyed on the card and the window — 36–60-month
validity, a 0.07/yr unscheduled loss/theft/damage hazard, and a 2015–2017 EMV
migration wave.

Point-in-time correct: the generation on a transaction row is the one live at
that row's timestamp, and a scheduled generation with no transaction in the
view is not written at all.

**Card cardinality per party is FEATURE-SAFE, and here is exactly how far to
trust it.** Fraud-driven reissue — ~27% of real reissuance, and the largest
single cause after expiry — is **deliberately not modelled**, because replacing
a card is causally downstream of the compromise and the only fraud signal
available at export time is the withheld full-window verdict. Two consequences,
both stated so nobody re-derives them from the data:

1. **Exported reissue rates are LOWER than production.** Do not calibrate an
   issuer-side reissuance expectation against this corpus.
2. **"This party holds more than one card number" carries NO systematic fraud
   signal here**, where in reality it carries a strong one. Measured lift
   1.012x / 0.990x across two legs (`test_card_endpoint_graph` sub-gate H).
   The residual correlation that does exist is via ACTIVITY, not compromise: a
   heavily-used card both straddles a boundary more often and draws more
   unauthorized attention. A model that leans on card cardinality will
   therefore transfer POORLY to production in the optimistic direction — it
   will underperform relative to what this corpus suggests, not overperform.

#### Coordinates (`merchant-coordinates-2026-07`)

`cf_Merchant_Location(merchant_id, lat, lon)` plus `lat`/`lon` on
`cf_Zipcode` and `cf_City` carry decimal degrees converted at export from
the catalogue's integer microdegrees. SAFE, and static world state fixed
in G1c before any transaction settles.

Three limits, and the first one binds hardest:

- **They are AREA CENTROIDS, so co-located merchants share a point.**
  `Record.location` is a `GeoAreaId`, not a street coordinate. A feature
  that assumes distinct outlet coordinates — nearest-neighbour merchant,
  intra-ZIP clustering, a "same building" signal — is reading resolution
  the generator does not have. Measured: 149 merchants across 48 distinct
  centroids on the e2e window.
- **Row presence in `cf_Merchant_Location` is the `has_coordinates`
  mask.** Online merchants and non-catalog fraud billers are ABSENT, not
  zeroed, so a `LEFT JOIN` yields NULL rather than the Gulf of Guinea. The
  absence itself correlates with card-not-present, exactly as
  `Has_City`/`Has_Zip` absence already does — it is the same modality
  split, not a new signal, and `use_chip` already exposes it.
- **Distance-from-home IS computable as of `party-geography-2026-07`** —
  see the next section. This bullet previously said it was not.

Gated by the coordinate block in `tests/test_pipeline_e2e.cpp`: US
bounding box, byte-identity against the merchant's own `cf_Zipcode` row,
coverage equal to `cf_Has_Zip`, and more than one distinct point. Proved
non-vacuous by two disarms — a lat/lon swap reds 149/149 on bounds and
agreement; a constant point reds bounds and the distinct-point floor.

#### Cardholder-to-merchant DISTANCE (`party-geography-2026-07`)

`cf_Has_Std_City(party_id, city_id, since_unix_time)`,
`cf_Has_Std_Postcode(party_id, zipcode_id, since_unix_time)` and
`cf_Has_Std_State(party_id, state_id, since_unix_time)` export each party's
home-area HISTORY, pointing at the SAME City/Zipcode/State vertices merchant
geography uses. SAFE and prefix-invariant — the relocation schedule is world
state fixed before the fold — so `test_card_point_in_time` classifies all three
as world-derived-identical.

**⚠️ ONE ROW PER TENURE AS OF `relocation-2026-07`, NOT ONE PER PARTY. THIS IS
THE ONE THING TO GET RIGHT HERE.** A party who moved has several rows, each
stamped with the epoch that home began. **You must pick the tenure live at the
transaction's own timestamp.** An undated join does not fail — it returns a
plausible distance for the wrong home, silently, for every mover. A party who
never moved has exactly one row, stamped at the window start, so the no-move
case needs no special handling.

Resolve as: the row with the greatest `since_unix_time` that is
`<= transaction.unix_time`. `Street_Address.lat/lon` in TigerGraph is a SINGLE
point and carries the party's CURRENT home only — do not use it for
point-in-time distance.

**The distance walk, both ends now present:**

| End | Path |
|---|---|
| cardholder | `Party` → `Party_Has_Std_Postcode` → `Zipcode.lat/lon` |
| merchant | `Merchant` → `Merchant_Has_Location` → `Merchant_Location.lat/lon` |

This is the axis the generator's own selection kernel is built on
(`popularity * exp(-distanceMiles / scaleMiles(homeArea))`), so
distance-from-home carries real modelled signal, and the fraud rails read
the same distance. **Use it. Normalize it in miles**, which is the unit
the whole model reasons in (`area.hpp`, owner directive 2026-07-21).

Four limits:

- **Both ends are AREA CENTROIDS**, so intra-area distance is always
  exactly zero and co-located merchants are equidistant from everyone. A
  zero is "same postal area", not "next door".
- **Coresidents share a home area by construction** (the household lane),
  so a household's members have identical distance profiles. That is
  realistic, and it means distance is not an individual-discriminating
  feature within a household.
- **Foreign-domiciled parties are included** (~4% of the roster; the
  production mix is `LocaleMix::usBankDefault`). Their distance to a US
  merchant is genuinely thousands of miles. Do NOT clip or winsorize those
  rows away as outliers — they are the population an issuer most wants to
  reason about, and dropping them turns "has geography" into a residency
  flag.
- **Home area DOES move as of `relocation-2026-07`** — 0.1047
  moves/person-year over a 20-year window, off the Census CPS ASEC series,
  declining across the era. This bullet previously said relocation was
  unmodelled. What remains registered: household composition is static so
  nobody moves OUT of a household; moves stay inside the origin's country;
  the hazard carries NO age or tenure tilt (a household has no single age);
  and a party re-occupying a previously-held area collapses to one graph edge
  stamped at the earliest occupancy, because TigerGraph keys an edge by
  (from, to) with no discriminator.

Gated by the distance-computability block in
`tests/test_pipeline_e2e.cpp`: coverage counts DISTINCT parties against the
Party vertex count (equality on ROWS was re-specified in
`relocation-2026-07` — it would have kept passing vacuously on that gate's
seven-day window, where nobody moves), row-count agreement across the three
edge tables, every `since_unix_time` inside the window, referential integrity
in BOTH directions (party areas the
merchant loop never visited must be unioned into the vertex tables), an
end-to-end `Party → Zipcode → coordinate` walk with zero unreachable
parties, more than one distinct home point, and **at least one FOREIGN
home centroid**, so the harness cannot silently lose non-US coverage.
That last check exists because every gate harness in this repo ran
`LocaleMix::usOnly()` until this round; `test_pipeline_e2e` now runs the
production mix. Proved non-vacuous by dropping foreign parties: coverage
reds at 94/100 and the foreign check reds at 0.

### `cf_Party` attributes

| Column | Note |
|---|---|
| `gender` | a content-keyed even split; **not a modelled attribute** — it carries no mechanism and should be treated as noise |
| `dob`, `party_type`, `name` | static identity |
| `created_at` | **SAFE since H3.** It is the membership `joinTs` — when the customer relationship began — written through the one membership construction path. The audit's old prohibition on this column is hereby **lifted**; it predates H3 and no longer describes the code. |

### The PII / investigative layer

`Address`, `Phone`, `Email`, `IP.id`, `Device.id`, `ID`, `Full_Name`,
`DOB`, plus the timestamped `Transaction_Uses_Device` and
`Transaction_Uses_IP` edges.

Static world facts, so point-in-time safe. Three things to understand
before using them:

- **Shared infrastructure is real signal.** Fraud rings deliberately
  share devices and IPs. A model that learns from prior transactions
  sharing an endpoint is learning modeled behavior, not an artifact —
  this is the intended graph signal.
- **Cross-victim endpoint reuse is THE card-fraud graph signal, and it
  exists as of attacker-infra-2026-07.** Attacker endpoints used to be
  minted one per compromise, so no endpoint was ever seen twice and the
  Device/IP layers could pass no message between two victims. They now
  come from campaign-scoped infrastructure with a heavy-tailed case load.
  Measured: **74–82% of attacker devices are seen by more than one
  victim, mean 5–9 victims, max 37–40.** Endpoint degree and endpoint
  history are first-class features.
- **Use the transaction-time edges for online scoring.**
  `Transaction_Uses_Device` and `Transaction_Uses_IP` carry the actual
  session endpoint and `edge_unix_time`. Score first, then append the
  current edge to memory.
- **`Has_IP` / `Has_Device` are the institution's ENDPOINT REGISTRY, and
  it is deliberately INCOMPLETE.** Read them as "this association is on
  file", never as "this party owns this endpoint": coverage is ~72% for
  devices and ~61% for addresses. Absence is therefore weak evidence, not
  proof of an exogenous endpoint — which is exactly the production
  semantics, and exactly why the table is now safe to feed. They are
  whole-window and carry no interval, so use them for STRUCTURE (reaching
  Party from an endpoint) rather than as a timestamped fact.
- **`Device.id` here is a VERTEX identity, and the vertex set is the
  union of the world roster and endpoints observed in card-view rows.**
  Exogenous attacker endpoints therefore cannot be detected by absence
  from the vertex universe. Its companion `is_blocked` column is
  withheld (written 0) and its verdict lives in the quarantined overlay.
- **Device rendering is role-neutral.** Personal, legitimate-shared, ring,
  and exogenous attacker identities all render through the same fixed-width
  opaque `D…` namespace. The string is a categorical join key, never a
  numeric feature.
- **`Email_Minhash` / `Has_Email_Minhash` (added 2026-07-27, owner
  request) are FEATURE-SAFE as graph structure.** The bucket ids are LSH
  band buckets (shared `common/minhash`, `EMH` prefix, b=10 bands × r=1,
  3-gram shingles) derived solely from the email string already exported
  in `Email`/`Has_Email` — no new information channel, no time axis, no
  label content, and emails belong to customer parties only (attacker
  rings carry no email), so bucket membership cannot encode role. Emails
  sharing a band bucket are textually similar; that adjacency is the
  intended signal. Treat the bucket id itself as an opaque categorical
  join key, never a numeric feature.

### `public.transactions` (the raw ledger), row-level

`src_acct`, `dst_acct`, `amount`, `ts`, `channel`, `ip_address`,
`device_id` — observable at the row's own timestamp.

`ip_address` is safe **because the shortcut in it was closed**: attacker
addresses come from `network::randomIpv4`, the same generator legitimate
sessions draw from, so no prefix lookup separates them. Before that fix
they were TEST-NET-2 and this line would have been wrong.

`device_id` is safe only as a categorical identity used to retrieve prior
state. Do not parse, bucket, or feed identifier magnitude. The prior `FD…`
role namespace was a deterministic label and is now closed by
`exporter/common/render.hpp`.

## THE TARGET (never an input)

`cf_Payment_Transaction.is_fraud` — and its raw twin
`public.transactions.is_fraud`.

This is the **only** supervised label inside the feature graph. It is a
per-row fact observable at that row's timestamp, which is exactly what
makes it a legitimate target.

### The label is CENSORED on authorization attempts — exclude them from the loss

**A row with a non-empty `error` is an authorization ATTEMPT, not a settled
purchase, and its `is_fraud` is 0 because no label exists — not because the row
is known-good.** Treating those zeros as negatives poisons the training set.

Three populations carry a non-empty `error`, all separable by its value:

| `error` | population | share of the payment table |
|---|---|---|
| empty / NULL | settled purchase — **the only rows with a real label** | ~94% |
| `Insufficient Balance` | the replay's own funding declines | ~0.1% |
| `Do Not Honor` | card-testing probes (`infra/enumeration.hpp`) | ~0.03% |
| anything else | non-funding declines (bad PIN, bad CVV, …) | ~5.6% |

**WHY THE ZEROS ARE NOT NEGATIVES, and the reason is a property of the real
world rather than of this generator.** Production fraud labels come from
disputes and chargebacks. A declined authorization never settles, so it is
never disputed, so it **never receives a label at all** — the *censored
feedback* problem, "endogenously missing for declined transactions" (Fundamental
Limits of Fraud Detection in Card Payment Networks, arXiv 2605.27557; accessed
2026-08-07). No production pipeline can supply a ground truth for these rows, so
neither can this corpus. A share of the funding declines ARE fraud attempts —
the unauthorized rail drains a victim, so its own later charges cannot fund —
and labelling them 1 would be just as wrong, because production never learns
that either.

**WHAT A CONSUMER SHOULD DO: keep the nodes, drop them from the loss.** The
declined rows carry real `Card_Send` / `Merchant_Receive` / `Uses_Device` /
`Uses_IP` edges, so excluding them entirely throws away graph structure a GNN
wants. Masking the loss keeps the structure and refuses the false negative.

**AND THE DISCRIMINATOR MUST REACH THE CONSUMER, WHICH TODAY IT DOES NOT.**
`tf_gnn_loader_v2/sql/postgres/070_create_transaction_views.sql` deliberately
does not load `error` — correctly, since a decline code is a property of the
authorization RESPONSE and the scoring moment is the REQUEST. But it also
applies no filter, so every attempt loads as an ordinary transaction node with
`is_fraud = 0` and nothing downstream can tell them apart. **This repo's export
is not the defect: the fact is present in `error` on every row.** The action is
one line in the loader's view — filter, or expose a boolean derived from
`error` so the training pipeline can mask.

Same shape as `cf_Is_Merchant` (see CLAUDE.md `merchant-ownership-2026-07`): an
export decision that is invisible to the repository depending on it. Recorded
here so it is a stated contract rather than a fact a consumer has to rediscover.

## PROHIBITED (leaks, ground truth, or both)

| Column | Why |
|---|---|
| `cf_Card.is_fraud`, `cf_Party.is_fraud`, `cf_Device.is_blocked`, `cf_IP.is_blocked` | full-window entity verdicts. **Written as 0** since round 1 — the columns are retained only so TF_GNN_v3 loading jobs map positionally. Restoring their content re-opens the leak the arc exists to close. |
| `cf_Ground_Truth_Label` (whole table) | the quarantined investigative overlay: the four withheld verdicts, positives only. Future-dependent by construction — `test_card_point_in_time` prints how much it moves across the cutoff as standing evidence. **Evaluation only.** No TF_GNN_v3 job loads it and no edge points at it. |
| `public.transactions.ring_id`, `.fraud_type` | the generator's own ground truth. That table is the corpus, not the feature graph. Use them to slice an evaluation, never to fit. |
| ~~`cf_Has_Device`, `cf_Has_IP`~~ | **MOVED TO FEATURE-SAFE (WITH CARE) — attacker-infra-2026-07.** They were header-only because every customer endpoint had a Party owner and no attacker endpoint did, so missing adjacency was a synonym for attacker role. That was a generator property, not a fact about the world, and the generator changed on both sides: registry coverage is partial (`infra::enrollment`), and a declared share of unauthorized fraud is operated from the victim's own endpoint or exits through a residential proxy. Measured residual: "endpoint not on file ⇒ fraud" precision **0.027**, lift **2.9x** over base rate — a real weak feature. See USE WITH CARE. |

## USE WITH CARE (safe, but not what they look like)

| Column | The catch |
|---|---|
| `cf_Payment_Transaction.error` | an **export-time content hash** of the row, 2% incidence. It is deterministic in the row and carries no future, so it passes this contract — but authorization attempts are not modelled, so this column has no cause behind it. Treat as noise or drop. It is the remaining hash half of online-GNN gate 4 now that `use_chip` is causal (see FEATURE-SAFE above). |
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

**And re-classifying a column is a first-class outcome.** Device identity
moved safe → prohibited when the `FD…` namespace was discovered, then back
to categorical-safe only after the renderer became role-neutral and the
endpoint-universe and prefix tests shipped. `use_chip` moved the other way
— USE WITH CARE (hash noise) → FEATURE-SAFE — only when ROUND 8 replaced
the hash with the acceptance-environment mechanism and shipped its gate.
A point-in-time gate cannot see a stable generator-role artifact, so
**"passes gate 4" is not the same as "safe to feed."**
