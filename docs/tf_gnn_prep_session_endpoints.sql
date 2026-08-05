\set ON_ERROR_STOP on
\pset pager off
\timing on

-- Repairs the tf_gnn_prep Device / IP loader path.
--
-- THE DEFECT
--   tf_gnn_prep.load_devices -> loaded_devices INNER JOINs card_fraud."cf_Device"
--   against loaded_party_has_device, which reads card_fraud."cf_Has_Device".
--   That table is header-only BY DESIGN (exporter/card_fraud/schema.hpp:283):
--   exogenous attacker endpoints have no truthful Party owner, so exporting
--   only the customers' ownership edges would make "adjacent to a Party" a
--   deterministic legit-vs-attacker bit, and
--   docs/card_fraud_postgres_acceptance.sql FAILS the release if either table
--   ever carries a row.
--
--   An INNER JOIN against an empty relation yields nothing, so the loader
--   reported 0 Device and 0 IP vertices while cf_Device and cf_IP were fully
--   populated. tf_gnn_prep also had no view of any kind over
--   cf_Transaction_Uses_Device / cf_Transaction_Uses_IP -- the only tables
--   carrying real endpoint evidence -- so there was no edge to attach the
--   vertices by either.
--
-- THE REPAIR
--   Re-anchor the Device / IP vertex sets on the timestamped session edges
--   instead of the withheld ownership edges, and add the two missing loader
--   edge views. This preserves the layer's existing "only load a vertex that
--   has at least one edge" rule (the same shape as loaded_addresses,
--   loaded_emails, loaded_phones) and adds NO Party<->Device edge, so the
--   acceptance gate stays green.
--
--   loaded_party_has_device / loaded_party_has_ip are deliberately left
--   untouched. They still truthfully report cf_Has_Device / cf_Has_IP, and
--   audit_identity_fanout should keep reporting 0 linked parties for device
--   and ip, because that is the true state of the corpus.
--
-- Idempotent and re-runnable. Everything up to the verification block runs in
-- ONE transaction: an error rolls the whole repair back and leaves the prep
-- layer exactly as it was. Apply with:
--   psql "dbname=phantomledger" -f docs/tf_gnn_prep_session_endpoints.sql
--
-- NOTE ON INDEXES: do not add indexes to the card_fraud.* tables to speed
-- this up. The generator DROPs and CREATEs every one of them on each run
-- (exporter/sinks/table_mirror.cpp:105), so any index is silently lost.

BEGIN;

DO $guard$
BEGIN
  IF to_regnamespace('tf_gnn_prep') IS NULL THEN
    RAISE EXCEPTION 'schema tf_gnn_prep does not exist -- install the prep layer first';
  END IF;
  IF to_regclass('card_fraud."cf_Transaction_Uses_Device"') IS NULL THEN
    RAISE EXCEPTION 'card_fraud."cf_Transaction_Uses_Device" is missing -- run --usecase card-fraud first';
  END IF;
  IF to_regclass('tf_gnn_prep.transaction_event_seq') IS NULL THEN
    RAISE EXCEPTION 'tf_gnn_prep.transaction_event_seq is missing -- the live prep schema is incomplete; rebuild the sequence table before wiring the session edges';
  END IF;
END
$guard$;

-- ------------------------------------------------------------------
-- Drop in reverse dependency order so a re-run cannot trip over a
-- changed column list. CREATE OR REPLACE VIEW cannot alter column sets
-- or types, and both change here (see the first_seen note below).
--
-- audit_load_counts is dropped first because it counts load_devices and
-- load_ips; PostgreSQL refuses to drop a view that still has dependents.
-- It is REBUILT DYNAMICALLY at the end of this script from the load_*
-- views that actually exist in the live catalog. (An earlier revision
-- restored it verbatim from the tf_gnn_prep_ddl.sql dump and failed:
-- the live schema does not carry every load_* view the dump does, e.g.
-- load_addresses. The catalog, not the dump, is the source of truth.)
-- No CASCADE anywhere: every drop is named, so nothing can be removed
-- by surprise.
-- ------------------------------------------------------------------

DROP VIEW IF EXISTS tf_gnn_prep.audit_load_counts;
DROP VIEW IF EXISTS tf_gnn_prep.load_devices;
DROP VIEW IF EXISTS tf_gnn_prep.load_ips;
DROP VIEW IF EXISTS tf_gnn_prep.load_transaction_uses_device;
DROP VIEW IF EXISTS tf_gnn_prep.load_transaction_uses_ip;
DROP VIEW IF EXISTS tf_gnn_prep.loaded_devices;
DROP VIEW IF EXISTS tf_gnn_prep.loaded_ips;
DROP VIEW IF EXISTS tf_gnn_prep.loaded_transaction_uses_device;
DROP VIEW IF EXISTS tf_gnn_prep.loaded_transaction_uses_ip;

