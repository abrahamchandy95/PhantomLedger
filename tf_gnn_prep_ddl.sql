--
-- PostgreSQL database dump
--

\restrict T1JG2oRAs9yHARvcIlXS2agelPem6SMyggY8sdMmwrJQhd534nAWjAChqaLwOQk

-- Dumped from database version 17.6 (Postgres.app)
-- Dumped by pg_dump version 17.6 (Postgres.app)

SET statement_timeout = 0;
SET lock_timeout = 0;
SET idle_in_transaction_session_timeout = 0;
SET transaction_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SELECT pg_catalog.set_config('search_path', '', false);
SET check_function_bodies = false;
SET xmloption = content;
SET client_min_messages = warning;
SET row_security = off;

--
-- Name: tf_gnn_prep; Type: SCHEMA; Schema: -; Owner: abrahamchandy
--

CREATE SCHEMA tf_gnn_prep;


ALTER SCHEMA tf_gnn_prep OWNER TO abrahamchandy;

--
-- Name: boolean_text(text); Type: FUNCTION; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE FUNCTION tf_gnn_prep.boolean_text(value text) RETURNS text
    LANGUAGE sql IMMUTABLE PARALLEL SAFE
    AS $$
    SELECT CASE
        WHEN lower(
            btrim(coalesce(value, ''))
        ) IN (
            '1',
            't',
            'true',
            'yes',
            'y'
        )
        THEN 'true'

        ELSE 'false'
    END;
$$;


ALTER FUNCTION tf_gnn_prep.boolean_text(value text) OWNER TO abrahamchandy;

--
-- Name: clean_text(text); Type: FUNCTION; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE FUNCTION tf_gnn_prep.clean_text(value text) RETURNS text
    LANGUAGE sql IMMUTABLE PARALLEL SAFE
    AS $$
    SELECT btrim(
        replace(
            replace(
                replace(
                    replace(
                        replace(
                            coalesce(value, ''),
                            '|',
                            ' '
                        ),
                        E'\t',
                        ' '
                    ),
                    E'\n',
                    ' '
                ),
                E'\r',
                ' '
            ),
            E'\\',
            ' '
        )
    );
$$;


ALTER FUNCTION tf_gnn_prep.clean_text(value text) OWNER TO abrahamchandy;

--
-- Name: event_seq_after(bigint); Type: FUNCTION; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE FUNCTION tf_gnn_prep.event_seq_after(epoch bigint) RETURNS bigint
    LANGUAGE sql STABLE PARALLEL SAFE
    AS $$
    SELECT coalesce(
        (
            SELECT cumulative.latest_event_seq
            FROM tf_gnn_prep.event_seq_by_time AS cumulative
            WHERE cumulative.unix_time <= epoch
            ORDER BY cumulative.unix_time DESC
            LIMIT 1
        ),
        0
    ) + 1;
$$;


ALTER FUNCTION tf_gnn_prep.event_seq_after(epoch bigint) OWNER TO abrahamchandy;

--
-- Name: first_name_token(text); Type: FUNCTION; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE FUNCTION tf_gnn_prep.first_name_token(value text) RETURNS text
    LANGUAGE sql IMMUTABLE PARALLEL SAFE
    AS $$
    SELECT (string_to_array(
        btrim(regexp_replace(coalesce(value, ''), '\s+', ' ', 'g')),
        ' '
    ))[1];
$$;


ALTER FUNCTION tf_gnn_prep.first_name_token(value text) OWNER TO abrahamchandy;

--
-- Name: last_name_token(text); Type: FUNCTION; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE FUNCTION tf_gnn_prep.last_name_token(value text) RETURNS text
    LANGUAGE sql IMMUTABLE PARALLEL SAFE
    AS $$
    SELECT parts[array_upper(parts, 1)]
    FROM (
        SELECT string_to_array(
            btrim(regexp_replace(coalesce(value, ''), '\s+', ' ', 'g')),
            ' '
        ) AS parts
    ) AS split_name;
$$;


ALTER FUNCTION tf_gnn_prep.last_name_token(value text) OWNER TO abrahamchandy;

--
-- Name: normalize_name_part(text); Type: FUNCTION; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE FUNCTION tf_gnn_prep.normalize_name_part(value text) RETURNS text
    LANGUAGE sql IMMUTABLE PARALLEL SAFE
    AS $$
    SELECT regexp_replace(
        lower(coalesce(value, '')),
        '[^a-z]',
        '',
        'g'
    );
$$;


ALTER FUNCTION tf_gnn_prep.normalize_name_part(value text) OWNER TO abrahamchandy;

--
-- Name: soundex(text); Type: FUNCTION; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE FUNCTION tf_gnn_prep.soundex(value text) RETURNS text
    LANGUAGE plpgsql IMMUTABLE PARALLEL SAFE
    AS $$
DECLARE
    cleaned text;
    current_letter text;
    current_code text;
    previous_code text;
    result text;
    position_index integer;
BEGIN
    cleaned := regexp_replace(
        upper(coalesce(value, '')),
        '[^A-Z]',
        '',
        'g'
    );

    IF cleaned = '' THEN
        RETURN '';
    END IF;

    result := substr(cleaned, 1, 1);
    previous_code := tf_gnn_prep.soundex_code(result);

    position_index := 2;

    WHILE position_index <= length(cleaned)
          AND length(result) < 4 LOOP
        current_letter := substr(cleaned, position_index, 1);
        current_code := tf_gnn_prep.soundex_code(current_letter);

        IF current_code <> '' THEN
            IF current_code <> previous_code THEN
                result := result || current_code;
            END IF;

            previous_code := current_code;

        ELSIF current_letter NOT IN ('H', 'W') THEN
            previous_code := '';
        END IF;

        position_index := position_index + 1;
    END LOOP;

    RETURN rpad(result, 4, '0');
END
$$;


ALTER FUNCTION tf_gnn_prep.soundex(value text) OWNER TO abrahamchandy;

--
-- Name: soundex_code(text); Type: FUNCTION; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE FUNCTION tf_gnn_prep.soundex_code(letter text) RETURNS text
    LANGUAGE sql IMMUTABLE PARALLEL SAFE
    AS $$
    SELECT CASE
        WHEN letter IN ('B', 'F', 'P', 'V') THEN '1'
        WHEN letter IN ('C', 'G', 'J', 'K', 'Q', 'S', 'X', 'Z') THEN '2'
        WHEN letter IN ('D', 'T') THEN '3'
        WHEN letter = 'L' THEN '4'
        WHEN letter IN ('M', 'N') THEN '5'
        WHEN letter = 'R' THEN '6'
        ELSE ''
    END;
$$;


ALTER FUNCTION tf_gnn_prep.soundex_code(letter text) OWNER TO abrahamchandy;

--
-- Name: audit_category_consistency; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.audit_category_consistency AS
 SELECT ( SELECT count(*) AS count
           FROM ( SELECT DISTINCT txn.mer_cat
                   FROM card_fraud."cf_Payment_Transaction" txn
                  WHERE (NOT (EXISTS ( SELECT 1
                           FROM card_fraud."cf_Merchant_Category" category
                          WHERE (category.category = txn.mer_cat))))) unknown) AS unknown_transaction_categories,
    ( SELECT count(*) AS count
           FROM ( SELECT edge.merchant_id
                   FROM (card_fraud."cf_Merchant_Receive_Transaction" edge
                     JOIN card_fraud."cf_Payment_Transaction" txn ON ((txn.id = edge.txn_id)))
                  GROUP BY edge.merchant_id
                 HAVING (count(DISTINCT txn.mer_cat) > 1)) multi_category) AS merchants_with_multiple_transaction_categories;


ALTER VIEW tf_gnn_prep.audit_category_consistency OWNER TO abrahamchandy;

SET default_tablespace = '';

SET default_table_access_method = heap;

--
-- Name: label_policy; Type: TABLE; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE TABLE tf_gnn_prep.label_policy (
    policy_id text NOT NULL,
    fraud_confirmation_delay_seconds bigint NOT NULL,
    maturation_window_seconds bigint NOT NULL,
    description text NOT NULL,
    created_at timestamp with time zone DEFAULT now() NOT NULL,
    CONSTRAINT label_policy_delay_check CHECK ((fraud_confirmation_delay_seconds > 0)),
    CONSTRAINT label_policy_window_check CHECK ((maturation_window_seconds > 0))
);


ALTER TABLE tf_gnn_prep.label_policy OWNER TO abrahamchandy;

--
-- Name: observation_bounds; Type: TABLE; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE UNLOGGED TABLE tf_gnn_prep.observation_bounds (
    minimum_epoch bigint,
    maximum_epoch bigint,
    maximum_event_seq bigint,
    transaction_count bigint
);


ALTER TABLE tf_gnn_prep.observation_bounds OWNER TO abrahamchandy;

--
-- Name: split_policy; Type: TABLE; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE TABLE tf_gnn_prep.split_policy (
    policy_id text NOT NULL,
    train_end_epoch bigint NOT NULL,
    validation_end_epoch bigint NOT NULL,
    require_train_seen_card boolean NOT NULL,
    require_train_seen_merchant boolean NOT NULL,
    description text NOT NULL,
    created_at timestamp with time zone DEFAULT now() NOT NULL,
    CONSTRAINT split_policy_order_check CHECK ((train_end_epoch < validation_end_epoch))
);


ALTER TABLE tf_gnn_prep.split_policy OWNER TO abrahamchandy;

--
-- Name: train_cards; Type: TABLE; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE UNLOGGED TABLE tf_gnn_prep.train_cards (
    card_number text NOT NULL
);


