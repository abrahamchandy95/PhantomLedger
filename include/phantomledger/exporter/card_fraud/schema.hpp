#pragma once
/*
  Table contract for the `card-fraud` use case: a TabFormer-shaped,
  transaction-fraud-only corpus for the TigerGraph TF_GNN_v3 schema. The
  generator emits LOADED attributes only; every computed slot in TF_GNN_v3
  (pagerank/community/c_id/c_size, the 25 engineered Payment_Transaction
  features, prev_state_differs, and the interaction, co-occurrence and
  community edges) is in-graph query work, not ours.

  COLUMN ORDER MATCHES TF_GNN_v3'S LOADED-ATTRIBUTE ORDER because the
  loading jobs map POSITIONALLY. Append new columns; inserting one
  mid-table silently corrupts a live load. Table count is kTableCount at
  the foot of this file — do not restate it in prose.

  THE CARD VIEW (which corpus rows become Payment_Transaction):
    channel == Legit::cardPurchase  credit-card purchases; row.source IS
                                    the card Key.
    channel == Legit::merchant      account-paid POS purchases; row.source
                                    is the deposit-account Key, read as a
                                    DEBIT card (CHOICE; TabFormer mixes
                                    credit and debit).

  Registered limitation: unauthorized card fraud and the authorized
  gift-card scam ride the cardPurchase channel with fraud.flag=1 but carry
  the victim's primary deposit account, so they export as debit. Fraud is
  planned after CardCycleDriver closes legitimate cycles; moving these rows
  onto an issued credit-card liability needs a lifecycle-ordering redesign,
  because a late key swap would bypass statements, payments and interest.

  CARD ATTRIBUTION (deterministic, export-time): a source Key in
  entity::card::Registry.byKey is that credit card (at most one per
  person); any other source Key is the account's derived DEBIT card
  (derive.hpp), so every account owns exactly one debit-card identity for
  the whole run.

  STREAMED (row-scale): the Payment_Transaction vertex plus four
  timestamped relationships, including Transaction_Uses_Device and
  Transaction_Uses_IP, which name the endpoint the transaction actually
  used. Has_Device/Has_IP carry the party -> endpoint associations the
  institution has ON FILE.

  DO NOT WITHHOLD THE ENDPOINT OWNERSHIP TABLES. Party is the ONLY path
  TF_GNN_v3 has to Device and IP, so empty ownership makes every endpoint
  vertex isolated and the whole layer inert. They were once withheld
  because every customer endpoint had an owning Party and no attacker
  endpoint did, making "no Party edge" an exact synonym for "attacker
  endpoint" — but that was a property of the GENERATOR, and the generator
  no longer has it: registry coverage is partial (`infra::enrollment`) and
  a declared share of unauthorized fraud runs from the victim's own
  endpoint or a residential proxy. The residual is a weak, real feature,
  sized and banded by tests/test_card_endpoint_graph.cpp.

  LABELS. The one supervised target is `Payment_Transaction.is_fraud`
  (gate 1 of docs/card_fraud_online_gnn.md). Four columns below are
  FULL-WINDOW entity verdicts by construction — "this card ever carried a
  flag-1 row", "this party is a fraud actor", "this device/IP is flagged":

    kCardCols[1]    "is_fraud"    always 0
    kPartyCols[1]   "is_fraud"    always 0
    kIpCols[1]      "is_blocked"  always 0
    kDeviceCols[1]  "is_blocked"  always 0

  Read as features they answer the training question before the model sees
  a transaction: a GNN given Card.is_fraud scores ~100% and learns nothing
  about behaviour. Schema conformance is not a shield — a leaking column
  leaks whether or not TF_GNN_v3 declares it. They are RETAINED so the
  loading jobs keep mapping positionally and WRITTEN AS 0 so no leak
  reaches the feature graph; their investigative content lives in
  kGroundTruthLabel, which nothing loads and no edge points at. Evaluate
  entity-level detection against THAT; train against the streamed target.

  Every other exported attribute is point-in-time honest: Party created_at
  is the membership joinTs, and each transaction attribute is observable at
  its own timestamp.

  MERCHANT SET: the union of destination Keys observed in the view. Catalog
  merchants carry their category; card-rail fraud destinations come from
  the SAME acceptance catalogue as legitimate spend, modality-conditioned
  through the same distance-decay kernel, degrading to the blueprint's
  biller accounts only when no eligible merchant exists — those degenerate
  destinations may lie outside the catalog, still become Merchant vertices,
  and take derive.hpp's content-keyed category fallback. Geography comes
  from entity::merchant::Record.location and the world catalogue; online
  merchants and non-catalog fraud billers are geography-free. Coordinates
  are AREA CENTROIDS — see kMerchantLocation for what that forbids
  downstream. `use_chip` is CAUSAL: Online iff the destination is a
  geography-free acceptance endpoint, Chip/Swipe on physical outlets by the
  dated US EMV terminal mix. `error` is an export-time content hash and
  tracked realism debt (authorization attempts are unmodelled);
  City.population is the catalogue value.

  The PII investigative layer (Address/Phone/Email/IP/Device/ID/Full_Name/
  DOB + Has_* edges) is DEMO ONLY in TF_GNN_v3 and empty on real TabFormer;
  PhantomLedger populates all of it from its PII and access synthesis.
 */

