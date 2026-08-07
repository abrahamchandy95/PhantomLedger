\set ON_ERROR_STOP on
\pset pager off
\timing on

\if :{?expected_population}
\else
  \set expected_population 6000
\endif
\if :{?expected_days}
\else
  \set expected_days 7305
\endif
\if :{?expected_start}
\else
  \set expected_start 2000-01-01
\endif
\if :{?expected_end}
\else
  \set expected_end 2020-01-01
\endif
\if :{?expected_seed}
\else
  \set expected_seed 3735928559
\endif

-- Read-only release gate for the requested card-fraud corpus. The generator
-- creates its bulk tables UNLOGGED; a PASS validates current contents but
-- does not make them crash-durable. Back up or promote the accepted tables
-- before treating this as a persistent production artifact.
-- Run only after:
--   make run ARGS="--start 2000-01-01 --days 7305 --population 6000 --seed 3735928559 --usecase card-fraud"
-- Usage:
--   psql "dbname=phantomledger" -f docs/card_fraud_postgres_acceptance.sql
--
-- 39 tables: the original 34 TF_GNN_v3 graph tables, the two timestamped
-- transaction-session edges, cf_Ground_Truth_Label (the quarantined
-- investigative overlay that holds the four full-window entity verdicts
-- the graph no longer carries), and the email LSH minhash pair
-- (cf_Email_Minhash buckets + cf_Has_Email_Minhash edges).

BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ READ ONLY;

SET LOCAL phantomledger.expected_population TO :'expected_population';
SET LOCAL phantomledger.expected_days TO :'expected_days';
SET LOCAL phantomledger.expected_start TO :'expected_start';
SET LOCAL phantomledger.expected_end TO :'expected_end';
SET LOCAL phantomledger.expected_seed TO :'expected_seed';

DO $acceptance$
DECLARE
  registered_count bigint;
  physical_count bigint;
  payment_count bigint;
  party_count bigint;
  card_edge_count bigint;
  merchant_edge_count bigint;
  device_edge_count bigint;
  ip_edge_count bigint;
  raw_count bigint;
  membership_hidden_raw_count bigint;
  manifest_rows bigint;
  manifest_status text;
  problem_count bigint;
  min_payment_ts timestamp;
  max_payment_ts timestamp;
