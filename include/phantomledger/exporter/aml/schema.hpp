#pragma once

#include "phantomledger/exporter/schema.hpp"

#include <array>
#include <string_view>

namespace PhantomLedger::exporter::schema::aml {

using ::PhantomLedger::exporter::schema::Table;
using ::PhantomLedger::exporter::schema::detail::make;

// ────────────────────────── Vertices ──────────────────────────────

inline constexpr std::array<std::string_view, 10> kCustomerHeader{
    "id",
    "type_of",
    "status",
    "marital_status",
    "networth_code",
    "annual_income_code",
    "occupation",
    "risk_rating",
    "nationality",
    "onboarding_date",
};
inline constexpr Table kCustomer = make("Customer.csv", kCustomerHeader);

inline constexpr std::array<std::string_view, 10> kAccountHeader{
    "id",          "account_number", "balance",
    "open_date",   "status",         "most_recent_transaction_date",
    "close_date",  "account_type",   "currency",
    "branch_code",
};
inline constexpr Table kAccount = make("Account.csv", kAccountHeader);

inline constexpr std::array<std::string_view, 5> kCounterpartyHeader{
    "id", "name", "bank_id_code", "bank_name", "country",
};
inline constexpr Table kCounterparty =
    make("Counterparty.csv", kCounterpartyHeader);

inline constexpr std::array<std::string_view, 4> kNameHeader{
    "id",
    "First_Name",
    "Middle_Name",
    "Last_Name",
};
inline constexpr Table kName = make("Name.csv", kNameHeader);

inline constexpr std::array<std::string_view, 9> kAddressHeader{
    "id",      "street_line1", "street_line2",
    "city",    "state",        "postal_code",
    "country", "address_type", "is_high_risk_geo",
};
inline constexpr Table kAddress = make("Address.csv", kAddressHeader);

inline constexpr std::array<std::string_view, 3> kCountryHeader{
    "id",
    "name",
    "risk_score",
};
inline constexpr Table kCountry = make("Country.csv", kCountryHeader);

inline constexpr std::array<std::string_view, 4> kWatchlistHeader{
    "id",
    "list_name",
    "list_type",
    "entry_date",
};
inline constexpr Table kWatchlist = make("Watchlist.csv", kWatchlistHeader);

inline constexpr std::array<std::string_view, 8> kDeviceHeader{
    "device_id", "device_type", "ip_address", "ip_country",
    "os",        "first_seen",  "last_seen",  "is_vpn",
};
inline constexpr Table kDevice = make("Device.csv", kDeviceHeader);

inline constexpr std::array<std::string_view, 10> kTransactionHeader{
    "id",
    "credit_debit_code",
    "execution_date",
    "channel",
    "purpose",
    "risk",
    "currency",
    "transaction_amount_dbl",
    "unit_quantity",
    "transaction_amount",
};
inline constexpr Table kTransaction =
    make("Transaction.csv", kTransactionHeader);

inline constexpr std::array<std::string_view, 6> kSarHeader{
    "sar_id",         "filing_date",  "amount_involved",
    "activity_start", "activity_end", "violation_type",
};
inline constexpr Table kSar = make("SAR.csv", kSarHeader);

inline constexpr std::array<std::string_view, 1> kBankHeader{"id"};
inline constexpr Table kBank = make("Bank.csv", kBankHeader);

inline constexpr std::array<std::string_view, 1> kMinHashHeader{"id"};
inline constexpr Table kNameMinhash = make("Name_MinHash.csv", kMinHashHeader);
inline constexpr Table kAddressMinhash =
    make("Address_MinHash.csv", kMinHashHeader);
inline constexpr Table kStreetLine1Minhash =
    make("Street_Line1_MinHash.csv", kMinHashHeader);
inline constexpr Table kCityMinhash = make("City_MinHash.csv", kMinHashHeader);
inline constexpr Table kStateMinhash =
    make("State_MinHash.csv", kMinHashHeader);

inline constexpr std::array<std::string_view, 1> kConnectedComponentHeader{
    "id"};
inline constexpr Table kConnectedComponent =
    make("Connected_Component.csv", kConnectedComponentHeader);

// ────────────────────────── Edges ─────────────────────────────────

inline constexpr std::array<std::string_view, 2> kFromTo2Header{"FROM", "TO"};

inline constexpr std::array<std::string_view, 2> kCustomerHasAccountHeader{
    "Customer", "Account"};
inline constexpr Table kCustomerHasAccount =
    make("customer_has_account.csv", kCustomerHasAccountHeader);

inline constexpr std::array<std::string_view, 2>
    kAccountHasPrimaryCustomerHeader{"Account", "Customer"};
inline constexpr Table kAccountHasPrimaryCustomer =
    make("account_has_primary_customer.csv", kAccountHasPrimaryCustomerHeader);

inline constexpr std::array<std::string_view, 2> kSendTransactionHeader{
    "Account", "Transaction"};
inline constexpr Table kSendTransaction =
    make("send_transaction.csv", kSendTransactionHeader);

inline constexpr std::array<std::string_view, 2> kReceiveTransactionHeader{
    "Account", "Transaction"};
inline constexpr Table kReceiveTransaction =
    make("receive_transaction.csv", kReceiveTransactionHeader);

inline constexpr std::array<std::string_view, 3>
    kCounterpartySendTransactionHeader{"Counterparty", "Transaction",
                                       "counterparty_name"};
inline constexpr Table kCounterpartySendTransaction = make(
    "counterparty_send_transaction.csv", kCounterpartySendTransactionHeader);

inline constexpr std::array<std::string_view, 3>
    kCounterpartyReceiveTransactionHeader{"Counterparty", "Transaction",
                                          "counterparty_name"};
inline constexpr Table kCounterpartyReceiveTransaction =
    make("counterparty_receive_transaction.csv",
         kCounterpartyReceiveTransactionHeader);

inline constexpr std::array<std::string_view, 2>
    kSentTransactionToCounterpartyHeader{"Account", "Counterparty"};
inline constexpr Table kSentTransactionToCounterparty =
    make("sent_transaction_to_counterparty.csv",
         kSentTransactionToCounterpartyHeader);

inline constexpr std::array<std::string_view, 2>
    kReceivedTransactionFromCounterpartyHeader{"Account", "Counterparty"};
inline constexpr Table kReceivedTransactionFromCounterparty =
    make("received_transaction_from_counterparty.csv",
         kReceivedTransactionFromCounterpartyHeader);

inline constexpr std::array<std::string_view, 5> kUsesDeviceHeader{
    "Customer", "Device", "first_seen", "last_seen", "session_count",
};
inline constexpr Table kUsesDevice = make("uses_device.csv", kUsesDeviceHeader);

inline constexpr std::array<std::string_view, 5> kLoggedFromHeader{
    "Account", "Device", "first_access", "last_access", "access_count",
};
inline constexpr Table kLoggedFrom = make("logged_from.csv", kLoggedFromHeader);

inline constexpr std::array<std::string_view, 3> kCustomerHasNameHeader{
    "Customer", "Name", "last_update"};
inline constexpr Table kCustomerHasName =
    make("customer_has_name.csv", kCustomerHasNameHeader);

inline constexpr std::array<std::string_view, 3> kCustomerHasAddressHeader{
    "Customer", "Address", "last_update"};
inline constexpr Table kCustomerHasAddress =
    make("customer_has_address.csv", kCustomerHasAddressHeader);

inline constexpr std::array<std::string_view, 3>
    kCustomerAssociatedWithCountryHeader{"Customer", "Country", "last_update"};
inline constexpr Table kCustomerAssociatedWithCountry =
    make("customer_associated_with_country.csv",
         kCustomerAssociatedWithCountryHeader);

inline constexpr std::array<std::string_view, 4> kAccountHasNameHeader{
    "Account", "Name", "type_of", "last_update"};
inline constexpr Table kAccountHasName =
    make("account_has_name.csv", kAccountHasNameHeader);

inline constexpr std::array<std::string_view, 4> kAccountHasAddressHeader{
    "Account", "Address", "type_of", "last_update"};
inline constexpr Table kAccountHasAddress =
    make("account_has_address.csv", kAccountHasAddressHeader);

inline constexpr std::array<std::string_view, 3>
    kAccountAssociatedWithCountryHeader{"Account", "Country", "last_update"};
inline constexpr Table kAccountAssociatedWithCountry = make(
    "account_associated_with_country.csv", kAccountAssociatedWithCountryHeader);

inline constexpr std::array<std::string_view, 3> kAddressInCountryHeader{
    "Address", "Country", "last_update"};
inline constexpr Table kAddressInCountry =
    make("address_in_country.csv", kAddressInCountryHeader);

inline constexpr std::array<std::string_view, 2>
    kCustomerMatchesWatchlistHeader{"Customer", "Watchlist"};
inline constexpr Table kCustomerMatchesWatchlist =
    make("customer_matches_watchlist.csv", kCustomerMatchesWatchlistHeader);

inline constexpr std::array<std::string_view, 3> kCounterpartyHasNameHeader{
    "Counterparty", "Name", "last_update"};
inline constexpr Table kCounterpartyHasName =
    make("counterparty_has_name.csv", kCounterpartyHasNameHeader);

inline constexpr std::array<std::string_view, 3> kCounterpartyHasAddressHeader{
    "Counterparty", "Address", "last_update"};
inline constexpr Table kCounterpartyHasAddress =
    make("counterparty_has_address.csv", kCounterpartyHasAddressHeader);

inline constexpr std::array<std::string_view, 2>
    kCounterpartyAssociatedWithCountryHeader{"Counterparty", "Country"};
inline constexpr Table kCounterpartyAssociatedWithCountry =
    make("counterparty_associated_with_country.csv",
         kCounterpartyAssociatedWithCountryHeader);

inline constexpr std::array<std::string_view, 3> kSameAsHeader{
    "Customer", "Customer", "score"};
inline constexpr Table kSameAs = make("same_as.csv", kSameAsHeader);

inline constexpr std::array<std::string_view, 3> kReferencesHeader{
    "SAR", "Customer", "role"};
inline constexpr Table kReferences = make("references.csv", kReferencesHeader);

inline constexpr std::array<std::string_view, 3> kSarCoversHeader{
    "SAR", "Account", "activity_amount"};
inline constexpr Table kSarCovers = make("sar_covers.csv", kSarCoversHeader);

inline constexpr std::array<std::string_view, 2> kBeneficiaryBankHeader{
    "Bank", "Counterparty"};
inline constexpr Table kBeneficiaryBank =
    make("beneficiary_bank.csv", kBeneficiaryBankHeader);

inline constexpr std::array<std::string_view, 2> kOriginatorBankHeader{
    "Bank", "Counterparty"};
inline constexpr Table kOriginatorBank =
    make("originator_bank.csv", kOriginatorBankHeader);

inline constexpr std::array<std::string_view, 2>
    kBankAssociatedWithCountryHeader{"Bank", "Country"};
inline constexpr Table kBankAssociatedWithCountry =
    make("bank_associated_with_country.csv", kBankAssociatedWithCountryHeader);

inline constexpr std::array<std::string_view, 3> kBankHasAddressHeader{
    "Bank", "Address", "last_update"};
inline constexpr Table kBankHasAddress =
    make("bank_has_address.csv", kBankHasAddressHeader);

inline constexpr std::array<std::string_view, 3> kBankHasNameHeader{
    "Bank", "Name", "last_update"};
inline constexpr Table kBankHasName =
    make("bank_has_name.csv", kBankHasNameHeader);

inline constexpr std::array<std::string_view, 2>
    kCustomerInConnectedComponentHeader{"Customer", "Connected_Component"};
inline constexpr Table kCustomerInConnectedComponent = make(
    "customer_in_connected_component.csv", kCustomerInConnectedComponentHeader);

// ── MinHash edges — all share a 2-column (entity, minhash) header ──

inline constexpr std::array<std::string_view, 2> kCustomerHasNameMinhashHeader{
    "Customer", "Name_MinHash"};
inline constexpr Table kCustomerHasNameMinhash =
    make("customer_has_name_minhash.csv", kCustomerHasNameMinhashHeader);

inline constexpr std::array<std::string_view, 2>
    kCustomerHasAddressMinhashHeader{"Customer", "Address_MinHash"};
inline constexpr Table kCustomerHasAddressMinhash =
    make("customer_has_address_minhash.csv", kCustomerHasAddressMinhashHeader);

inline constexpr std::array<std::string_view, 2>
    kCustomerHasAddressStreetLine1MinhashHeader{"Customer",
                                                "Street_Line1_MinHash"};
inline constexpr Table kCustomerHasAddressStreetLine1Minhash =
    make("customer_has_address_street_line1_minhash.csv",
         kCustomerHasAddressStreetLine1MinhashHeader);

inline constexpr std::array<std::string_view, 2>
    kCustomerHasAddressCityMinhashHeader{"Customer", "City_MinHash"};
inline constexpr Table kCustomerHasAddressCityMinhash =
    make("customer_has_address_city_minhash.csv",
         kCustomerHasAddressCityMinhashHeader);

inline constexpr std::array<std::string_view, 2>
    kCustomerHasAddressStateMinhashHeader{"Customer", "State_MinHash"};
inline constexpr Table kCustomerHasAddressStateMinhash =
    make("customer_has_address_state_minhash.csv",
         kCustomerHasAddressStateMinhashHeader);

inline constexpr std::array<std::string_view, 2> kAccountHasNameMinhashHeader{
    "Account", "Name_MinHash"};
inline constexpr Table kAccountHasNameMinhash =
    make("account_has_name_minhash.csv", kAccountHasNameMinhashHeader);

inline constexpr std::array<std::string_view, 2>
    kCounterpartyHasNameMinhashHeader{"Counterparty", "Name_MinHash"};
inline constexpr Table kCounterpartyHasNameMinhash = make(
    "counterparty_has_name_minhash.csv", kCounterpartyHasNameMinhashHeader);

inline constexpr std::array<std::string_view, 5> kResolvesToHeader{
    "Counterparty", "Customer",      "match_confidence",
    "match_method", "resolved_date",
};
inline constexpr Table kResolvesTo = make("resolves_to.csv", kResolvesToHeader);

} // namespace PhantomLedger::exporter::schema::aml
