from collections.abc import Iterator

from entities import models
from ..csv_io import Row

_PREFIX_KIND_CATEGORY: tuple[tuple[str, str, str], ...] = (
    ("XF", "family_external", "family"),
    ("XGOV", "government_external", "government"),
    ("XINS", "insurance_external", "insurance"),
    ("XIRS", "tax_authority_external", "tax"),
    ("XLND", "lender_external", "lending"),
    ("XOB", "business_operating_external", "business"),
    ("XBR", "brokerage_custody_external", "brokerage"),
    ("XE", "employer_external", "employer"),
    ("XL", "landlord_external", "landlord"),
)


def _merchant_external_categories(merchants: models.Merchants) -> dict[str, str]:
    return {
        acct: category
        for acct, category in zip(
            merchants.counterparties,
            merchants.categories,
            strict=True,
        )
        if acct.startswith("XM")
    }


def _kind_and_category(
    account_id: str,
    merchant_categories: dict[str, str],
) -> tuple[str, str]:
    merchant_category = merchant_categories.get(account_id)
    if merchant_category is not None:
        return "merchant_external", merchant_category

    for prefix, kind, category in _PREFIX_KIND_CATEGORY:
        if account_id.startswith(prefix):
            return kind, category

    return "external_account", "unknown"


def external_account(
    accounts: models.Accounts,
    merchants: models.Merchants,
) -> Iterator[Row]:
    """
    Yields rows for the EXTERNAL_ACCOUNT vertex table.
    (account_id, kind, category)

    Important:
    - preserves the existing schema exactly
    - emits all represented external accounts from the registry
    - uses merchant metadata only to label XM... rows
    """
    merchant_categories = _merchant_external_categories(merchants)

    for account_id in accounts.ids:
        if account_id not in accounts.externals:
            continue

        kind, category = _kind_and_category(account_id, merchant_categories)
        yield (account_id, kind, category)