-- ------------------------------------------------------------------
-- 1. Session-edge base views.
--
-- edge_unix_time comes straight off the corpus column the exporter wrote
-- (exporter/card_fraud/streaming.hpp:254); the acceptance script already
-- pins its parity with Payment_Transaction.unix_time, so no join is needed
-- for the timestamp. Only event_seq requires transaction_event_seq, and it
-- is LEFT JOINed with COALESCE 0 -- the same defensive shape loaded_cards
-- uses for card_first_seen -- so an unpopulated sequence table degrades the
-- ordinal instead of silently emptying the edge set. That silent-empty
-- failure mode is exactly the bug being fixed here; it is not reintroduced.
--
-- No DISTINCT: the exporter writes exactly one session edge per
-- Payment_Transaction row and refuses to emit a row with no endpoint, and
-- acceptance check [6/9a] pins that 1:1 identity. Deduplicating 20M rows
-- on every scan would cost real time and remove nothing.
-- ------------------------------------------------------------------

CREATE VIEW tf_gnn_prep.loaded_transaction_uses_device AS
 SELECT edge.txn_id                     AS transaction_id,
        edge.device_id,
        edge.edge_unix_time::bigint     AS edge_unix_time,
        COALESCE(sequence.event_seq, 0::bigint) AS edge_event_seq
   FROM card_fraud."cf_Transaction_Uses_Device" edge
   LEFT JOIN tf_gnn_prep.transaction_event_seq sequence
          ON sequence.transaction_id = edge.txn_id;

CREATE VIEW tf_gnn_prep.loaded_transaction_uses_ip AS
 SELECT edge.txn_id                     AS transaction_id,
        edge.ip_id,
        edge.edge_unix_time::bigint     AS edge_unix_time,
        COALESCE(sequence.event_seq, 0::bigint) AS edge_event_seq
   FROM card_fraud."cf_Transaction_Uses_IP" edge
   LEFT JOIN tf_gnn_prep.transaction_event_seq sequence
          ON sequence.transaction_id = edge.txn_id;

-- ------------------------------------------------------------------
-- 2. Vertex views, re-anchored on the session edges.
--
-- cf_Device / cf_IP stay authoritative for the identifier -- an endpoint
-- appearing only on an edge cannot invent a vertex -- while the join to the
-- session edges keeps the "must have at least one edge" rule that every
-- other PII-layer vertex view follows.
--
-- Devices in cf_Device with no session edge are roster endpoints whose owner
-- never produced a card-view row. They are correctly excluded: loading them
-- would add isolated vertices carrying no signal.
--
-- first_seen is now computed rather than hard-coded 0. The columns already
-- existed on these views and were lying; a 0 first-seen fed to a temporal
-- sampler is a live footgun. Types widen from integer to bigint, matching
-- how loaded_cards / loaded_merchants already type theirs.
-- ------------------------------------------------------------------

CREATE VIEW tf_gnn_prep.loaded_devices AS
 SELECT vertex.id,
        min(edge.edge_unix_time) AS first_seen_unix_time,
        min(edge.edge_event_seq) AS first_seen_event_seq
   FROM card_fraud."cf_Device" vertex
   JOIN tf_gnn_prep.loaded_transaction_uses_device edge
     ON edge.device_id = vertex.id
  GROUP BY vertex.id;

CREATE VIEW tf_gnn_prep.loaded_ips AS
 SELECT vertex.id,
        min(edge.edge_unix_time) AS first_seen_unix_time,
        min(edge.edge_event_seq) AS first_seen_event_seq
   FROM card_fraud."cf_IP" vertex
   JOIN tf_gnn_prep.loaded_transaction_uses_ip edge
     ON edge.ip_id = vertex.id
  GROUP BY vertex.id;

-- ------------------------------------------------------------------
-- 3. Loader-facing views.
--
-- load_devices / load_ips keep their existing column names, order and count
-- (id, first_seen_unix_time, first_seen_event_seq), so the TigerGraph
-- loading jobs map exactly as before -- only the values change from empty
-- and 0 to real ones.
--
-- The two edge views follow the load_* convention: clean_text on the join
-- keys, edge_unix_time and edge_event_seq trailing. Unlike the static edges
-- (which pass 0), these carry real event times -- that is the whole point of
-- Transaction_Uses_*, and what lets a temporal sampler score a row from
-- prior endpoint history before appending the current edge.
-- ------------------------------------------------------------------

CREATE VIEW tf_gnn_prep.load_devices AS
 SELECT tf_gnn_prep.clean_text(id) AS id,
        first_seen_unix_time,
        first_seen_event_seq
   FROM tf_gnn_prep.loaded_devices;

CREATE VIEW tf_gnn_prep.load_ips AS
 SELECT tf_gnn_prep.clean_text(id) AS id,
        first_seen_unix_time,
        first_seen_event_seq
   FROM tf_gnn_prep.loaded_ips;

CREATE VIEW tf_gnn_prep.load_transaction_uses_device AS
 SELECT tf_gnn_prep.clean_text(transaction_id) AS transaction_id,
        tf_gnn_prep.clean_text(device_id)      AS device_id,
        edge_unix_time,
        edge_event_seq
   FROM tf_gnn_prep.loaded_transaction_uses_device;

