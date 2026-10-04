# Card-fraud online GNN contract

Status: current implementation contract, 2026-07-27.

## Verdict

`card-fraud` supports building and testing a point-in-time temporal GNN
pipeline. It is not yet a public calibrated benchmark (see
[Remaining benchmark gates](#remaining-benchmark-gates)), and a high score
means something only under the replay and feature rules below.

- Closed shortcuts: fraud-only merchants, entity labels, TEST-NET IPs, device
  prefixes, missing session endpoints. `use_chip` is a causal entry mode, not
  a content hash (use-chip-causal-2026-07).
- Endpoints carry a message since attacker-infra-2026-07. Per-compromise
  attacker devices and IPs had made cross-victim sharing, the most valuable
  card-fraud graph signal, zero while every gate stayed green (gates checked
  presence, not sharing). Campaign-scoped infrastructure now shares them
  (74-82% of attacker devices seen by more than one victim, mean 5-9, max
  37-40), `Has_Device`/`Has_IP` connect Device and IP to Party, and the
  "endpoint not on file" residual is a 2.9x lift, not deterministic.
- Unauthorized card cases use a modeled credit-card channel but settle from
  the victim's primary account, so they export as derived debit activity.
  Every visible payment has timestamped device and IP edges.

## What is implemented

| Requirement | State | Enforcement |
|---|---|---|
| No fraud-only merchants | closed: all card rows share the acceptance catalog and geographic kernel | `test_card_merchant_overlap`, `test_card_baselines` |
| No full-window entity labels | closed: `Card.is_fraud`, `Party.is_fraud`, `Device.is_blocked`, `IP.is_blocked` written 0 (kept positionally); positives only in `cf_Ground_Truth_Label` | `test_pipeline_e2e`, table golden |
| Point-in-time features | closed for repository-owned exported features (full-versus-prefix truncation) | `test_card_point_in_time` |
| Victim selection | exposure-weighted for card/ATO; persona × age for authorized scams; the full case span fits inside owned endpoints' `[join, close)`, with authorized victims alive throughout | `test_card_victim_baselines`, `test_card_scam_rail`, `test_membership`, `test_unauthorized_keyed` |
| Compromised card | open: rows stay derived-debit because fraud is planned after `CardCycleDriver` closes and services legitimate cycles; `test_card_prevalence` blocks an unserviced credit-liability key swap posing as a fix | benchmark blocker |
| Attacker IPs | closed: one address generator for all | `test_unauthorized_keyed` |
| Device namespace | closed: one fixed-width opaque `D…` namespace; owner type not visible in prefix, width or range | `test_unauthorized_keyed`, `test_pipeline_e2e` |
| Session edges | `Transaction_Uses_Device`, `Transaction_Uses_IP` with `edge_unix_time`; observed exogenous endpoints are in `Device`/`IP` | `test_pipeline_e2e`, `test_card_point_in_time` |
| Membership | payments outside either owned endpoint's membership interval are excluded | `test_membership`, `test_card_point_in_time` |
| Per-year behavior | stability, channel, typology, episode size, merchant overlap, CPI-scaled amounts | `test_card_prevalence`, `test_card_class_f` |
| Entry mode (`use_chip`) | closed (round 8): Online exactly at geography-free acceptance endpoints (the `Footprint` axis both legitimate and fraud selection use); Chip/Swipe by the dated US EMV mix (zero before 2012) | `test_card_use_chip`, `test_card_point_in_time` |

The 2026-07-21 smoke result (748 positives all on 100 fraud-only merchants,
full-window labels in the graph) is pre-v2: the gates keep it as the failure
case.

## Point-in-time prediction contract

For a payment at time `t`:

1. build features only from events before `t`, plus the current payment's
   observable request context;
2. score before adding the transaction or its session edges to memory;
3. record the prediction;
4. append the transaction, card, merchant, device and IP event to memory;
5. expose the target only when the operational label policy says it was
   observed.

Request context: amount, timestamp, merchant/category, entry mode
(`use_chip`), instrument, device, IP. Device/IP identifiers select prior
state; they are categorical keys, never numbers. Valid: historical velocity,
amount deviation, merchant novelty, card-device/IP recency, prior outcomes
known before `t`, point-in-time neighbourhood features.

Never use as inputs:

- `Payment_Transaction.is_fraud`, `public.transactions.is_fraud`,
  `cf_Ground_Truth_Label`;
- `Card.is_fraud`, `Party.is_fraud`, `Device.is_blocked`, `IP.is_blocked`
  (zero; restoring them is forbidden);
- `public.transactions.ring_id`, `.fraud_type`;
- `row_seq`, `span_index`, identifier prefixes or magnitude, other
  generator/debug metadata;
- PageRank, communities, co-occurrence edges, aggregates or embeddings that
  include events at or after `t`;
- PII, unless a separately reviewed use case requires it;
- `error` in the default model: a content-keyed compatibility field, not
  causal authorization state (see `docs/card_fraud_feature_contract.md`).

Allowed: `Has_Device`/`Has_IP` (since attacker-infra-2026-07; an incomplete
registry at ~72% device and ~61% address coverage, "not on file ⇒ fraud"
precision 0.027 at 2.9x lift) for structure only, using the timestamped
session edges for anything point-in-time; `use_chip` (since round 8);
`Party.created_at`, the modeled membership `joinTs` (the old prohibition
predated H3).

## Evaluation protocol

Use strict event order, never a random row or edge split.

Split by fractions of `[windowStart, windowEndExcl)`: train the first ~67%,
validation the next ~16%, test the final ~17%. An earlier fixed split (train
`[1999-01-01, 2015-01-01)`, validation `[2015, 2017)`, test `[2017, 2020)`)
assumed a two-decade corpus. No target-scale run can meet it: card-fraud is
era-locked to the pinned macro series (1990-2024, `src/app/cli.cpp:185`), and
the target corpus is ~100,000 people over ~3 years.

Reference window `--start 2022-01-01 --days 1096` (through 2024-12-31, the
latest three full years in the era lock):

| Split | Interval | Epoch bound |
|---|---|---|
| train | `[2022-01-01, 2024-01-01)` | `1704067200` |
| validation | `[2024-01-01, 2024-07-01)` | `1719792000` |
| test | `[2024-07-01, 2025-01-01)` | n/a |

The TigerGraph loader, not this repository, holds these bounds and assigns
each row's split. Update them whenever the generated window changes: a stale
split policy raises no error and silently mislabels every row.

Report PR-AUC, precision and recall at a fixed alert-review budget, false
positives per million payments, calibration, and time to first detection in
a compromise episode. Keep natural prevalence in validation and test. If
training undersamples positives or uses class weights, document the posterior
correction and pick thresholds on validation only. Include:

- rolling-origin results by year and seed;
- inductive cards, merchants, devices and IPs first seen after the training
  cutoff;
- amount/time-only, merchant-ID-only, instrument-type-only, entry-mode-only
  and device/IP-namespace baselines (round 8 made `use_chip` carry the real
  CNP-majority signal; it must help, not solve the task);
- episode-clustered confidence intervals;
- slices for unauthorized debit and victim-authorized gift-card scams. An
  unauthorized-credit slice is due once credit-card fraud joins lifecycle
  servicing; its absence is a known gap, not a favorable result.

The exporter builds an offline corpus; "online" means replaying it in strict
timestamp order under this contract. Live serving still needs an incrementally
committed event outbox and delayed label events. Target architecture:
[Temporal Graph Networks](https://arxiv.org/abs/2006.10637). Each payment is a
timestamped card-merchant interaction whose device and IP edges share its
instant, so TGN memory updates and temporal neighbour sampling key off
`unix_time` directly.

## Remaining benchmark gates

1. Calibrate the fraud level on the final payment view against a named
   issuer-side count series; do not compare a count rate with value-loss
   basis points.
2. Plan fraud early enough that unauthorized credit-card purchases go through
   statement close, payments, interest, chargebacks, credit limits and later
   spending. Then add effective-dated card issue/expiry/replacement/closure,
   device/IP usage, residence and merchant availability. The decades-long macro
   and persona timelines are real; the wallet and access graph are too static.
3. Replace the single whole-window, era-flat fraud process with content-keyed
   calendar buckets and era-varying card-not-present, gift-card, wire/P2P and
   reporting behavior. A short run must be a byte-identical prefix of a
   longer run with the same seed and start. Round 8's EMV mix covers
   presentation only; compromise incidence, `kCardNotPresentShare` and the
   legitimate CNP share are still era-flat.
4. Entry mode is done (round 8, `test_card_use_chip`). Still open: model
   authorization-attempt outcomes and remove the remaining compatibility
   hash derivation for `error`.
5. Add `case_id`, `label_observed_at`, dispute/report linkage, censoring and
   intervention-aware outcome metrics; deliver delayed operational labels.
6. Put the production TigerGraph/GSQL feature allowlist and engineered query
   under test; export causality does not prove that query is causal.

The victim-authorized jail-relative/impostor wire/P2P rail (`scam_impostor`)
is in the raw ledger but outside the card-only `Payment_Transaction` view. A
broader "Payment Transaction Fraud" benchmark needs its own graph or view, not
a silent mix into the card contract.
