#pragma once

#include <array>
#include <span>
#include <string_view>

namespace PhantomLedger::exporter::schema {

struct Table {
  std::string_view filename;
  std::span<const std::string_view> header;
};

namespace detail {

template <std::size_t N>
[[nodiscard]] constexpr Table
make(std::string_view filename,
     const std::array<std::string_view, N> &columns) noexcept {
  return Table{filename, std::span<const std::string_view>{columns}};
}

} // namespace detail

inline constexpr std::array<std::string_view, 5> kPersonHeader{
    "customer_id", "mule", "fraud", "victim", "solo_fraud",
};
inline constexpr Table kPerson = detail::make("person.csv", kPersonHeader);

inline constexpr std::array<std::string_view, 3> kDeviceHeader{
    "device_id",
    "device_type",
    "flagged_device",
};
inline constexpr Table kDevice = detail::make("device.csv", kDeviceHeader);

inline constexpr std::array<std::string_view, 2> kIpAddressHeader{
    "ip_address",
    "blacklisted_ip",
};
inline constexpr Table kIpAddress =
    detail::make("ipaddress.csv", kIpAddressHeader);

inline constexpr std::array<std::string_view, 5> kAccountNumberHeader{
    "account_number", "mule", "fraud", "victim", "is_external",
};
inline constexpr Table kAccountNumber =
    detail::make("accountnumber.csv", kAccountNumberHeader);

inline constexpr std::array<std::string_view, 1> kPhoneHeader{
    "phone_id",
};
inline constexpr Table kPhone = detail::make("phone.csv", kPhoneHeader);

inline constexpr std::array<std::string_view, 1> kEmailHeader{
    "email_id",
};
inline constexpr Table kEmail = detail::make("email.csv", kEmailHeader);

inline constexpr std::array<std::string_view, 5> kMerchantHeader{
    "merchant_id", "counterparty_acct", "category", "weight", "in_bank",
};
inline constexpr Table kMerchant =
    detail::make("merchants.csv", kMerchantHeader);

inline constexpr std::array<std::string_view, 3> kExternalAccountHeader{
    "account_id",
    "kind",
    "category",
};
inline constexpr Table kExternalAccount =
    detail::make("external_accounts.csv", kExternalAccountHeader);

inline constexpr std::array<Table, 7> kVertices{
    kPerson, kDevice, kIpAddress, kAccountNumber, kPhone, kEmail, kMerchant,
};

inline constexpr std::array<std::string_view, 2> kHasAccountHeader{
    "FROM",
    "TO",
};
inline constexpr Table kHasAccount =
    detail::make("HAS_ACCOUNT.csv", kHasAccountHeader);

inline constexpr std::array<std::string_view, 2> kHasPhoneHeader{
    "FROM",
    "TO",
};
inline constexpr Table kHasPhone =
    detail::make("HAS_PHONE.csv", kHasPhoneHeader);

inline constexpr std::array<std::string_view, 2> kHasEmailHeader{
    "FROM",
    "TO",
};
inline constexpr Table kHasEmail =
    detail::make("HAS_EMAIL.csv", kHasEmailHeader);

inline constexpr std::array<std::string_view, 4> kHasUsedHeader{
    "FROM",
    "TO",
    "first_seen",
    "last_seen",
};
inline constexpr Table kHasUsed = detail::make("HAS_USED.csv", kHasUsedHeader);

inline constexpr std::array<std::string_view, 4> kHasIpHeader{
    "FROM",
    "TO",
    "first_seen",
    "last_seen",
};
inline constexpr Table kHasIp = detail::make("HAS_IP.csv", kHasIpHeader);

inline constexpr std::array<std::string_view, 6> kHasPaidHeader{
    "FROM",          "TO", "total_amount", "total_num_txns", "first_txn_date",
    "last_txn_date",
};
inline constexpr Table kHasPaid = detail::make("HAS_PAID.csv", kHasPaidHeader);

inline constexpr std::array<std::string_view, 9> kLedgerHeader{
    "src_acct", "dst_acct",  "amount",     "ts",      "is_fraud",
    "ring_id",  "device_id", "ip_address", "channel",
};
inline constexpr Table kLedger =
    detail::make("transactions.csv", kLedgerHeader);

inline constexpr std::array<Table, 6> kEdges{
    kHasAccount, kHasPhone, kHasEmail, kHasUsed, kHasIp, kHasPaid,
};

inline constexpr std::array<std::string_view, 14> kMlPartyHeader{
    "id",      "isFraud", "phoneNumber", "email",   "name",    "SSN", "dob",
    "address", "state",   "city",        "zipcode", "country", "ip",  "device",
};
inline constexpr Table kMlParty = detail::make("Party.csv", kMlPartyHeader);

inline constexpr std::array<std::string_view, 5> kMlTransferHeader{
    "id", "fromAccountID", "toAccountID", "amount", "transfer_time",
};
inline constexpr Table kMlTransfer =
    detail::make("Transfer_Transaction.csv", kMlTransferHeader);

inline constexpr std::array<std::string_view, 5> kMlAccountDeviceHeader{
    "accountID", "device_id", "txn_count", "first_seen", "last_seen",
};
inline constexpr Table kMlAccountDevice =
    detail::make("Account_Device.csv", kMlAccountDeviceHeader);

inline constexpr std::array<std::string_view, 5> kMlAccountIpHeader{
    "accountID", "ip_address", "txn_count", "first_seen", "last_seen",
};
inline constexpr Table kMlAccountIp =
    detail::make("Account_IP.csv", kMlAccountIpHeader);

inline constexpr std::array<Table, 4> kMlTables{
    kMlParty,
    kMlTransfer,
    kMlAccountDevice,
    kMlAccountIp,
};

} // namespace PhantomLedger::exporter::schema