ALTER TABLE tf_gnn_prep.train_cards OWNER TO abrahamchandy;

--
-- Name: train_merchants; Type: TABLE; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE UNLOGGED TABLE tf_gnn_prep.train_merchants (
    merchant_id text NOT NULL
);


ALTER TABLE tf_gnn_prep.train_merchants OWNER TO abrahamchandy;

--
-- Name: transaction_event_seq; Type: TABLE; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE UNLOGGED TABLE tf_gnn_prep.transaction_event_seq (
    transaction_id text NOT NULL,
    unix_time bigint,
    event_seq bigint
);


ALTER TABLE tf_gnn_prep.transaction_event_seq OWNER TO abrahamchandy;

--
-- Name: transaction_manifest; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.transaction_manifest AS
 SELECT txn.id AS transaction_id,
    sequence.unix_time,
    sequence.event_seq,
    (sequence.event_seq - 1) AS node_index,
    (txn.amount)::double precision AS amount,
    (txn.is_fraud)::integer AS is_fraud,
    txn.mer_cat,
    txn.use_chip,
    txn.error,
    (POSITION(('online'::text) IN (lower(COALESCE(txn.use_chip, ''::text)))) > 0) AS is_online,
    card_edge.card_number,
    merchant_edge.merchant_id,
    (
        CASE
            WHEN (sequence.unix_time < policy.train_end_epoch) THEN 0
            WHEN (sequence.unix_time < policy.validation_end_epoch) THEN 1
            ELSE 2
        END)::smallint AS split_id,
        CASE
            WHEN ((txn.is_fraud)::integer = 1) THEN 'confirmed_fraud'::text
            WHEN ((sequence.unix_time + label.maturation_window_seconds) <= bounds.maximum_epoch) THEN 'matured_clean'::text
            ELSE 'unresolved'::text
        END AS label_resolution_status,
        CASE
            WHEN ((txn.is_fraud)::integer = 1) THEN (sequence.unix_time + label.fraud_confirmation_delay_seconds)
            WHEN ((sequence.unix_time + label.maturation_window_seconds) <= bounds.maximum_epoch) THEN (sequence.unix_time + label.maturation_window_seconds)
            ELSE (0)::bigint
        END AS label_available_unix_time,
        CASE
            WHEN ((txn.is_fraud)::integer = 1) THEN tf_gnn_prep.event_seq_after((sequence.unix_time + label.fraud_confirmation_delay_seconds))
            WHEN ((sequence.unix_time + label.maturation_window_seconds) <= bounds.maximum_epoch) THEN tf_gnn_prep.event_seq_after((sequence.unix_time + label.maturation_window_seconds))
            ELSE (0)::bigint
        END AS label_available_after_event_seq,
    (sequence.unix_time + label.maturation_window_seconds) AS label_maturation_deadline_unix_time,
    (train_card.card_number IS NOT NULL) AS card_seen_in_train,
    (train_merchant.merchant_id IS NOT NULL) AS merchant_seen_in_train
   FROM ((((((((card_fraud."cf_Payment_Transaction" txn
     JOIN tf_gnn_prep.transaction_event_seq sequence ON ((sequence.transaction_id = txn.id)))
     JOIN card_fraud."cf_Card_Send_Transaction" card_edge ON ((card_edge.txn_id = txn.id)))
     JOIN card_fraud."cf_Merchant_Receive_Transaction" merchant_edge ON ((merchant_edge.txn_id = txn.id)))
     CROSS JOIN tf_gnn_prep.split_policy policy)
     CROSS JOIN tf_gnn_prep.label_policy label)
     CROSS JOIN tf_gnn_prep.observation_bounds bounds)
     LEFT JOIN tf_gnn_prep.train_cards train_card ON ((train_card.card_number = card_edge.card_number)))
     LEFT JOIN tf_gnn_prep.train_merchants train_merchant ON ((train_merchant.merchant_id = merchant_edge.merchant_id)))
  WHERE ((policy.policy_id = 'nvidia_tabformer_v1'::text) AND (label.policy_id = 'phantomledger_v2'::text));


ALTER VIEW tf_gnn_prep.transaction_manifest OWNER TO abrahamchandy;

--
-- Name: audit_date_boundaries; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.audit_date_boundaries AS
 SELECT split_id,
        CASE split_id
            WHEN 0 THEN 'train'::text
            WHEN 1 THEN 'validation'::text
            WHEN 2 THEN 'test'::text
            ELSE 'invalid'::text
        END AS split_name,
    min(unix_time) AS minimum_epoch,
    max(unix_time) AS maximum_epoch,
    (to_timestamp((min(unix_time))::double precision) AT TIME ZONE 'UTC'::text) AS minimum_timestamp_utc,
    (to_timestamp((max(unix_time))::double precision) AT TIME ZONE 'UTC'::text) AS maximum_timestamp_utc
   FROM tf_gnn_prep.transaction_manifest
  GROUP BY split_id
  ORDER BY split_id;


ALTER VIEW tf_gnn_prep.audit_date_boundaries OWNER TO abrahamchandy;

--
-- Name: merchant_first_seen; Type: TABLE; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE UNLOGGED TABLE tf_gnn_prep.merchant_first_seen (
    merchant_id text NOT NULL,
    first_seen_unix_time bigint,
    first_seen_event_seq bigint
);


ALTER TABLE tf_gnn_prep.merchant_first_seen OWNER TO abrahamchandy;

--
-- Name: merchant_geography; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.merchant_geography AS
 SELECT merchant.id AS merchant_id,
    count(DISTINCT city_edge.city_id) AS city_count,
    min(city_edge.city_id) AS city_id,
    count(DISTINCT state_edge.state_id) AS state_count,
    min(state_edge.state_id) AS state_id,
    count(DISTINCT zip_edge.zipcode_id) AS zipcode_count,
    min(zip_edge.zipcode_id) AS zipcode_id
   FROM (((card_fraud."cf_Merchant" merchant
     LEFT JOIN card_fraud."cf_Has_City" city_edge ON ((city_edge.merchant_id = merchant.id)))
     LEFT JOIN card_fraud."cf_Has_State" state_edge ON ((state_edge.merchant_id = merchant.id)))
     LEFT JOIN card_fraud."cf_Has_Zip" zip_edge ON ((zip_edge.merchant_id = merchant.id)))
  GROUP BY merchant.id;


ALTER VIEW tf_gnn_prep.merchant_geography OWNER TO abrahamchandy;

--
-- Name: merchant_locations; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.merchant_locations AS
 SELECT geography.merchant_id,
        CASE
            WHEN ((geography.city_id IS NOT NULL) AND (geography.state_id IS NOT NULL) AND (geography.zipcode_id IS NOT NULL)) THEN ((geography.merchant_id || '@'::text) || geography.zipcode_id)
            ELSE (geography.merchant_id || '@online'::text)
        END AS location_id,
    (NOT ((geography.city_id IS NOT NULL) AND (geography.state_id IS NOT NULL) AND (geography.zipcode_id IS NOT NULL))) AS is_online,
    COALESCE(city.city, ''::text) AS city_name,
    COALESCE(geography.state_id, ''::text) AS state_id,
    COALESCE(geography.zipcode_id, ''::text) AS zipcode_id,
    geography.city_id,
    COALESCE(first_seen.first_seen_unix_time, (0)::bigint) AS first_seen_unix_time,
    COALESCE(first_seen.first_seen_event_seq, (0)::bigint) AS first_seen_event_seq
   FROM ((tf_gnn_prep.merchant_geography geography
     LEFT JOIN card_fraud."cf_City" city ON ((city.id = geography.city_id)))
     LEFT JOIN tf_gnn_prep.merchant_first_seen first_seen ON ((first_seen.merchant_id = geography.merchant_id)));


ALTER VIEW tf_gnn_prep.merchant_locations OWNER TO abrahamchandy;

--
-- Name: audit_endpoint_integrity; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.audit_endpoint_integrity AS
 SELECT ( SELECT count(*) AS count
           FROM (card_fraud."cf_Card_Send_Transaction" edge
             LEFT JOIN card_fraud."cf_Card" card ON ((card.card_number = edge.card_number)))
          WHERE (card.card_number IS NULL)) AS transaction_cards_missing_from_registry,
    ( SELECT count(*) AS count
           FROM (card_fraud."cf_Merchant_Receive_Transaction" edge
             LEFT JOIN card_fraud."cf_Merchant" merchant ON ((merchant.id = edge.merchant_id)))
          WHERE (merchant.id IS NULL)) AS transaction_merchants_missing_from_registry,
    ( SELECT count(*) AS count
           FROM (tf_gnn_prep.transaction_manifest txn
             LEFT JOIN tf_gnn_prep.merchant_locations location ON ((location.merchant_id = txn.merchant_id)))
          WHERE (location.location_id IS NULL)) AS transactions_without_location;


ALTER VIEW tf_gnn_prep.audit_endpoint_integrity OWNER TO abrahamchandy;

