# Card-fraud online GNN contract

**Status: current implementation contract, 2026-07-27.**

## Verdict

The `card-fraud` use case is now suitable for building and testing a
point-in-time temporal GNN pipeline. The known deterministic merchant,
entity-label, TEST-NET IP, device-prefix, and missing-session-endpoint
shortcuts are closed, and `use_chip` is a causal entry mode rather than a
content hash (use-chip-causal-2026-07).

**THE ENDPOINT LAYER CARRIES A MESSAGE as of attacker-infra-2026-07, and
before that round it did not.** Attacker devices and IPs were minted one
per compromise, so cross-victim endpoint sharing was zero by construction
— the single most valuable card-fraud graph signal was absent while every
existing gate stayed green, because they all checked that endpoints were
PRESENT and none checked that they were SHARED. Attacker endpoints now
come from campaign-scoped infrastructure with a heavy-tailed case load
(measured: 74–82% of attacker devices seen by more than one victim, mean
5–9, max 37–40), `Has_Device`/`Has_IP` are populated so Device and IP are
reachable from Party at all, and the residual "endpoint not on file"
signal is sized at 2.9x lift rather than being deterministic.

Unauthorized card cases use a modeled
credit-card channel but currently settle from the victim's primary account and
therefore export as derived debit-card activity; every visible payment carries
timestamped device and IP edges.

It is still **not a public calibrated benchmark**. The remaining blockers are
level calibration against a named issuer-side count series, integration of
unauthorized credit-card events into lifecycle servicing, effective-dated
card/device/residence lifecycles across the full horizon, era-varying fraud
technology and rail mix, delayed operational labels, and verification of the
actual TigerGraph engineered-feature query. A high model score is meaningful
only if the replay and feature rules below are followed.

## What is implemented

| Risk or requirement | Current state | Enforcement |
|---|---|---|
| Fraud-only merchant identity | Closed: legitimate and fraudulent card rows draw from the same acceptance catalog and geographic kernel | `test_card_merchant_overlap`, `test_card_baselines` |
| Full-window entity labels | Closed: `Card.is_fraud`, `Party.is_fraud`, `Device.is_blocked`, and `IP.is_blocked` are retained positionally but written `0`; positives live only in `cf_Ground_Truth_Label` | `test_pipeline_e2e`, table golden |
| Point-in-time feature drift | Closed for repository-owned exported features by a full-versus-prefix truncation experiment | `test_card_point_in_time` |
| Victim selection | Exposure-weighted for card/ATO; persona × age susceptibility for authorized scams; every complete case span must fit inside owned endpoints' `[join, close)` intervals and authorized victims must remain alive through the span | `test_card_victim_baselines`, `test_card_scam_rail`, `test_membership`, `test_unauthorized_keyed` |
| Compromised card | Open: unauthorized rows remain primary-account/derived-debit backed because fraud is planned after `CardCycleDriver` has closed and serviced legitimate cycles. `test_card_prevalence` prevents an unserviced credit-liability key swap from masquerading as a fix | explicit benchmark blocker |
| Attacker IP namespace | Closed: attacker and legitimate IPs use the same address generator | `test_unauthorized_keyed` |
| Device role namespace | Closed: every assigned device renders through one fixed-width opaque `D…` namespace; owner type is not exposed in prefix, width, or numeric range | `test_unauthorized_keyed`, `test_pipeline_e2e` |
| Transaction-time infrastructure | Implemented: `Transaction_Uses_Device` and `Transaction_Uses_IP`, both with `edge_unix_time`; observed exogenous endpoints are included in `Device`/`IP` | `test_pipeline_e2e`, `test_card_point_in_time` |
| Membership-visible card graph | Implemented: payment rows outside either owned endpoint's membership interval are excluded from the card graph | `test_membership`, `test_card_point_in_time` |
| Per-year prevalence and amount behavior | Measured and gated for stability, channel, typology, episode size, merchant overlap, and CPI-scaled amount behavior | `test_card_prevalence`, `test_card_class_f` |
| Entry mode (`use_chip`) | Closed (ROUND 8, use-chip-causal-2026-07): Online ⟺ geography-free acceptance endpoint — the same `Footprint` axis both legitimate selection and the fraud rails partition destinations on — with the physical Chip/Swipe split following the dated US EMV terminal mix (zero before 2012). `error` remains a content hash and stays out of the default feature set | `test_card_use_chip`, `test_card_point_in_time` |

The original 2026-07-21 smoke result—748 positives all landing on 100
fraud-only merchants and full-window labels in the graph—is historical
pre-v2 evidence. It is the failure case the current gates preserve; it no
longer describes the generator.

## Point-in-time prediction contract

For a payment at time `t`, the system must:

1. construct features only from events with timestamp `< t`, plus request
   context observable for the current payment;
2. score before adding the current transaction or its session edges to graph
   memory;
3. record the prediction;
4. append the transaction, card, merchant, device, and IP event to memory;
5. expose the target only when the chosen operational label policy says it was
   observed.

Current request context may include amount, timestamp, merchant/category,
entry mode (`use_chip`, causal since ROUND 8), instrument, device, and IP.
The current device/IP identifiers select prior state; they are categorical
join keys, never numeric features. Historical velocity, amount deviation,
merchant novelty, card-device/IP recency, prior outcomes available before
`t`, and point-in-time neighborhood features are valid.

Never use these as predictive inputs:

- `Payment_Transaction.is_fraud` or `public.transactions.is_fraud`;
- `cf_Ground_Truth_Label`;
- `Card.is_fraud`, `Party.is_fraud`, `Device.is_blocked`, or `IP.is_blocked`
  (they are currently zero, and restoring them is forbidden);
