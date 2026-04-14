from dataclasses import dataclass
from typing import Final


@dataclass(frozen=True, slots=True)
class Table:
    filename: str
    header: tuple[str, ...]


# -----------------------------
# Vertex CSV specs
# -----------------------------
PERSON: Final[Table] = Table(
    filename="person.csv",
    header=("customer_id", "mule", "fraud", "victim", "solo_fraud"),
)

DEVICE: Final[Table] = Table(
    filename="device.csv",
    header=("device_id", "device_type", "flagged_device"),
)

IP_ADDRESS: Final[Table] = Table(
    filename="ipaddress.csv",
    header=("ip_address", "blacklisted_ip"),
)

ACCOUNT_NUMBER: Final[Table] = Table(
    filename="accountnumber.csv",
    header=("account_number", "mule", "fraud", "victim", "is_external"),
)

PHONE: Final[Table] = Table(
    filename="phone.csv",
    header=("phone_id",),
)

EMAIL: Final[Table] = Table(
    filename="email.csv",
    header=("email_id",),
)

MERCHANT: Final[Table] = Table(
    filename="merchants.csv",
    header=("merchant_id", "counterparty_acct", "category", "weight", "in_bank"),
)

EXTERNAL_ACCOUNT: Final[Table] = Table(
    filename="external_accounts.csv",
    header=("account_id", "kind", "category"),
)

VERTICES: Final[tuple[Table, ...]] = (
    PERSON,
    DEVICE,
    IP_ADDRESS,
    ACCOUNT_NUMBER,
    PHONE,
    EMAIL,
    MERCHANT,
)

# -----------------------------
# Edge CSV specs
# -----------------------------

HAS_ACCOUNT: Final[Table] = Table(
    filename="HAS_ACCOUNT.csv",
    header=("FROM", "TO"),
)

HAS_PHONE: Final[Table] = Table(
    filename="HAS_PHONE.csv",
    header=("FROM", "TO"),
)

HAS_EMAIL: Final[Table] = Table(
    filename="HAS_EMAIL.csv",
    header=("FROM", "TO"),
)

HAS_USED: Final[Table] = Table(
    filename="HAS_USED.csv",
    header=("FROM", "TO", "first_seen", "last_seen"),
)

HAS_IP: Final[Table] = Table(
    filename="HAS_IP.csv",
    header=("FROM", "TO", "first_seen", "last_seen"),
)

HAS_PAID: Final[Table] = Table(
    filename="HAS_PAID.csv",
    header=(
        "FROM",
        "TO",
        "total_amount",
        "total_num_txns",
        "first_txn_date",
        "last_txn_date",
    ),
)

LEDGER: Final[Table] = Table(
    filename="transactions.csv",
    header=(
        "src_acct",
        "dst_acct",
        "amount",
        "ts",
        "is_fraud",
        "ring_id",
        "device_id",
        "ip_address",
        "channel",
    ),
)

EDGES: Final[tuple[Table, ...]] = (
    HAS_ACCOUNT,
    HAS_PHONE,
    HAS_EMAIL,
    HAS_USED,
    HAS_IP,
    HAS_PAID,
)

# -----------------------------
# ML-ready CSV specs (Mule Detection)
# -----------------------------

ML_PARTY: Final[Table] = Table(
    filename="Party.csv",
    header=(
        "id",
        "isFraud",
        "phoneNumber",
        "email",
        "name",
        "SSN",
        "dob",
        "address",
        "state",
        "city",
        "zipcode",
        "country",
        "ip",
        "device",
    ),
)

ML_TRANSFER: Final[Table] = Table(
    filename="Transfer_Transaction.csv",
    header=(
        "id",
        "fromAccountID",
        "toAccountID",
        "amount",
        "transfer_time",
    ),
)

ML_ACCOUNT_DEVICE: Final[Table] = Table(
    filename="Account_Device.csv",
    header=(
        "accountID",
        "device_id",
        "txn_count",
        "first_seen",
        "last_seen",
    ),
)

ML_ACCOUNT_IP: Final[Table] = Table(
    filename="Account_IP.csv",
    header=(
        "accountID",
        "ip_address",
        "txn_count",
        "first_seen",
        "last_seen",
    ),
)

ML_TABLES: Final[tuple[Table, ...]] = (
    ML_PARTY,
    ML_TRANSFER,
    ML_ACCOUNT_DEVICE,
    ML_ACCOUNT_IP,
)