--
-- Name: audit_event_sequence; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.audit_event_sequence AS
 SELECT ( SELECT count(*) AS count
           FROM tf_gnn_prep.transaction_event_seq) AS transaction_count,
    ( SELECT min(transaction_event_seq.event_seq) AS min
           FROM tf_gnn_prep.transaction_event_seq) AS minimum_event_seq,
    ( SELECT max(transaction_event_seq.event_seq) AS max
           FROM tf_gnn_prep.transaction_event_seq) AS maximum_event_seq,
    ( SELECT count(DISTINCT transaction_event_seq.event_seq) AS count
           FROM tf_gnn_prep.transaction_event_seq) AS distinct_event_seq,
    ( SELECT count(*) AS count
           FROM tf_gnn_prep.transaction_event_seq
          WHERE (transaction_event_seq.event_seq = 0)) AS zero_event_seq_rows,
    ( SELECT count(*) AS count
           FROM ( SELECT (transaction_event_seq.unix_time - lag(transaction_event_seq.unix_time) OVER (ORDER BY transaction_event_seq.event_seq)) AS time_step
                   FROM tf_gnn_prep.transaction_event_seq) ordering
          WHERE (ordering.time_step < 0)) AS out_of_order_rows;


ALTER VIEW tf_gnn_prep.audit_event_sequence OWNER TO abrahamchandy;

--
-- Name: audit_geography; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.audit_geography AS
 WITH classified AS (
         SELECT geography.merchant_id,
            geography.city_count,
            geography.city_id,
            geography.state_count,
            geography.state_id,
            geography.zipcode_count,
            geography.zipcode_id,
            ((
                CASE
                    WHEN (geography.city_count > 0) THEN 1
                    ELSE 0
                END +
                CASE
                    WHEN (geography.state_count > 0) THEN 1
                    ELSE 0
                END) +
                CASE
                    WHEN (geography.zipcode_count > 0) THEN 1
                    ELSE 0
                END) AS geography_component_count
           FROM tf_gnn_prep.merchant_geography geography
        )
 SELECT count(*) AS merchants,
    count(*) FILTER (WHERE (geography_component_count = 0)) AS merchants_without_geography,
    count(*) FILTER (WHERE ((geography_component_count >= 1) AND (geography_component_count <= 2))) AS merchants_with_partial_geography,
    count(*) FILTER (WHERE (city_count > 1)) AS merchants_with_multiple_cities,
    count(*) FILTER (WHERE (state_count > 1)) AS merchants_with_multiple_states,
    count(*) FILTER (WHERE (zipcode_count > 1)) AS merchants_with_multiple_zipcodes,
    count(*) FILTER (WHERE ((geography_component_count = 3) AND (city_count = 1) AND (zipcode_count = 1) AND (NOT (EXISTS ( SELECT 1
           FROM card_fraud."cf_Assigned_To" assignment
          WHERE ((assignment.zipcode_id = classified.zipcode_id) AND (assignment.city_id = classified.city_id))))))) AS merchants_with_zip_city_mismatch,
    count(*) FILTER (WHERE ((geography_component_count = 3) AND (city_count = 1) AND (state_count = 1) AND (NOT (EXISTS ( SELECT 1
           FROM card_fraud."cf_Located_In" location
          WHERE ((location.city_id = classified.city_id) AND (location.state_id = classified.state_id))))))) AS merchants_with_city_state_mismatch
   FROM classified;


ALTER VIEW tf_gnn_prep.audit_geography OWNER TO abrahamchandy;

--
-- Name: loaded_party_has_address; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_party_has_address AS
 SELECT DISTINCT party_id,
    address
   FROM card_fraud."cf_Has_Address" edge;


ALTER VIEW tf_gnn_prep.loaded_party_has_address OWNER TO abrahamchandy;

--
-- Name: loaded_party_has_device; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_party_has_device AS
 SELECT DISTINCT party_id,
    device_id
   FROM card_fraud."cf_Has_Device" edge;


ALTER VIEW tf_gnn_prep.loaded_party_has_device OWNER TO abrahamchandy;

--
-- Name: loaded_party_has_email; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_party_has_email AS
 SELECT DISTINCT party_id,
    email
   FROM card_fraud."cf_Has_Email" edge;


ALTER VIEW tf_gnn_prep.loaded_party_has_email OWNER TO abrahamchandy;

--
-- Name: loaded_party_has_identity_document; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_party_has_identity_document AS
 SELECT DISTINCT party_id,
    id
   FROM card_fraud."cf_Has_ID" edge;


ALTER VIEW tf_gnn_prep.loaded_party_has_identity_document OWNER TO abrahamchandy;

--
-- Name: loaded_party_has_ip; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_party_has_ip AS
 SELECT DISTINCT party_id,
    ip_id
   FROM card_fraud."cf_Has_IP" edge;


ALTER VIEW tf_gnn_prep.loaded_party_has_ip OWNER TO abrahamchandy;

--
-- Name: loaded_party_has_phone; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_party_has_phone AS
 SELECT DISTINCT party_id,
    phone_number
   FROM card_fraud."cf_Has_Phone" edge;


ALTER VIEW tf_gnn_prep.loaded_party_has_phone OWNER TO abrahamchandy;

--
-- Name: audit_identity_fanout; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.audit_identity_fanout AS
 SELECT 'address'::text AS attribute,
    ( SELECT count(DISTINCT loaded_party_has_address.party_id) AS count
           FROM tf_gnn_prep.loaded_party_has_address) AS linked_parties,
    count(*) AS distinct_values,
    ((( SELECT count(DISTINCT loaded_party_has_address.party_id) AS count
           FROM tf_gnn_prep.loaded_party_has_address))::numeric / (NULLIF(count(*), 0))::numeric) AS linking_fanout,
    max(per_value.parties) AS max_parties_per_value
   FROM ( SELECT loaded_party_has_address.address,
            count(*) AS parties
           FROM tf_gnn_prep.loaded_party_has_address
          GROUP BY loaded_party_has_address.address) per_value
UNION ALL
 SELECT 'phone'::text AS attribute,
    ( SELECT count(DISTINCT loaded_party_has_phone.party_id) AS count
           FROM tf_gnn_prep.loaded_party_has_phone) AS linked_parties,
    count(*) AS distinct_values,
    ((( SELECT count(DISTINCT loaded_party_has_phone.party_id) AS count
           FROM tf_gnn_prep.loaded_party_has_phone))::numeric / (NULLIF(count(*), 0))::numeric) AS linking_fanout,
    max(per_value.parties) AS max_parties_per_value
   FROM ( SELECT loaded_party_has_phone.phone_number,
            count(*) AS parties
           FROM tf_gnn_prep.loaded_party_has_phone
          GROUP BY loaded_party_has_phone.phone_number) per_value
UNION ALL
 SELECT 'email'::text AS attribute,
    ( SELECT count(DISTINCT loaded_party_has_email.party_id) AS count
           FROM tf_gnn_prep.loaded_party_has_email) AS linked_parties,
    count(*) AS distinct_values,
    ((( SELECT count(DISTINCT loaded_party_has_email.party_id) AS count
           FROM tf_gnn_prep.loaded_party_has_email))::numeric / (NULLIF(count(*), 0))::numeric) AS linking_fanout,
    max(per_value.parties) AS max_parties_per_value
   FROM ( SELECT loaded_party_has_email.email,
            count(*) AS parties
           FROM tf_gnn_prep.loaded_party_has_email
          GROUP BY loaded_party_has_email.email) per_value
UNION ALL
 SELECT 'ip'::text AS attribute,
    ( SELECT count(DISTINCT loaded_party_has_ip.party_id) AS count
           FROM tf_gnn_prep.loaded_party_has_ip) AS linked_parties,
    count(*) AS distinct_values,
    ((( SELECT count(DISTINCT loaded_party_has_ip.party_id) AS count
           FROM tf_gnn_prep.loaded_party_has_ip))::numeric / (NULLIF(count(*), 0))::numeric) AS linking_fanout,
    max(per_value.parties) AS max_parties_per_value
   FROM ( SELECT loaded_party_has_ip.ip_id,
            count(*) AS parties
           FROM tf_gnn_prep.loaded_party_has_ip
          GROUP BY loaded_party_has_ip.ip_id) per_value
UNION ALL
 SELECT 'device'::text AS attribute,
    ( SELECT count(DISTINCT loaded_party_has_device.party_id) AS count
           FROM tf_gnn_prep.loaded_party_has_device) AS linked_parties,
    count(*) AS distinct_values,
    ((( SELECT count(DISTINCT loaded_party_has_device.party_id) AS count
           FROM tf_gnn_prep.loaded_party_has_device))::numeric / (NULLIF(count(*), 0))::numeric) AS linking_fanout,
    max(per_value.parties) AS max_parties_per_value
   FROM ( SELECT loaded_party_has_device.device_id,
            count(*) AS parties
           FROM tf_gnn_prep.loaded_party_has_device
          GROUP BY loaded_party_has_device.device_id) per_value
UNION ALL
 SELECT 'identity_document'::text AS attribute,
    ( SELECT count(DISTINCT loaded_party_has_identity_document.party_id) AS count
           FROM tf_gnn_prep.loaded_party_has_identity_document) AS linked_parties,
    count(*) AS distinct_values,
    ((( SELECT count(DISTINCT loaded_party_has_identity_document.party_id) AS count
           FROM tf_gnn_prep.loaded_party_has_identity_document))::numeric / (NULLIF(count(*), 0))::numeric) AS linking_fanout,
    max(per_value.parties) AS max_parties_per_value
   FROM ( SELECT loaded_party_has_identity_document.id,
            count(*) AS parties
           FROM tf_gnn_prep.loaded_party_has_identity_document
          GROUP BY loaded_party_has_identity_document.id) per_value;


ALTER VIEW tf_gnn_prep.audit_identity_fanout OWNER TO abrahamchandy;

