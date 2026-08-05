\set ON_ERROR_STOP on
\pset pager off
\timing on

-- Read-only investigation of the card-fraud Device / IP endpoint layer.
--
-- Written for the question "the Device vertex count is 0 — where did the
-- devices and IPs go?". It answered that in three moves: prove the vertex and
-- session-edge tables are populated, prove Has_Device / Has_IP are empty by
-- design, and prove the tf_gnn_prep loader layer is where the count collapses.
--
-- THE SECOND MOVE NO LONGER HOLDS: `attacker-infra-2026-07` populated both
-- ownership tables, so the loader's INNER JOIN has something to join and the
-- collapse this file was written to diagnose should no longer occur at all.
-- The file is retained as the endpoint-layer census and as the record of how
-- the collapse was found.
--
-- Nothing here writes. Run after:
--   make run ARGS="--usecase card-fraud ..."
-- Usage:
--   psql "dbname=phantomledger" -f docs/card_fraud_device_ip_investigate.sql
--
-- RUNTIME: sections 1-3 and 5 are fast. Section 4 makes several full passes
-- over the ~20M-row session-edge tables (unindexed UNLOGGED heaps) and takes
-- minutes. \timing is on so each step reports its own cost.
--
-- Section 6 (endpoint sharing fan-out) joins two ~20M-row unindexed UNLOGGED
-- tables and is OFF by default. Turn it on with:
--   psql "dbname=phantomledger" -v heavy=1 \
--        -f docs/card_fraud_device_ip_investigate.sql
--
-- READING THE OUTPUT — the expected shape of a healthy run:
--   cf_Device / cf_IP                       > 0
--   cf_Transaction_Uses_Device / _IP        = cf_Payment_Transaction rows
--   cf_Has_Device / cf_Has_IP               > 0   <- INVERTED, see below
--   orphan endpoints                        = 0
--   is_blocked values other than '0'        = 0   <- CORRECT, label withheld
--
-- THE Has_Device / Has_IP EXPECTATION IS INVERTED as of
-- attacker-infra-2026-07. This file was written when both tables were
-- header-only by design: every customer endpoint had a Party owner and no
-- attacker endpoint did, so "adjacent to a Party" was a deterministic
-- legit-vs-attacker bit and the acceptance script failed the release if
-- either table carried a row.
--
-- That asymmetry was a property of the GENERATOR, and it has been removed
-- there rather than hidden here: the institution's endpoint registry is
-- modelled as INCOMPLETE (~72% device / ~61% address coverage), so
-- legitimate rows arrive on endpoints with no Party edge; and a declared
-- share of unauthorized fraud is operated from the victim's own endpoint
-- or exits through a residential proxy, so fraud arrives on endpoints that
-- have one. The acceptance script now fails the release if either table is
-- EMPTY — Party is TF_GNN_v3's only path to Device and IP, so empty
-- ownership tables leave every endpoint vertex isolated and the whole
-- endpoint layer inert.
--
-- Transaction-time endpoint evidence still lives in
-- cf_Transaction_Uses_Device / cf_Transaction_Uses_IP, and that is still
-- what an online model should score from.
--
-- SECTION 6 IS NOW THE INTERESTING ONE. Endpoint sharing fan-out used to
-- be identically 1 by construction (attacker endpoints were minted one per
-- compromise). It is now heavy-tailed — expect a meaningful share of
-- attacker devices seen by many distinct cards.

\if :{?heavy}
\else
  \set heavy 0
\endif

BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ READ ONLY;

-- ===================================================================
\echo ''
\echo '[1/6] which run wrote these tables'
-- ===================================================================

SELECT id,
       status,
       seed,
       population,
       days,
       start_date,
       txn_rows,
       started,
       finished
  FROM public.pl_run_manifest
 ORDER BY id DESC
 LIMIT 3;

