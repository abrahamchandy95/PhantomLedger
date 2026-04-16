from collections.abc import Iterator

from entities import models
from ..csv_io import Row

# Order matters: more specific prefixes (XLI/XLS/XLC) must come before the
# generic XL fallback so typed landlord externals get their proper labels.
_PREFIX_KIND_CATEGORY: tuple[tuple[str, str, str], ...] = (
    ("XF", "family_external", "family"),
    ("XGOV", "government_external", "government"),
    ("XINS", "insurance_external", "insurance"),
    ("XIRS", "tax_authority_external", "tax"),
    ("XLND", "lender_external", "lending"),
    ("XLI", "landlord_individual_external", "landlord_individual"),
    ("XLS", "landlord_small_llc_external", "landlord_small_llc"),
    ("XLC", "landlord_corporate_external", "landlord_corporate"),
    ("XL", "landlord_external", "landlord"),
    ("XE", "employer_external", "employer"),
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

    - emits all represented external accounts from the registry
    - uses merchant metadata only to label XM... rows
    - landlord externals are split by typology (XLI/XLS/XLC) so the
      exported graph carries individual / small-LLC / corporate labels
    """
    merchant_categories = _merchant_external_categories(merchants)

    for account_id in accounts.ids:
        if account_id not in accounts.externals:
            continue

        kind, category = _kind_and_category(account_id, merchant_categories)
        yield (account_id, kind, category)