--
-- Name: audit_label_maturity; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.audit_label_maturity AS
 SELECT count(*) FILTER (WHERE (transaction_manifest.label_resolution_status = 'confirmed_fraud'::text)) AS confirmed_fraud_rows,
    count(*) FILTER (WHERE (transaction_manifest.label_resolution_status = 'matured_clean'::text)) AS matured_clean_rows,
    count(*) FILTER (WHERE (transaction_manifest.label_resolution_status = 'unresolved'::text)) AS unresolved_rows,
    count(*) FILTER (WHERE ((transaction_manifest.is_fraud = 1) AND (transaction_manifest.label_resolution_status <> 'confirmed_fraud'::text))) AS fraud_rows_not_confirmed,
    count(*) FILTER (WHERE ((transaction_manifest.is_fraud = 0) AND (transaction_manifest.label_resolution_status = 'confirmed_fraud'::text))) AS clean_rows_confirmed,
    count(*) FILTER (WHERE ((transaction_manifest.label_resolution_status = ANY (ARRAY['confirmed_fraud'::text, 'matured_clean'::text])) AND (transaction_manifest.label_available_unix_time = 0))) AS labelled_rows_without_availability,
    count(*) FILTER (WHERE ((transaction_manifest.label_resolution_status = 'confirmed_fraud'::text) AND (transaction_manifest.label_available_unix_time <= transaction_manifest.unix_time))) AS confirmed_rows_not_after_transaction,
    count(*) FILTER (WHERE ((transaction_manifest.label_resolution_status = 'unresolved'::text) AND ((transaction_manifest.label_available_unix_time <> 0) OR (transaction_manifest.label_available_after_event_seq <> 0)))) AS unresolved_rows_with_availability,
    count(*) FILTER (WHERE ((transaction_manifest.label_available_unix_time > 0) AND (transaction_manifest.label_available_after_event_seq = 0))) AS available_rows_without_event_rank,
    count(*) FILTER (WHERE (transaction_manifest.label_maturation_deadline_unix_time <> (transaction_manifest.unix_time + label.maturation_window_seconds))) AS maturation_deadline_mismatch_rows
   FROM (tf_gnn_prep.transaction_manifest
     CROSS JOIN tf_gnn_prep.label_policy label)
  WHERE (label.policy_id = 'phantomledger_v2'::text);


ALTER VIEW tf_gnn_prep.audit_label_maturity OWNER TO abrahamchandy;

--
-- Name: card_first_seen; Type: TABLE; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE UNLOGGED TABLE tf_gnn_prep.card_first_seen (
    card_number text NOT NULL,
    first_seen_unix_time bigint,
    first_seen_event_seq bigint
);


ALTER TABLE tf_gnn_prep.card_first_seen OWNER TO abrahamchandy;

--
-- Name: graph_policy; Type: TABLE; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE TABLE tf_gnn_prep.graph_policy (
    policy_id text NOT NULL,
    projection_bucket_count integer NOT NULL,
    description text NOT NULL,
    created_at timestamp with time zone DEFAULT now() NOT NULL,
    CONSTRAINT graph_policy_bucket_check CHECK ((projection_bucket_count > 0))
);


ALTER TABLE tf_gnn_prep.graph_policy OWNER TO abrahamchandy;

--
-- Name: loaded_cards; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_cards AS
 SELECT card.card_number,
    (row_number() OVER (ORDER BY card.card_number) - 1) AS node_index,
    ((row_number() OVER (ORDER BY card.card_number) - 1) % (bucket.projection_bucket_count)::bigint) AS projection_bucket,
    COALESCE(first_seen.first_seen_unix_time, (0)::bigint) AS first_seen_unix_time,
    COALESCE(first_seen.first_seen_event_seq, (0)::bigint) AS first_seen_event_seq
   FROM ((card_fraud."cf_Card" card
     CROSS JOIN tf_gnn_prep.graph_policy bucket)
     LEFT JOIN tf_gnn_prep.card_first_seen first_seen ON ((first_seen.card_number = card.card_number)))
  WHERE (bucket.policy_id = 'phantomledger_v2'::text);


ALTER VIEW tf_gnn_prep.loaded_cards OWNER TO abrahamchandy;

--
-- Name: load_cards; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_cards AS
 SELECT tf_gnn_prep.clean_text(card_number) AS card_number,
    node_index,
    first_seen_unix_time,
    first_seen_event_seq,
    projection_bucket
   FROM tf_gnn_prep.loaded_cards;


ALTER VIEW tf_gnn_prep.load_cards OWNER TO abrahamchandy;

--
-- Name: loaded_merchants; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_merchants AS
 SELECT merchant.id,
    (row_number() OVER (ORDER BY merchant.id) - 1) AS node_index,
    ((row_number() OVER (ORDER BY merchant.id) - 1) % (bucket.projection_bucket_count)::bigint) AS projection_bucket,
    COALESCE(first_seen.first_seen_unix_time, (0)::bigint) AS first_seen_unix_time,
    COALESCE(first_seen.first_seen_event_seq, (0)::bigint) AS first_seen_event_seq
   FROM ((card_fraud."cf_Merchant" merchant
     CROSS JOIN tf_gnn_prep.graph_policy bucket)
     LEFT JOIN tf_gnn_prep.merchant_first_seen first_seen ON ((first_seen.merchant_id = merchant.id)))
  WHERE (bucket.policy_id = 'phantomledger_v2'::text);


ALTER VIEW tf_gnn_prep.loaded_merchants OWNER TO abrahamchandy;

--
-- Name: load_merchants; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_merchants AS
 SELECT tf_gnn_prep.clean_text(id) AS id,
    node_index,
    first_seen_unix_time,
    first_seen_event_seq,
    projection_bucket
   FROM tf_gnn_prep.loaded_merchants;


ALTER VIEW tf_gnn_prep.load_merchants OWNER TO abrahamchandy;

--
-- Name: party_first_seen; Type: TABLE; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE UNLOGGED TABLE tf_gnn_prep.party_first_seen (
    party_id text NOT NULL,
    first_seen_unix_time bigint,
    first_seen_event_seq bigint
);


ALTER TABLE tf_gnn_prep.party_first_seen OWNER TO abrahamchandy;

--
-- Name: loaded_parties; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_parties AS
 SELECT party.id,
    (row_number() OVER (ORDER BY party.id) - 1) AS node_index,
    party.gender,
    party.party_type,
    party.name,
    COALESCE((to_char(((party.dob)::date)::timestamp with time zone, 'YYYYMMDD'::text))::integer, 0) AS dob_key,
    COALESCE((EXTRACT(year FROM (party.dob)::date))::integer, 0) AS dob_year,
    COALESCE((EXTRACT(month FROM (party.dob)::date))::integer, 0) AS dob_month,
    COALESCE((EXTRACT(day FROM (party.dob)::date))::integer, 0) AS dob_day,
        CASE
            WHEN (party.dob IS NULL) THEN 'unknown'::text
            ELSE 'day'::text
        END AS dob_precision,
    tf_gnn_prep.normalize_name_part(tf_gnn_prep.last_name_token(party.name)) AS name_last_norm,
    tf_gnn_prep.normalize_name_part(tf_gnn_prep.first_name_token(party.name)) AS name_first_norm,
    tf_gnn_prep.soundex(tf_gnn_prep.last_name_token(party.name)) AS name_last_soundex,
    (party.created_at)::timestamp without time zone AS created_at,
    (floor(EXTRACT(epoch FROM (party.created_at)::timestamp without time zone)))::bigint AS created_unix_time,
    COALESCE(first_seen.first_seen_unix_time, (0)::bigint) AS first_seen_unix_time,
    COALESCE(first_seen.first_seen_event_seq, (0)::bigint) AS first_seen_event_seq
   FROM (card_fraud."cf_Party" party
     LEFT JOIN tf_gnn_prep.party_first_seen first_seen ON ((first_seen.party_id = party.id)));


ALTER VIEW tf_gnn_prep.loaded_parties OWNER TO abrahamchandy;

--
-- Name: load_parties; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_parties AS
 SELECT tf_gnn_prep.clean_text(id) AS id,
    node_index,
    tf_gnn_prep.clean_text(gender) AS gender,
    dob_key,
    dob_year,
    dob_month,
    dob_day,
    dob_precision,
    tf_gnn_prep.clean_text(party_type) AS party_type,
    tf_gnn_prep.clean_text(name) AS name,
    tf_gnn_prep.clean_text(name_last_norm) AS name_last_norm,
    tf_gnn_prep.clean_text(name_first_norm) AS name_first_norm,
    tf_gnn_prep.clean_text(name_last_soundex) AS name_last_soundex,
    to_char(created_at, 'YYYY-MM-DD HH24:MI:SS'::text) AS created_at,
    created_unix_time,
    first_seen_unix_time,
    first_seen_event_seq
   FROM tf_gnn_prep.loaded_parties;


ALTER VIEW tf_gnn_prep.load_parties OWNER TO abrahamchandy;

--
-- Name: audit_node_index; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.audit_node_index AS
 SELECT 'cards'::text AS dataset,
    count(*) AS rows,
    min(load_cards.node_index) AS minimum_node_index,
    max(load_cards.node_index) AS maximum_node_index,
    count(DISTINCT load_cards.node_index) AS distinct_node_index
   FROM tf_gnn_prep.load_cards
UNION ALL
 SELECT 'merchants'::text AS dataset,
    count(*) AS rows,
    min(load_merchants.node_index) AS minimum_node_index,
    max(load_merchants.node_index) AS maximum_node_index,
    count(DISTINCT load_merchants.node_index) AS distinct_node_index
   FROM tf_gnn_prep.load_merchants
