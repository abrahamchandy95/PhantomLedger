"""
Well-known external counterparty accounts.

These are fixed account IDs representing institutional entities
outside the bank: government agencies, insurance carriers,
lenders, loan servicers, the IRS, and the bank's own internal
servicing books for fees and line-of-credit interest.

They are registered as external accounts during entity building
so the balance ledger and exporters recognise them.
"""

# --- Government ---
GOV_SSA = "XGOV00000001"
GOV_DISABILITY = "XGOV00000002"

# --- Insurance carriers ---
INS_AUTO = "XINS00000001"
INS_HOME = "XINS00000002"
INS_LIFE = "XINS00000003"

# --- Lenders / Servicers ---
LENDER_MORTGAGE = "XLND00000001"
LENDER_AUTO = "XLND00000002"
SERVICER_STUDENT = "XLND00000003"

# --- Tax authority ---
IRS_TREASURY = "XIRS00000001"

# --- Bank servicing ---
#
# These counterparties represent the bank's own internal books for fee
# collection and overdraft-line-of-credit interest revenue. In real
# systems these are general ledger accounts, not customer accounts, but
# from the transaction-graph perspective they behave like externals:
# outbound debits that never post back as inflows to the customer.
#
# BANK_FEE_COLLECTION: destination for $27 courtesy-overdraft fees.
# BANK_OD_LOC:         destination for monthly LOC interest accrual.
BANK_FEE_COLLECTION = "XBNK00000001"
BANK_OD_LOC = "XBNK00000002"

# --- Grouped for bulk registration ---
_GOV: tuple[str, ...] = (GOV_SSA, GOV_DISABILITY)
_INS: tuple[str, ...] = (INS_AUTO, INS_HOME, INS_LIFE)
_LND: tuple[str, ...] = (LENDER_MORTGAGE, LENDER_AUTO, SERVICER_STUDENT)
_TAX: tuple[str, ...] = (IRS_TREASURY,)
_BNK: tuple[str, ...] = (BANK_FEE_COLLECTION, BANK_OD_LOC)

ALL: tuple[str, ...] = _GOV + _INS + _LND + _TAX + _BNK
