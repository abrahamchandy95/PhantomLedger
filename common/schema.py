from dataclasses import dataclass
from typing import Final


@dataclass(frozen=True, slots=True)
class CsvSpec:
    filename: str
    header: tuple[str, ...]


# -----------------------------
# Vertex CSV specs
# -----------------------------

PERSON: Final[CsvSpec] = CsvSpec(
    filename="person.csv",
    header=("customer_id", "mule", "fraud", "victim"),
)

DEVICE: Final[CsvSpec] = CsvSpec(
    filename="device.csv",
    header=("device_id", "device_type", "flagged_device"),
)

IPADDRESS: Final[CsvSpec] = CsvSpec(
    filename="ipaddress.csv",
    header=("ip_address", "blacklisted_ip"),
)

ACCOUNTNUMBER: Final[CsvSpec] = CsvSpec(
    filename="accountnumber.csv",
    header=("account_number", "mule", "fraud", "victim"),
)

PHONE: Final[CsvSpec] = CsvSpec(
    filename="phone.csv",
    header=("phone_id",),
)

EMAIL: Final[CsvSpec] = CsvSpec(
    filename="email.csv",
    header=("email_id",),
)


VERTEX_SPECS: Final[tuple[CsvSpec, ...]] = (
    PERSON,
    DEVICE,
    IPADDRESS,
    ACCOUNTNUMBER,
    PHONE,
    EMAIL,
)


# -----------------------------
# Edge CSV specs
# -----------------------------

HAS_ACCOUNT: Final[CsvSpec] = CsvSpec(
    filename="HAS_ACCOUNT.csv",
    header=("FROM", "TO"),
)

HAS_PHONE: Final[CsvSpec] = CsvSpec(
    filename="HAS_PHONE.csv",
    header=("FROM", "TO"),
)

HAS_EMAIL: Final[CsvSpec] = CsvSpec(
    filename="HAS_EMAIL.csv",
    header=("FROM", "TO"),
)

HAS_USED: Final[CsvSpec] = CsvSpec(
    filename="HAS_USED.csv",
    header=("FROM", "TO", "first_seen", "last_seen"),
)

HAS_IP: Final[CsvSpec] = CsvSpec(
    filename="HAS_IP.csv",
    header=("FROM", "TO", "first_seen", "last_seen"),
)

HAS_PAID: Final[CsvSpec] = CsvSpec(
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

RAW_LEDGER: Final[CsvSpec] = CsvSpec(
    filename="transactions_raw.csv",
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


EDGE_SPECS: Final[tuple[CsvSpec, ...]] = (
    HAS_ACCOUNT,
    HAS_PHONE,
    HAS_EMAIL,
    HAS_USED,
    HAS_IP,
    HAS_PAID,
)