BEGIN
  IF current_setting('phantomledger.expected_start')::date
       + current_setting('phantomledger.expected_days')::integer
     <> current_setting('phantomledger.expected_end')::date THEN
    RAISE EXCEPTION
      'acceptance variables do not describe one half-open window: % + % days != %',
      current_setting('phantomledger.expected_start'),
      current_setting('phantomledger.expected_days'),
      current_setting('phantomledger.expected_end');
  END IF;

  RAISE NOTICE '[1/9] checking registered and physical table sets';
  SELECT count(*)
    INTO registered_count
    FROM public.pl_direct_tables
   WHERE schema_name = 'card_fraud';
  -- 43, and THIS NUMBER WENT STALE ONCE ALREADY — it stood at 39 while the
  -- export grew to 43 across merchant-coordinates-2026-07 (+cf_Merchant_Location)
  -- and party-geography-2026-07 (+cf_Has_Std_City/_Postcode/_State). Both
  -- rounds updated the MANIFEST below and neither updated this scalar, so this
  -- script would have ABORTED A CORRECT CORPUS with "expected 39 ... found 43".
  -- `test_table_golden`'s own check was `>= 39`, which 43 satisfies, so nothing
  -- caught it.
  --
  -- The source of truth is now `kTableCount` in
  -- include/phantomledger/exporter/card_fraud/schema.hpp, and
  -- `test_table_golden` asserts the live registry equals it EXACTLY. This file
  -- cannot include that header, so it must be updated in the same commit —
  -- which is precisely why the constant's comment names this file.
  IF registered_count <> 43 THEN
    RAISE EXCEPTION 'expected 43 registered card_fraud tables, found % (source of truth: schema.hpp kTableCount)',
      registered_count;
  END IF;

  WITH expected(table_name) AS (
    VALUES
      ('cf_Address'),
      ('cf_Assigned_To'),
      ('cf_Card'),
      ('cf_Card_Send_Transaction'),
      ('cf_City'),
      ('cf_DOB'),
      ('cf_Device'),
      ('cf_Email'),
      ('cf_Email_Minhash'),
      ('cf_Full_Name'),
      ('cf_Ground_Truth_Label'),
      ('cf_Has_Address'),
      ('cf_Has_City'),
      ('cf_Has_DOB'),
      ('cf_Has_Device'),
      ('cf_Has_Email'),
      ('cf_Has_Email_Minhash'),
      ('cf_Has_Full_Name'),
      ('cf_Has_ID'),
      ('cf_Has_IP'),
      ('cf_Has_Phone'),
      ('cf_Has_State'),
      ('cf_Has_Std_City'),
      ('cf_Has_Std_Postcode'),
      ('cf_Has_Std_State'),
      ('cf_Has_Zip'),
      ('cf_ID'),
      ('cf_IP'),
      ('cf_Is_Merchant'),
      ('cf_Located_In'),
      ('cf_Merchant'),
      ('cf_Merchant_Assigned'),
      ('cf_Merchant_Category'),
      ('cf_Merchant_Location'),
      ('cf_Merchant_Receive_Transaction'),
      ('cf_Party'),
      ('cf_Party_Has_Card'),
      ('cf_Payment_Transaction'),
      ('cf_Phone'),
      ('cf_State'),
      ('cf_Transaction_Uses_Device'),
      ('cf_Transaction_Uses_IP'),
      ('cf_Zipcode')
  ),
  actual AS (
    SELECT table_name
      FROM public.pl_direct_tables
     WHERE schema_name = 'card_fraud'
  )
  SELECT count(*)
    INTO problem_count
    FROM (
      (SELECT table_name FROM expected
       EXCEPT
       SELECT table_name FROM actual)
      UNION ALL
      (SELECT table_name FROM actual
       EXCEPT
       SELECT table_name FROM expected)
    ) AS manifest_difference;
  IF problem_count <> 0 THEN
    RAISE EXCEPTION
      'registered card_fraud table manifest differs from the declared 43-table contract: % entries',
      problem_count;
  END IF;

  SELECT count(*)
    INTO physical_count
    FROM information_schema.tables
   WHERE table_schema = 'card_fraud'
     AND table_type = 'BASE TABLE'
     AND table_name LIKE 'cf\_%' ESCAPE '\';
  -- 43, same source of truth (schema.hpp kTableCount) and the same staleness
  -- history as the registered count above.
  IF physical_count <> 43 THEN
    RAISE EXCEPTION 'expected 43 physical card_fraud tables, found % (source of truth: schema.hpp kTableCount)',
      physical_count;
  END IF;

  SELECT count(*)
    INTO problem_count
    FROM (
      (SELECT table_name
         FROM public.pl_direct_tables
        WHERE schema_name = 'card_fraud'
       EXCEPT
       SELECT table_name
         FROM information_schema.tables
        WHERE table_schema = 'card_fraud'
          AND table_type = 'BASE TABLE'
          AND table_name LIKE 'cf\_%' ESCAPE '\')
      UNION ALL
      (SELECT table_name
         FROM information_schema.tables
        WHERE table_schema = 'card_fraud'
          AND table_type = 'BASE TABLE'
          AND table_name LIKE 'cf\_%' ESCAPE '\'
       EXCEPT
       SELECT table_name
         FROM public.pl_direct_tables
        WHERE schema_name = 'card_fraud')
    ) AS table_set_difference;
  IF problem_count <> 0 THEN
    RAISE EXCEPTION
      'registered and physical card_fraud table identities differ: % entries',
      problem_count;
  END IF;

  RAISE NOTICE
    '[2/9] scanning Payment and four transaction-edge row counts';
  SELECT count(*) INTO payment_count
    FROM card_fraud."cf_Payment_Transaction";
  SELECT count(*) INTO card_edge_count
    FROM card_fraud."cf_Card_Send_Transaction";
  SELECT count(*) INTO merchant_edge_count
    FROM card_fraud."cf_Merchant_Receive_Transaction";
  SELECT count(*) INTO device_edge_count
    FROM card_fraud."cf_Transaction_Uses_Device";
  SELECT count(*) INTO ip_edge_count
    FROM card_fraud."cf_Transaction_Uses_IP";
  IF payment_count = 0 OR payment_count <> card_edge_count
     OR payment_count <> merchant_edge_count
     OR payment_count <> device_edge_count
     OR payment_count <> ip_edge_count THEN
    RAISE EXCEPTION
      'payment/card/merchant/device/IP edge counts differ or are empty: %/%/%/%/%',
      payment_count, card_edge_count, merchant_edge_count,
      device_edge_count, ip_edge_count;
  END IF;

  SELECT count(*) INTO party_count
    FROM card_fraud."cf_Party";
  IF party_count
       <> current_setting('phantomledger.expected_population')::bigint THEN
    RAISE EXCEPTION 'Party rows do not match expected population: % vs %',
      party_count, current_setting('phantomledger.expected_population');
  END IF;

  RAISE NOTICE '[3/9] checking Payment_Transaction id uniqueness';
  SELECT count(*) - count(DISTINCT id)
    INTO problem_count
    FROM card_fraud."cf_Payment_Transaction";
  IF problem_count <> 0 THEN
    RAISE EXCEPTION 'duplicate Payment_Transaction ids: %', problem_count;
  END IF;

  RAISE NOTICE '[4/9] checking bidirectional raw-ledger payment parity';
  -- NON-VACUITY FIRST, AND IT IS HERE BECAUSE THIS CHECK ONCE PASSED ON AN
  -- EMPTY SET FOR AN ENTIRE ROUND.
  --
  -- The mirror COPYs with (FORMAT csv), and in CSV mode an unquoted empty
  -- field is read as NULL -- not as the empty string the exporter wrote. So
  -- `p.error = ''` is NULL for every settled row, never true, and both parity
  -- checks below counted zero rows and reported PASS while comparing nothing.
  -- The filter is now coalesce()'d; this guard is what makes the failure
  -- LOUD if the representation shifts again.
  SELECT count(*)
    INTO problem_count
    FROM card_fraud."cf_Payment_Transaction" p
   WHERE coalesce(p.error, '') = '';
  IF problem_count = 0 THEN
    RAISE EXCEPTION 'no SETTLED payment rows matched the parity filter, so '
      'the two checks below would compare an empty set. Either the corpus '
      'is empty or the error column no longer represents "settled" as NULL '
      'or empty string.';
  END IF;
  RAISE NOTICE '  settled rows entering parity: %', problem_count;
  -- SETTLED ROWS ONLY, and the filter is the contract rather than a
  -- convenience. A non-empty `error` marks a DECLINED AUTHORIZATION: an
  -- attempt the issuer refused, which by definition never posted and so has
  -- no raw-ledger counterpart to join to. Declines carry a suffixed id
  -- ('T<n>D') precisely so they cannot collide with a settled row_seq, and
  -- every consumer that means "settled" -- this parity check, the manifest
  -- reconciliation, and the graph loader itself -- means coalesce(error,'')=''.
  --
  -- IT IS NULL, NOT '', AND THAT COST AN ENTIRE ROUND. The exporter writes an
  -- empty string; the mirror COPYs with (FORMAT csv); CSV reads an unquoted
  -- empty field as NULL. Any consumer written against the CSV FILES sees '',
  -- any consumer written against POSTGRES sees NULL, and the two are not
  -- interchangeable in a WHERE clause. Always coalesce.
  SELECT count(*)
    INTO problem_count
    FROM card_fraud."cf_Payment_Transaction" p
    LEFT JOIN public.transactions t
      ON p.id = 'T' || t.row_seq::text
   WHERE coalesce(p.error, '') = ''
     AND t.row_seq IS NULL;
  IF problem_count <> 0 THEN
    RAISE EXCEPTION 'Payment_Transaction rows missing raw-ledger joins: %',
      problem_count;
  END IF;

  SELECT count(*)
    INTO problem_count
    FROM card_fraud."cf_Payment_Transaction" p
    JOIN public.transactions t
      ON p.id = 'T' || t.row_seq::text
   WHERE coalesce(p.error, '') = ''
     AND (t.channel NOT IN ('card_purchase', 'merchant')
      OR p.is_fraud::integer IS DISTINCT FROM t.is_fraud::integer
      OR p.transaction_time::timestamp IS DISTINCT FROM t.ts
      OR p.unix_time::bigint IS DISTINCT
         FROM extract(epoch FROM t.ts)::bigint
      OR p.amount::double precision IS DISTINCT FROM t.amount);
  IF problem_count <> 0 THEN
    RAISE EXCEPTION
      'Payment_Transaction rows disagree with raw channel/label/time/amount: %',
      problem_count;
  END IF;

  -- The raw ledger is a generation/audit stream and intentionally retains
  -- rows outside customer membership. The card feature graph is the causal
  -- view and filters those rows at transaction time, so reverse parity is
  -- not equality. Count the withheld raw rows for operator visibility.
  SELECT count(*)
    INTO membership_hidden_raw_count
    FROM public.transactions t
    LEFT JOIN card_fraud."cf_Payment_Transaction" p
      ON p.id = 'T' || t.row_seq::text
   WHERE t.channel IN ('card_purchase', 'merchant')
     AND p.id IS NULL;

  RAISE NOTICE
    'raw card-channel rows withheld by point-in-time membership: %',
    membership_hidden_raw_count;

  -- Every exported payment must still resolve to an owned card and may not
  -- precede the owner's membership start. Account closure is not a loaded
  -- TF_GNN_v3 attribute (exporting the future close timestamp would leak);
  -- its exclusive bound is enforced in generation/export regression gates.
  SELECT count(*)
    INTO problem_count
    FROM card_fraud."cf_Payment_Transaction" p
    JOIN card_fraud."cf_Card_Send_Transaction" c
      ON c.txn_id = p.id
    LEFT JOIN card_fraud."cf_Party_Has_Card" h
      ON h.card_number = c.card_number
    LEFT JOIN card_fraud."cf_Party" party
      ON party.id = h.party_id
   WHERE h.party_id IS NULL
      OR party.id IS NULL
      OR p.transaction_time::timestamp < party.created_at::timestamp;
  IF problem_count <> 0 THEN
    RAISE EXCEPTION
      'exported payments missing an owner or preceding owner membership: %',
      problem_count;
  END IF;

  RAISE NOTICE '[5/9] checking Card_Send identity, endpoint, and time parity';
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

  -- card-churn-2026-07: REISSUE GENERATIONS ARE PRESENT AND MONOTONE IN TIME.
  --
  -- One cf_Card vertex per OBSERVED generation, so an account whose card was
  -- replaced mid-window holds several card_numbers distinguished by a `-G<n>`
  -- suffix. Two things are checked and neither is a row count.
  --
  -- First, that reissue happened at all. Before this round the exported card
  -- number was a pure function of the account and this query returned ZERO for
  -- every window, which is what the check exists to detect if the schedule
  -- ever stops reaching the exporter. It is a floor and not an equality: the
  -- realized number depends on window length, and on a window shorter than the
  -- 36-month minimum validity only the unscheduled hazard can fire.
  SELECT count(*)
    INTO problem_count
    FROM card_fraud."cf_Card"
   WHERE card_number LIKE '%-G%';
  IF problem_count = 0 THEN
    RAISE WARNING 'no reissued card generations exported. Expected on a window under a few months (only the 0.07/yr unscheduled hazard can fire); on the owner target window this is a DEFECT — the pre-round state was exactly one card number per account forever';
  END IF;

  -- Second, and this is the load-bearing half: a later generation must never
  -- transact before an earlier one. Generations tile the window in order, so
  -- max(time) of -G<n> must precede min(time) of -G<n+1> on the same account.
  -- An overlap here would mean `generationAt` resolved ambiguously and the
  -- graph carries two live cards for one account at one instant.
  WITH spans AS (
    SELECT split_part(c.card_number, '-G', 1) AS account,
           COALESCE(NULLIF(split_part(c.card_number, '-G', 2), ''), '0')::int
             AS generation,
           min(p.unix_time::bigint) AS first_seen,
           max(p.unix_time::bigint) AS last_seen
      FROM card_fraud."cf_Card_Send_Transaction" c
      JOIN card_fraud."cf_Payment_Transaction" p
        ON p.id = c.txn_id
     GROUP BY 1, 2
  )
  SELECT count(*)
    INTO problem_count
    FROM spans a
    JOIN spans b
      ON b.account = a.account
     AND b.generation = a.generation + 1
   WHERE a.last_seen >= b.first_seen;
  IF problem_count <> 0 THEN
    RAISE EXCEPTION 'card generations OVERLAP in time on % account(s): a later generation transacts before its predecessor stops', problem_count;
  END IF;

  -- relocation-2026-07: PARTY HOME TENURES ARE COVERING AND ORDERED.
  --
  -- cf_Has_Std_* carries one row per occupied home tenure, each stamped with
  -- the epoch it began. Two properties matter and neither is a row count.
  --
  -- First, EVERY party is covered. A party with no home tenure has no
  -- exported geography at all, which silently drops them from every distance
  -- feature rather than failing loudly.
  SELECT count(*)
    INTO problem_count
    FROM card_fraud."cf_Party" p
    LEFT JOIN card_fraud."cf_Has_Std_City" h ON h.party_id = p.id
   WHERE h.party_id IS NULL;
  IF problem_count <> 0 THEN
    RAISE EXCEPTION 'parties with no home-city tenure: %', problem_count;
  END IF;

  -- Second, the three edge tables agree row for row. They are written from ONE
  -- loop over the same tenures, so a divergence means a mover received a
  -- current city beside a stale state — wrong in the direction that still
  -- joins cleanly and returns a plausible distance.
  SELECT count(*)
    INTO problem_count
    FROM (
      SELECT (SELECT count(*) FROM card_fraud."cf_Has_Std_City")     AS c,
             (SELECT count(*) FROM card_fraud."cf_Has_Std_Postcode") AS z,
             (SELECT count(*) FROM card_fraud."cf_Has_Std_State")    AS s
    ) counts
   WHERE c <> z OR c <> s;
  IF problem_count <> 0 THEN
    RAISE EXCEPTION 'cf_Has_Std_City / _Postcode / _State row counts disagree; they are written from one tenure loop';
  END IF;

  -- And no tenure may predate the corpus. `since_unix_time` before the first
  -- exported transaction would leave the earliest rows with no resolvable
  -- home.
  SELECT count(*)
    INTO problem_count
    FROM card_fraud."cf_Has_Std_City" h
   WHERE h.since_unix_time::bigint <
         (SELECT min(unix_time::bigint) FROM card_fraud."cf_Payment_Transaction");
  IF problem_count <> 0 THEN
    RAISE WARNING 'home tenures starting before the first exported payment: % (expected: tenure 0 is stamped at the WINDOW start, which precedes the first transaction — investigate only if the count exceeds the party count)', problem_count;
  END IF;

  RAISE NOTICE
    '[6/9] checking Merchant_Receive identity, endpoint, and time parity';
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

  RAISE NOTICE
    '[6/9a] checking Transaction_Uses_Device identity, endpoint, and time parity';
  SELECT count(*)
    INTO problem_count
    FROM card_fraud."cf_Payment_Transaction" p
    FULL JOIN card_fraud."cf_Transaction_Uses_Device" d
      ON d.txn_id = p.id
   WHERE p.id IS NULL OR d.txn_id IS NULL;
  IF problem_count <> 0 THEN
    RAISE EXCEPTION
      'Payment/Transaction_Uses_Device identity mismatch rows: %',
      problem_count;
  END IF;

  SELECT count(*)
    INTO problem_count
    FROM card_fraud."cf_Transaction_Uses_Device" d
    JOIN card_fraud."cf_Payment_Transaction" p
      ON p.id = d.txn_id
    LEFT JOIN card_fraud."cf_Device" device
      ON device.id = d.device_id
   WHERE d.device_id IS NULL
      OR device.id IS NULL
      OR d.edge_unix_time::bigint IS DISTINCT FROM p.unix_time::bigint;
  IF problem_count <> 0 THEN
    RAISE EXCEPTION
      'Transaction_Uses_Device endpoint/time integrity failures: %',
      problem_count;
  END IF;

  RAISE NOTICE
    '[6/9b] checking Transaction_Uses_IP identity, endpoint, and time parity';
  SELECT count(*)
    INTO problem_count
    FROM card_fraud."cf_Payment_Transaction" p
    FULL JOIN card_fraud."cf_Transaction_Uses_IP" i
      ON i.txn_id = p.id
   WHERE p.id IS NULL OR i.txn_id IS NULL;
  IF problem_count <> 0 THEN
    RAISE EXCEPTION
      'Payment/Transaction_Uses_IP identity mismatch rows: %',
      problem_count;
  END IF;

  SELECT count(*)
    INTO problem_count
    FROM card_fraud."cf_Transaction_Uses_IP" i
    JOIN card_fraud."cf_Payment_Transaction" p
      ON p.id = i.txn_id
    LEFT JOIN card_fraud."cf_IP" ip
      ON ip.id = i.ip_id
   WHERE i.ip_id IS NULL
      OR ip.id IS NULL
      OR i.edge_unix_time::bigint IS DISTINCT FROM p.unix_time::bigint;
  IF problem_count <> 0 THEN
    RAISE EXCEPTION
      'Transaction_Uses_IP endpoint/time integrity failures: %',
      problem_count;
  END IF;

  RAISE NOTICE '[7/9] checking payment window and transaction labels';
  SELECT count(*)
    INTO problem_count
    FROM public.transactions
   WHERE ts < current_setting('phantomledger.expected_start')::timestamp
      OR ts >= current_setting('phantomledger.expected_end')::timestamp;
  IF problem_count <> 0 THEN
    RAISE EXCEPTION
      'raw ledger contains % rows outside the requested half-open window',
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
  RAISE NOTICE '[8/9] checking entity-label quarantine and overlay integrity';
  -- NULL COUNTS AS A VIOLATION, and that direction is deliberate. These four
  -- are LEAK DETECTORS: they pass when nothing carries a full-window verdict.
  -- Written bare as `is_fraud::integer <> 0`, a NULL yields NULL, the row is
  -- not counted, and the detector goes silent exactly when the column has
  -- stopped meaning what it is supposed to mean. The columns are non-NULL
  -- today -- the exporter writes a literal 0 -- but `error` was also "empty
  -- string" right up until COPY (FORMAT csv) turned it into NULL and made two
  -- parity checks vacuous for a whole round. Fail LOUD instead.
  SELECT
      (SELECT count(*) FROM card_fraud."cf_Card"
        WHERE is_fraud IS NULL OR is_fraud::integer <> 0)
    + (SELECT count(*) FROM card_fraud."cf_Party"
        WHERE is_fraud IS NULL OR is_fraud::integer <> 0)
    + (SELECT count(*) FROM card_fraud."cf_Device"
        WHERE is_blocked IS NULL OR is_blocked::integer <> 0)
    + (SELECT count(*) FROM card_fraud."cf_IP"
        WHERE is_blocked IS NULL OR is_blocked::integer <> 0)
    INTO problem_count;
  IF problem_count <> 0 THEN
    RAISE EXCEPTION
      'entity label leak: % vertex rows carry a full-window verdict',
      problem_count;
  END IF;

  -- ENDPOINT REACHABILITY — THIS CHECK IS INVERTED as of
  -- attacker-infra-2026-07. It used to raise when either table carried
  -- ANY row: every customer endpoint had a Party owner and no attacker
  -- endpoint did, so missing Party adjacency was a deterministic
  -- attacker-role feature and withholding the topology was the only safe
  -- option available to the exporter.
  --
  -- The generator no longer produces that asymmetry. The institution's
  -- endpoint registry is modelled as INCOMPLETE, so legitimate rows sit
  -- on endpoints with no Party edge; and a declared share of
  -- unauthorized cases is operated from the victim's own endpoint or
  -- exits through a residential proxy, so fraud sits on endpoints that
  -- have one. The residual lift is sized and banded in
  -- tests/test_card_endpoint_graph.cpp, which is where that measurement
  -- belongs — a row count cannot make it.
  --
  -- The condition worth hard-failing is now the opposite one. Party is
  -- the ONLY path TF_GNN_v3 has to Device and IP (there is no
  -- transaction->endpoint edge type in that schema), so empty ownership
  -- tables leave every endpoint vertex isolated and the layer inert.
  SELECT
      LEAST((SELECT count(*) FROM card_fraud."cf_Has_Device"),
            (SELECT count(*) FROM card_fraud."cf_Has_IP"))
    INTO problem_count;
  IF problem_count = 0 THEN
    RAISE EXCEPTION
      'unreachable endpoint layer: Has_Device/Has_IP must carry the '
      'party->endpoint associations on file; the smaller of the two is empty';
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

  RAISE NOTICE '[9/9] checking completed run manifest against the raw ledger';
  SELECT count(*) INTO raw_count FROM public.transactions;
  SELECT status, txn_rows
    INTO manifest_status, manifest_rows
    FROM public.pl_run_manifest
   WHERE seed = current_setting('phantomledger.expected_seed')
     AND population = current_setting('phantomledger.expected_population')::integer
     AND days = current_setting('phantomledger.expected_days')::integer
     AND start_date = current_setting('phantomledger.expected_start')
   ORDER BY id DESC
   LIMIT 1;
  IF NOT FOUND THEN
    RAISE EXCEPTION 'no run manifest for seed=%/population=%/days=%/start=%',
      current_setting('phantomledger.expected_seed'),
      current_setting('phantomledger.expected_population'),
      current_setting('phantomledger.expected_days'),
      current_setting('phantomledger.expected_start');
  END IF;
  IF manifest_status <> 'complete' OR manifest_rows IS NULL
     OR manifest_rows <> raw_count THEN
    RAISE EXCEPTION 'manifest/raw mismatch: status %, manifest %, raw %',
      manifest_status, manifest_rows, raw_count;
  END IF;

  RAISE NOTICE
    'PASS: exact acceptance checks complete (ledger %, payment %, fraud-positive)',
    raw_count, payment_count;
END
$acceptance$;

\echo '[summary] scanning ledger and Payment_Transaction totals'
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
\echo '[overlay] reporting quarantined entity-label totals'
SELECT entity_type, label, count(*) AS entities
FROM card_fraud."cf_Ground_Truth_Label"
GROUP BY entity_type, label
ORDER BY entity_type, label;

\echo '[commit] closing the read-only acceptance transaction'
COMMIT;
\echo 'PASS: card-fraud PostgreSQL acceptance completed'
