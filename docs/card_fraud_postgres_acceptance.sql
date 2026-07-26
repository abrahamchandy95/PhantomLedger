\set ON_ERROR_STOP on

\if :{?expected_population}
\else
  \set expected_population 10000
\endif
\if :{?expected_days}
\else
  \set expected_days 10592
\endif
\if :{?expected_start}
\else
  \set expected_start 1991-01-01
\endif
\if :{?expected_end}
\else
  \set expected_end 2020-01-01
\endif

-- Read-only release gate for the requested persistent card-fraud corpus.
-- Run only after:
--   make run ARGS="--start 1991-01-01 --days 10592 --population 10000 --usecase card-fraud"
-- Usage:
--   psql "dbname=phantomledger" -f docs/card_fraud_postgres_acceptance.sql
--
-- 35 tables since card-fraud-realism-v2: the 34 TF_GNN_v3 graph tables
-- plus cf_Ground_Truth_Label, the quarantined investigative overlay that
-- holds the four full-window entity verdicts the graph no longer carries.

BEGIN TRANSACTION READ ONLY;

SET LOCAL phantomledger.expected_population TO :'expected_population';
SET LOCAL phantomledger.expected_days TO :'expected_days';
SET LOCAL phantomledger.expected_start TO :'expected_start';
SET LOCAL phantomledger.expected_end TO :'expected_end';

DO $acceptance$
DECLARE
  registered_count bigint;
  physical_count bigint;
  payment_count bigint;
  card_edge_count bigint;
  merchant_edge_count bigint;
  raw_count bigint;
  manifest_rows bigint;
  manifest_status text;
  problem_count bigint;
  min_payment_ts timestamp;
  max_payment_ts timestamp;