-- The generator rewrites public.pl_direct_tables for a schema the first time
-- it opens a mirror there, so this lists exactly what the LAST card-fraud run
-- wrote. `physical_rows` is the live count; `planner_estimate` is whatever
-- ANALYZE last saw and may be stale or -1 on these UNLOGGED tables.
SELECT reg.table_name,
       cls.reltuples::bigint AS planner_estimate,
       (xpath('/row/c/text()',
              query_to_xml(format('SELECT count(*) AS c FROM %I.%I',
                                  reg.schema_name, reg.table_name),
                           false, true, '')))[1]::text::bigint
         AS physical_rows
  FROM public.pl_direct_tables reg
  JOIN pg_namespace nsp ON nsp.nspname = reg.schema_name
  JOIN pg_class cls ON cls.relnamespace = nsp.oid
                   AND cls.relname = reg.table_name
 WHERE reg.schema_name = 'card_fraud'
   AND (reg.table_name LIKE '%Device%'
     OR reg.table_name LIKE '%IP%'
     OR reg.table_name = 'cf_Payment_Transaction')
 ORDER BY reg.table_name;

-- ===================================================================
\echo ''
\echo '[2/6] device / IP census -- the numbers that answer the question'
-- ===================================================================

-- Note every identifier is double-quoted. TableMirror creates these through
-- escapeIdentifier, so the real names are mixed-case; an unquoted
-- card_fraud.cf_device folds to lowercase and errors with "does not exist".
SELECT 'cf_Device (vertices)'               AS table_name,
       (SELECT count(*) FROM card_fraud."cf_Device")                   AS row_count,
       'expected > 0'                       AS expectation
UNION ALL SELECT 'cf_IP (vertices)',
       (SELECT count(*) FROM card_fraud."cf_IP"),                      'expected > 0'
UNION ALL SELECT 'cf_Transaction_Uses_Device (session edges)',
       (SELECT count(*) FROM card_fraud."cf_Transaction_Uses_Device"), 'expected = Payment_Transaction rows'
UNION ALL SELECT 'cf_Transaction_Uses_IP (session edges)',
       (SELECT count(*) FROM card_fraud."cf_Transaction_Uses_IP"),     'expected = Payment_Transaction rows'
UNION ALL SELECT 'cf_Payment_Transaction',
       (SELECT count(*) FROM card_fraud."cf_Payment_Transaction"),     'the transaction universe'
UNION ALL SELECT 'cf_Has_Device (party ownership)',
       (SELECT count(*) FROM card_fraud."cf_Has_Device"),              'expected > 0 since attacker-infra-2026-07'
UNION ALL SELECT 'cf_Has_IP (party ownership)',
       (SELECT count(*) FROM card_fraud."cf_Has_IP"),                  'expected > 0 since attacker-infra-2026-07';

\echo '-- sample device vertices'
SELECT * FROM card_fraud."cf_Device" ORDER BY id LIMIT 5;

\echo '-- sample IP vertices'
SELECT * FROM card_fraud."cf_IP" ORDER BY id LIMIT 5;

\echo '-- sample session edges (device)'
SELECT * FROM card_fraud."cf_Transaction_Uses_Device" LIMIT 5;

\echo '-- sample session edges (IP)'
SELECT * FROM card_fraud."cf_Transaction_Uses_IP" LIMIT 5;

-- ===================================================================
\echo ''
\echo '[3/6] the 0-vertex proof -- where the loader layer drops them'
-- ===================================================================

-- Skipped cleanly if the tf_gnn_prep schema is not installed.
DO $probe$
DECLARE
  vertices bigint;
  ownership bigint;
  loaded bigint;
