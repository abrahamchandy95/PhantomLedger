"""
Well-known external counterparty accounts.

These are fixed account IDs representing institutional entities
outside the bank: government agencies, insurance carriers,
lenders, loan servicers, and the IRS.

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

# --- Grouped for bulk registration ---
_GOV: tuple[str, ...] = (GOV_SSA, GOV_DISABILITY)
_INS: tuple[str, ...] = (INS_AUTO, INS_HOME, INS_LIFE)
_LND: tuple[str, ...] = (LENDER_MORTGAGE, LENDER_AUTO, SERVICER_STUDENT)
_TAX: tuple[str, ...] = (IRS_TREASURY,)

ALL: tuple[str, ...] = _GOV + _INS + _LND + _TAX
