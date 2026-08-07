-- docs/card_fraud_enumeration_investigate.sql
--
-- DOES DEVICE FAN-OUT PREDICT FRAUD? The one question the exported graph has
-- to answer NO to before a GNN is trained on it.
--
-- `attacker-infra-2026-07` gave attacker endpoints real cross-victim reuse.
-- That closed a defect and opened its mirror image: if the only endpoints with
-- many cards are attackers running compromises, then device DEGREE alone
-- separates the label and a model learns the shortcut instead of the graph.
-- `device-fanout-2026-08` added card-testing probes — an operator sweeping a
-- stolen list, one declined authorization per card, LABEL WITHHELD — so that
-- high degree has an innocent explanation too.
--
-- WHY THE EXISTING SCRIPT CANNOT ANSWER THIS. Section 6 of
-- card_fraud_device_ip_investigate.sql histograms cards-per-device, but it
-- truncates at 25 and does not separate probe rows from settled ones. Probe
-- endpoints sit at 47-92 cards, so the interesting tail is exactly the part
-- that gets cut.
--
-- RUN IT AGAINST A LONG WINDOW. A probe fires on a card's FIRST appearance in
-- the view, so a 60-day corpus has almost no enumeration in it. Two years or
-- more. And note that `ctest -R test_table_golden` REGENERATES the database
-- from its own short config — re-run your acceptance corpus before this if the
-- golden test has run since.
--
-- Usage:
--   psql "dbname=phantomledger" -f docs/card_fraud_enumeration_investigate.sql
--
-- The three decline populations are separable by `error`:
--   ''                     settled purchase, label is real
--   'Insufficient Balance' this corpus's own cardholder was refused
--   'Do Not Honor'         card-testing probe, label withheld
-- everything else          non-funding decline (bad PIN, bad CVV, ...)

\set ON_ERROR_STOP on
\pset pager off

BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ READ ONLY;

\echo ''
\echo '[1/4] row census by authorization outcome'
SELECT CASE
         WHEN error = '' THEN 'settled purchase'
         WHEN error = 'Insufficient Balance' THEN 'funding decline'
         WHEN error = 'Do Not Honor' THEN 'card-testing probe'
         ELSE 'non-funding decline'
       END AS outcome,
       count(*) AS rows,
       sum(CASE WHEN is_fraud <> 0 THEN 1 ELSE 0 END) AS labelled_fraud
FROM card_fraud."cf_Payment_Transaction"
GROUP BY 1
ORDER BY rows DESC;

\echo ''
\echo '[2/4] the top endpoints by distinct cards -- what are they?'
\echo '      probe_share near 1.0 means an enumeration endpoint.'
WITH edge AS (
  SELECT d.device_id,
         cs.card_number,
         p.error
  FROM card_fraud."cf_Transaction_Uses_Device" d
  JOIN card_fraud."cf_Payment_Transaction" p ON p.id = d.txn_id
  JOIN card_fraud."cf_Card_Send_Transaction" cs ON cs.txn_id = d.txn_id
)
SELECT device_id,
       count(DISTINCT card_number) AS distinct_cards,
       count(*) AS rows,
       round(avg(CASE WHEN error = 'Do Not Honor' THEN 1.0 ELSE 0.0 END), 4)
         AS probe_share
FROM edge
GROUP BY device_id
ORDER BY distinct_cards DESC
LIMIT 20;

\echo ''
\echo '[3/4] THE HEADLINE: fraud rate of the cards a device touches, by degree.'
\echo '      A rising fraud rate with degree is the shortcut. It must be flat.'
WITH card_truth AS (
  -- Card-level truth from SETTLED rows only: a withheld decline must not
  -- decide whether its card counts as a victim.
  SELECT cs.card_number,
         bool_or(p.is_fraud <> 0) AS ever_fraud
  FROM card_fraud."cf_Card_Send_Transaction" cs
  JOIN card_fraud."cf_Payment_Transaction" p ON p.id = cs.txn_id
  WHERE p.error = ''
  GROUP BY cs.card_number
), device_card AS (
  SELECT DISTINCT d.device_id, cs.card_number
  FROM card_fraud."cf_Transaction_Uses_Device" d
  JOIN card_fraud."cf_Card_Send_Transaction" cs ON cs.txn_id = d.txn_id
), degree AS (
  SELECT device_id, count(*) AS distinct_cards
  FROM device_card GROUP BY device_id
)
SELECT CASE
         WHEN g.distinct_cards = 1 THEN '01 card'
         WHEN g.distinct_cards <= 5 THEN '02-05 cards'
         WHEN g.distinct_cards <= 15 THEN '06-15 cards'
         WHEN g.distinct_cards <= 40 THEN '16-40 cards'
         ELSE '41+ cards'
       END AS degree_bucket,
       count(DISTINCT g.device_id) AS devices,
       count(*) AS device_card_pairs,
       round(avg(CASE WHEN t.ever_fraud THEN 1.0 ELSE 0.0 END), 4)
         AS fraud_rate_of_touched_cards
FROM degree g
JOIN device_card dc ON dc.device_id = g.device_id
JOIN card_truth t ON t.card_number = dc.card_number
GROUP BY 1
ORDER BY 1;

\echo ''
\echo '[4/4] the same cut for IP, since IP carried the heavier tail'
WITH card_truth AS (
  SELECT cs.card_number,
         bool_or(p.is_fraud <> 0) AS ever_fraud
  FROM card_fraud."cf_Card_Send_Transaction" cs
  JOIN card_fraud."cf_Payment_Transaction" p ON p.id = cs.txn_id
  WHERE p.error = ''
  GROUP BY cs.card_number
), ip_card AS (
  SELECT DISTINCT i.ip_id, cs.card_number
  FROM card_fraud."cf_Transaction_Uses_IP" i
  JOIN card_fraud."cf_Card_Send_Transaction" cs ON cs.txn_id = i.txn_id
), degree AS (
  SELECT ip_id, count(*) AS distinct_cards
  FROM ip_card GROUP BY ip_id
)
SELECT CASE
         WHEN g.distinct_cards = 1 THEN '01 card'
         WHEN g.distinct_cards <= 5 THEN '02-05 cards'
         WHEN g.distinct_cards <= 15 THEN '06-15 cards'
         WHEN g.distinct_cards <= 40 THEN '16-40 cards'
         ELSE '41+ cards'
       END AS degree_bucket,
       count(DISTINCT g.ip_id) AS ips,
       count(*) AS ip_card_pairs,
       round(avg(CASE WHEN t.ever_fraud THEN 1.0 ELSE 0.0 END), 4)
         AS fraud_rate_of_touched_cards
FROM degree g
JOIN ip_card ic ON ic.ip_id = g.ip_id
JOIN card_truth t ON t.card_number = ic.card_number
GROUP BY 1
ORDER BY 1;

COMMIT;

\echo ''
\echo 'Enumeration investigation complete.'
\echo 'READ [3/4] FIRST. fraud_rate_of_touched_cards should be roughly FLAT'
\echo 'across degree buckets. A monotone rise means device degree is a fraud'
\echo 'feature and the corpus will teach a GNN to use it instead of structure.'
