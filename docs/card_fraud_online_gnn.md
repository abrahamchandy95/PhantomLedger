# Card-fraud online GNN contract

Status: realism audit, 2026-07-21.

## Verdict

The `card-fraud` use case is runnable and useful for PostgreSQL, TigerGraph,
temporal-loading, and model-pipeline development. It is **not yet a credible
online fraud benchmark**. The current generator contains a merchant-identity
shortcut and several full-window attributes that leak the target.

This distinction matters: a high score on the current corpus can demonstrate
that the graph pipeline works without demonstrating that a model learned fraud
behavior.

## Evidence from the current smoke corpus

The checked PostgreSQL build used population 10,000, 60 days, seed 7, and
start date 2025-01-01. It contained 533,382 `Payment_Transaction` rows and 748
fraud rows.

- All 748 fraud rows targeted 100 merchants with no legitimate card-view row.
  A model can therefore classify by merchant identity alone. This follows from
  unauthorized card and gift-card events targeting the legitimate-transfer
  biller/hub pool, while ordinary card-view purchases use the merchant catalog.
- The 748 fraud rows occurred on 97 cards, and every one of those cards also
  had legitimate history. That is useful longitudinal structure, but
  `Card.is_fraud` is computed from the entire output window: it gives 100%
  recall on fraud rows by construction and must never be a predictive input.
- `Party.is_fraud`, `Device.is_blocked`, and `IP.is_blocked` are also timeless
  investigative labels. The unauthorized transaction's attacker device/IP is
  not represented as a transaction-time edge in this schema.
- The raw `public.transactions` table exposes `is_fraud`, `ring_id`, and
  `fraud_type`, plus generator-role artifacts such as fraud-device identifiers
  and TEST-NET IPs. These are ground truth/debug fields, never features.
- `use_chip` and `error` are export-time hashes over settled rows, not causal
  transaction-mode/authentication and authorization-attempt state.

The merchant shortcut can be measured on any completed corpus with:

```sql
WITH merchant_labels AS (
  SELECT e.merchant_id, p.is_fraud::integer AS label
  FROM card_fraud."cf_Merchant_Receive_Transaction" AS e
  JOIN card_fraud."cf_Payment_Transaction" AS p ON p.id = e.txn_id
), merchant_summary AS (
  SELECT merchant_id,
         count(*) FILTER (WHERE label = 1) AS fraud_rows,
         count(*) FILTER (WHERE label = 0) AS legitimate_rows
  FROM merchant_labels
  GROUP BY merchant_id
)
SELECT count(*) FILTER (WHERE fraud_rows > 0) AS fraud_merchants,
       count(*) FILTER (
         WHERE fraud_rows > 0 AND legitimate_rows = 0
       ) AS fraud_only_merchants,
       sum(fraud_rows) AS fraud_rows,
       sum(fraud_rows) FILTER (
         WHERE legitimate_rows = 0
       ) AS fraud_rows_at_fraud_only_merchants
FROM merchant_summary;
```

## Point-in-time prediction contract

For a transaction at time `t`, the model must score it before observing its
label and before updating node memory with that transaction. It may use the
current card and merchant endpoints, current transaction amount/time/category,
and graph events with `edge_unix_time < t`. Labels may update training state
only after the score is recorded.

Never use these as predictive features:

- `Payment_Transaction.is_fraud`, except as the supervised target;
- `Card.is_fraud`, `Party.is_fraud`, `Device.is_blocked`, or `IP.is_blocked`;
- aggregates, PageRank, communities, co-occurrence edges, or embeddings
  computed from events at or after `t`;
- `Party.created_at` until membership-time consistency is fixed;
- `row_seq`, `span_index`, `ring_id`, `fraud_type`, identifier prefixes, or
  other generator/debug metadata;
- `error` until authorization attempts and declines are modeled.

Historical velocity, amount deviation, merchant novelty, card-merchant
recency, prior legitimate/fraud outcomes available before `t`, and
point-in-time neighborhood features are valid. PII should stay outside the
model unless a separately reviewed use case needs it.

## Evaluation protocol

Use an event-stream model such as a Temporal Graph Network and maintain strict
timestamp order. A suitable first split for the requested corpus is:

- train: `[1991-01-01, 2015-01-01)`;
- validation: `[2015-01-01, 2017-01-01)`;
- test: `[2017-01-01, 2020-01-01)`.

Do not use a random row or random edge split. Report PR-AUC, recall at a fixed
false-positive or alert-review budget, precision at the operating threshold,
false positives per million transactions, calibration, and time to first
detection within a compromise episode. Include an inductive test for cards and
merchants first observed after the training cutoff, and compare against
amount/time-only and merchant-ID-only baselines.

The current exporter is an offline corpus builder, not an inference service:
its card tables use run-long PostgreSQL COPY streams and become queryable after
the run closes. “Online” here means replaying the completed corpus in strict
event order and enforcing point-in-time visibility. A true live path would
also need an incrementally committed event/outbox, transaction-time
device/IP/channel edges, and a delayed `label_observed_at` feedback event.

This follows the continuous-time event framing in
[Temporal Graph Networks](https://arxiv.org/abs/2006.10637). IBM's
[TabFormer work](https://research.ibm.com/publications/tabular-transformers-for-modeling-multivariate-time-series)
is a longitudinal/schema comparison target, not evidence that PhantomLedger's
current causal mechanisms are calibrated.

## Minimum realism gates before a public benchmark

1. Unauthorized card fraud must select a modeled compromised card, modality,
   eligible merchant acceptance endpoint, and transaction-time device/IP. The
   same merchant endpoint population must support both legitimate and
   fraudulent activity; fraud-only merchant identity cannot explain every
   positive row.
2. Fraud cases, cards, residence, modes, and compromise state need effective
   dates and drift over the 29-year window.
3. Fraud prevalence must be calibrated on the final Payment view, with
   per-year, channel, typology, amount, episode-size, and merchant-overlap
   gates—not only one aggregate rate.
4. Every in-graph feature must have a point-in-time implementation and a test
   that adding future events cannot change an earlier score-time feature.
5. A merchant-ID-only baseline must not solve the task. Performance should
   persist on chronological and inductive holdouts.
