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
//   the account's derived DEBIT card: a stable identifier derived from
//   the account Key (derive.hpp), so every account owns exactly one
//   debit-card identity across the whole run. Fraud rows therefore
//   label the victim account's debit card (Card.is_fraud = card ever
//   carried a flag-1 view row).
//
// MERCHANT SET: the union of destination Keys observed in the view.
// Catalog merchants carry their category; fraud destinations are drawn
// from the blueprint's biller accounts (unauthorized.cpp) and may lie
// outside the merchant catalog — they still become Merchant vertices,
// with the content-keyed category fallback (derive.hpp). Merchant
// geography does not exist in the world model: city/state/zip are
// content-keyed derivations from the merchant identity (doc-anchored
// CHOICE rows), as are City.population, use_chip, error, and the
// chronological is_train/is_val/is_test split.
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

inline constexpr std::array<std::string_view, 2> kCardCols{"card_number",
                                                           "is_fraud"};
inline constexpr Table kCard = detail::make("Card.csv", kCardCols);

inline constexpr std::array<std::string_view, 1> kMerchantCols{"id"};
inline constexpr Table kMerchant = detail::make("Merchant.csv", kMerchantCols);

inline constexpr std::array<std::string_view, 1> kMerchantCategoryCols{
    "category"};
inline constexpr Table kMerchantCategory =
    detail::make("Merchant_Category.csv", kMerchantCategoryCols);

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

// The streamed transaction vertex: 8 loaded attributes + the 3
// chronological split flags. Everything else on the TF_GNN_v3 vertex
// is computed in-graph. `id` carries the corpus row_seq (T<row_seq>),
// so every Payment_Transaction cross-references the streamed
// 'transactions' table directly.
inline constexpr std::array<std::string_view, 11> kPaymentTransactionCols{
    "id",        "transaction_time", "amount",   "is_fraud",
    "unix_time", "mer_cat",          "use_chip", "error",
    "is_train",  "is_val",           "is_test"};
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

inline constexpr std::array<std::string_view, 2> kIsMerchantCols{
    "merchant_id", "party_id"};
inline constexpr Table kIsMerchant =
    detail::make("Is_Merchant.csv", kIsMerchantCols);

inline constexpr std::array<std::string_view, 2> kHasStateCols{"merchant_id",
                                                               "state_id"};
inline constexpr Table kHasState =
    detail::make("Has_State.csv", kHasStateCols);

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

inline constexpr std::array<std::string_view, 2> kIpCols{"id", "is_blocked"};
inline constexpr Table kIp = detail::make("IP.csv", kIpCols);

inline constexpr std::array<std::string_view, 2> kDeviceCols{"id",
                                                             "is_blocked"};
inline constexpr Table kDevice = detail::make("Device.csv", kDeviceCols);

inline constexpr std::array<std::string_view, 2> kIdCols{"id", "id_type"};
inline constexpr Table kId = detail::make("ID.csv", kIdCols);

inline constexpr std::array<std::string_view, 1> kFullNameCols{"name"};
inline constexpr Table kFullName =
    detail::make("Full_Name.csv", kFullNameCols);

inline constexpr std::array<std::string_view, 1> kDobCols{"dob"};
inline constexpr Table kDob = detail::make("DOB.csv", kDobCols);

// PII edges. Column order follows the TF_GNN_v3 FROM/TO direction
// (note Has_Address runs Address -> Party).

inline constexpr std::array<std::string_view, 2> kHasAddressCols{"address",
                                                                 "party_id"};
inline constexpr Table kHasAddress =
    detail::make("Has_Address.csv", kHasAddressCols);

inline constexpr std::array<std::string_view, 2> kHasPhoneCols{
    "party_id", "phone_number"};
inline constexpr Table kHasPhone =
    detail::make("Has_Phone.csv", kHasPhoneCols);

inline constexpr std::array<std::string_view, 2> kHasEmailCols{"party_id",
                                                               "email"};
inline constexpr Table kHasEmail =
    detail::make("Has_Email.csv", kHasEmailCols);

inline constexpr std::array<std::string_view, 2> kHasIdCols{"party_id", "id"};
inline constexpr Table kHasId = detail::make("Has_ID.csv", kHasIdCols);

inline constexpr std::array<std::string_view, 2> kHasIpCols{"party_id",
                                                            "ip_id"};
inline constexpr Table kHasIp = detail::make("Has_IP.csv", kHasIpCols);

inline constexpr std::array<std::string_view, 2> kHasDeviceCols{"party_id",
                                                                "device_id"};
inline constexpr Table kHasDevice =
    detail::make("Has_Device.csv", kHasDeviceCols);

inline constexpr std::array<std::string_view, 2> kHasDobCols{"party_id",
                                                             "dob"};
inline constexpr Table kHasDob = detail::make("Has_DOB.csv", kHasDobCols);

inline constexpr std::array<std::string_view, 2> kHasFullNameCols{"party_id",
                                                                  "name"};
inline constexpr Table kHasFullName =
    detail::make("Has_Full_Name.csv", kHasFullNameCols);

} // namespace PhantomLedger::exporter::schema::card_fraud