UNION ALL
 SELECT 'parties'::text AS dataset,
    count(*) AS rows,
    min(load_parties.node_index) AS minimum_node_index,
    max(load_parties.node_index) AS maximum_node_index,
    count(DISTINCT load_parties.node_index) AS distinct_node_index
   FROM tf_gnn_prep.load_parties
UNION ALL
 SELECT 'transactions'::text AS dataset,
    count(*) AS rows,
    min(transaction_manifest.node_index) AS minimum_node_index,
    max(transaction_manifest.node_index) AS maximum_node_index,
    count(DISTINCT transaction_manifest.node_index) AS distinct_node_index
   FROM tf_gnn_prep.transaction_manifest;


ALTER VIEW tf_gnn_prep.audit_node_index OWNER TO abrahamchandy;

--
-- Name: audit_source_counts; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.audit_source_counts AS
 SELECT ( SELECT count(*) AS count
           FROM card_fraud."cf_Payment_Transaction") AS transactions,
    ( SELECT count(*) AS count
           FROM card_fraud."cf_Card_Send_Transaction") AS card_edges,
    ( SELECT count(*) AS count
           FROM card_fraud."cf_Merchant_Receive_Transaction") AS merchant_edges;


ALTER VIEW tf_gnn_prep.audit_source_counts OWNER TO abrahamchandy;

--
-- Name: load_transactions; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_transactions AS
 SELECT tf_gnn_prep.clean_text(txn.transaction_id) AS transaction_id,
    to_char((to_timestamp((txn.unix_time)::double precision) AT TIME ZONE 'UTC'::text), 'YYYY-MM-DD HH24:MI:SS'::text) AS transaction_time,
    txn.unix_time,
    txn.event_seq,
    txn.node_index,
    txn.split_id,
    '-1'::integer AS causal_fold,
        CASE
            WHEN (txn.split_id = 0) THEN 'true'::text
            ELSE 'false'::text
        END AS is_train,
        CASE
            WHEN (txn.split_id = 1) THEN 'true'::text
            ELSE 'false'::text
        END AS is_val,
        CASE
            WHEN (txn.split_id = 2) THEN 'true'::text
            ELSE 'false'::text
        END AS is_test,
    txn.amount,
    txn.is_fraud,
    txn.label_resolution_status,
    txn.label_available_unix_time,
    txn.label_available_after_event_seq,
    txn.label_maturation_deadline_unix_time,
    tf_gnn_prep.clean_text(txn.mer_cat) AS mer_cat,
    tf_gnn_prep.clean_text(txn.use_chip) AS use_chip,
    tf_gnn_prep.clean_text(txn.error) AS error,
        CASE
            WHEN txn.is_online THEN 'true'::text
            ELSE 'false'::text
        END AS is_online,
    tf_gnn_prep.clean_text(txn.card_number) AS card_number,
    tf_gnn_prep.clean_text(txn.merchant_id) AS merchant_id,
    tf_gnn_prep.clean_text(COALESCE(location.location_id, ''::text)) AS location_id
   FROM (tf_gnn_prep.transaction_manifest txn
     LEFT JOIN tf_gnn_prep.merchant_locations location ON ((location.merchant_id = txn.merchant_id)));


ALTER VIEW tf_gnn_prep.load_transactions OWNER TO abrahamchandy;

--
-- Name: audit_split_flags; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.audit_split_flags AS
 SELECT count(*) FILTER (WHERE ((((
        CASE
            WHEN (is_train = 'true'::text) THEN 1
            ELSE 0
        END +
        CASE
            WHEN (is_val = 'true'::text) THEN 1
            ELSE 0
        END) +
        CASE
            WHEN (is_test = 'true'::text) THEN 1
            ELSE 0
        END) <> 1) OR ((is_train = 'true'::text) <> (split_id = 0)) OR ((is_val = 'true'::text) <> (split_id = 1)) OR ((is_test = 'true'::text) <> (split_id = 2)))) AS invalid_split_flag_rows
   FROM tf_gnn_prep.load_transactions;


ALTER VIEW tf_gnn_prep.audit_split_flags OWNER TO abrahamchandy;

--
-- Name: audit_failures; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.audit_failures AS
 SELECT 'transaction_card_edge_count_mismatch'::text AS check_name,
    abs((audit_source_counts.transactions - audit_source_counts.card_edges)) AS failure_count
   FROM tf_gnn_prep.audit_source_counts
UNION ALL
 SELECT 'transaction_merchant_edge_count_mismatch'::text AS check_name,
    abs((audit_source_counts.transactions - audit_source_counts.merchant_edges)) AS failure_count
   FROM tf_gnn_prep.audit_source_counts
UNION ALL
 SELECT 'event_seq_not_starting_at_one'::text AS check_name,
        CASE
            WHEN ((audit_event_sequence.transaction_count > 0) AND (audit_event_sequence.minimum_event_seq <> 1)) THEN 1
            ELSE 0
        END AS failure_count
   FROM tf_gnn_prep.audit_event_sequence
UNION ALL
 SELECT 'event_seq_not_dense'::text AS check_name,
        CASE
            WHEN ((audit_event_sequence.maximum_event_seq <> audit_event_sequence.transaction_count) OR (audit_event_sequence.distinct_event_seq <> audit_event_sequence.transaction_count)) THEN 1
            ELSE 0
        END AS failure_count
   FROM tf_gnn_prep.audit_event_sequence
UNION ALL
 SELECT 'event_seq_zero_rows'::text AS check_name,
    audit_event_sequence.zero_event_seq_rows AS failure_count
   FROM tf_gnn_prep.audit_event_sequence
UNION ALL
 SELECT 'event_seq_out_of_order_rows'::text AS check_name,
    audit_event_sequence.out_of_order_rows AS failure_count
   FROM tf_gnn_prep.audit_event_sequence
UNION ALL
 SELECT 'node_index_not_dense_datasets'::text AS check_name,
    count(*) AS failure_count
   FROM tf_gnn_prep.audit_node_index
  WHERE ((audit_node_index.rows > 0) AND ((audit_node_index.minimum_node_index <> 0) OR (audit_node_index.maximum_node_index <> (audit_node_index.rows - 1)) OR (audit_node_index.distinct_node_index <> audit_node_index.rows)))
UNION ALL
 SELECT 'invalid_split_flag_rows'::text AS check_name,
    audit_split_flags.invalid_split_flag_rows AS failure_count
   FROM tf_gnn_prep.audit_split_flags
UNION ALL
 SELECT 'fraud_rows_not_confirmed'::text AS check_name,
    audit_label_maturity.fraud_rows_not_confirmed AS failure_count
   FROM tf_gnn_prep.audit_label_maturity
UNION ALL
 SELECT 'clean_rows_confirmed'::text AS check_name,
    audit_label_maturity.clean_rows_confirmed AS failure_count
   FROM tf_gnn_prep.audit_label_maturity
UNION ALL
 SELECT 'labelled_rows_without_availability'::text AS check_name,
    audit_label_maturity.labelled_rows_without_availability AS failure_count
   FROM tf_gnn_prep.audit_label_maturity
UNION ALL
 SELECT 'confirmed_rows_not_after_transaction'::text AS check_name,
    audit_label_maturity.confirmed_rows_not_after_transaction AS failure_count
   FROM tf_gnn_prep.audit_label_maturity
UNION ALL
 SELECT 'unresolved_rows_with_availability'::text AS check_name,
    audit_label_maturity.unresolved_rows_with_availability AS failure_count
   FROM tf_gnn_prep.audit_label_maturity
UNION ALL
 SELECT 'available_rows_without_event_rank'::text AS check_name,
    audit_label_maturity.available_rows_without_event_rank AS failure_count
   FROM tf_gnn_prep.audit_label_maturity
UNION ALL
 SELECT 'maturation_deadline_mismatch_rows'::text AS check_name,
    audit_label_maturity.maturation_deadline_mismatch_rows AS failure_count
   FROM tf_gnn_prep.audit_label_maturity
UNION ALL
 SELECT 'merchants_with_partial_geography'::text AS check_name,
    audit_geography.merchants_with_partial_geography AS failure_count
   FROM tf_gnn_prep.audit_geography
UNION ALL
 SELECT 'merchants_with_multiple_cities'::text AS check_name,
    audit_geography.merchants_with_multiple_cities AS failure_count
   FROM tf_gnn_prep.audit_geography
UNION ALL
 SELECT 'merchants_with_multiple_states'::text AS check_name,
    audit_geography.merchants_with_multiple_states AS failure_count
   FROM tf_gnn_prep.audit_geography
UNION ALL
 SELECT 'merchants_with_multiple_zipcodes'::text AS check_name,
    audit_geography.merchants_with_multiple_zipcodes AS failure_count
   FROM tf_gnn_prep.audit_geography
UNION ALL
 SELECT 'merchants_with_zip_city_mismatch'::text AS check_name,
    audit_geography.merchants_with_zip_city_mismatch AS failure_count
   FROM tf_gnn_prep.audit_geography
UNION ALL
 SELECT 'merchants_with_city_state_mismatch'::text AS check_name,
    audit_geography.merchants_with_city_state_mismatch AS failure_count
   FROM tf_gnn_prep.audit_geography
UNION ALL
 SELECT 'transaction_cards_missing_from_registry'::text AS check_name,
    audit_endpoint_integrity.transaction_cards_missing_from_registry AS failure_count
   FROM tf_gnn_prep.audit_endpoint_integrity