- `public.transactions.ring_id` or `.fraud_type`;
- `row_seq`, `span_index`, identifier prefixes, identifier magnitude, or
  other generator/debug metadata;
- PageRank, communities, co-occurrence edges, aggregates, or embeddings that
  include events at or after `t`;
- (`Has_Device` / `Has_IP` LEFT THIS LIST in attacker-infra-2026-07. They
  are now populated, and they are the institution's INCOMPLETE endpoint
  registry rather than ground-truth ownership — ~72% device / ~61% address
  coverage, so absence is weak evidence. Measured "not on file ⇒ fraud"
  precision 0.027 at 2.9x lift. Use them for graph STRUCTURE; use the
  timestamped transaction-session edges for anything point-in-time.)
- PII unless a separately reviewed use case explicitly requires it;
- `error` in the default model. It remains a content-keyed compatibility
  field rather than causal authorization state. (`use_chip` left this list
  in ROUND 8: it is now the causal entry mode — see the feature contract.)

`Party.created_at` is safe: it is the modeled membership `joinTs`. The old
prohibition predated the H3 membership implementation.

## Evaluation protocol

Use strict event order, not a random row or edge split.

**THE SPLIT IS WINDOW-RELATIVE, NOT A FIXED SET OF CALENDAR DATES.** An earlier
revision of this document pinned train `[1999-01-01, 2015-01-01)` / validation
`[2015, 2017)` / test `[2017, 2020)`. That was written against a two-decade
corpus and **no run at the current target scale can satisfy it** — the
card-fraud use case is era-locked to the pinned macro series (1990–2024,
`src/app/cli.cpp:185`), and the target corpus is ~100,000 people over ~3 years.
Express the split as fractions of `[windowStart, windowEndExcl)`:

- train: first ~67% of the window;
- validation: next ~16%;
- test: final ~17%.

For the reference target window `--start 2022-01-01 --days 1096` (through
2024-12-31, the latest 3 full years inside the era lock):

| split | interval | epoch bound |
|---|---|---|
| train | `[2022-01-01, 2024-01-01)` | `1704067200` |
| validation | `[2024-01-01, 2024-07-01)` | `1719792000` |
| test | `[2024-07-01, 2025-01-01)` | — |

**These bounds live in `tf_gnn_prep.split_policy` in the loader repo, not
here** (`sql/postgres/020_create_split_policy.sql`), and that row is the
authority the `transaction_manifest` view reads. Whenever the generated window
changes, the split-policy row must change in the same commit — a stale policy
does not error, it silently mislabels `split_id` for every row.

Report PR-AUC, precision and recall at a fixed alert-review budget, false
positives per million payments, calibration, and time to first detection
within a compromise episode. Keep natural prevalence in validation/test.
If training undersamples positives or uses class weights, document the
posterior correction and choose thresholds on validation only.

Include:

- rolling-origin results by year and seed;
- inductive cards, merchants, devices, and IPs first observed after the
  training cutoff;
- amount/time-only, merchant-ID-only, instrument-type-only, entry-mode-only,
  and device/IP-namespace baselines (the entry-mode baseline exists because
  ROUND 8 deliberately made `use_chip` carry the real CNP-majority signal —
  it must help a model, not solve the task);
- episode-clustered confidence intervals;
- subtype slices for unauthorized debit and victim-authorized gift-card scams.
  An unauthorized-credit slice is required only after credit-card fraud is
  integrated into lifecycle servicing; the current absence is a known gap,
  not a favorable result.

The exporter builds an offline corpus. “Online” means replaying that corpus in
strict timestamp order under this visibility contract. A live serving path
still needs an incrementally committed event/outbox and delayed label events.
This follows the continuous-time event framing in
[Temporal Graph Networks](https://arxiv.org/abs/2006.10637), which is the
target architecture for this corpus: each payment is a timestamped interaction
event between a card node and a merchant node, carrying device and IP session
edges stamped with the same instant, so TGN memory updates and temporal
neighbour sampling both key off `unix_time` with no reconstruction step.

## Remaining benchmark gates

1. Calibrate fraud **level** on the final payment view against a named
   issuer-side count series; do not compare a count rate with value-loss basis
   points.
2. Move fraud planning early enough that unauthorized credit-card purchases
   participate in statement close, payments, interest, chargebacks, credit
   limits, and later spending. Then add effective-dated card
   issue/expiry/replacement/closure, device/IP usage, residence, and merchant
   availability. The current decades-long macro and persona timelines are
   real, but the wallet and access graph are too static.
3. Replace one whole-window, era-flat fraud process with content-keyed
   calendar buckets and era-varying card-not-present, gift-card, wire/P2P, and
   reporting behavior. A short run must be a byte-identical prefix of a longer
   run with the same seed/start. (ROUND 8's dated EMV terminal mix covers the
   PRESENTATION layer only; the fraud process itself — compromise incidence,
   `kCardNotPresentShare`, the legitimate CNP share — is still era-flat.)
4. ~~Export causal entry mode~~ **entry mode DONE (ROUND 8,
   `test_card_use_chip`)**; still open: model authorization-attempt outcomes
   and remove the remaining compatibility hash derivation for `error`.
5. Add `case_id`, `label_observed_at`, dispute/report linkage, censoring, and
   intervention-aware outcome metrics.
6. Put the production TigerGraph/GSQL feature allowlist and query under test;
   repository export causality does not prove an external engineered query is
   causal.

The victim-authorized jail-relative/impostor wire/P2P rail is present in the
raw ledger as `scam_impostor`, but it is outside the card-only
`Payment_Transaction` view. A broader “Payment Transaction Fraud” benchmark
should be a separate graph/view rather than silently mixing that target into
the card contract.