BEGIN
  SELECT count(*)
    INTO registered_count
    FROM public.pl_direct_tables
   WHERE schema_name = 'card_fraud';
  IF registered_count <> 35 THEN
    RAISE EXCEPTION 'expected 35 registered card_fraud tables, found %',
      registered_count;
  END IF;

  SELECT count(*)
    INTO physical_count
    FROM information_schema.tables
   WHERE table_schema = 'card_fraud'
     AND table_type = 'BASE TABLE'
     AND table_name LIKE 'cf\_%' ESCAPE '\';
  IF physical_count <> 35 THEN
    RAISE EXCEPTION 'expected 35 physical card_fraud tables, found %',
      physical_count;
  END IF;

  SELECT count(*) INTO payment_count
    FROM card_fraud."cf_Payment_Transaction";
  SELECT count(*) INTO card_edge_count
    FROM card_fraud."cf_Card_Send_Transaction";
  SELECT count(*) INTO merchant_edge_count
    FROM card_fraud."cf_Merchant_Receive_Transaction";
  IF payment_count = 0 OR payment_count <> card_edge_count
     OR payment_count <> merchant_edge_count THEN
    RAISE EXCEPTION
      'payment/card-edge/merchant-edge counts differ or are empty: %/%/%',
      payment_count, card_edge_count, merchant_edge_count;
  END IF;

  SELECT count(*) - count(DISTINCT id)
    INTO problem_count
    FROM card_fraud."cf_Payment_Transaction";
  IF problem_count <> 0 THEN
    RAISE EXCEPTION 'duplicate Payment_Transaction ids: %', problem_count;
  END IF;

  SELECT count(*)
    INTO problem_count
    FROM card_fraud."cf_Payment_Transaction" p
    LEFT JOIN public.transactions t
      ON p.id = 'T' || t.row_seq::text
   WHERE t.row_seq IS NULL;
  IF problem_count <> 0 THEN
    RAISE EXCEPTION 'Payment_Transaction rows missing raw-ledger joins: %',
      problem_count;
  END IF;

  SELECT count(*)
    INTO problem_count
    FROM card_fraud."cf_Payment_Transaction" p
    JOIN public.transactions t
      ON p.id = 'T' || t.row_seq::text
   WHERE t.channel NOT IN ('card_purchase', 'merchant')
      OR p.is_fraud::integer IS DISTINCT FROM t.is_fraud::integer
      OR p.transaction_time::timestamp IS DISTINCT FROM t.ts
      OR p.amount::double precision IS DISTINCT FROM t.amount;
  IF problem_count <> 0 THEN
    RAISE EXCEPTION
      'Payment_Transaction rows disagree with raw channel/label/time/amount: %',
      problem_count;
  END IF;

  SELECT count(*)
    INTO problem_count
    FROM public.transactions t
    LEFT JOIN card_fraud."cf_Payment_Transaction" p
      ON p.id = 'T' || t.row_seq::text
   WHERE t.channel IN ('card_purchase', 'merchant')
     AND p.id IS NULL;
  IF problem_count <> 0 THEN
    RAISE EXCEPTION 'raw card-view rows missing Payment_Transaction rows: %',
      problem_count;
  END IF;

  SELECT count(*)
    INTO problem_count
    FROM card_fraud."cf_Payment_Transaction" p
    FULL JOIN card_fraud."cf_Card_Send_Transaction" c
      ON c.txn_id = p.id
   WHERE p.id IS NULL OR c.txn_id IS NULL;
  IF problem_count <> 0 THEN
    RAISE EXCEPTION 'Payment/Card_Send identity mismatch rows: %', problem_count;
  END IF;

  SELECT count(*)
    INTO problem_count
    FROM card_fraud."cf_Card_Send_Transaction" c
    JOIN card_fraud."cf_Payment_Transaction" p
      ON p.id = c.txn_id
    LEFT JOIN card_fraud."cf_Card" card
      ON card.card_number = c.card_number
   WHERE c.card_number IS NULL
      OR card.card_number IS NULL
      OR c.edge_unix_time::bigint IS DISTINCT FROM p.unix_time::bigint;
  IF problem_count <> 0 THEN
    RAISE EXCEPTION 'Card_Send endpoint/time integrity failures: %',
      problem_count;
  END IF;

  SELECT count(*)
    INTO problem_count
    FROM card_fraud."cf_Payment_Transaction" p
    FULL JOIN card_fraud."cf_Merchant_Receive_Transaction" m
      ON m.txn_id = p.id
   WHERE p.id IS NULL OR m.txn_id IS NULL;
  IF problem_count <> 0 THEN
    RAISE EXCEPTION 'Payment/Merchant_Receive identity mismatch rows: %',
      problem_count;
  END IF;

  SELECT count(*)
    INTO problem_count
    FROM card_fraud."cf_Merchant_Receive_Transaction" m
    JOIN card_fraud."cf_Payment_Transaction" p
      ON p.id = m.txn_id
    LEFT JOIN card_fraud."cf_Merchant" merchant
      ON merchant.id = m.merchant_id
   WHERE m.merchant_id IS NULL
      OR merchant.id IS NULL
      OR m.edge_unix_time::bigint IS DISTINCT FROM p.unix_time::bigint;
  IF problem_count <> 0 THEN
    RAISE EXCEPTION 'Merchant_Receive endpoint/time integrity failures: %',
      problem_count;
  END IF;

  SELECT min(transaction_time::timestamp), max(transaction_time::timestamp)
    INTO min_payment_ts, max_payment_ts
    FROM card_fraud."cf_Payment_Transaction";
  IF min_payment_ts < current_setting('phantomledger.expected_start')::timestamp
     OR max_payment_ts >= current_setting('phantomledger.expected_end')::timestamp THEN
    RAISE EXCEPTION 'payment timestamp range is outside requested window: % to %',
      min_payment_ts, max_payment_ts;
  END IF;

  SELECT count(*) INTO problem_count
    FROM card_fraud."cf_Payment_Transaction"
   WHERE is_fraud IS NULL OR is_fraud::integer NOT IN (0, 1);
  IF problem_count <> 0 THEN
    RAISE EXCEPTION 'non-binary transaction fraud labels: %', problem_count;
  END IF;

  SELECT count(*) INTO problem_count
    FROM card_fraud."cf_Payment_Transaction"
   WHERE is_fraud::integer = 1;
  IF problem_count = 0 THEN
    RAISE EXCEPTION 'card-fraud corpus contains no positive transaction labels';
  END IF;

  -- ------------------------------------------------ THE LABEL-LEAK GATE
  -- card-fraud-realism-v2, gate 1 of docs/card_fraud_online_gnn.md.
  -- Card.is_fraud, Party.is_fraud, Device.is_blocked and IP.is_blocked
  -- are FULL-WINDOW entity verdicts. They keep their columns so
  -- TF_GNN_v3 loading jobs map positionally, and they must be withheld
  -- (0) so nothing in the feature graph answers the training question
  -- before the model sees a transaction.
  SELECT
      (SELECT count(*) FROM card_fraud."cf_Card"
        WHERE is_fraud::integer <> 0)
    + (SELECT count(*) FROM card_fraud."cf_Party"
        WHERE is_fraud::integer <> 0)
    + (SELECT count(*) FROM card_fraud."cf_Device"
        WHERE is_blocked::integer <> 0)
    + (SELECT count(*) FROM card_fraud."cf_IP"
        WHERE is_blocked::integer <> 0)
    INTO problem_count;
  IF problem_count <> 0 THEN
    RAISE EXCEPTION
      'entity label leak: % vertex rows carry a full-window verdict',
      problem_count;
  END IF;

  -- Withholding is not deletion. The corpus has positive transaction
  -- labels (checked above), so the quarantined overlay must carry the
  -- cards they touched, and every overlay id must join its vertex.
  SELECT count(*) INTO problem_count
    FROM card_fraud."cf_Ground_Truth_Label"
   WHERE entity_type = 'card' AND label = 'ever_fraud';
  IF problem_count = 0 THEN
    RAISE EXCEPTION
      'ground-truth overlay carries no ever_fraud cards although the '
      'corpus has positive transaction labels';
  END IF;

  SELECT count(*) INTO problem_count
    FROM card_fraud."cf_Ground_Truth_Label" g
    LEFT JOIN card_fraud."cf_Card" c ON c.card_number = g.entity_id
   WHERE g.entity_type = 'card' AND c.card_number IS NULL;
  IF problem_count <> 0 THEN
    RAISE EXCEPTION 'ground-truth card ids joining no Card vertex: %',
      problem_count;
  END IF;

  SELECT count(*) INTO problem_count
    FROM card_fraud."cf_Ground_Truth_Label" g
    LEFT JOIN card_fraud."cf_Party" p ON p.id = g.entity_id
   WHERE g.entity_type = 'party' AND p.id IS NULL;
  IF problem_count <> 0 THEN
    RAISE EXCEPTION 'ground-truth party ids joining no Party vertex: %',
      problem_count;
  END IF;

  SELECT count(*) INTO problem_count
    FROM card_fraud."cf_Ground_Truth_Label"
   WHERE entity_type NOT IN ('card', 'party', 'device', 'ip')
      OR label NOT IN ('ever_fraud', 'fraud_actor', 'flagged',
                       'blacklisted');
  IF problem_count <> 0 THEN
    RAISE EXCEPTION 'ground-truth rows outside the declared vocabulary: %',
      problem_count;
  END IF;

  SELECT count(*) INTO raw_count FROM public.transactions;
  SELECT status, txn_rows
    INTO manifest_status, manifest_rows
    FROM public.pl_run_manifest
   WHERE population = current_setting('phantomledger.expected_population')::integer
     AND days = current_setting('phantomledger.expected_days')::integer
     AND start_date = current_setting('phantomledger.expected_start')
   ORDER BY id DESC
   LIMIT 1;
  IF NOT FOUND THEN
    RAISE EXCEPTION 'no run manifest for population=%/days=%/start=%',
      current_setting('phantomledger.expected_population'),
      current_setting('phantomledger.expected_days'),
      current_setting('phantomledger.expected_start');
  END IF;
  IF manifest_status <> 'complete' OR manifest_rows IS NULL
     OR manifest_rows <> raw_count THEN
    RAISE EXCEPTION 'manifest/raw mismatch: status %, manifest %, raw %',
      manifest_status, manifest_rows, raw_count;
  END IF;
END
$acceptance$;

SELECT
  (SELECT count(*) FROM public.transactions) AS ledger_rows,
  count(*) AS payment_rows,
  count(*) FILTER (WHERE is_fraud::integer = 1) AS fraud_rows,
  round(
    100.0 * count(*) FILTER (WHERE is_fraud::integer = 1)
    / NULLIF(count(*), 0),
    5
  ) AS fraud_percent,
  min(transaction_time::timestamp) AS first_payment,
  max(transaction_time::timestamp) AS last_payment
FROM card_fraud."cf_Payment_Transaction";

-- The investigative overlay, reported so the operator sees where the
-- entity labels went. NOTHING here may be joined into model features.
SELECT entity_type, label, count(*) AS entities
FROM card_fraud."cf_Ground_Truth_Label"
GROUP BY entity_type, label
ORDER BY entity_type, label;

COMMIT;
