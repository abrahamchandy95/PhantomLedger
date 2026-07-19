#pragma once
//
// phantomledger/exporter/card_fraud/schema.hpp
//
// Table contract for the `card-fraud` use case: a TabFormer-scale,
// transaction-fraud-only corpus shaped for the owner's TigerGraph
// TF_GNN_v3 schema. The generator emits LOADED attributes only; every
// computed slot in TF_GNN_v3 (pagerank/community/c_id/c_size, the 25
// engineered Payment_Transaction features, prev_state_differs, and the
// train/val/test interaction, co-occurrence and community edges) is
// in-graph TigerGraph query work, not ours.
//
// THE CARD VIEW (which corpus rows become Payment_Transaction):
//   channel == Legit::cardPurchase  credit-card purchases; row.source
//                                   IS the card Key (payments.cpp
//                                   selectPaymentRoute; the card-cycle
//                                   driver keys ingestion by source).
//                                   Unauthorized card fraud and the
//                                   gift-card scam ride this SAME
//                                   channel with fraud.flag=1 and
//                                   row.source = the VICTIM's deposit
//                                   account (unauthorized.cpp).
//   channel == Legit::merchant      account-paid POS purchases;
//                                   row.source is the deposit-account
//                                   Key. Interpreted as DEBIT-card
//                                   transactions (CHOICE; TabFormer
//                                   mixes credit and debit cards).
//
// CARD ATTRIBUTION (deterministic, export-time; no core-model change):
//   source Key found in entity::card::Registry.byKey -> that credit
//   card (at most one credit card per person). Any other source Key ->
//   the account's derived DEBIT card: a stable card_number derived
//   content-keyed from the account Key, so every account owns exactly
//   one debit-card identity across the whole run. Fraud rows therefore
//   label the victim account's debit card (Card.is_fraud = card ever
//   carried a flag-1 row).
//
// MERCHANT SET: the union of destination Keys observed in the view.
// Catalog merchants carry their category; fraud destinations are drawn
// from the blueprint's biller accounts (unauthorized.cpp) and may lie
// outside the merchant catalog — they still become Merchant vertices,
// with a content-keyed category fallback. Merchant geography does not
// exist in the world model: city/state/zip are content-keyed
// derivations from the merchant identity (doc-anchored CHOICE rows),
// as are City.population, Payment_Transaction.use_chip, .error, and
// the chronological is_train/is_val/is_test split.
//
// The PII investigative layer (Address/Phone/Email/IP/Device/ID/
// Full_Name/DOB + Has_* edges) is DEMO ONLY in TF_GNN_v3 and empty on
// real TabFormer; PhantomLedger populates it from its PII synthesis.
//
// Column order matches the TF_GNN_v3 loaded-attribute order so the
// owner's TigerGraph loading jobs map positionally.
//

#include "phantomledger/exporter/schema.hpp"

