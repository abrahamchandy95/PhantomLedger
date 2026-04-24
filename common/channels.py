"""
Canonical channel identifiers for transaction classification.

Every TransactionDraft and Transaction uses one of these values
in its `channel` field. Centralizing them prevents typo drift
and makes grep/refactor trivial.
"""

# --- Legitimate channels ---
SALARY = "salary"
RENT = "rent"
RENT_ACH = "rent_ach"
RENT_PORTAL = "rent_portal"
RENT_P2P = "rent_p2p"
RENT_CHECK = "rent_check"
SUBSCRIPTION = "subscription"
ATM = "atm_withdrawal"
SELF_TRANSFER = "self_transfer"
MERCHANT = "merchant"
CARD_PURCHASE = "card_purchase"
BILL = "bill"
P2P = "p2p"
EXTERNAL_UNKNOWN = "external_unknown"
CLIENT_ACH_CREDIT = "client_ach_credit"
CARD_SETTLEMENT = "card_settlement"
PLATFORM_PAYOUT = "platform_payout"
OWNER_DRAW = "owner_draw"
INVESTMENT_INFLOW = "investment_inflow"

# --- Family channels ---
ALLOWANCE = "allowance"
TUITION = "tuition"
FAMILY_SUPPORT = "family_support"
SPOUSE_TRANSFER = "spouse_transfer"
PARENT_GIFT = "parent_gift"
SIBLING_TRANSFER = "sibling_transfer"
GRANDPARENT_GIFT = "grandparent_gift"
INHERITANCE = "inheritance"

# --- Credit card lifecycle ---
CC_PAYMENT = "cc_payment"
CC_INTEREST = "cc_interest"
CC_LATE_FEE = "cc_late_fee"
CC_REFUND = "cc_refund"
CC_CHARGEBACK = "cc_chargeback"

# --- Financial products / obligations ---
MORTGAGE_PAYMENT = "mortgage_payment"
AUTO_LOAN_PAYMENT = "auto_loan_payment"
STUDENT_LOAN_PAYMENT = "student_loan_payment"
TAX_ESTIMATED_PAYMENT = "tax_estimated_payment"
TAX_BALANCE_DUE = "tax_balance_due"
TAX_REFUND = "tax_refund"

# --- Deposit-account liquidity events ---
#
# Emitted by the authoritative chronological replay when a debit taps the
# account's overdraft protection. They appear as visible outflows from the
# customer's account to the bank's servicing counterparty.
#
# OVERDRAFT_FEE: ~$27 flat fee charged per item that overdraws a courtesy-
#                protected checking account. Capped at 3 items per day.
#                Source: Bankrate 2025, avg $26.95 industry fee.
#
# LOC_INTEREST:  Monthly interest accrual on the negative balance of an
#                overdraft line-of-credit (LOC) account. APR ~18-25%,
#                billed on the account's cycle day using a dollar-days
#                integral over the billing period.
OVERDRAFT_FEE = "overdraft_fee"
LOC_INTEREST = "loc_interest"

# --- Fraud channels ---
FRAUD_CLASSIC = "fraud_classic"
FRAUD_CYCLE = "fraud_cycle"
FRAUD_LAYERING_IN = "fraud_layering_in"
FRAUD_LAYERING_HOP = "fraud_layering_hop"
FRAUD_LAYERING_OUT = "fraud_layering_out"
FRAUD_FUNNEL_IN = "fraud_funnel_in"
FRAUD_FUNNEL_OUT = "fraud_funnel_out"
FRAUD_STRUCTURING = "fraud_structuring"
FRAUD_INVOICE = "fraud_invoice"
FRAUD_MULE_IN = "fraud_mule_in"
FRAUD_MULE_FORWARD = "fraud_mule_forward"

# --- Camouflage channels ---
CAMOUFLAGE_BILL = "camouflage_bill"
CAMOUFLAGE_P2P = "camouflage_p2p"
CAMOUFLAGE_SALARY = "camouflage_salary"

# --- Government ---
GOV_SOCIAL_SECURITY = "gov_social_security"
GOV_PENSION = "gov_pension"
DISABILITY = "gov_disability"

# --- Insurance ---
INSURANCE_PREMIUM = "insurance_premium"
INSURANCE_CLAIM = "insurance_claim"

# Grouped rent channels. Callers that want "any rent-flavored channel"
# (exporters, dashboards, fraud detectors) can use this instead of hard-coding
# every variant.
RENT_CHANNELS: frozenset[str] = frozenset(
    {
        RENT,
        RENT_ACH,
        RENT_PORTAL,
        RENT_P2P,
        RENT_CHECK,
    }
)

PAYDAY_INBOUND_CHANNELS: frozenset[str] = frozenset(
    {
        SALARY,
        GOV_SOCIAL_SECURITY,
        GOV_PENSION,
        DISABILITY,
        CLIENT_ACH_CREDIT,
        CARD_SETTLEMENT,
        PLATFORM_PAYOUT,
        OWNER_DRAW,
        INVESTMENT_INFLOW,
    }
)

# Liquidity-event channels are emitted by the balance replay itself rather
# than by any upstream transfer generator. The balance ledger uses this set
# to break recursion: a courtesy tap on an OVERDRAFT_FEE debit does not
# itself trigger another fee, and an LOC_INTEREST posting never pays its
# own interest.
LIQUIDITY_EVENT_CHANNELS: frozenset[str] = frozenset(
    {
        OVERDRAFT_FEE,
        LOC_INTEREST,
    }
)

# Convenience set for fraud detection labeling
FRAUD_CHANNELS: frozenset[str] = frozenset(
    {
        FRAUD_CLASSIC,
        FRAUD_CYCLE,
        FRAUD_LAYERING_IN,
        FRAUD_LAYERING_HOP,
        FRAUD_LAYERING_OUT,
        FRAUD_FUNNEL_IN,
        FRAUD_FUNNEL_OUT,
        FRAUD_STRUCTURING,
        FRAUD_INVOICE,
        FRAUD_MULE_IN,
        FRAUD_MULE_FORWARD,
    }
)
