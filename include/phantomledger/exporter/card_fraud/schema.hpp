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
//                                   gift-card scam currently ride this SAME
//                                   channel with fraud.flag=1, but use the
//                                   victim's DEPOSIT ACCOUNT as row.source
//                                   (unauthorized.cpp). That is documented
//                                   realism debt: the fraud event does not
//                                   yet carry a compromised card instrument.
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
// `use_chip` and `error` remain export-time derivations and tracked
// realism debt; City.population is the catalogue value.
//
// The PII investigative layer (Address/Phone/Email/IP/Device/ID/
// Full_Name/DOB + Has_* edges) is DEMO ONLY in TF_GNN_v3 and empty on
// real TabFormer; PhantomLedger populates it from its PII synthesis.
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

inline constexpr std::array<std::string_view, 3> kCityCols{"id", "city",
                                                           "population"};
inline constexpr Table kCity = detail::make("City.csv", kCityCols);

inline constexpr std::array<std::string_view, 1> kStateCols{"id"};
inline constexpr Table kState = detail::make("State.csv", kStateCols);

inline constexpr std::array<std::string_view, 1> kZipcodeCols{"id"};
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

// ---------------------------------------------- static structural edges

inline constexpr std::array<std::string_view, 2> kMerchantAssignedCols{
    "merchant_id", "category"};
inline constexpr Table kMerchantAssigned =
    detail::make("Merchant_Assigned.csv", kMerchantAssignedCols);

inline constexpr std::array<std::string_view, 2> kPartyHasCardCols{
    "party_id", "card_number"};
inline constexpr Table kPartyHasCard =
    detail::make("Party_Has_Card.csv", kPartyHasCardCols);

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

inline constexpr std::array<std::string_view, 2> kHasIdCols{"party_id", "id"};
inline constexpr Table kHasId = detail::make("Has_ID.csv", kHasIdCols);

inline constexpr std::array<std::string_view, 2> kHasIpCols{"party_id",
                                                            "ip_id"};
inline constexpr Table kHasIp = detail::make("Has_IP.csv", kHasIpCols);

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

} // namespace PhantomLedger::exporter::schema::card_fraud