namespace PhantomLedger::exporter::schema {

// ------------------------------------------------------------ vertices

inline constexpr std::array<std::string_view, 2> kCfCardCols{
    "card_number", "is_fraud"};
inline constexpr Table kCfCard = detail::make("Card.csv", kCfCardCols);

inline constexpr std::array<std::string_view, 1> kCfMerchantCols{"id"};
inline constexpr Table kCfMerchant =
    detail::make("Merchant.csv", kCfMerchantCols);

inline constexpr std::array<std::string_view, 1> kCfMerchantCategoryCols{
    "category"};
inline constexpr Table kCfMerchantCategory =
    detail::make("Merchant_Category.csv", kCfMerchantCategoryCols);

inline constexpr std::array<std::string_view, 7> kCfPartyCols{
    "id", "is_fraud", "gender", "dob", "party_type", "name", "created_at"};
inline constexpr Table kCfParty = detail::make("Party.csv", kCfPartyCols);

inline constexpr std::array<std::string_view, 3> kCfCityCols{"id", "city",
                                                             "population"};
inline constexpr Table kCfCity = detail::make("City.csv", kCfCityCols);

inline constexpr std::array<std::string_view, 1> kCfStateCols{"id"};
inline constexpr Table kCfState = detail::make("State.csv", kCfStateCols);

inline constexpr std::array<std::string_view, 1> kCfZipcodeCols{"id"};
inline constexpr Table kCfZipcode =
    detail::make("Zipcode.csv", kCfZipcodeCols);

// The streamed transaction vertex: 8 loaded attributes + the 3
// chronological split flags. Everything else on the TF_GNN_v3 vertex
// is computed in-graph.
inline constexpr std::array<std::string_view, 11> kCfPaymentTransactionCols{
    "id",      "transaction_time", "amount",   "is_fraud",
    "unix_time", "mer_cat",        "use_chip", "error",
    "is_train", "is_val",          "is_test"};
inline constexpr Table kCfPaymentTransaction =
    detail::make("Payment_Transaction.csv", kCfPaymentTransactionCols);

// --------------------------------------------------- transaction edges
// Streamed alongside Payment_Transaction during the fold;
// edge_unix_time drives the GNN temporal sampler.

inline constexpr std::array<std::string_view, 3> kCfCardSendCols{
    "txn_id", "card_number", "edge_unix_time"};
inline constexpr Table kCfCardSend =
    detail::make("Card_Send_Transaction.csv", kCfCardSendCols);

inline constexpr std::array<std::string_view, 3> kCfMerchantReceiveCols{
    "txn_id", "merchant_id", "edge_unix_time"};
inline constexpr Table kCfMerchantReceive =
    detail::make("Merchant_Receive_Transaction.csv", kCfMerchantReceiveCols);

// ---------------------------------------------- static structural edges

inline constexpr std::array<std::string_view, 2> kCfMerchantAssignedCols{
    "merchant_id", "category"};
inline constexpr Table kCfMerchantAssigned =
    detail::make("Merchant_Assigned.csv", kCfMerchantAssignedCols);

inline constexpr std::array<std::string_view, 2> kCfPartyHasCardCols{
    "party_id", "card_number"};
inline constexpr Table kCfPartyHasCard =
    detail::make("Party_Has_Card.csv", kCfPartyHasCardCols);

inline constexpr std::array<std::string_view, 2> kCfIsMerchantCols{
    "merchant_id", "party_id"};
inline constexpr Table kCfIsMerchant =
    detail::make("Is_Merchant.csv", kCfIsMerchantCols);

inline constexpr std::array<std::string_view, 2> kCfHasStateCols{
    "merchant_id", "state_id"};
inline constexpr Table kCfHasState =
    detail::make("Has_State.csv", kCfHasStateCols);

inline constexpr std::array<std::string_view, 2> kCfHasCityCols{"merchant_id",
                                                                "city_id"};
inline constexpr Table kCfHasCity =
    detail::make("Has_City.csv", kCfHasCityCols);

inline constexpr std::array<std::string_view, 2> kCfHasZipCols{"merchant_id",
                                                               "zipcode_id"};
inline constexpr Table kCfHasZip = detail::make("Has_Zip.csv", kCfHasZipCols);

inline constexpr std::array<std::string_view, 2> kCfAssignedToCols{
    "zipcode_id", "city_id"};
inline constexpr Table kCfAssignedTo =
    detail::make("Assigned_To.csv", kCfAssignedToCols);

inline constexpr std::array<std::string_view, 2> kCfLocatedInCols{"city_id",
                                                                  "state_id"};
inline constexpr Table kCfLocatedIn =
    detail::make("Located_In.csv", kCfLocatedInCols);

// -------------------------------------- PII / investigative layer
// DEMO ONLY in TF_GNN_v3 (empty on TabFormer; excluded from the GNN).
// PhantomLedger populates it from its PII synthesis.

inline constexpr std::array<std::string_view, 1> kCfAddressCols{"address"};
inline constexpr Table kCfAddress =
    detail::make("Address.csv", kCfAddressCols);

inline constexpr std::array<std::string_view, 1> kCfPhoneCols{"phone_number"};
inline constexpr Table kCfPhone = detail::make("Phone.csv", kCfPhoneCols);

inline constexpr std::array<std::string_view, 1> kCfEmailCols{"email"};
inline constexpr Table kCfEmail = detail::make("Email.csv", kCfEmailCols);

inline constexpr std::array<std::string_view, 2> kCfIpCols{"id", "is_blocked"};
inline constexpr Table kCfIp = detail::make("IP.csv", kCfIpCols);

inline constexpr std::array<std::string_view, 2> kCfDeviceCols{"id",
                                                               "is_blocked"};
inline constexpr Table kCfDevice = detail::make("Device.csv", kCfDeviceCols);

inline constexpr std::array<std::string_view, 2> kCfIdCols{"id", "id_type"};
inline constexpr Table kCfId = detail::make("ID.csv", kCfIdCols);

inline constexpr std::array<std::string_view, 1> kCfFullNameCols{"name"};
inline constexpr Table kCfFullName =
    detail::make("Full_Name.csv", kCfFullNameCols);

inline constexpr std::array<std::string_view, 1> kCfDobCols{"dob"};
inline constexpr Table kCfDob = detail::make("DOB.csv", kCfDobCols);

// PII edges. Column order follows the TF_GNN_v3 FROM/TO direction
// (note Has_Address runs Address -> Party).

inline constexpr std::array<std::string_view, 2> kCfHasAddressCols{
    "address", "party_id"};
inline constexpr Table kCfHasAddress =
    detail::make("Has_Address.csv", kCfHasAddressCols);

inline constexpr std::array<std::string_view, 2> kCfHasPhoneCols{
    "party_id", "phone_number"};
inline constexpr Table kCfHasPhone =
    detail::make("Has_Phone.csv", kCfHasPhoneCols);

inline constexpr std::array<std::string_view, 2> kCfHasEmailCols{"party_id",
                                                                 "email"};
inline constexpr Table kCfHasEmail =
    detail::make("Has_Email.csv", kCfHasEmailCols);

inline constexpr std::array<std::string_view, 2> kCfHasIdCols{"party_id",
                                                              "id"};
inline constexpr Table kCfHasId = detail::make("Has_ID.csv", kCfHasIdCols);

inline constexpr std::array<std::string_view, 2> kCfHasIpCols{"party_id",
                                                              "ip_id"};
inline constexpr Table kCfHasIp = detail::make("Has_IP.csv", kCfHasIpCols);

inline constexpr std::array<std::string_view, 2> kCfHasDeviceCols{
    "party_id", "device_id"};
inline constexpr Table kCfHasDevice =
    detail::make("Has_Device.csv", kCfHasDeviceCols);

inline constexpr std::array<std::string_view, 2> kCfHasDobCols{"party_id",
                                                               "dob"};
inline constexpr Table kCfHasDob =
    detail::make("Has_DOB.csv", kCfHasDobCols);

inline constexpr std::array<std::string_view, 2> kCfHasFullNameCols{
    "party_id", "name"};
inline constexpr Table kCfHasFullName =
    detail::make("Has_Full_Name.csv", kCfHasFullNameCols);

} // namespace PhantomLedger::exporter::schema
