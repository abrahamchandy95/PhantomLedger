#pragma once
//
// phantomledger/exporter/card_fraud/schema.hpp
//
// Table contract for the `card-fraud` use case: a TabFormer-shaped,
// transaction-fraud-only corpus shaped for the owner's TigerGraph
// TF_GNN_v3 schema. The generator emits LOADED attributes only; every
// computed slot in TF_GNN_v3 (pagerank/community/c_id/c_size, the 25
// engineered Payment_Transaction features, prev_state_differs, and the
// interaction, co-occurrence and community edges) is in-graph
// TigerGraph query work, not ours.
//
// THE CARD VIEW (which corpus rows become Payment_Transaction):
//   channel == Legit::cardPurchase  credit-card purchases; row.source
//                                   IS the card Key (payments.cpp
//                                   selectPaymentRoute; the card-cycle
//                                   driver keys ingestion by source).
//                                   Unauthorized card fraud and the
//                                   authorized gift-card scam ride this SAME
//                                   channel with fraud.flag=1, but currently
//                                   carry the victim's primary deposit
//                                   account and export as debit. Fraud is
//                                   planned after CardCycleDriver has closed
//                                   legitimate cycles, so moving these rows
//                                   onto an issued credit-card liability
//                                   requires a lifecycle-ordering redesign;
//                                   a late key swap would bypass statements,
//                                   payments and interest.
//   channel == Legit::merchant      account-paid POS purchases;
//                                   row.source is the deposit-account
//                                   Key. Interpreted as DEBIT-card
//                                   transactions (CHOICE; TabFormer
//                                   mixes credit and debit cards).
//
// CARD ATTRIBUTION (deterministic, export-time; no core-model change):
//   source Key found in entity::card::Registry.byKey -> that credit
//   card (at most one credit card per person). Any other source Key ->
//   the account's derived DEBIT card: a stable identifier derived from
//   the account Key (derive.hpp), so every account owns exactly one
//   debit-card identity across the whole run.
//
// EXPORT SHAPE: 43 tables total (37 + the owner-requested email LSH
// pair: Email_Minhash buckets and the Email -> bucket Has_Email_Minhash
// edge, derived solely from the already-exported email string via the
// shared common/minhash EMH wrapper; + Merchant_Location, the owner-
// requested merchant coordinate pair, merchant-coordinates-2026-07;
// + the three party home-geography edges, party-geography-2026-07, which
// together with the coordinates make cardholder-to-merchant DISTANCE
// computable downstream for the first time).
// Five row-scale tables are streamed:
// one Payment_Transaction vertex table and four timestamped relationship
// tables, including Transaction_Uses_Device and Transaction_Uses_IP.
// Those two session edges identify the endpoint used by the transaction.
// Has_Device and Has_IP carry the party -> endpoint associations the
// institution has ON FILE (attacker-infra-2026-07).
//
// THEY WERE HEADER-ONLY FOR FOUR ROUNDS and the reason is worth keeping:
// every customer endpoint had an owning Party and no attacker endpoint
// did, so "no Party edge" was an exact synonym for "attacker endpoint".
// That was a property of the GENERATOR, not of the world, and the
// generator changed on both sides of it — registry coverage is now
// partial (`infra::enrollment`), and a declared share of unauthorized
// fraud runs from the victim's own endpoint or a residential proxy. The
// residual is a weak, real feature, sized and banded by
// tests/test_card_endpoint_graph.cpp.
//
// Withholding was never free: Party is the ONLY path TF_GNN_v3 has to
// Device and IP, so empty ownership tables made every endpoint vertex
// isolated and the whole endpoint layer inert.
//
// ============================================================ LABELS
// THE ONE SUPERVISED TARGET IS `Payment_Transaction.is_fraud`
// (card-fraud-realism-v2, gate 1 of docs/card_fraud_online_gnn.md).
//
// Four columns below are FULL-WINDOW entity labels by construction —
// "this card ever carried a flag-1 row", "this party is a fraud actor",
// "this device/IP is flagged". Read as features they answer the
// training question before the model sees a single transaction: a GNN
// given Card.is_fraud scores ~100% and learns nothing about behavior.
// Schema conformance is not a shield — a leaking column is a leaking
// column whether or not TF_GNN_v3 declares it.
//
//   kCardCols[1]    "is_fraud"    always 0
//   kPartyCols[1]   "is_fraud"    always 0
//   kIpCols[1]      "is_blocked"  always 0
//   kDeviceCols[1]  "is_blocked"  always 0
//
// They are RETAINED (not dropped) so the owner's TigerGraph loading
// jobs keep mapping positionally, and WRITTEN AS 0 so no leak reaches
// the feature graph. Their investigative content moves to
// kGroundTruthLabel below — one table, outside the vertex/edge graph,
// which nothing in TF_GNN_v3 loads and no edge points at. Evaluate
// entity-level detection against THAT; train against the streamed
// transaction target.
//
// The remaining exported attributes are point-in-time honest: Party
// created_at is the membership joinTs (H3), and every transaction
// attribute is observable at its own timestamp.
// ===================================================================
//
// MERCHANT SET: the union of destination Keys observed in the view.
// Catalog merchants carry their category; card-rail fraud destinations
// are drawn from the SAME merchant acceptance catalogue as legitimate
// spend, modality-conditioned through the same distance-decay kernel
// (card-fraud-realism-v2 step b-2, unauthorized.cpp), degrading to the
// blueprint's biller accounts only when no eligible merchant exists —
// those degenerate destinations may lie outside the catalog, still
// become Merchant vertices, and take the content-keyed category
// fallback (derive.hpp). Merchant geography comes from
// entity::merchant::Record.location and the world geography catalogue.
// Online merchants and non-catalog fraud billers are geography-free.
// COORDINATES (merchant-coordinates-2026-07): the catalogue's integer
// microdegrees are rendered as decimal degrees onto Merchant_Location
// (per merchant), Zipcode and City. They are AREA CENTROIDS — see the
// Merchant_Location note below for why co-located merchants share a
// point and what that forbids downstream.
// `use_chip` is CAUSAL (ROUND 8, use-chip-causal-2026-07): Online iff
// the destination is a geography-free acceptance endpoint (catalog
// `Footprint::online` or non-catalog remote biller), Chip/Swipe on
// physical outlets by the dated US EMV terminal mix. `error` remains an
// export-time content hash and tracked realism debt (authorization
// attempts are unmodelled); City.population is the catalogue value.
//
// The PII investigative layer (Address/Phone/Email/IP/Device/ID/
// Full_Name/DOB + Has_* edges) is DEMO ONLY in TF_GNN_v3 and empty on
// real TabFormer; PhantomLedger populates ALL of it from its PII and
// access synthesis, Has_Device/Has_IP included as of
// attacker-infra-2026-07.
//
// Column order matches the TF_GNN_v3 loaded-attribute order so the
// owner's TigerGraph loading jobs map positionally.
//

