#pragma once
//
// phantomledger/exporter/standard/schema.hpp
//
// The standard exporter's table descriptors, next to their consumers
// (standard/export.cpp and standard/streaming.hpp): core entity
// vertices/edges, the entity-resolution kit, and the two
// transaction-scale aggregates. Same namespace as the shared kernel
// in exporter/schema.hpp; the aml exporters follow the same
// per-exporter pattern.
//

#include "phantomledger/exporter/schema.hpp"

namespace PhantomLedger::exporter::schema {

inline constexpr std::string_view kPersonHeader[]{
    "customer_id", "mule", "fraud", "victim", "solo_fraud"};
inline constexpr Table kPerson{"person.csv", kPersonHeader};

inline constexpr std::string_view kDeviceHeader[]{"device_id", "device_type",
                                                  "flagged_device"};
inline constexpr Table kDevice{"device.csv", kDeviceHeader};

inline constexpr std::string_view kIpAddressHeader[]{"ip_address",
                                                     "blacklisted_ip"};
inline constexpr Table kIpAddress{"ipaddress.csv", kIpAddressHeader};

inline constexpr std::string_view kAccountNumberHeader[]{
    "account_number", "mule", "fraud", "victim", "is_external"};
inline constexpr Table kAccountNumber{"accountnumber.csv",
                                      kAccountNumberHeader};

inline constexpr std::string_view kPhoneHeader[]{"phone_id"};
inline constexpr Table kPhone{"phone.csv", kPhoneHeader};

inline constexpr std::string_view kEmailHeader[]{"email_id"};
inline constexpr Table kEmail{"email.csv", kEmailHeader};

inline constexpr std::string_view kMerchantHeader[]{
    "merchant_id", "counterparty_acct", "category", "weight", "in_bank"};
inline constexpr Table kMerchant{"merchants.csv", kMerchantHeader};

inline constexpr std::string_view kExternalAccountHeader[]{"account_id", "kind",
                                                           "category"};
inline constexpr Table kExternalAccount{"external_accounts.csv",
                                        kExternalAccountHeader};

inline constexpr std::string_view kHasAccountHeader[]{"FROM", "TO"};
inline constexpr Table kHasAccount{"HAS_ACCOUNT.csv", kHasAccountHeader};

inline constexpr std::string_view kHasPhoneHeader[]{"FROM", "TO"};
inline constexpr Table kHasPhone{"HAS_PHONE.csv", kHasPhoneHeader};

inline constexpr std::string_view kHasEmailHeader[]{"FROM", "TO"};
inline constexpr Table kHasEmail{"HAS_EMAIL.csv", kHasEmailHeader};

inline constexpr std::string_view kHasUsedHeader[]{"FROM", "TO", "first_seen",
                                                   "last_seen"};
inline constexpr Table kHasUsed{"HAS_USED.csv", kHasUsedHeader};

inline constexpr std::string_view kHasIpHeader[]{"FROM", "TO", "first_seen",
                                                 "last_seen"};
inline constexpr Table kHasIp{"HAS_IP.csv", kHasIpHeader};

inline constexpr std::string_view kHasPaidHeader[]{
    "FROM",           "TO",           "total_amount", "total_num_txns",
    "first_txn_date", "last_txn_date"};
inline constexpr Table kHasPaid{"HAS_PAID.csv", kHasPaidHeader};

// Standard-path temporal flow aggregates (fixed-width bin sequence per pair).
// Distinct from the aml_txn_edges kit's 30d/90d kAccountFlowAgg; same on-disk
// filename, but written into the standard export directory by a separate path.
inline constexpr std::string_view kAccountFlowAggBinHeader[]{
    "from_id",        "to_id",         "total_amount", "txn_count",
    "first_txn_date", "last_txn_date", "span_days",    "num_bins",
    "bin_days",       "amount_bins",   "count_bins"};
inline constexpr Table kAccountFlowAggBin{"ACCOUNT_FLOW_AGG.csv",
                                          kAccountFlowAggBinHeader};

// ===========================================================================
// Entity Resolution
// ===========================================================================

inline constexpr std::string_view kErCustomerHeader[]{"customer_id",
                                                      "created_at"};
inline constexpr Table kErCustomer{"customer.csv", kErCustomerHeader};

inline constexpr std::string_view kErAccountHeader[]{"account_id", "is_fraud"};
inline constexpr Table kErAccount{"account.csv", kErAccountHeader};

// ---- exact-match PII value vertices ----

inline constexpr std::string_view kNameHeader[]{"name_id"};
inline constexpr Table kName{"name.csv", kNameHeader};

inline constexpr std::string_view kBirthdateHeader[]{"birthdate_id"};
inline constexpr Table kBirthdate{"birthdate.csv", kBirthdateHeader};

inline constexpr std::string_view kStreetAddressHeader[]{"street_address_id"};
inline constexpr Table kStreetAddress{"street_address.csv",
                                      kStreetAddressHeader};

inline constexpr std::string_view kCityHeader[]{"city_id"};
inline constexpr Table kCity{"city.csv", kCityHeader};

inline constexpr std::string_view kStateHeader[]{"state_id"};
inline constexpr Table kState{"state.csv", kStateHeader};

inline constexpr std::string_view kPostcodeHeader[]{"postcode_id"};
inline constexpr Table kPostcode{"postcode.csv", kPostcodeHeader};

// ---- MinHash-LSH bucket vertices (fuzzy string blocking) ----
// Parties sharing a bucket are fuzzy-match candidates; the COUNT of shared
// buckets is the match strength. Email/phone/city/state are exact-match and
// are covered by their plain value vertices above -- no fuzzy bucket.

inline constexpr std::string_view kNameMinhashHeader[]{"name_minhash_id"};
inline constexpr Table kNameMinhash{"name_minhash.csv", kNameMinhashHeader};

inline constexpr std::string_view kAddressMinhashHeader[]{"address_minhash_id"};
inline constexpr Table kAddressMinhash{"address_minhash.csv",
                                       kAddressMinhashHeader};

inline constexpr std::string_view kStreetMinhashHeader[]{"street_minhash_id"};
inline constexpr Table kStreetMinhash{"street_minhash.csv",
                                      kStreetMinhashHeader};

inline constexpr std::string_view kHasNameHeader[]{"FROM", "TO"};
inline constexpr Table kHasName{"HAS_NAME.csv", kHasNameHeader};

inline constexpr std::string_view kHasBirthdateHeader[]{"FROM", "TO"};
inline constexpr Table kHasBirthdate{"HAS_BIRTHDATE.csv", kHasBirthdateHeader};

inline constexpr std::string_view kHasStreetAddressHeader[]{"FROM", "TO"};
inline constexpr Table kHasStreetAddress{"HAS_STREET_ADDRESS.csv",
                                         kHasStreetAddressHeader};

inline constexpr std::string_view kHasCityHeader[]{"FROM", "TO"};
inline constexpr Table kHasCity{"HAS_CITY.csv", kHasCityHeader};

inline constexpr std::string_view kHasStateHeader[]{"FROM", "TO"};
inline constexpr Table kHasState{"HAS_STATE.csv", kHasStateHeader};

inline constexpr std::string_view kHasPostcodeHeader[]{"FROM", "TO"};
inline constexpr Table kHasPostcode{"HAS_POSTCODE.csv", kHasPostcodeHeader};

inline constexpr std::string_view kHasDeviceErHeader[]{"FROM", "TO"};
inline constexpr Table kHasDeviceEr{"HAS_DEVICE.csv", kHasDeviceErHeader};

inline constexpr std::string_view kHasIpErHeader[]{"FROM", "TO"};
inline constexpr Table kHasIpEr{"HAS_IP_ER.csv", kHasIpErHeader};

// ---- MinHash bucket edges (customer -> bucket, ~10 per attribute) ----

inline constexpr std::string_view kHasNameMinhashHeader[]{"FROM", "TO"};
inline constexpr Table kHasNameMinhash{"HAS_NAME_MINHASH.csv",
                                       kHasNameMinhashHeader};

inline constexpr std::string_view kHasAddressMinhashHeader[]{"FROM", "TO"};
inline constexpr Table kHasAddressMinhash{"HAS_ADDRESS_MINHASH.csv",
                                          kHasAddressMinhashHeader};

inline constexpr std::string_view kHasStreetMinhashHeader[]{"FROM", "TO"};
inline constexpr Table kHasStreetMinhash{"HAS_STREET_MINHASH.csv",
                                         kHasStreetMinhashHeader};

} // namespace PhantomLedger::exporter::schema
