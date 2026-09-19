-- Read-only validation for the synthetic Zelle scenario. Run with psql -f.
-- The simulator audit ledger is used only for validation, never model features.
\set ON_ERROR_STOP on
\timing on
BEGIN;
-- Materialize and analyze the numeric audit key so large runs can hash the
-- small Zelle event set instead of the complete payment ledger. An unmeasured
-- substring expression gives the planner poor distinct-value estimates.
CREATE TEMP TABLE zelle_events ON COMMIT DROP AS
SELECT z.*, substring(z.transfer_id FROM 2)::bigint AS audit_row_seq
FROM mule_temporal."mt_Zelle_Transfer" z;
ANALYZE zelle_events;
CREATE TEMP TABLE zelle_audit ON COMMIT DROP AS
SELECT z.*, t.src_acct, t.dst_acct, t.channel AS audit_channel,
       f.to_id AS sender_account, r.to_id AS recipient_account,
       CASE WHEN t.src_acct ~ '^(BOP|XO|XL|LI)' THEN 'business:' ELSE 'consumer:' END ||
         coalesce(o.from_id,f.to_id) AS sender_profile,
       t.src_acct ~ '^(BOP|XO|XL|LI)' AS business_sender,
       a.is_external::boolean AS sender_external, b.is_external::boolean AS recipient_external
FROM zelle_events z
JOIN transactions t ON t.row_seq=z.audit_row_seq
JOIN mule_temporal."mt_Transfer_From_Account" f ON f.from_id=z.transfer_id
JOIN mule_temporal."mt_Transfer_To_Account" r ON r.from_id=z.transfer_id
JOIN mule_temporal."mt_Account" a ON a.id=f.to_id
JOIN mule_temporal."mt_Account" b ON b.id=r.to_id
LEFT JOIN mule_temporal."mt_Party_Owns_Account" o ON o.to_id=f.to_id
 AND o.valid_from_seq::bigint <= z.event_seq::bigint
 AND (o.valid_to_seq='0' OR z.event_seq::bigint < o.valid_to_seq::bigint);

DO $$ BEGIN
 IF EXISTS(SELECT 1 FROM zelle_audit WHERE amount_present::boolean IS NOT TRUE OR amount::numeric <= 0) THEN
   RAISE EXCEPTION 'Zelle amount missing or nonpositive'; END IF;
 IF (SELECT count(*) FROM zelle_audit) <> (SELECT count(*) FROM mule_temporal."mt_Zelle_Transfer") THEN
   RAISE EXCEPTION 'Audit join lost or duplicated payments'; END IF;
 IF EXISTS(
   SELECT 1 FROM (
     SELECT business_sender,
       sum(round(amount::numeric*100)) OVER (PARTITION BY sender_profile ORDER BY event_ts_ms::bigint
         RANGE BETWEEN 86399999 PRECEDING AND CURRENT ROW) AS daily_cents,
       sum(round(amount::numeric*100)) OVER (PARTITION BY sender_profile ORDER BY event_ts_ms::bigint
         RANGE BETWEEN 2591999999 PRECEDING AND CURRENT ROW) AS monthly_cents
     FROM zelle_audit
   ) budgets WHERE daily_cents > CASE WHEN business_sender THEN 1500000 ELSE 350000 END
      OR monthly_cents > CASE WHEN business_sender THEN 6000000 ELSE 2000000 END
 ) THEN RAISE EXCEPTION 'Rolling sender profile limit exceeded'; END IF;
 IF NOT EXISTS(SELECT 1 FROM zelle_audit WHERE sender_external)
    OR NOT EXISTS(SELECT 1 FROM zelle_audit WHERE recipient_external) THEN
   RAISE EXCEPTION 'External Zelle direction missing'; END IF;
END $$;

DO $$ DECLARE direction text; n bigint; BEGIN
 FOREACH direction IN ARRAY ARRAY['From','To'] LOOP
   EXECUTE format($q$
     SELECT count(*) FROM mule_temporal."mt_Zelle_Transfer" z
     LEFT JOIN mule_temporal.%I tok ON tok.from_id=z.transfer_id
     LEFT JOIN mule_temporal.%I acct ON acct.from_id=z.transfer_id
     LEFT JOIN mule_temporal."mt_Token_Bound_To_Account" bind
       ON bind.from_id=tok.to_id AND bind.to_id=acct.to_id
       AND bind.valid_from_seq::bigint <= z.event_seq::bigint
       AND (bind.valid_to_seq='0' OR z.event_seq::bigint < bind.valid_to_seq::bigint)
     WHERE bind.from_id IS NULL OR tok.event_seq<>z.event_seq OR tok.event_ts_ms<>z.event_ts_ms
   $q$, 'mt_Transfer_'||direction||'_Token', 'mt_Transfer_'||direction||'_Account') INTO n;
   IF n<>0 THEN RAISE EXCEPTION '% token binding/clock failures: %',direction,n; END IF;
   EXECUTE format('SELECT count(*) FROM (SELECT from_id FROM mule_temporal.%I GROUP BY 1 HAVING count(*)<>1) bad',
                   'mt_Transfer_'||direction||'_Token') INTO n;
   IF n<>0 THEN RAISE EXCEPTION '% duplicate token roles',direction; END IF;
 END LOOP;
END $$;

SELECT 'zelle_by_audit_purpose' AS metric, audit_channel, count(*) FROM zelle_audit GROUP BY audit_channel ORDER BY 3 DESC;
SELECT 'zelle_business_involved' AS metric, count(*) FROM zelle_audit
 WHERE business_sender OR dst_acct ~ '^(BOP|XO|XL|LI)';
SELECT 'non_zelle_deposit_to_deposit' AS metric, count(*)
 FROM mule_temporal."mt_Payment_Transaction" p
 JOIN mule_temporal."mt_Transaction_From_Account" f ON f.from_id=p.transaction_id
 JOIN mule_temporal."mt_Transaction_To_Account" r ON r.from_id=p.transaction_id
 JOIN mule_temporal."mt_Account" a ON a.id=f.to_id
 JOIN mule_temporal."mt_Account" b ON b.id=r.to_id
 WHERE a.account_type='deposit' AND b.account_type='deposit';
SELECT 'zelle_policy_validation_passed' AS result;
ROLLBACK;