BEGIN
  IF to_regclass('tf_gnn_prep.load_devices') IS NULL THEN
    RAISE NOTICE 'tf_gnn_prep.load_devices not present -- skipping section 3';
    RETURN;
  END IF;

  SELECT count(*) INTO vertices  FROM card_fraud."cf_Device";
  SELECT count(*) INTO ownership FROM card_fraud."cf_Has_Device";
  SELECT count(*) INTO loaded    FROM tf_gnn_prep.load_devices;
  RAISE NOTICE 'DEVICE  cf_Device=%  cf_Has_Device=%  tf_gnn_prep.load_devices=%',
    vertices, ownership, loaded;
  IF loaded = 0 AND vertices > 0 AND ownership = 0 THEN
    RAISE NOTICE '  -> confirmed: loaded_devices INNER JOINs cf_Device against';
    RAISE NOTICE '     loaded_party_has_device (= cf_Has_Device; POPULATED since attacker-infra-2026-07),';
    RAISE NOTICE '     so the vertex set collapses to zero at that join.';
  END IF;

  SELECT count(*) INTO vertices  FROM card_fraud."cf_IP";
  SELECT count(*) INTO ownership FROM card_fraud."cf_Has_IP";
  SELECT count(*) INTO loaded    FROM tf_gnn_prep.load_ips;
  RAISE NOTICE 'IP      cf_IP=%  cf_Has_IP=%  tf_gnn_prep.load_ips=%',
    vertices, ownership, loaded;

  IF to_regclass('tf_gnn_prep.load_transaction_uses_device') IS NULL THEN
    RAISE NOTICE 'GAP: tf_gnn_prep has NO loader view over';
    RAISE NOTICE '     cf_Transaction_Uses_Device / cf_Transaction_Uses_IP --';
    RAISE NOTICE '     the only tables carrying real endpoint evidence.';
  END IF;
END
$probe$;

-- ===================================================================
\echo ''
\echo '[4/6] endpoint integrity -- every session edge resolves to a vertex'
-- ===================================================================

SELECT 'device endpoints observed in the card view' AS metric,
       (SELECT count(DISTINCT device_id)
          FROM card_fraud."cf_Transaction_Uses_Device")               AS value
UNION ALL SELECT 'device vertex universe (roster + observed)',
       (SELECT count(*) FROM card_fraud."cf_Device")
UNION ALL SELECT 'ORPHAN device edges (endpoint with no vertex) -- expect 0',
       (SELECT count(*)
          FROM (SELECT DISTINCT e.device_id
                  FROM card_fraud."cf_Transaction_Uses_Device" e
                  LEFT JOIN card_fraud."cf_Device" v ON v.id = e.device_id
                 WHERE v.id IS NULL) orphans)
UNION ALL SELECT 'IP endpoints observed in the card view',
       (SELECT count(DISTINCT ip_id) FROM card_fraud."cf_Transaction_Uses_IP")
UNION ALL SELECT 'IP vertex universe (roster + observed)',
       (SELECT count(*) FROM card_fraud."cf_IP")
UNION ALL SELECT 'ORPHAN IP edges (endpoint with no vertex) -- expect 0',
       (SELECT count(*)
          FROM (SELECT DISTINCT e.ip_id
                  FROM card_fraud."cf_Transaction_Uses_IP" e
                  LEFT JOIN card_fraud."cf_IP" v ON v.id = e.ip_id
                 WHERE v.id IS NULL) orphans);

-- Endpoint degree: how many transactions each endpoint carries. A single
-- group-by, no join. Isolated vertices (degree 0) are roster endpoints whose
-- owner never produced a card-view row -- they exist, they are simply unused.
\echo '-- device degree distribution over the card view'
SELECT min(txns) AS min_txns,
       percentile_disc(0.5) WITHIN GROUP (ORDER BY txns) AS median_txns,
       max(txns) AS max_txns,
       count(*) AS devices_used
  FROM (SELECT device_id, count(*) AS txns
          FROM card_fraud."cf_Transaction_Uses_Device"
         GROUP BY device_id) degree;

\echo '-- IP degree distribution over the card view'
SELECT min(txns) AS min_txns,
       percentile_disc(0.5) WITHIN GROUP (ORDER BY txns) AS median_txns,
       max(txns) AS max_txns,
       count(*) AS ips_used
  FROM (SELECT ip_id, count(*) AS txns
          FROM card_fraud."cf_Transaction_Uses_IP"
         GROUP BY ip_id) degree;

-- ===================================================================
\echo ''
\echo '[5/6] labels -- withheld on the vertex, retained in the overlay'
-- ===================================================================