CREATE VIEW tf_gnn_prep.load_transaction_uses_ip AS
 SELECT tf_gnn_prep.clean_text(transaction_id) AS transaction_id,
        tf_gnn_prep.clean_text(ip_id)          AS ip_id,
        edge_unix_time,
        edge_event_seq
   FROM tf_gnn_prep.loaded_transaction_uses_ip;

-- ------------------------------------------------------------------
-- 4. audit_load_counts, rebuilt from the live catalog.
--
-- One branch per load_* view that ACTUALLY EXISTS in tf_gnn_prep at this
-- moment -- which now includes load_transaction_uses_device / _ip created
-- above. The dataset name is the view name minus the load_ prefix, exactly
-- the convention the original audit used ('cards' <- load_cards,
-- 'party_has_device' <- load_party_has_device, ...). Branches are ordered
-- alphabetically by view name.
--
-- Built dynamically instead of restored from tf_gnn_prep_ddl.sql because
-- the live schema is the source of truth: the dump carries load_* views
-- (e.g. load_addresses) that do not exist in every installation, and a
-- verbatim restore aborts the whole transaction when one is absent.
-- The LIKE pattern escapes the underscore, so loaded_* views are NOT
-- swept in ('loaded_cards' does not match 'load\_%').
-- ------------------------------------------------------------------

DO $audit$
DECLARE
  branches text;
BEGIN
  SELECT string_agg(
           format(E' SELECT %L::text AS dataset,\n    count(*) AS rows\n   FROM %I.%I',
                  substring(viewname from 6), schemaname, viewname),
           E'\nUNION ALL\n' ORDER BY viewname)
    INTO branches
    FROM pg_views
   WHERE schemaname = 'tf_gnn_prep'
     AND viewname LIKE 'load\_%';

  IF branches IS NULL THEN
    RAISE EXCEPTION 'no tf_gnn_prep.load_* views found -- cannot rebuild audit_load_counts';
  END IF;

  EXECUTE 'CREATE VIEW tf_gnn_prep.audit_load_counts AS ' || branches;
END
$audit$;

COMMIT;

-- ==================================================================
\echo ''
\echo 'verifying the repair (scans ~20M rows twice; expect minutes)'
-- ==================================================================

DO $verify$
DECLARE
  device_vertices bigint;
  device_loaded bigint;
  device_edges bigint;
  ip_vertices bigint;
  ip_loaded bigint;
  ip_edges bigint;
  zero_seq bigint;
BEGIN
  -- Cheap structural check first: the rebuilt audit must count the new
  -- session-edge datasets, or the load-count audit silently omits the very
  -- tables this repair adds.
  IF position('load_transaction_uses_device'
              in pg_get_viewdef('tf_gnn_prep.audit_load_counts'::regclass)) = 0
     OR position('load_transaction_uses_ip'
              in pg_get_viewdef('tf_gnn_prep.audit_load_counts'::regclass)) = 0 THEN
    RAISE EXCEPTION 'audit_load_counts rebuild missed the session-edge datasets';
  END IF;

  SELECT count(*) INTO device_vertices FROM card_fraud."cf_Device";
  SELECT count(*) INTO device_loaded   FROM tf_gnn_prep.load_devices;
  SELECT count(*) INTO device_edges    FROM tf_gnn_prep.load_transaction_uses_device;
  SELECT count(*) INTO ip_vertices     FROM card_fraud."cf_IP";
  SELECT count(*) INTO ip_loaded       FROM tf_gnn_prep.load_ips;
  SELECT count(*) INTO ip_edges        FROM tf_gnn_prep.load_transaction_uses_ip;

  RAISE NOTICE 'DEVICE  universe=%  loaded=%  session edges=%',
    device_vertices, device_loaded, device_edges;
  RAISE NOTICE 'IP      universe=%  loaded=%  session edges=%',
    ip_vertices, ip_loaded, ip_edges;

  IF device_loaded = 0 OR ip_loaded = 0 THEN
    RAISE EXCEPTION 'repair did not take: loader still reports 0 vertices';
  END IF;

  -- Unpopulated transaction_event_seq degrades edge_event_seq to 0. The edge
  -- set is still complete and edge_unix_time is still real, but a sampler
  -- ordering on event_seq would see every edge tied at 0.
  SELECT count(*) INTO zero_seq
    FROM tf_gnn_prep.load_transaction_uses_device
   WHERE edge_event_seq = 0;
  IF zero_seq > 0 THEN
    RAISE WARNING 'edge_event_seq is 0 on % device edges -- tf_gnn_prep.transaction_event_seq is not fully populated; rebuild it before loading', zero_seq;
  END IF;

  RAISE NOTICE 'OK. Device/IP vertices and session edges are now loadable.';
  RAISE NOTICE 'Next: re-run docs/card_fraud_postgres_acceptance.sql -- it must';
  RAISE NOTICE 'still PASS, including the check that cf_Has_Device + cf_Has_IP';
  RAISE NOTICE 'carry 0 rows. TigerGraph also needs a loading job and a';
  RAISE NOTICE 'Transaction_Uses_Device / _IP edge type in TF_GNN_v3.';
END
$verify$;
