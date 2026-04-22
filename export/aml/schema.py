"""
TigerGraph AML_Schema_V1 CSV table definitions.

Each Table maps to a TigerGraph vertex or edge type. Headers match
the attribute order expected by a standard GSQL loading job.

Vertex files:  one row per vertex, columns = attributes.
Edge files:    (FROM_id, TO_id, ...edge_attributes).

Naming convention:  filenames use the exact TigerGraph type name so
the loading job can reference them directly.
"""

from dataclasses import dataclass
from typing import Final


@dataclass(frozen=True, slots=True)
class Table:
    filename: str
    header: tuple[str, ...]


# ── Vertices ─────────────────────────────────────────────────────

CUSTOMER: Final[Table] = Table(
    filename="Customer.csv",
    header=(
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
    ),
)

ACCOUNT: Final[Table] = Table(
    filename="Account.csv",
    header=(
        "id",
        "account_number",
        "balance",
        "open_date",
        "status",
        "most_recent_transaction_date",
        "close_date",
        "account_type",
        "currency",
        "branch_code",
    ),
)

COUNTERPARTY: Final[Table] = Table(
    filename="Counterparty.csv",
    header=(
        "id",
        "name",
        "bank_id_code",
        "bank_name",
        "country",
    ),
)

NAME: Final[Table] = Table(
    filename="Name.csv",
    header=(
        "id",
        "First_Name",
        "Middle_Name",
        "Last_Name",
    ),
)

ADDRESS: Final[Table] = Table(
    filename="Address.csv",
    header=(
        "id",
        "street_line1",
        "street_line2",
        "city",
        "state",
        "postal_code",
        "country",
        "address_type",
        "is_high_risk_geo",
    ),
)

COUNTRY: Final[Table] = Table(
    filename="Country.csv",
    header=(
        "id",
        "name",
        "risk_score",
    ),
)

WATCHLIST: Final[Table] = Table(
    filename="Watchlist.csv",
    header=(
        "id",
        "list_name",
        "list_type",
        "entry_date",
    ),
)

DEVICE: Final[Table] = Table(
    filename="Device.csv",
    header=(
        "device_id",
        "device_type",
        "ip_address",
        "ip_country",
        "os",
        "first_seen",
        "last_seen",
        "is_vpn",
    ),
)

TRANSACTION: Final[Table] = Table(
    filename="Transaction.csv",
    header=(
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
    ),
)

SAR: Final[Table] = Table(
    filename="SAR.csv",
    header=(
        "sar_id",
        "filing_date",
        "amount_involved",
        "activity_start",
        "activity_end",
        "violation_type",
    ),
)

BANK: Final[Table] = Table(
    filename="Bank.csv",
    header=("id",),
)

NAME_MINHASH: Final[Table] = Table(
    filename="Name_MinHash.csv",
    header=("id",),
)

ADDRESS_MINHASH: Final[Table] = Table(
    filename="Address_MinHash.csv",
    header=("id",),
)

STREET_LINE1_MINHASH: Final[Table] = Table(
    filename="Street_Line1_MinHash.csv",
    header=("id",),
)

CITY_MINHASH: Final[Table] = Table(
    filename="City_MinHash.csv",
    header=("id",),
)

STATE_MINHASH: Final[Table] = Table(
    filename="State_MinHash.csv",
    header=("id",),
)

CONNECTED_COMPONENT: Final[Table] = Table(
    filename="Connected_Component.csv",
    header=("id",),
)


# ── Edges ────────────────────────────────────────────────────────

CUSTOMER_HAS_ACCOUNT: Final[Table] = Table(
    filename="customer_has_account.csv",
    header=("Customer", "Account"),
)

ACCOUNT_HAS_PRIMARY_CUSTOMER: Final[Table] = Table(
    filename="account_has_primary_customer.csv",
    header=("Account", "Customer"),
)

SEND_TRANSACTION: Final[Table] = Table(
    filename="send_transaction.csv",
    header=("Account", "Transaction"),
)

RECEIVE_TRANSACTION: Final[Table] = Table(
    filename="receive_transaction.csv",
    header=("Account", "Transaction"),
)

COUNTERPARTY_SEND_TRANSACTION: Final[Table] = Table(
    filename="counterparty_send_transaction.csv",
    header=("Counterparty", "Transaction", "counterparty_name"),
)

COUNTERPARTY_RECEIVE_TRANSACTION: Final[Table] = Table(
    filename="counterparty_receive_transaction.csv",
    header=("Counterparty", "Transaction", "counterparty_name"),
)

SENT_TRANSACTION_TO_COUNTERPARTY: Final[Table] = Table(
    filename="sent_transaction_to_counterparty.csv",
    header=("Account", "Counterparty"),
)

RECEIVED_TRANSACTION_FROM_COUNTERPARTY: Final[Table] = Table(
    filename="received_transaction_from_counterparty.csv",
    header=("Account", "Counterparty"),
)

USES_DEVICE: Final[Table] = Table(
    filename="uses_device.csv",
    header=("Customer", "Device", "first_seen", "last_seen", "session_count"),
)

