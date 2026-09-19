#pragma once

#include "phantomledger/exporter/schema.hpp"

namespace PhantomLedger::exporter::mule_temporal::schema {

inline constexpr std::string_view kPartyHeader[]{
    "id", "party_type", "first_seen_seq", "first_seen_ts_ms"};
inline constexpr exporter::schema::Table kParty{"Party.csv", kPartyHeader};

inline constexpr std::string_view kAccountHeader[]{"id",
                                                   "account_type",
                                                   "is_external",
                                                   "first_seen_seq",
                                                   "first_seen_ts_ms",
                                                   "is_mule"};
inline constexpr exporter::schema::Table kAccount{"Account.csv",
                                                  kAccountHeader};

inline constexpr std::string_view kTokenHeader[]{"token_id", "first_seen_seq",
                                                 "first_seen_ts_ms",
                                                 "token_kind", "token_network"};
inline constexpr exporter::schema::Table kToken{"Token.csv", kTokenHeader};

inline constexpr std::string_view kDeviceHeader[]{
    "id", "device_type", "first_seen_seq", "first_seen_ts_ms"};
inline constexpr exporter::schema::Table kDevice{"Device.csv", kDeviceHeader};

inline constexpr std::string_view kIPHeader[]{"id", "first_seen_seq",
                                              "first_seen_ts_ms"};
inline constexpr exporter::schema::Table kIP{"IP.csv", kIPHeader};

inline constexpr std::string_view kAddressHeader[]{
    "address_id", "first_seen_seq", "first_seen_ts_ms", "country_code"};
inline constexpr exporter::schema::Table kAddress{"Address.csv",
                                                  kAddressHeader};

inline constexpr std::string_view kPayment_TransactionHeader[]{
    "transaction_id", "amount",      "currency",  "payment_rail",  "channel",
    "event_time",     "event_ts_ms", "event_seq", "amount_present"};
inline constexpr exporter::schema::Table kPayment_Transaction{
    "Payment_Transaction.csv", kPayment_TransactionHeader};

inline constexpr std::string_view kZelle_TransferHeader[]{
    "transfer_id",
    "event_time",
    "event_ts_ms",
    "event_seq",
    "amount",
    "amount_present",
    "currency",
    "channel",
    "fraud_label",
    "label_known",
    "label_available_seq",
    "label_available_ts_ms"};
inline constexpr exporter::schema::Table kZelle_Transfer{"Zelle_Transfer.csv",
                                                         kZelle_TransferHeader};

inline constexpr std::string_view kAssociationHeader[]{
    "from_id",      "to_id",      "valid_from_seq",
    "valid_to_seq", "confidence", "source_system"};
inline constexpr std::string_view kParticipationHeader[]{
    "from_id", "to_id", "event_ts_ms", "event_seq"};

inline constexpr exporter::schema::Table kParty_Owns_Account{
    "Party_Owns_Account.csv", kAssociationHeader};
inline constexpr exporter::schema::Table kParty_Uses_Token{
    "Party_Uses_Token.csv", kAssociationHeader};
inline constexpr exporter::schema::Table kToken_Bound_To_Account{
    "Token_Bound_To_Account.csv", kAssociationHeader};
inline constexpr exporter::schema::Table kParty_Uses_Device{
    "Party_Uses_Device.csv", kAssociationHeader};
inline constexpr exporter::schema::Table kAccount_Uses_Device{
    "Account_Uses_Device.csv", kAssociationHeader};
inline constexpr exporter::schema::Table kParty_Uses_IP{"Party_Uses_IP.csv",
                                                        kAssociationHeader};
inline constexpr exporter::schema::Table kParty_Has_Address{
    "Party_Has_Address.csv", kAssociationHeader};
inline constexpr exporter::schema::Table kTransfer_From_Account{
    "Transfer_From_Account.csv", kParticipationHeader};
inline constexpr exporter::schema::Table kTransfer_To_Account{
    "Transfer_To_Account.csv", kParticipationHeader};
inline constexpr exporter::schema::Table kTransfer_From_Token{
    "Transfer_From_Token.csv", kParticipationHeader};
inline constexpr exporter::schema::Table kTransfer_To_Token{
    "Transfer_To_Token.csv", kParticipationHeader};
inline constexpr exporter::schema::Table kTransfer_Used_Device{
    "Transfer_Used_Device.csv", kParticipationHeader};
inline constexpr exporter::schema::Table kTransfer_Used_IP{
    "Transfer_Used_IP.csv", kParticipationHeader};
inline constexpr exporter::schema::Table kTransaction_From_Account{
    "Transaction_From_Account.csv", kParticipationHeader};
inline constexpr exporter::schema::Table kTransaction_To_Account{
    "Transaction_To_Account.csv", kParticipationHeader};
inline constexpr exporter::schema::Table kTransaction_From_Token{
    "Transaction_From_Token.csv", kParticipationHeader};
inline constexpr exporter::schema::Table kTransaction_To_Token{
    "Transaction_To_Token.csv", kParticipationHeader};
inline constexpr exporter::schema::Table kTransaction_Used_Device{
    "Transaction_Used_Device.csv", kParticipationHeader};
inline constexpr exporter::schema::Table kTransaction_Used_IP{
    "Transaction_Used_IP.csv", kParticipationHeader};

inline constexpr auto kTables = std::to_array<exporter::schema::Table>({
    kParty,
    kAccount,
    kToken,
    kDevice,
    kIP,
    kAddress,
    kPayment_Transaction,
    kZelle_Transfer,
    kParty_Owns_Account,
    kParty_Uses_Token,
    kToken_Bound_To_Account,
    kParty_Uses_Device,
    kAccount_Uses_Device,
    kParty_Uses_IP,
    kParty_Has_Address,
    kTransfer_From_Account,
    kTransfer_To_Account,
    kTransfer_From_Token,
    kTransfer_To_Token,
    kTransfer_Used_Device,
    kTransfer_Used_IP,
    kTransaction_From_Account,
    kTransaction_To_Account,
    kTransaction_From_Token,
    kTransaction_To_Token,
    kTransaction_Used_Device,
    kTransaction_Used_IP,
});

} // namespace PhantomLedger::exporter::mule_temporal::schema