UNION ALL
 SELECT 'transaction_merchants_missing_from_registry'::text AS check_name,
    audit_endpoint_integrity.transaction_merchants_missing_from_registry AS failure_count
   FROM tf_gnn_prep.audit_endpoint_integrity
UNION ALL
 SELECT 'transactions_without_location'::text AS check_name,
    audit_endpoint_integrity.transactions_without_location AS failure_count
   FROM tf_gnn_prep.audit_endpoint_integrity
UNION ALL
 SELECT 'unknown_transaction_categories'::text AS check_name,
    audit_category_consistency.unknown_transaction_categories AS failure_count
   FROM tf_gnn_prep.audit_category_consistency
UNION ALL
 SELECT 'pii_linking_fanout_violations'::text AS check_name,
    count(*) AS failure_count
   FROM tf_gnn_prep.audit_identity_fanout
  WHERE (audit_identity_fanout.linking_fanout > (3)::numeric);


ALTER VIEW tf_gnn_prep.audit_failures OWNER TO abrahamchandy;

--
-- Name: category_first_seen; Type: TABLE; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE UNLOGGED TABLE tf_gnn_prep.category_first_seen (
    category text NOT NULL,
    first_seen_unix_time bigint,
    first_seen_event_seq bigint
);


ALTER TABLE tf_gnn_prep.category_first_seen OWNER TO abrahamchandy;

--
-- Name: loaded_addresses; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_addresses AS
 SELECT DISTINCT vertex.address
   FROM (card_fraud."cf_Address" vertex
     JOIN tf_gnn_prep.loaded_party_has_address edge ON ((edge.address = vertex.address)));


ALTER VIEW tf_gnn_prep.loaded_addresses OWNER TO abrahamchandy;

--
-- Name: load_addresses; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_addresses AS
 SELECT tf_gnn_prep.clean_text(address) AS address,
    0 AS first_seen_unix_time,
    0 AS first_seen_event_seq
   FROM tf_gnn_prep.loaded_addresses;


ALTER VIEW tf_gnn_prep.load_addresses OWNER TO abrahamchandy;

--
-- Name: loaded_location_in_city; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_location_in_city AS
 SELECT location_id,
    city_id
   FROM tf_gnn_prep.merchant_locations location
  WHERE ((NOT is_online) AND (city_id IS NOT NULL));


ALTER VIEW tf_gnn_prep.loaded_location_in_city OWNER TO abrahamchandy;

--
-- Name: loaded_cities; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_cities AS
 SELECT DISTINCT city.id,
    city.city,
    city.population
   FROM (card_fraud."cf_City" city
     JOIN tf_gnn_prep.loaded_location_in_city edge ON ((edge.city_id = city.id)));


ALTER VIEW tf_gnn_prep.loaded_cities OWNER TO abrahamchandy;

--
-- Name: load_cities; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_cities AS
 SELECT tf_gnn_prep.clean_text(id) AS id,
    tf_gnn_prep.clean_text(city) AS city,
    (COALESCE(NULLIF(btrim(population), ''::text), '0'::text))::integer AS population,
    0 AS first_seen_unix_time,
    0 AS first_seen_event_seq
   FROM tf_gnn_prep.loaded_cities;


ALTER VIEW tf_gnn_prep.load_cities OWNER TO abrahamchandy;

--
-- Name: loaded_city_located_in_state; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_city_located_in_state AS
 SELECT DISTINCT edge.city_id,
    edge.state_id
   FROM (card_fraud."cf_Located_In" edge
     JOIN tf_gnn_prep.loaded_cities city ON ((city.id = edge.city_id)));


ALTER VIEW tf_gnn_prep.loaded_city_located_in_state OWNER TO abrahamchandy;

--
-- Name: load_city_located_in_state; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_city_located_in_state AS
 SELECT tf_gnn_prep.clean_text(city_id) AS city_id,
    tf_gnn_prep.clean_text(state_id) AS state_id,
    0 AS edge_unix_time,
    0 AS edge_event_seq
   FROM tf_gnn_prep.loaded_city_located_in_state;


ALTER VIEW tf_gnn_prep.load_city_located_in_state OWNER TO abrahamchandy;

--
-- Name: loaded_devices; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_devices AS
 SELECT DISTINCT vertex.id
   FROM (card_fraud."cf_Device" vertex
     JOIN tf_gnn_prep.loaded_party_has_device edge ON ((edge.device_id = vertex.id)));


ALTER VIEW tf_gnn_prep.loaded_devices OWNER TO abrahamchandy;

--
-- Name: load_devices; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_devices AS
 SELECT tf_gnn_prep.clean_text(id) AS id,
    0 AS first_seen_unix_time,
    0 AS first_seen_event_seq
   FROM tf_gnn_prep.loaded_devices;


ALTER VIEW tf_gnn_prep.load_devices OWNER TO abrahamchandy;

--
-- Name: loaded_emails; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_emails AS
 SELECT DISTINCT vertex.email
   FROM (card_fraud."cf_Email" vertex
     JOIN tf_gnn_prep.loaded_party_has_email edge ON ((edge.email = vertex.email)));


ALTER VIEW tf_gnn_prep.loaded_emails OWNER TO abrahamchandy;

--
-- Name: load_emails; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_emails AS
 SELECT tf_gnn_prep.clean_text(email) AS email,
    0 AS first_seen_unix_time,
    0 AS first_seen_event_seq
   FROM tf_gnn_prep.loaded_emails;


ALTER VIEW tf_gnn_prep.load_emails OWNER TO abrahamchandy;

--
-- Name: loaded_identity_documents; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_identity_documents AS
 SELECT DISTINCT vertex.id,
    vertex.id_type
   FROM (card_fraud."cf_ID" vertex
     JOIN tf_gnn_prep.loaded_party_has_identity_document edge ON ((edge.id = vertex.id)));


ALTER VIEW tf_gnn_prep.loaded_identity_documents OWNER TO abrahamchandy;

--
-- Name: load_identity_documents; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_identity_documents AS
 SELECT tf_gnn_prep.clean_text(id) AS id,
    tf_gnn_prep.clean_text(id_type) AS id_type,
    0 AS first_seen_unix_time,
    0 AS first_seen_event_seq
   FROM tf_gnn_prep.loaded_identity_documents;


ALTER VIEW tf_gnn_prep.load_identity_documents OWNER TO abrahamchandy;

--
-- Name: loaded_ips; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_ips AS
 SELECT DISTINCT vertex.id
   FROM (card_fraud."cf_IP" vertex
     JOIN tf_gnn_prep.loaded_party_has_ip edge ON ((edge.ip_id = vertex.id)));


ALTER VIEW tf_gnn_prep.loaded_ips OWNER TO abrahamchandy;

--
-- Name: load_ips; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_ips AS
 SELECT tf_gnn_prep.clean_text(id) AS id,
    0 AS first_seen_unix_time,
    0 AS first_seen_event_seq
   FROM tf_gnn_prep.loaded_ips;


ALTER VIEW tf_gnn_prep.load_ips OWNER TO abrahamchandy;

--
-- Name: load_location_in_city; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_location_in_city AS
 SELECT tf_gnn_prep.clean_text(location_id) AS location_id,
    tf_gnn_prep.clean_text(city_id) AS city_id,
    0 AS edge_unix_time,
    0 AS edge_event_seq
   FROM tf_gnn_prep.loaded_location_in_city;


ALTER VIEW tf_gnn_prep.load_location_in_city OWNER TO abrahamchandy;

--
-- Name: loaded_location_in_zip; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_location_in_zip AS
 SELECT location_id,
    zipcode_id
   FROM tf_gnn_prep.merchant_locations location
  WHERE ((NOT is_online) AND (zipcode_id <> ''::text));


ALTER VIEW tf_gnn_prep.loaded_location_in_zip OWNER TO abrahamchandy;

--
-- Name: load_location_in_zip; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_location_in_zip AS
 SELECT tf_gnn_prep.clean_text(location_id) AS location_id,
    tf_gnn_prep.clean_text(zipcode_id) AS zipcode_id,
    0 AS edge_unix_time,
    0 AS edge_event_seq
   FROM tf_gnn_prep.loaded_location_in_zip;


ALTER VIEW tf_gnn_prep.load_location_in_zip OWNER TO abrahamchandy;

--
-- Name: loaded_merchant_assigned; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_merchant_assigned AS
 SELECT DISTINCT merchant_id,
    category
   FROM card_fraud."cf_Merchant_Assigned" edge;


ALTER VIEW tf_gnn_prep.loaded_merchant_assigned OWNER TO abrahamchandy;

--
-- Name: load_merchant_assigned; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_merchant_assigned AS
 SELECT tf_gnn_prep.clean_text(merchant_id) AS merchant_id,
    tf_gnn_prep.clean_text(category) AS category
   FROM tf_gnn_prep.loaded_merchant_assigned;


ALTER VIEW tf_gnn_prep.load_merchant_assigned OWNER TO abrahamchandy;

--
-- Name: loaded_categories; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_categories AS
 SELECT category.category,
    COALESCE(first_seen.first_seen_unix_time, (0)::bigint) AS first_seen_unix_time,
    COALESCE(first_seen.first_seen_event_seq, (0)::bigint) AS first_seen_event_seq
   FROM (card_fraud."cf_Merchant_Category" category
     LEFT JOIN tf_gnn_prep.category_first_seen first_seen ON ((first_seen.category = category.category)));


ALTER VIEW tf_gnn_prep.loaded_categories OWNER TO abrahamchandy;