-- Device.is_blocked and IP.is_blocked are FULL-WINDOW entity verdicts, so the
-- exporter writes a constant 0 and moves the real verdicts to the quarantined
-- cf_Ground_Truth_Label overlay. Both counts below being 0 is CORRECT.
SELECT 'cf_Device rows with is_blocked <> 0 -- expect 0' AS metric,
       (SELECT count(*) FROM card_fraud."cf_Device"
         WHERE is_blocked IS DISTINCT FROM '0')                       AS value
UNION ALL SELECT 'cf_IP rows with is_blocked <> 0 -- expect 0',
       (SELECT count(*) FROM card_fraud."cf_IP"
         WHERE is_blocked IS DISTINCT FROM '0');

\echo '-- the quarantined ground-truth overlay (positives only)'
SELECT entity_type,
       label,
       count(*) AS positives
  FROM card_fraud."cf_Ground_Truth_Label"
 GROUP BY entity_type, label
 ORDER BY entity_type, label;

-- Flagged devices / blacklisted IPs that the card view actually touched.
-- These are the entities an investigative evaluation can score against.
SELECT 'flagged devices, total' AS metric,
       (SELECT count(*) FROM card_fraud."cf_Ground_Truth_Label"
         WHERE entity_type = 'device')                                AS value
UNION ALL SELECT 'flagged devices seen in the card view',
       (SELECT count(*)
          FROM card_fraud."cf_Ground_Truth_Label" g
         WHERE g.entity_type = 'device'
           AND EXISTS (SELECT 1 FROM card_fraud."cf_Transaction_Uses_Device" e
                        WHERE e.device_id = g.entity_id))
UNION ALL SELECT 'blacklisted IPs, total',
       (SELECT count(*) FROM card_fraud."cf_Ground_Truth_Label"
         WHERE entity_type = 'ip')
UNION ALL SELECT 'blacklisted IPs seen in the card view',
       (SELECT count(*)
          FROM card_fraud."cf_Ground_Truth_Label" g
         WHERE g.entity_type = 'ip'
           AND EXISTS (SELECT 1 FROM card_fraud."cf_Transaction_Uses_IP" e
                        WHERE e.ip_id = g.entity_id));

-- ===================================================================
\echo ''
\echo '[6/6] endpoint sharing fan-out (heavy; -v heavy=1 to run)'
-- ===================================================================

\if :heavy
-- Joins cf_Transaction_Uses_Device to cf_Card_Send_Transaction across ~20M
-- rows on each side, unindexed. Minutes, not seconds. This is the structure a
-- GNN actually exploits: one endpoint touching several cards.
\echo '-- devices by number of distinct cards'
SELECT cards_per_device,
       count(*) AS devices
  FROM (SELECT d.device_id, count(DISTINCT c.card_number) AS cards_per_device
          FROM card_fraud."cf_Transaction_Uses_Device" d
          JOIN card_fraud."cf_Card_Send_Transaction" c ON c.txn_id = d.txn_id
         GROUP BY d.device_id) fanout
 GROUP BY cards_per_device
 ORDER BY cards_per_device
 LIMIT 25;

\echo '-- IPs by number of distinct cards'
SELECT cards_per_ip,
       count(*) AS ips
  FROM (SELECT i.ip_id, count(DISTINCT c.card_number) AS cards_per_ip
          FROM card_fraud."cf_Transaction_Uses_IP" i
          JOIN card_fraud."cf_Card_Send_Transaction" c ON c.txn_id = i.txn_id
         GROUP BY i.ip_id) fanout
 GROUP BY cards_per_ip
 ORDER BY cards_per_ip
 LIMIT 25;
\else
\echo 'skipped -- re-run with -v heavy=1 to include it'
\endif

COMMIT;

\echo ''
\echo 'Investigation complete.'
\echo 'If cf_Device/cf_IP are populated and tf_gnn_prep.load_devices/load_ips'
\echo 'are 0, apply docs/tf_gnn_prep_session_endpoints.sql.'