#include "phantomledger/exporter/schema.hpp"

namespace PhantomLedger::exporter::schema::card_fraud {

// ------------------------------------------------------------ vertices

// is_fraud: RETAINED FOR POSITIONAL LOADING, ALWAYS 0 (see LABELS).
inline constexpr std::array<std::string_view, 2> kCardCols{"card_number",
                                                           "is_fraud"};
inline constexpr Table kCard = detail::make("Card.csv", kCardCols);

inline constexpr std::array<std::string_view, 1> kMerchantCols{"id"};
inline constexpr Table kMerchant = detail::make("Merchant.csv", kMerchantCols);

inline constexpr std::array<std::string_view, 1> kMerchantCategoryCols{
    "category"};
inline constexpr Table kMerchantCategory =
    detail::make("Merchant_Category.csv", kMerchantCategoryCols);

// is_fraud: RETAINED FOR POSITIONAL LOADING, ALWAYS 0 (see LABELS).
inline constexpr std::array<std::string_view, 7> kPartyCols{
    "id", "is_fraud", "gender", "dob", "party_type", "name", "created_at"};
inline constexpr Table kParty = detail::make("Party.csv", kPartyCols);

// lat/lon are the catalogue area's CENTROID in decimal degrees, converted
// from the world's integer microdegrees at export (merchant-coordinates-
// 2026-07). Appended after the pre-existing columns so the loader's
// positional mapping for `id`/`city`/`population` is unmoved, and in
// TF_GNN_v3's own attribute order (City: id, city, population, lat, lon).
inline constexpr std::array<std::string_view, 5> kCityCols{
    "id", "city", "population", "lat", "lon"};
inline constexpr Table kCity = detail::make("City.csv", kCityCols);

// No coordinate: the catalogue models areas, not state polygons, and a
// state centroid would have to be invented.
inline constexpr std::array<std::string_view, 1> kStateCols{"id"};
inline constexpr Table kState = detail::make("State.csv", kStateCols);

// TF_GNN_v3 order is Zipcode(id, lat, lon, has_coordinates, ...), so these
// land in the loader's positional slots directly.
inline constexpr std::array<std::string_view, 3> kZipcodeCols{"id", "lat",
                                                              "lon"};
inline constexpr Table kZipcode = detail::make("Zipcode.csv", kZipcodeCols);

// The streamed transaction vertex. `is_fraud` HERE is the one
// supervised target: a per-row label observable at that row's own
// timestamp, not an entity summary.
inline constexpr std::array<std::string_view, 8> kPaymentTransactionCols{
    "id",        "transaction_time", "amount",   "is_fraud",
    "unix_time", "mer_cat",          "use_chip", "error"};
inline constexpr Table kPaymentTransaction =
    detail::make("Payment_Transaction.csv", kPaymentTransactionCols);

// --------------------------------------------------- transaction edges
// Streamed alongside Payment_Transaction during the fold;
// edge_unix_time drives the GNN temporal sampler.

inline constexpr std::array<std::string_view, 3> kCardSendCols{
    "txn_id", "card_number", "edge_unix_time"};
inline constexpr Table kCardSend =
    detail::make("Card_Send_Transaction.csv", kCardSendCols);

inline constexpr std::array<std::string_view, 3> kMerchantReceiveCols{
    "txn_id", "merchant_id", "edge_unix_time"};
inline constexpr Table kMerchantReceive =
    detail::make("Merchant_Receive_Transaction.csv", kMerchantReceiveCols);

// The session actually used for this transaction. These are event-time
// edges, not the whole-window Party Has_* associations below. A temporal
// model may use prior device/IP history to score the row, then append the
// current edge to memory only after recording the prediction.
inline constexpr std::array<std::string_view, 3> kTransactionUsesDeviceCols{
    "txn_id", "device_id", "edge_unix_time"};
inline constexpr Table kTransactionUsesDevice =
    detail::make("Transaction_Uses_Device.csv", kTransactionUsesDeviceCols);

inline constexpr std::array<std::string_view, 3> kTransactionUsesIpCols{
    "txn_id", "ip_id", "edge_unix_time"};
inline constexpr Table kTransactionUsesIp =
    detail::make("Transaction_Uses_IP.csv", kTransactionUsesIpCols);

// ---------------------------------------------- static structural edges

inline constexpr std::array<std::string_view, 2> kMerchantAssignedCols{
    "merchant_id", "category"};
inline constexpr Table kMerchantAssigned =
    detail::make("Merchant_Assigned.csv", kMerchantAssignedCols);

inline constexpr std::array<std::string_view, 2> kPartyHasCardCols{
    "party_id", "card_number"};
inline constexpr Table kPartyHasCard =
    detail::make("Party_Has_Card.csv", kPartyHasCardCols);

// The merchant -> PROPRIETOR register (merchant-ownership-2026-07); loaded
// as `Party_Is_Merchant(FROM Party, TO Merchant)`, reverse edge
// `Merchant_Owned_By_Party`, so the loader's view flips this column order.
// Header-only until that round, which hard-aborted the downstream push.
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

// MERCHANT COORDINATES (merchant-coordinates-2026-07).
//
// TF_GNN_v3 has nowhere to hang a coordinate on `Merchant` — the schema
// deliberately moved geography onto `Merchant_Location(lat, lon,
// has_coordinates)` — and the loader has been filling all three from
// defaults with the note "the source has no coordinates, and 0,0 is a real
// place. has_coordinates=false is the mask". This table supplies them.
//
// The value is the merchant's catalogue AREA CENTROID, the same point
// `Zipcode` carries, because that is the resolution the world actually
// models: `Record.location` is a GeoAreaId, not a street coordinate. TWO
// MERCHANTS IN ONE AREA THEREFORE SHARE A POINT, and intra-area separation
// is not modelled — do not build a feature that assumes distinct outlet
// coordinates. Emitting the centroid per merchant rather than only on
// `Zipcode` saves the consumer a two-hop join and is what makes
// merchant-to-anything distance computable in one step.
//
// ROW PRESENCE IS THE `has_coordinates` MASK. A row exists iff the
// merchant has a modelled physical area, so online merchants and
// non-catalog fraud billers are ABSENT rather than carrying a fake 0,0 —
// the same convention the loader already documents, expressed as
// cardinality instead of a constant column.
//
// DRAW-FREE: read straight off world state already resolved for the
// Has_City/Has_State/Has_Zip chain, so the corpus stream cannot move.
inline constexpr std::array<std::string_view, 3> kMerchantLocationCols{
    "merchant_id", "lat", "lon"};
inline constexpr Table kMerchantLocation =
    detail::make("Merchant_Location.csv", kMerchantLocationCols);

// PARTY HOME GEOGRAPHY (party-geography-2026-07).
//
// The cardholder side of the geography layer, and the reason it exists:
// with merchant coordinates alone, CARDHOLDER-TO-MERCHANT DISTANCE — the
// single most-cited card-fraud feature, and the axis the generator's own
// selection kernel is built on (`local_pools.hpp` scores
// `popularity * exp(-distanceMiles / scaleMiles(homeArea))`) — was not
// computable downstream at all. `cf_Address` is a bare street string and
// `pii::Address::geoArea` had no exported representation, so the model
// could see where a purchase happened but never how far from home.
//
// These load as `Party_Has_Std_City` / `_Std_Postcode` / `_Std_State`,
// which TF_GNN_v3 has declared since inception and `tf_gnn_loader_v2` had
// listed as permanently unloadable ("the source has no party-level
// geography"). They point at the SAME City/State/Zipcode vertices merchant
// geography uses — the schema's own note says a cardholder's city and a
// merchant's city are the same real-world thing — so the cross-link is
// genuine and one hop of `Zipcode.lat/lon` gives both endpoints of a
// haversine.
//
// FULL ROSTER, world-derived. Home area is assigned at PII synthesis on the
// isolated `{"home-geo", <household>}` lane (coresidents share an area), so
// these rows are prefix-invariant and observable at any timestamp —
// `test_card_point_in_time` classifies them as world-derived-identical, the
// strictest of its three classes.
//
// relocation-2026-07: ONE ROW PER OCCUPIED TENURE, NOT ONE PER PARTY, and
// each carries `since_unix_time` — the epoch second from which that home
// holds. A party who never moves still emits exactly one row, stamped at the
// window start, so the pre-round shape is the no-move case rather than a
// special case.
//
// THE TIMESTAMP IS THE WHOLE POINT: a distance feature computed from an
// undated home edge is silently wrong for every mover, and wrong in the
// direction that looks right — the join succeeds and returns a plausible
// number. A consumer must pick the tenure live at the transaction's own
// timestamp, which is the same discipline the session edges already require.
// TF_GNN_v3's `Party_Has_Std_*` already declares `edge_unix_time`, so the
// representation existed; `tf_gnn_loader_v2` hardcoded `0 AS edge_unix_time`
// and now reads the column.
//
// `since_unix_time` is APPENDED, never inserted, under schema.hpp's standing
// law: column order matches the loader's positional mapping, so `party_id` and
// the area id stay at index 0 and 1.
//
// COVERAGE IS TOTAL IN PRACTICE, and deliberately not assumed: the
// production locale mix (`LocaleMix::usBankDefault`, 96% US + 15 foreign
// weights) names exactly the 16 countries the catalogue carries, so every
// party resolves. The emit is still guarded on `contains(geoArea)` because
// a narrower catalogue or a wider mix would otherwise dangle an edge, and
// absence is the mask exactly as it is for merchants.
//
// FOREIGN-DOMICILED PARTIES ARE EMITTED, NOT DROPPED (~4% of the roster).
// A US bank has customers who live abroad, their distance to a US merchant
// is genuinely enormous, and suppressing the row would BOTH hide a real
// feature and silently make "has a Std_City edge" a US-residency flag. The
// 15 foreign areas carry real subdivision codes (LND, ON, CMX, ...) that
// collide with no US state code, so the shared State vertex stays
// unambiguous.
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

// zipcode -> city and city -> state. These now span BOTH merchant outlet
// areas and party home areas (party-geography-2026-07), because the vertex
// tables do and the loader validates that every edge endpoint resolves.
inline constexpr std::array<std::string_view, 2> kAssignedToCols{"zipcode_id",
                                                                 "city_id"};
inline constexpr Table kAssignedTo =
    detail::make("Assigned_To.csv", kAssignedToCols);

inline constexpr std::array<std::string_view, 2> kLocatedInCols{"city_id",
                                                                "state_id"};
inline constexpr Table kLocatedIn =
    detail::make("Located_In.csv", kLocatedInCols);

// -------------------------------------- PII / investigative layer
// DEMO ONLY in TF_GNN_v3 (empty on TabFormer; excluded from the GNN).
// PhantomLedger populates it from its PII synthesis.

inline constexpr std::array<std::string_view, 1> kAddressCols{"address"};
inline constexpr Table kAddress = detail::make("Address.csv", kAddressCols);

inline constexpr std::array<std::string_view, 1> kPhoneCols{"phone_number"};
inline constexpr Table kPhone = detail::make("Phone.csv", kPhoneCols);

inline constexpr std::array<std::string_view, 1> kEmailCols{"email"};
inline constexpr Table kEmail = detail::make("Email.csv", kEmailCols);

// LSH band-bucket vertices for email similarity (common/minhash EMH
// prefix, b=10 bands x r=1). Derived from the email string alone.
inline constexpr std::array<std::string_view, 1> kEmailMinhashCols{"id"};
inline constexpr Table kEmailMinhash =
    detail::make("Email_Minhash.csv", kEmailMinhashCols);

// is_blocked: RETAINED FOR POSITIONAL LOADING, ALWAYS 0 (see LABELS).
inline constexpr std::array<std::string_view, 2> kIpCols{"id", "is_blocked"};
inline constexpr Table kIp = detail::make("IP.csv", kIpCols);

// is_blocked: RETAINED FOR POSITIONAL LOADING, ALWAYS 0 (see LABELS).
inline constexpr std::array<std::string_view, 2> kDeviceCols{"id",
                                                             "is_blocked"};
inline constexpr Table kDevice = detail::make("Device.csv", kDeviceCols);

inline constexpr std::array<std::string_view, 2> kIdCols{"id", "id_type"};
inline constexpr Table kId = detail::make("ID.csv", kIdCols);

inline constexpr std::array<std::string_view, 1> kFullNameCols{"name"};
inline constexpr Table kFullName = detail::make("Full_Name.csv", kFullNameCols);

inline constexpr std::array<std::string_view, 1> kDobCols{"dob"};
inline constexpr Table kDob = detail::make("DOB.csv", kDobCols);

// PII edges. Column order follows the TF_GNN_v3 FROM/TO direction
// (note Has_Address runs Address -> Party).

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

// Loader-compatibility table, intentionally header-only. Transaction-time
// endpoint evidence is carried by Transaction_Uses_IP.
inline constexpr std::array<std::string_view, 2> kHasIpCols{"party_id",
                                                            "ip_id"};
inline constexpr Table kHasIp = detail::make("Has_IP.csv", kHasIpCols);

// Loader-compatibility table, intentionally header-only. Transaction-time
// endpoint evidence is carried by Transaction_Uses_Device.
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

// ------------------------------------------ INVESTIGATIVE GROUND TRUTH
//
// Deliberately NOT a graph vertex and NOT a graph edge: no TF_GNN_v3
// loading job reads this table, and nothing in the schema points at it.
// It is where the four zeroed entity labels went (see LABELS above), so
// the investigative view survives for EVALUATION while the feature
// graph stays free of full-window leakage.
//
//   entity_type  card | party | device | ip
//   entity_id    the same identifier the corresponding vertex table
//                writes (derive::cardId, renderCustomerId,
//                renderDeviceId, network::format), so it joins 1:1
//   label        ever_fraud   card carried >= 1 flag-1 view row
//                fraud_actor  party is a ring member, solo fraudster
//                             or mule (VICTIMS ARE NOT LABELLED —
//                             card-fraud-2026-07 label definition)
//                flagged      device carries the modelled flag
//                blacklisted  IP carries the modelled blacklist
//
// POSITIVES ONLY: an entity absent from this table has label 0. The
// entity universe is the vertex tables; this is the label overlay.
// Every one of these is a WHOLE-WINDOW fact — using any of them as a
// model input reintroduces exactly the leak this table exists to
// quarantine.
inline constexpr std::array<std::string_view, 3> kGroundTruthLabelCols{
    "entity_type", "entity_id", "label"};
inline constexpr Table kGroundTruthLabel =
    detail::make("Ground_Truth_Label.csv", kGroundTruthLabelCols);

// ============================================================ TABLE COUNT
//
// THE SINGLE SOURCE OF TRUTH FOR HOW MANY TABLES THIS SCHEMA SHIPS, and it
// exists because the number was previously duplicated into four places and
// three of them went stale.
//
// THE FAILURE THIS PREVENTS WAS A HARD ABORT ON A CORRECT CORPUS.
// `docs/card_fraud_postgres_acceptance.sql` — the artifact the owner runs to
// ACCEPT a generation run — asserted `registered_count <> 39` and
// `physical_count <> 39` and raised an exception otherwise. Meanwhile
// `merchant-coordinates-2026-07` took the set 39 -> 40 and
// `party-geography-2026-07` took it 40 -> 43. Both rounds updated this file's
// header comment and the acceptance script's table MANIFEST, and neither
// updated those two scalars, so the acceptance script would have aborted with
// "expected 39 registered card_fraud tables, found 43" on a corpus with
// nothing wrong with it. `test_table_golden` could not catch it either,
// because its own assertion was `>= 39`, which 43 satisfies.
//
// RULES:
//   * adding or removing a table MUST update this constant;
//   * `test_table_golden` asserts the live PostgreSQL registry equals it
//     EXACTLY — not `>=`, which cannot see a table that went missing;
//   * `docs/card_fraud_postgres_acceptance.sql` carries this number twice and
//     must be updated in the same commit. It is a separate repository-external
//     artifact and cannot include this header, which is exactly why it is
//     called out here rather than trusted to stay in step.
inline constexpr std::size_t kTableCount = 43;

} // namespace PhantomLedger::exporter::schema::card_fraud