--
-- Name: load_merchant_categories; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_merchant_categories AS
 SELECT tf_gnn_prep.clean_text(category) AS category,
    first_seen_unix_time,
    first_seen_event_seq
   FROM tf_gnn_prep.loaded_categories;


ALTER VIEW tf_gnn_prep.load_merchant_categories OWNER TO abrahamchandy;

--
-- Name: loaded_merchant_has_location; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_merchant_has_location AS
 SELECT merchant_id,
    location_id
   FROM tf_gnn_prep.merchant_locations location;


ALTER VIEW tf_gnn_prep.loaded_merchant_has_location OWNER TO abrahamchandy;

--
-- Name: load_merchant_has_location; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_merchant_has_location AS
 SELECT tf_gnn_prep.clean_text(merchant_id) AS merchant_id,
    tf_gnn_prep.clean_text(location_id) AS location_id,
    0 AS edge_unix_time,
    0 AS edge_event_seq
   FROM tf_gnn_prep.loaded_merchant_has_location;


ALTER VIEW tf_gnn_prep.load_merchant_has_location OWNER TO abrahamchandy;

--
-- Name: load_merchant_locations; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_merchant_locations AS
 SELECT tf_gnn_prep.clean_text(location_id) AS location_id,
        CASE
            WHEN is_online THEN 'true'::text
            ELSE 'false'::text
        END AS is_online,
    tf_gnn_prep.clean_text(city_name) AS city,
    tf_gnn_prep.clean_text(state_id) AS state,
    tf_gnn_prep.clean_text(zipcode_id) AS zipcode,
    first_seen_unix_time,
    first_seen_event_seq
   FROM tf_gnn_prep.merchant_locations;


ALTER VIEW tf_gnn_prep.load_merchant_locations OWNER TO abrahamchandy;

--
-- Name: load_party_has_address; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_party_has_address AS
 SELECT tf_gnn_prep.clean_text(party_id) AS party_id,
    tf_gnn_prep.clean_text(address) AS address,
    0 AS edge_unix_time,
    0 AS edge_event_seq
   FROM tf_gnn_prep.loaded_party_has_address;


ALTER VIEW tf_gnn_prep.load_party_has_address OWNER TO abrahamchandy;

--
-- Name: loaded_party_has_card; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_party_has_card AS
 SELECT DISTINCT party_id,
    card_number
   FROM card_fraud."cf_Party_Has_Card" edge;


ALTER VIEW tf_gnn_prep.loaded_party_has_card OWNER TO abrahamchandy;

--
-- Name: load_party_has_card; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_party_has_card AS
 SELECT tf_gnn_prep.clean_text(party_id) AS party_id,
    tf_gnn_prep.clean_text(card_number) AS card_number,
    0 AS edge_unix_time,
    0 AS edge_event_seq
   FROM tf_gnn_prep.loaded_party_has_card;


ALTER VIEW tf_gnn_prep.load_party_has_card OWNER TO abrahamchandy;

--
-- Name: load_party_has_device; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_party_has_device AS
 SELECT tf_gnn_prep.clean_text(party_id) AS party_id,
    tf_gnn_prep.clean_text(device_id) AS device_id,
    0 AS edge_unix_time,
    0 AS edge_event_seq
   FROM tf_gnn_prep.loaded_party_has_device;


ALTER VIEW tf_gnn_prep.load_party_has_device OWNER TO abrahamchandy;

--
-- Name: load_party_has_email; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_party_has_email AS
 SELECT tf_gnn_prep.clean_text(party_id) AS party_id,
    tf_gnn_prep.clean_text(email) AS email,
    0 AS edge_unix_time,
    0 AS edge_event_seq
   FROM tf_gnn_prep.loaded_party_has_email;


ALTER VIEW tf_gnn_prep.load_party_has_email OWNER TO abrahamchandy;

--
-- Name: load_party_has_identity_document; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_party_has_identity_document AS
 SELECT tf_gnn_prep.clean_text(party_id) AS party_id,
    tf_gnn_prep.clean_text(id) AS id,
    0 AS edge_unix_time,
    0 AS edge_event_seq
   FROM tf_gnn_prep.loaded_party_has_identity_document;


ALTER VIEW tf_gnn_prep.load_party_has_identity_document OWNER TO abrahamchandy;

--
-- Name: load_party_has_ip; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_party_has_ip AS
 SELECT tf_gnn_prep.clean_text(party_id) AS party_id,
    tf_gnn_prep.clean_text(ip_id) AS ip_id,
    0 AS edge_unix_time,
    0 AS edge_event_seq
   FROM tf_gnn_prep.loaded_party_has_ip;


ALTER VIEW tf_gnn_prep.load_party_has_ip OWNER TO abrahamchandy;

--
-- Name: load_party_has_phone; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_party_has_phone AS
 SELECT tf_gnn_prep.clean_text(party_id) AS party_id,
    tf_gnn_prep.clean_text(phone_number) AS phone_number,
    0 AS edge_unix_time,
    0 AS edge_event_seq
   FROM tf_gnn_prep.loaded_party_has_phone;


ALTER VIEW tf_gnn_prep.load_party_has_phone OWNER TO abrahamchandy;

--
-- Name: loaded_party_is_merchant; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_party_is_merchant AS
 SELECT DISTINCT party_id,
    merchant_id
   FROM card_fraud."cf_Is_Merchant" edge;


ALTER VIEW tf_gnn_prep.loaded_party_is_merchant OWNER TO abrahamchandy;

--
-- Name: load_party_is_merchant; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_party_is_merchant AS
 SELECT tf_gnn_prep.clean_text(party_id) AS party_id,
    tf_gnn_prep.clean_text(merchant_id) AS merchant_id,
    0 AS edge_unix_time,
    0 AS edge_event_seq
   FROM tf_gnn_prep.loaded_party_is_merchant;


ALTER VIEW tf_gnn_prep.load_party_is_merchant OWNER TO abrahamchandy;

--
-- Name: loaded_phones; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_phones AS
 SELECT DISTINCT vertex.phone_number
   FROM (card_fraud."cf_Phone" vertex
     JOIN tf_gnn_prep.loaded_party_has_phone edge ON ((edge.phone_number = vertex.phone_number)));


ALTER VIEW tf_gnn_prep.loaded_phones OWNER TO abrahamchandy;

--
-- Name: load_phones; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_phones AS
 SELECT tf_gnn_prep.clean_text(phone_number) AS phone_number,
    0 AS first_seen_unix_time,
    0 AS first_seen_event_seq
   FROM tf_gnn_prep.loaded_phones;


ALTER VIEW tf_gnn_prep.load_phones OWNER TO abrahamchandy;

--
-- Name: loaded_states; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_states AS
 SELECT DISTINCT state.id
   FROM (card_fraud."cf_State" state
     JOIN tf_gnn_prep.loaded_city_located_in_state edge ON ((edge.state_id = state.id)));


ALTER VIEW tf_gnn_prep.loaded_states OWNER TO abrahamchandy;

--
-- Name: load_states; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_states AS
 SELECT tf_gnn_prep.clean_text(id) AS id,
    0 AS first_seen_unix_time,
    0 AS first_seen_event_seq
   FROM tf_gnn_prep.loaded_states;


ALTER VIEW tf_gnn_prep.load_states OWNER TO abrahamchandy;

--
-- Name: loaded_zipcodes; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_zipcodes AS
 SELECT DISTINCT zipcode.id
   FROM (card_fraud."cf_Zipcode" zipcode
     JOIN tf_gnn_prep.loaded_location_in_zip edge ON ((edge.zipcode_id = zipcode.id)));


ALTER VIEW tf_gnn_prep.loaded_zipcodes OWNER TO abrahamchandy;

--
-- Name: loaded_zip_assigned_to_city; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.loaded_zip_assigned_to_city AS
 SELECT DISTINCT edge.zipcode_id,
    edge.city_id
   FROM ((card_fraud."cf_Assigned_To" edge
     JOIN tf_gnn_prep.loaded_zipcodes zipcode ON ((zipcode.id = edge.zipcode_id)))
     JOIN tf_gnn_prep.loaded_cities city ON ((city.id = edge.city_id)));


ALTER VIEW tf_gnn_prep.loaded_zip_assigned_to_city OWNER TO abrahamchandy;

--
-- Name: load_zip_assigned_to_city; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_zip_assigned_to_city AS
 SELECT tf_gnn_prep.clean_text(zipcode_id) AS zipcode_id,
    tf_gnn_prep.clean_text(city_id) AS city_id,
    0 AS edge_unix_time,
    0 AS edge_event_seq
   FROM tf_gnn_prep.loaded_zip_assigned_to_city;


ALTER VIEW tf_gnn_prep.load_zip_assigned_to_city OWNER TO abrahamchandy;

--
-- Name: load_zipcodes; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.load_zipcodes AS
 SELECT tf_gnn_prep.clean_text(id) AS id,
    0 AS first_seen_unix_time,
    0 AS first_seen_event_seq
   FROM tf_gnn_prep.loaded_zipcodes;


ALTER VIEW tf_gnn_prep.load_zipcodes OWNER TO abrahamchandy;

--
-- Name: audit_load_counts; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.audit_load_counts AS
 SELECT 'cards'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_cards
UNION ALL
 SELECT 'merchants'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_merchants
UNION ALL
 SELECT 'merchant_categories'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_merchant_categories
UNION ALL
 SELECT 'parties'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_parties
UNION ALL
 SELECT 'merchant_locations'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_merchant_locations
UNION ALL
 SELECT 'transactions'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_transactions
UNION ALL
 SELECT 'merchant_has_location'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_merchant_has_location