#include "phantomledger/exporter/schema.hpp"

namespace PhantomLedger::exporter::schema::card_fraud {

/* ----------------------------------------------------------- vertices */

/* is_fraud: RETAINED FOR POSITIONAL LOADING, ALWAYS 0 (see LABELS). */
inline constexpr std::array<std::string_view, 2> kCardCols{"card_number",
                                                           "is_fraud"};
inline constexpr Table kCard = detail::make("Card.csv", kCardCols);

inline constexpr std::array<std::string_view, 1> kMerchantCols{"id"};
inline constexpr Table kMerchant = detail::make("Merchant.csv", kMerchantCols);

inline constexpr std::array<std::string_view, 1> kMerchantCategoryCols{
    "category"};
inline constexpr Table kMerchantCategory =
    detail::make("Merchant_Category.csv", kMerchantCategoryCols);

/* is_fraud: RETAINED FOR POSITIONAL LOADING, ALWAYS 0 (see LABELS). */
inline constexpr std::array<std::string_view, 7> kPartyCols{
    "id", "is_fraud", "gender", "dob", "party_type", "name", "created_at"};
inline constexpr Table kParty = detail::make("Party.csv", kPartyCols);

/* lat/lon are the catalogue area's CENTROID in decimal degrees, converted
 * from the world's integer microdegrees at export. They sit LAST so the
 * loader's positional mapping for `id`/`city`/`population` is unmoved, and
 * because that is TF_GNN_v3's own order (id, city, population, lat, lon). */
inline constexpr std::array<std::string_view, 5> kCityCols{
    "id", "city", "population", "lat", "lon"};
inline constexpr Table kCity = detail::make("City.csv", kCityCols);

/* No coordinate: the catalogue models areas, not state polygons, and a
 * state centroid would have to be invented. */
inline constexpr std::array<std::string_view, 1> kStateCols{"id"};
inline constexpr Table kState = detail::make("State.csv", kStateCols);

/* TF_GNN_v3 order is Zipcode(id, lat, lon, has_coordinates, ...), so these
 * land in the loader's positional slots directly. */
inline constexpr std::array<std::string_view, 3> kZipcodeCols{"id", "lat",
                                                              "lon"};
inline constexpr Table kZipcode = detail::make("Zipcode.csv", kZipcodeCols);

/* The streamed transaction vertex. `is_fraud` HERE is the one supervised
 * target: a per-row label observable at that row's own timestamp, not an
 * entity summary. */
inline constexpr std::array<std::string_view, 8> kPaymentTransactionCols{
    "id",        "transaction_time", "amount",   "is_fraud",
    "unix_time", "mer_cat",          "use_chip", "error"};
inline constexpr Table kPaymentTransaction =
    detail::make("Payment_Transaction.csv", kPaymentTransactionCols);

/* -------------------------------------------------- transaction edges
 * Streamed alongside Payment_Transaction during the fold; edge_unix_time
 * drives the GNN temporal sampler. */

inline constexpr std::array<std::string_view, 3> kCardSendCols{
    "txn_id", "card_number", "edge_unix_time"};
inline constexpr Table kCardSend =
    detail::make("Card_Send_Transaction.csv", kCardSendCols);

inline constexpr std::array<std::string_view, 3> kMerchantReceiveCols{
    "txn_id", "merchant_id", "edge_unix_time"};
inline constexpr Table kMerchantReceive =
    detail::make("Merchant_Receive_Transaction.csv", kMerchantReceiveCols);

/* The session actually used for this transaction: event-time edges, not
 * the whole-window Party Has_* associations below. A temporal model may
 * score the row against prior device/IP history, then append the current
 * edge to memory only after recording the prediction. */
inline constexpr std::array<std::string_view, 3> kTransactionUsesDeviceCols{
    "txn_id", "device_id", "edge_unix_time"};
inline constexpr Table kTransactionUsesDevice =
    detail::make("Transaction_Uses_Device.csv", kTransactionUsesDeviceCols);

inline constexpr std::array<std::string_view, 3> kTransactionUsesIpCols{
    "txn_id", "ip_id", "edge_unix_time"};
inline constexpr Table kTransactionUsesIp =
    detail::make("Transaction_Uses_IP.csv", kTransactionUsesIpCols);

/* --------------------------------------------- static structural edges */

inline constexpr std::array<std::string_view, 2> kMerchantAssignedCols{
    "merchant_id", "category"};
inline constexpr Table kMerchantAssigned =
    detail::make("Merchant_Assigned.csv", kMerchantAssignedCols);

inline constexpr std::array<std::string_view, 2> kPartyHasCardCols{
    "party_id", "card_number"};
inline constexpr Table kPartyHasCard =
    detail::make("Party_Has_Card.csv", kPartyHasCardCols);

/* The merchant -> PROPRIETOR register, loaded as
 * `Party_Is_Merchant(FROM Party, TO Merchant)` with reverse edge
 * `Merchant_Owned_By_Party`, so the loader's view flips this column order.
 * MUST NOT BE EMPTY: tf_gnn_loader_v2 hard-aborts the whole push on it. */
inline constexpr std::array<std::string_view, 2> kIsMerchantCols{"merchant_id",
                                                                 "party_id"};
inline constexpr Table kIsMerchant =
    detail::make("Is_Merchant.csv", kIsMerchantCols);

inline constexpr std::array<std::string_view, 2> kHasStateCols{"merchant_id",
                                                               "state_id"};
inline constexpr Table kHasState = detail::make("Has_State.csv", kHasStateCols);

inline constexpr std::array<std::string_view, 2> kHasCityCols{"merchant_id",
                                                              "city_id"};
inline constexpr Table kHasCity = detail::make("Has_City.csv", kHasCityCols);

inline constexpr std::array<std::string_view, 2> kHasZipCols{"merchant_id",
                                                             "zipcode_id"};
inline constexpr Table kHasZip = detail::make("Has_Zip.csv", kHasZipCols);

/* MERCHANT COORDINATES. TF_GNN_v3 has nowhere to hang a coordinate on
 * `Merchant`; geography lives on `Merchant_Location(lat, lon,
 * has_coordinates)`, which the loader would otherwise fill from defaults.
 *
 * The value is the merchant's catalogue AREA CENTROID, the same point
 * `Zipcode` carries, because that is the resolution the world models:
 * `Record.location` is a GeoAreaId, not a street coordinate. TWO MERCHANTS
 * IN ONE AREA THEREFORE SHARE A POINT — do not build a feature that assumes
 * distinct outlet coordinates. Emitting it per merchant rather than only on
 * `Zipcode` saves a two-hop join and makes merchant-to-anything distance
 * computable in one step.
 *
 * ROW PRESENCE IS THE `has_coordinates` MASK: a row exists iff the merchant
 * has a modelled physical area, so online merchants and non-catalog fraud
 * billers are ABSENT. Do not zero-fill — 0,0 is the Gulf of Guinea, and a
 * LEFT JOIN returning NULL is the correct signal.
 *
 * DRAW-FREE: read off world state already resolved for the
 * Has_City/Has_State/Has_Zip chain, so the corpus stream cannot move. */
inline constexpr std::array<std::string_view, 3> kMerchantLocationCols{
    "merchant_id", "lat", "lon"};
inline constexpr Table kMerchantLocation =
    detail::make("Merchant_Location.csv", kMerchantLocationCols);

/* PARTY HOME GEOGRAPHY — the cardholder end of CARDHOLDER-TO-MERCHANT
 * DISTANCE, the most-cited card-fraud feature and the axis the generator's
 * own selection kernel is built on (`local_pools.hpp` scores
 * `popularity * exp(-distanceMiles / scaleMiles(homeArea))`). These load as
 * `Party_Has_Std_City` / `_Std_Postcode` / `_Std_State` and point at the
 * SAME City/State/Zipcode vertices merchant geography uses, so one hop of
 * `Zipcode.lat/lon` gives both endpoints of a haversine.
 *
 * FULL ROSTER, world-derived. Home area is assigned at PII synthesis on the
 * isolated `{"home-geo", <household>}` lane (coresidents share an area), so
 * these rows are prefix-invariant and observable at any timestamp —
 * `test_card_point_in_time` classifies them world-derived-identical, the
 * strictest of its three classes.
 *
 * ONE ROW PER OCCUPIED TENURE, NOT ONE PER PARTY, each carrying
 * `since_unix_time`: the epoch second from which that home holds. A party
 * who never moves emits exactly one row, stamped at the window start.
 *
 * THE TIMESTAMP IS THE WHOLE POINT: a distance feature computed from an
 * undated home edge is silently wrong for every mover, and wrong in the
 * direction that looks right — the join succeeds and returns a plausible
 * number. A consumer must pick the tenure live at the transaction's own
 * timestamp, the same discipline the session edges require.
 * `since_unix_time` is APPENDED so `party_id` and the area id stay at index
 * 0 and 1 for the loader's positional mapping.
 *
 * COVERAGE IS TOTAL IN PRACTICE and deliberately not assumed: the
 * production locale mix (`LocaleMix::usBankDefault`, 96% US + 15 foreign
 * weights) names exactly the 16 countries the catalogue carries. The emit
 * stays guarded on `contains(geoArea)` because a narrower catalogue or a
 * wider mix would dangle an edge; absence is the mask, as for merchants.
 *
 * DO NOT DROP FOREIGN-DOMICILED PARTIES (~4% of the roster). A US bank has
 * customers living abroad, their distance to a US merchant is genuinely
 * enormous, and suppressing the row would BOTH hide a real feature and
 * silently make "has a Std_City edge" a US-residency flag. The 15 foreign
 * areas carry real subdivision codes (LND, ON, CMX, ...) that collide with
 * no US state code, so the shared State vertex stays unambiguous. */
inline constexpr std::array<std::string_view, 3> kHasStdCityCols{
    "party_id", "city_id", "since_unix_time"};
inline constexpr Table kHasStdCity =
    detail::make("Has_Std_City.csv", kHasStdCityCols);

inline constexpr std::array<std::string_view, 3> kHasStdPostcodeCols{
    "party_id", "zipcode_id", "since_unix_time"};
inline constexpr Table kHasStdPostcode =
    detail::make("Has_Std_Postcode.csv", kHasStdPostcodeCols);

inline constexpr std::array<std::string_view, 3> kHasStdStateCols{
    "party_id", "state_id", "since_unix_time"};
inline constexpr Table kHasStdState =
    detail::make("Has_Std_State.csv", kHasStdStateCols);

/* zipcode -> city and city -> state. These span BOTH merchant outlet areas
 * and party home areas, because the vertex tables do and the loader
 * validates that every edge endpoint resolves. */
inline constexpr std::array<std::string_view, 2> kAssignedToCols{"zipcode_id",
                                                                 "city_id"};
inline constexpr Table kAssignedTo =
    detail::make("Assigned_To.csv", kAssignedToCols);

inline constexpr std::array<std::string_view, 2> kLocatedInCols{"city_id",
                                                                "state_id"};
inline constexpr Table kLocatedIn =
    detail::make("Located_In.csv", kLocatedInCols);

/* ------------------------------------- PII / investigative layer
 * DEMO ONLY in TF_GNN_v3 (empty on TabFormer; excluded from the GNN).
 * PhantomLedger populates it from its PII synthesis. */

inline constexpr std::array<std::string_view, 1> kAddressCols{"address"};
inline constexpr Table kAddress = detail::make("Address.csv", kAddressCols);

inline constexpr std::array<std::string_view, 1> kPhoneCols{"phone_number"};
inline constexpr Table kPhone = detail::make("Phone.csv", kPhoneCols);

inline constexpr std::array<std::string_view, 1> kEmailCols{"email"};
inline constexpr Table kEmail = detail::make("Email.csv", kEmailCols);

/* LSH band-bucket vertices for email similarity (common/minhash EMH prefix,
 * b=10 bands x r=1). Derived from the email string alone. */
inline constexpr std::array<std::string_view, 1> kEmailMinhashCols{"id"};
inline constexpr Table kEmailMinhash =
    detail::make("Email_Minhash.csv", kEmailMinhashCols);

/* is_blocked: RETAINED FOR POSITIONAL LOADING, ALWAYS 0 (see LABELS). */
inline constexpr std::array<std::string_view, 2> kIpCols{"id", "is_blocked"};
inline constexpr Table kIp = detail::make("IP.csv", kIpCols);

/* is_blocked: RETAINED FOR POSITIONAL LOADING, ALWAYS 0 (see LABELS). */
inline constexpr std::array<std::string_view, 2> kDeviceCols{"id",
                                                             "is_blocked"};
inline constexpr Table kDevice = detail::make("Device.csv", kDeviceCols);

inline constexpr std::array<std::string_view, 2> kIdCols{"id", "id_type"};
inline constexpr Table kId = detail::make("ID.csv", kIdCols);

inline constexpr std::array<std::string_view, 1> kFullNameCols{"name"};
inline constexpr Table kFullName = detail::make("Full_Name.csv", kFullNameCols);

inline constexpr std::array<std::string_view, 1> kDobCols{"dob"};
inline constexpr Table kDob = detail::make("DOB.csv", kDobCols);

/* PII edges. Column order follows the TF_GNN_v3 FROM/TO direction (note
 * Has_Address runs Address -> Party). */

inline constexpr std::array<std::string_view, 2> kHasAddressCols{"address",
                                                                 "party_id"};
inline constexpr Table kHasAddress =
    detail::make("Has_Address.csv", kHasAddressCols);

inline constexpr std::array<std::string_view, 2> kHasPhoneCols{"party_id",
                                                               "phone_number"};
inline constexpr Table kHasPhone = detail::make("Has_Phone.csv", kHasPhoneCols);

inline constexpr std::array<std::string_view, 2> kHasEmailCols{"party_id",
                                                               "email"};
inline constexpr Table kHasEmail = detail::make("Has_Email.csv", kHasEmailCols);

inline constexpr std::array<std::string_view, 2> kHasEmailMinhashCols{
    "email", "minhash_id"};
inline constexpr Table kHasEmailMinhash =
    detail::make("Has_Email_Minhash.csv", kHasEmailMinhashCols);

inline constexpr std::array<std::string_view, 2> kHasIdCols{"party_id", "id"};
inline constexpr Table kHasId = detail::make("Has_ID.csv", kHasIdCols);

/* The party -> endpoint associations the institution has ON FILE, filtered
 * by registry coverage (`infra::enrollment`). Whole-window and
 * world-derived; transaction-time endpoint evidence is carried by
 * Transaction_Uses_IP instead. MUST NOT BE EMPTY: tf_gnn_loader_v2
 * hard-aborts on it, and Party is the only path to the IP vertex. */
inline constexpr std::array<std::string_view, 2> kHasIpCols{"party_id",
                                                            "ip_id"};
inline constexpr Table kHasIp = detail::make("Has_IP.csv", kHasIpCols);

/* As kHasIp, for devices; transaction-time evidence is
 * Transaction_Uses_Device. MUST NOT BE EMPTY, same abort. */
inline constexpr std::array<std::string_view, 2> kHasDeviceCols{"party_id",
                                                                "device_id"};
inline constexpr Table kHasDevice =
    detail::make("Has_Device.csv", kHasDeviceCols);

inline constexpr std::array<std::string_view, 2> kHasDobCols{"party_id", "dob"};
inline constexpr Table kHasDob = detail::make("Has_DOB.csv", kHasDobCols);

inline constexpr std::array<std::string_view, 2> kHasFullNameCols{"party_id",
                                                                  "name"};
inline constexpr Table kHasFullName =
    detail::make("Has_Full_Name.csv", kHasFullNameCols);

/* INVESTIGATIVE GROUND TRUTH. Deliberately NOT a graph vertex and NOT a
 * graph edge: no TF_GNN_v3 loading job reads this table and nothing in the
 * schema points at it. It is where the four zeroed entity labels went (see
 * LABELS), so the investigative view survives for EVALUATION while the
 * feature graph stays free of full-window leakage.
 *
 *   entity_type  card | party | device | ip
 *   entity_id    the identifier the corresponding vertex table writes
 *                (derive::cardId, renderCustomerId, renderDeviceId,
 *                network::format), so it joins 1:1
 *   label        ever_fraud   card carried >= 1 flag-1 view row
 *                fraud_actor  party is a ring member, solo fraudster or
 *                             mule — VICTIMS ARE NOT LABELLED
 *                flagged      device carries the modelled flag
 *                blacklisted  IP carries the modelled blacklist
 *
 * POSITIVES ONLY: an entity absent here has label 0. The entity universe is
 * the vertex tables; this is the label overlay. Every row is a WHOLE-WINDOW
 * fact, so using any of them as a model input reintroduces exactly the leak
 * this table exists to quarantine. */
inline constexpr std::array<std::string_view, 3> kGroundTruthLabelCols{
    "entity_type", "entity_id", "label"};
inline constexpr Table kGroundTruthLabel =
    detail::make("Ground_Truth_Label.csv", kGroundTruthLabelCols);

/* THE SINGLE SOURCE OF TRUTH FOR HOW MANY TABLES THIS SCHEMA SHIPS. It
 * exists because the number was once duplicated into four places, three of
 * them went stale, and the result would have HARD-ABORTED A CORRECT CORPUS:
 * `docs/card_fraud_postgres_acceptance.sql` raised on
 * `registered_count <> 39` while the set had grown to 43, and
 * `test_table_golden`'s own `>= 39` could not see it.
 *
 * RULES:
 *   * adding or removing a table MUST update this constant;
 *   * `test_table_golden` asserts the live PostgreSQL registry equals it
 *     EXACTLY — not `>=`, which cannot see a table that went missing;
 *   * `docs/card_fraud_postgres_acceptance.sql` carries this number twice
 *     and MUST be updated in the same commit. It is repository-external and
 *     cannot include this header, which is why it is named here rather than
 *     trusted to stay in step. */
inline constexpr std::size_t kTableCount = 43;

} // namespace PhantomLedger::exporter::schema::card_fraud
