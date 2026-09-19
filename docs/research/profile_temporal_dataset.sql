\set ON_ERROR_STOP on
\pset format unaligned
\pset tuples_only on
WITH z AS MATERIALIZED (
 SELECT t.*, f.to_id AS sender, r.to_id AS recipient,
        a.is_external::boolean AS sender_external, b.is_external::boolean AS recipient_external
 FROM mule_temporal."mt_Zelle_Transfer" t
 JOIN mule_temporal."mt_Transfer_From_Account" f ON f.from_id=t.transfer_id
 JOIN mule_temporal."mt_Transfer_To_Account" r ON r.from_id=t.transfer_id
 JOIN mule_temporal."mt_Account" a ON a.id=f.to_id
 JOIN mule_temporal."mt_Account" b ON b.id=r.to_id
), directions AS (
 SELECT CASE WHEN sender_external THEN 'external' ELSE 'internal' END || '_to_' ||
        CASE WHEN recipient_external THEN 'external' ELSE 'internal' END AS direction, count(*) AS n
 FROM z GROUP BY 1
), rails AS (
 SELECT payment_rail AS rail,count(*) AS n,min(event_time) AS first_time,max(event_time) AS last_time FROM mule_temporal."mt_Payment_Transaction" GROUP BY 1
), ext AS (
 SELECT sender AS id FROM z WHERE sender_external UNION SELECT recipient FROM z WHERE recipient_external
)
SELECT jsonb_pretty(jsonb_build_object(
 'database',current_database(),'schema','mule_temporal',
 'population',(SELECT population FROM pl_run_manifest ORDER BY id DESC LIMIT 1),
 'start',(SELECT start_date FROM pl_run_manifest ORDER BY id DESC LIMIT 1),
 'days',(SELECT days FROM pl_run_manifest ORDER BY id DESC LIMIT 1),
 'seed',(SELECT seed FROM pl_run_manifest ORDER BY id DESC LIMIT 1),
 'party_vertices',(SELECT count(*) FROM mule_temporal."mt_Party"),
 'person_vertices',(SELECT count(*) FROM mule_temporal."mt_Party" WHERE party_type='person'),
 'account_vertices',(SELECT count(*) FROM mule_temporal."mt_Account"),
 'mule_account_labels',(SELECT jsonb_object_agg(is_mule,n) FROM
   (SELECT is_mule,count(*) AS n FROM mule_temporal."mt_Account" GROUP BY 1) labels),
 'mule_accounts_with_zelle',(SELECT count(*) FROM mule_temporal."mt_Account" a
   WHERE a.is_mule='1' AND a.id IN (SELECT sender FROM z UNION SELECT recipient FROM z)),
 'total_payments',(SELECT count(*) FROM transactions),'zelle_payments',(SELECT count(*) FROM z),
 'first_payment_time',least((SELECT min(first_time) FROM rails),(SELECT min(event_time) FROM z)),
 'last_payment_time',greatest((SELECT max(last_time) FROM rails),(SELECT max(event_time) FROM z)),
 'other_payments',(SELECT sum(n) FROM rails),'other_rails',(SELECT jsonb_object_agg(rail,n) FROM rails),
 'zelle_directions',(SELECT jsonb_object_agg(direction,n) FROM directions),
 'external_zelle_accounts',(SELECT count(*) FROM ext),
 'zelle_fraud_payments',(SELECT count(*) FROM z WHERE fraud_label='1'),
 'zelle_total_usd',(SELECT round(sum(amount::numeric),2) FROM z),
 'zelle_mean_usd',(SELECT round(avg(amount::numeric),2) FROM z),
 'zelle_median_usd',(SELECT percentile_cont(.5) WITHIN GROUP (ORDER BY amount::numeric) FROM z),
 'zelle_max_usd',(SELECT max(amount::numeric) FROM z),
 'tokens',(SELECT count(*) FROM mule_temporal."mt_Token"),
 'closed_token_bindings',(SELECT count(*) FROM mule_temporal."mt_Token_Bound_To_Account" WHERE valid_to_seq<>'0'),
 'run_manifest',(SELECT to_jsonb(m) FROM pl_run_manifest m ORDER BY id DESC LIMIT 1),
 'database_size',pg_size_pretty(pg_database_size(current_database()))));