UNION ALL
 SELECT 'cities'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_cities
UNION ALL
 SELECT 'states'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_states
UNION ALL
 SELECT 'zipcodes'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_zipcodes
UNION ALL
 SELECT 'location_in_city'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_location_in_city
UNION ALL
 SELECT 'location_in_zip'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_location_in_zip
UNION ALL
 SELECT 'zip_assigned_to_city'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_zip_assigned_to_city
UNION ALL
 SELECT 'city_located_in_state'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_city_located_in_state
UNION ALL
 SELECT 'party_has_card'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_party_has_card
UNION ALL
 SELECT 'party_is_merchant'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_party_is_merchant
UNION ALL
 SELECT 'merchant_assigned'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_merchant_assigned
UNION ALL
 SELECT 'addresses'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_addresses
UNION ALL
 SELECT 'phones'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_phones
UNION ALL
 SELECT 'emails'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_emails
UNION ALL
 SELECT 'ips'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_ips
UNION ALL
 SELECT 'devices'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_devices
UNION ALL
 SELECT 'identity_documents'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_identity_documents
UNION ALL
 SELECT 'party_has_address'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_party_has_address
UNION ALL
 SELECT 'party_has_phone'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_party_has_phone
UNION ALL
 SELECT 'party_has_email'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_party_has_email
UNION ALL
 SELECT 'party_has_identity_document'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_party_has_identity_document
UNION ALL
 SELECT 'party_has_ip'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_party_has_ip
UNION ALL
 SELECT 'party_has_device'::text AS dataset,
    count(*) AS rows
   FROM tf_gnn_prep.load_party_has_device;


ALTER VIEW tf_gnn_prep.audit_load_counts OWNER TO abrahamchandy;

--
-- Name: audit_split_summary; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.audit_split_summary AS
 SELECT split_id,
        CASE split_id
            WHEN 0 THEN 'train'::text
            WHEN 1 THEN 'validation'::text
            WHEN 2 THEN 'test'::text
            ELSE 'invalid'::text
        END AS split_name,
    count(*) AS rows,
    count(*) FILTER (WHERE (is_fraud = 1)) AS fraud_rows,
    count(*) FILTER (WHERE (label_resolution_status = 'confirmed_fraud'::text)) AS confirmed_fraud_rows,
    count(*) FILTER (WHERE (label_resolution_status = 'matured_clean'::text)) AS matured_clean_rows,
    count(*) FILTER (WHERE (label_resolution_status = 'unresolved'::text)) AS unresolved_rows,
    count(*) FILTER (WHERE (NOT card_seen_in_train)) AS unseen_card_rows,
    count(*) FILTER (WHERE (NOT merchant_seen_in_train)) AS unseen_merchant_rows,
    min(unix_time) AS minimum_epoch,
    max(unix_time) AS maximum_epoch
   FROM tf_gnn_prep.transaction_manifest
  GROUP BY split_id
  ORDER BY split_id;


ALTER VIEW tf_gnn_prep.audit_split_summary OWNER TO abrahamchandy;

--
-- Name: event_seq_by_time; Type: TABLE; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE UNLOGGED TABLE tf_gnn_prep.event_seq_by_time (
    unix_time bigint NOT NULL,
    latest_event_seq bigint
);


ALTER TABLE tf_gnn_prep.event_seq_by_time OWNER TO abrahamchandy;

--
-- Name: test_transactions; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.test_transactions AS
 SELECT transaction_id,
    unix_time,
    event_seq,
    node_index,
    amount,
    is_fraud,
    mer_cat,
    use_chip,
    error,
    is_online,
    card_number,
    merchant_id,
    split_id,
    label_resolution_status,
    label_available_unix_time,
    label_available_after_event_seq,
    label_maturation_deadline_unix_time,
    card_seen_in_train,
    merchant_seen_in_train
   FROM tf_gnn_prep.transaction_manifest
  WHERE (split_id = 2);


ALTER VIEW tf_gnn_prep.test_transactions OWNER TO abrahamchandy;

--
-- Name: train_transactions; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.train_transactions AS
 SELECT transaction_id,
    unix_time,
    event_seq,
    node_index,
    amount,
    is_fraud,
    mer_cat,
    use_chip,
    error,
    is_online,
    card_number,
    merchant_id,
    split_id,
    label_resolution_status,
    label_available_unix_time,
    label_available_after_event_seq,
    label_maturation_deadline_unix_time,
    card_seen_in_train,
    merchant_seen_in_train
   FROM tf_gnn_prep.transaction_manifest
  WHERE (split_id = 0);


ALTER VIEW tf_gnn_prep.train_transactions OWNER TO abrahamchandy;

--
-- Name: validation_transactions; Type: VIEW; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE VIEW tf_gnn_prep.validation_transactions AS
 SELECT transaction_id,
    unix_time,
    event_seq,
    node_index,
    amount,
    is_fraud,
    mer_cat,
    use_chip,
    error,
    is_online,
    card_number,
    merchant_id,
    split_id,
    label_resolution_status,
    label_available_unix_time,
    label_available_after_event_seq,
    label_maturation_deadline_unix_time,
    card_seen_in_train,
    merchant_seen_in_train
   FROM tf_gnn_prep.transaction_manifest
  WHERE (split_id = 1);


ALTER VIEW tf_gnn_prep.validation_transactions OWNER TO abrahamchandy;

--
-- Name: card_first_seen card_first_seen_pkey; Type: CONSTRAINT; Schema: tf_gnn_prep; Owner: abrahamchandy
--

ALTER TABLE ONLY tf_gnn_prep.card_first_seen
    ADD CONSTRAINT card_first_seen_pkey PRIMARY KEY (card_number);


--
-- Name: category_first_seen category_first_seen_pkey; Type: CONSTRAINT; Schema: tf_gnn_prep; Owner: abrahamchandy
--

ALTER TABLE ONLY tf_gnn_prep.category_first_seen
    ADD CONSTRAINT category_first_seen_pkey PRIMARY KEY (category);


--
-- Name: event_seq_by_time event_seq_by_time_pkey; Type: CONSTRAINT; Schema: tf_gnn_prep; Owner: abrahamchandy
--

ALTER TABLE ONLY tf_gnn_prep.event_seq_by_time
    ADD CONSTRAINT event_seq_by_time_pkey PRIMARY KEY (unix_time);


--
-- Name: graph_policy graph_policy_pkey; Type: CONSTRAINT; Schema: tf_gnn_prep; Owner: abrahamchandy
--

ALTER TABLE ONLY tf_gnn_prep.graph_policy
    ADD CONSTRAINT graph_policy_pkey PRIMARY KEY (policy_id);


--
-- Name: label_policy label_policy_pkey; Type: CONSTRAINT; Schema: tf_gnn_prep; Owner: abrahamchandy
--

ALTER TABLE ONLY tf_gnn_prep.label_policy
    ADD CONSTRAINT label_policy_pkey PRIMARY KEY (policy_id);


--
-- Name: merchant_first_seen merchant_first_seen_pkey; Type: CONSTRAINT; Schema: tf_gnn_prep; Owner: abrahamchandy
--

ALTER TABLE ONLY tf_gnn_prep.merchant_first_seen
    ADD CONSTRAINT merchant_first_seen_pkey PRIMARY KEY (merchant_id);


--
-- Name: party_first_seen party_first_seen_pkey; Type: CONSTRAINT; Schema: tf_gnn_prep; Owner: abrahamchandy
--

ALTER TABLE ONLY tf_gnn_prep.party_first_seen
    ADD CONSTRAINT party_first_seen_pkey PRIMARY KEY (party_id);


--
-- Name: split_policy split_policy_pkey; Type: CONSTRAINT; Schema: tf_gnn_prep; Owner: abrahamchandy
--

ALTER TABLE ONLY tf_gnn_prep.split_policy
    ADD CONSTRAINT split_policy_pkey PRIMARY KEY (policy_id);


--
-- Name: train_cards train_cards_pkey; Type: CONSTRAINT; Schema: tf_gnn_prep; Owner: abrahamchandy
--

ALTER TABLE ONLY tf_gnn_prep.train_cards
    ADD CONSTRAINT train_cards_pkey PRIMARY KEY (card_number);


--
-- Name: train_merchants train_merchants_pkey; Type: CONSTRAINT; Schema: tf_gnn_prep; Owner: abrahamchandy
--

ALTER TABLE ONLY tf_gnn_prep.train_merchants
    ADD CONSTRAINT train_merchants_pkey PRIMARY KEY (merchant_id);


--
-- Name: transaction_event_seq transaction_event_seq_pkey; Type: CONSTRAINT; Schema: tf_gnn_prep; Owner: abrahamchandy
--

ALTER TABLE ONLY tf_gnn_prep.transaction_event_seq
    ADD CONSTRAINT transaction_event_seq_pkey PRIMARY KEY (transaction_id);


--
-- Name: transaction_event_seq_seq_uq; Type: INDEX; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE UNIQUE INDEX transaction_event_seq_seq_uq ON tf_gnn_prep.transaction_event_seq USING btree (event_seq);


--
-- Name: transaction_event_seq_time_idx; Type: INDEX; Schema: tf_gnn_prep; Owner: abrahamchandy
--

CREATE INDEX transaction_event_seq_time_idx ON tf_gnn_prep.transaction_event_seq USING btree (unix_time);


--
-- PostgreSQL database dump complete
--

\unrestrict T1JG2oRAs9yHARvcIlXS2agelPem6SMyggY8sdMmwrJQhd534nAWjAChqaLwOQk

