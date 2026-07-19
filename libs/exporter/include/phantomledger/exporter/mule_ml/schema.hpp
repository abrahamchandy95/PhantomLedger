#pragma once
//
// phantomledger/exporter/mule_ml/schema.hpp
//
// The mule-ml exporter's table descriptors, next to their consumers
// (mule_ml/export.cpp and mule_ml/streaming.hpp). Same namespace as
// the shared kernel in exporter/schema.hpp; the aml exporters follow
// the same per-exporter pattern.
//

#include "phantomledger/exporter/schema.hpp"

namespace PhantomLedger::exporter::schema {

inline constexpr std::string_view kMlPartyHeader[]{
    "id",      "isFraud", "phoneNumber", "email",   "name",    "SSN", "dob",
    "address", "state",   "city",        "zipcode", "country", "ip",  "device"};
inline constexpr Table kMlParty{"Party.csv", kMlPartyHeader};

inline constexpr std::string_view kMlTransferHeader[]{
    "id", "fromAccountID", "toAccountID", "amount", "transfer_time"};
inline constexpr Table kMlTransfer{"Transfer_Transaction.csv",
                                   kMlTransferHeader};

inline constexpr std::string_view kMlAccountDeviceHeader[]{
    "accountID", "device_id", "txn_count", "first_seen", "last_seen"};
inline constexpr Table kMlAccountDevice{"Account_Device.csv",
                                        kMlAccountDeviceHeader};

inline constexpr std::string_view kMlAccountIpHeader[]{
    "accountID", "ip_address", "txn_count", "first_seen", "last_seen"};
inline constexpr Table kMlAccountIp{"Account_IP.csv", kMlAccountIpHeader};

} // namespace PhantomLedger::exporter::schema