LOGGED_FROM: Final[Table] = Table(
    filename="logged_from.csv",
    header=("Account", "Device", "first_access", "last_access", "access_count"),
)

CUSTOMER_HAS_NAME: Final[Table] = Table(
    filename="customer_has_name.csv",
    header=("Customer", "Name", "last_update"),
)

CUSTOMER_HAS_ADDRESS: Final[Table] = Table(
    filename="customer_has_address.csv",
    header=("Customer", "Address", "last_update"),
)

CUSTOMER_ASSOCIATED_WITH_COUNTRY: Final[Table] = Table(
    filename="customer_associated_with_country.csv",
    header=("Customer", "Country", "last_update"),
)

ACCOUNT_HAS_NAME: Final[Table] = Table(
    filename="account_has_name.csv",
    header=("Account", "Name", "type_of", "last_update"),
)

ACCOUNT_HAS_ADDRESS: Final[Table] = Table(
    filename="account_has_address.csv",
    header=("Account", "Address", "type_of", "last_update"),
)

ACCOUNT_ASSOCIATED_WITH_COUNTRY: Final[Table] = Table(
    filename="account_associated_with_country.csv",
    header=("Account", "Country", "last_update"),
)

ADDRESS_IN_COUNTRY: Final[Table] = Table(
    filename="address_in_country.csv",
    header=("Address", "Country", "last_update"),
)

CUSTOMER_MATCHES_WATCHLIST: Final[Table] = Table(
    filename="customer_matches_watchlist.csv",
    header=("Customer", "Watchlist"),
)

COUNTERPARTY_HAS_NAME: Final[Table] = Table(
    filename="counterparty_has_name.csv",
    header=("Counterparty", "Name", "last_update"),
)

COUNTERPARTY_HAS_ADDRESS: Final[Table] = Table(
    filename="counterparty_has_address.csv",
    header=("Counterparty", "Address", "last_update"),
)

COUNTERPARTY_ASSOCIATED_WITH_COUNTRY: Final[Table] = Table(
    filename="counterparty_associated_with_country.csv",
    header=("Counterparty", "Country"),
)

SAME_AS: Final[Table] = Table(
    filename="same_as.csv",
    header=("Customer", "Customer", "score"),
)

REFERENCES: Final[Table] = Table(
    filename="references.csv",
    header=("SAR", "Customer", "role"),
)

SAR_COVERS: Final[Table] = Table(
    filename="sar_covers.csv",
    header=("SAR", "Account", "activity_amount"),
)

BENEFICIARY_BANK: Final[Table] = Table(
    filename="beneficiary_bank.csv",
    header=("Bank", "Counterparty"),
)

ORIGINATOR_BANK: Final[Table] = Table(
    filename="originator_bank.csv",
    header=("Bank", "Counterparty"),
)

BANK_ASSOCIATED_WITH_COUNTRY: Final[Table] = Table(
    filename="bank_associated_with_country.csv",
    header=("Bank", "Country"),
)

BANK_HAS_ADDRESS: Final[Table] = Table(
    filename="bank_has_address.csv",
    header=("Bank", "Address", "last_update"),
)

BANK_HAS_NAME: Final[Table] = Table(
    filename="bank_has_name.csv",
    header=("Bank", "Name", "last_update"),
)

CUSTOMER_IN_CONNECTED_COMPONENT: Final[Table] = Table(
    filename="customer_in_connected_component.csv",
    header=("Customer", "Connected_Component"),
)

CUSTOMER_HAS_NAME_MINHASH: Final[Table] = Table(
    filename="customer_has_name_minhash.csv",
    header=("Customer", "Name_MinHash"),
)

CUSTOMER_HAS_ADDRESS_MINHASH: Final[Table] = Table(
    filename="customer_has_address_minhash.csv",
    header=("Customer", "Address_MinHash"),
)

CUSTOMER_HAS_ADDRESS_STREET_LINE1_MINHASH: Final[Table] = Table(
    filename="customer_has_address_street_line1_minhash.csv",
    header=("Customer", "Street_Line1_MinHash"),
)

CUSTOMER_HAS_ADDRESS_CITY_MINHASH: Final[Table] = Table(
    filename="customer_has_address_city_minhash.csv",
    header=("Customer", "City_MinHash"),
)

CUSTOMER_HAS_ADDRESS_STATE_MINHASH: Final[Table] = Table(
    filename="customer_has_address_state_minhash.csv",
    header=("Customer", "State_MinHash"),
)

ACCOUNT_HAS_NAME_MINHASH: Final[Table] = Table(
    filename="account_has_name_minhash.csv",
    header=("Account", "Name_MinHash"),
)

COUNTERPARTY_HAS_NAME_MINHASH: Final[Table] = Table(
    filename="counterparty_has_name_minhash.csv",
    header=("Counterparty", "Name_MinHash"),
)

RESOLVES_TO: Final[Table] = Table(
    filename="resolves_to.csv",
    header=(
        "Counterparty",
        "Customer",
        "match_confidence",
        "match_method",
        "resolved_date",
    ),
)
