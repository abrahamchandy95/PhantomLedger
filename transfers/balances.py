"""
Balance ledger with realistic overdraft protection, merchant seeding,
and tiered bank-fee assignment.

Design:

Every internal account carries exactly one overdraft-protection product,
drawn from a per-persona multinomial mix:

  NONE     — hard decline at zero
  COURTESY — small discretionary buffer (~$100-300), per-account lognormal
             fee drawn from a per-tier distribution, capped at 3 taps/day
  LINKED   — free sweep from a linked savings account; no fee, just a cap
  LOC      — overdraft line of credit; interest accrues at a persona-
             specific APR on the dollar-seconds integral of negative balance,
             billed monthly on the account's cycle day

Exclusivity is important: real customers don't stack all three. The liquidity
sum `balance + overdraft + linked + courtesy` still works as before because
at most one buffer is non-zero per account.

Each internal account is also assigned a `BankTier`:

  ZERO_FEE     — Capital One, Citibank, Ally etc. (~15% of accounts).
                 Courtesy tap still happens (the account can go negative
                 under courtesy coverage), but no overdraft fee is emitted.
  REDUCED_FEE  — Bank of America style (~10% of accounts, $10 per item).
  STANDARD_FEE — big-bank mainstream (~75% of accounts, $30-35 per item).

The per-account fee amount is drawn from a tier-specific lognormal at
initialization. This means fee heterogeneity comes from inter-account
(i.e. inter-bank) variation, not per-transaction noise — which matches
real banking, where an account's overdraft fee is set by its bank.

Merchant internals (non-X-prefixed M... accounts) get a lognormal business-
checking seed (~$8k median) instead of the personal SALARIED default of
$1.2k. Refunds can then succeed without bouncing. Merchants carry no
personal protection products and no bank-tier fee.

Credit card accounts (L...) are explicitly excluded from protection
assignment. `set_credit_limit` repurposes the `overdrafts` field to hold
the credit limit but keeps `protection_type = NONE`, so they never
accidentally register as LOC interest-bearing accounts.
"""

from collections.abc import Callable
from dataclasses import dataclass, field
from datetime import datetime
from enum import IntEnum
from typing import cast

import numpy as np
import numpy.typing as npt

from common.channels import LIQUIDITY_EVENT_CHANNELS, SELF_TRANSFER
from common.ids import is_external
from common.math import Bool, F64, I64, lognormal_by_median
from common.persona_names import (
    FREELANCER,
    HNW,
    RETIRED,
    SALARIED,
    SMALLBIZ,
    STUDENT,
)
from common.random import Rng
from common.validate import between, ge, gt
from entities.personas import get_persona

REJECT_INVALID_AMOUNT = "invalid_amount"
REJECT_EXTERNAL_TO_EXTERNAL = "external_to_external"
REJECT_INSUFFICIENT_FUNDS = "insufficient_funds"

type PersonaFloatLookup = Callable[[str], float]

_SECONDS_PER_YEAR: float = 365.0 * 86400.0


class ProtectionType(IntEnum):
    """
    Single overdraft-protection product carried by an internal account.

    Values double as indices into the int8 array stored on ClearingHouse.
    """

    NONE = 0
    COURTESY = 1
    LINKED = 2
    LOC = 3


class BankTier(IntEnum):
    """
    Per-account overdraft-fee regime.

    Real-world composition (industry reporting, Bankrate 2025, Consumer
    Reports 2024):
      ZERO_FEE     ~15%  (Capital One, Citibank, Ally)
      REDUCED_FEE  ~10%  (Bank of America)
      STANDARD_FEE ~75%  (Chase, Wells Fargo, US Bank, PNC, etc.)

    The tier determines the lognormal distribution from which the
    account's own overdraft fee is sampled at initialization.
    """

    ZERO_FEE = 0
    REDUCED_FEE = 1
    STANDARD_FEE = 2


@dataclass(frozen=True, slots=True)
class ProtectionMix:
    """Per-persona protection-type distribution. Remainder is NONE."""

    courtesy_p: float
    linked_p: float
    loc_p: float

    def __post_init__(self) -> None:
        between("courtesy_p", self.courtesy_p, 0.0, 1.0)
        between("linked_p", self.linked_p, 0.0, 1.0)
        between("loc_p", self.loc_p, 0.0, 1.0)
        total = self.courtesy_p + self.linked_p + self.loc_p
        if total > 1.0 + 1e-9:
            raise ValueError(
                f"protection probs sum to {total:.3f}, must be <= 1.0 "
                + "(remainder is ProtectionType.NONE)"
            )


@dataclass(frozen=True, slots=True)
class BankTierMix:
    """
    Population-level distribution of bank-tier assignments.

    Remainder (1 - zero_fee_p - reduced_fee_p) is STANDARD_FEE.
    Defaults match published account-share composition.
    """

    zero_fee_p: float = 0.15
    reduced_fee_p: float = 0.10

    def __post_init__(self) -> None:
        between("zero_fee_p", self.zero_fee_p, 0.0, 1.0)
        between("reduced_fee_p", self.reduced_fee_p, 0.0, 1.0)
        total = self.zero_fee_p + self.reduced_fee_p
        if total > 1.0 + 1e-9:
            raise ValueError(
                f"bank-tier probs sum to {total:.3f}, must be <= 1.0 "
                + "(remainder is BankTier.STANDARD_FEE)"
            )

    @property
    def standard_fee_p(self) -> float:
        return max(0.0, 1.0 - self.zero_fee_p - self.reduced_fee_p)


@dataclass(frozen=True, slots=True)
class TierFeeParams:
    """
    Lognormal overdraft-fee distribution for one bank tier.

    `median` of 0.0 is a sentinel for "this tier never charges a fee"
    (used for ZERO_FEE). In that case `sigma`, `floor`, `ceiling` are
    ignored and the per-account fee is stored as 0.0.
    """

    median: float
    sigma: float
    floor: float
    ceiling: float

    def __post_init__(self) -> None:
        ge("median", self.median, 0.0)
        ge("sigma", self.sigma, 0.0)
        ge("floor", self.floor, 0.0)
        ge("ceiling", self.ceiling, self.floor)


@dataclass(frozen=True, slots=True)
class TransferDecision:
    """
    Outcome of a single balance-gated transfer attempt.

    The optional `courtesy_tapped_*` fields carry side-channel information
    for the caller: when a debit on a COURTESY-protected account ends with a
    negative balance, the ledger populates these fields so the replay
    accumulator can emit a matching overdraft-fee transaction.
    """

    accepted: bool
    reason: str | None = None
    courtesy_tapped_idx: int | None = None
    courtesy_tapped_amount: float | None = None


_DECISION_ACCEPTED = TransferDecision(True)
_DECISION_INVALID = TransferDecision(False, REJECT_INVALID_AMOUNT)
_DECISION_EXT_TO_EXT = TransferDecision(False, REJECT_EXTERNAL_TO_EXTERNAL)
_DECISION_INSUFFICIENT = TransferDecision(False, REJECT_INSUFFICIENT_FUNDS)


def _default_protection_mix() -> dict[str, ProtectionMix]:
    """
    Default per-persona mix of overdraft-protection products.

    Research anchors:
      - CFPB 2024: ~20% of US consumers have opted into standard courtesy
        overdraft; enrollment skews higher among low-balance cohorts.
      - Bankrate 2025: ~40% of checking customers link a savings sweep for
        free overdraft protection when offered.
      - SoFi / NerdWallet 2025: formal OD LOCs are less common (~10% of
        checking customers), more prevalent among freelancers / small
        business owners who need interest-bearing credit.
      - HNW customers skew toward linked sweeps (large savings balances)
        and private-banking LOCs; small business cohorts skew toward LOCs
        for working capital.
      - Remainder (no protection) is hard decline, more common for
        students and recently-closed courtesy opt-ins.
    """
    return {
        STUDENT: ProtectionMix(courtesy_p=0.30, linked_p=0.20, loc_p=0.03),
        RETIRED: ProtectionMix(courtesy_p=0.35, linked_p=0.38, loc_p=0.05),
        SALARIED: ProtectionMix(courtesy_p=0.40, linked_p=0.35, loc_p=0.10),
        FREELANCER: ProtectionMix(courtesy_p=0.38, linked_p=0.28, loc_p=0.12),
        SMALLBIZ: ProtectionMix(courtesy_p=0.35, linked_p=0.30, loc_p=0.20),
        HNW: ProtectionMix(courtesy_p=0.25, linked_p=0.50, loc_p=0.20),
    }


def _default_loc_apr() -> dict[str, float]:
    """
    Annual percentage rate on overdraft line-of-credit balances, per persona.

    Typical OD LOC pricing:
      - Big-bank standard: 18-22% APR
      - Subprime / limited credit history: 24-30%
      - Private / HNW relationship pricing: 14-18%
    """
    return {
        STUDENT: 0.24,
        RETIRED: 0.19,
        SALARIED: 0.21,
        FREELANCER: 0.22,
        SMALLBIZ: 0.22,
        HNW: 0.16,
    }


def _default_tier_fees() -> dict[int, TierFeeParams]:
    """
    Default per-tier fee distributions.

    Anchors (Bankrate 2025, Consumer Reports 2024, published big-bank
    fee schedules):
      ZERO_FEE:     $0 flat. Capital One, Citibank, Ally, Discover.
      REDUCED_FEE:  $10 median, tight spread. BofA published the $10
                    fee in 2022 and the market has coalesced near it.
      STANDARD_FEE: $32 median, sigma 0.15. Chase is $34, Wells is $35,
                    US Bank is $36, PNC is $36 — narrow real spread.
    """
    return {
        int(BankTier.ZERO_FEE): TierFeeParams(
            median=0.0, sigma=0.0, floor=0.0, ceiling=0.0
        ),
        int(BankTier.REDUCED_FEE): TierFeeParams(
            median=10.0, sigma=0.20, floor=5.0, ceiling=15.0
        ),
        int(BankTier.STANDARD_FEE): TierFeeParams(
            median=32.0, sigma=0.15, floor=20.0, ceiling=40.0
        ),
    }


@dataclass(frozen=True, slots=True)
class Rules:
    enable_constraints: bool = True

    # Size sigma for each buffer. The median is drawn from persona-specific
    # lookup tables below; sigma controls the spread within each persona.
    courtesy_buffer_sigma: float = 0.45
    linked_buffer_sigma: float = 0.90
    overdraft_limit_sigma: float = 0.60

    # Personal checking balance seeding (non-merchant internal accounts).
    initial_balance_sigma: float = 1.00

    # Merchant working-capital seeding.
    #
    # Bluevine 2025: 39% of small businesses have < 1 month of operating
    # expenses on hand; healthier SMBs hold 2-3 months. Typical small-
    # business checking clusters in $2k-$20k. $8k median puts us in the
    # realistic middle — a single refund doesn't bust the account, but
    # sustained drain can still produce realistic shortfalls.
    merchant_balance_median: float = 8_000.0
    merchant_balance_sigma: float = 0.90

    # Per-persona mix of mutually-exclusive protection products.
    protection_mix: dict[str, ProtectionMix] = field(
        default_factory=_default_protection_mix
    )

    # Per-persona APR applied to LOC negative-balance accruals.
    loc_apr: dict[str, float] = field(default_factory=_default_loc_apr)

    # Population-level bank-tier distribution and per-tier fee parameters.
    bank_tier_mix: BankTierMix = field(default_factory=BankTierMix)
    tier_fees: dict[int, TierFeeParams] = field(default_factory=_default_tier_fees)

    def __post_init__(self) -> None:
        ge("courtesy_buffer_sigma", self.courtesy_buffer_sigma, 0.0)
        ge("linked_buffer_sigma", self.linked_buffer_sigma, 0.0)
        ge("overdraft_limit_sigma", self.overdraft_limit_sigma, 0.0)
        ge("initial_balance_sigma", self.initial_balance_sigma, 0.0)
        gt("merchant_balance_median", self.merchant_balance_median, 0.0)
        ge("merchant_balance_sigma", self.merchant_balance_sigma, 0.0)

        if not self.protection_mix:
            raise ValueError("protection_mix must be non-empty")
        if not self.loc_apr:
            raise ValueError("loc_apr must be non-empty")

        for persona, apr in self.loc_apr.items():
            between(f"loc_apr[{persona}]", apr, 0.0, 1.0)

        required_tiers = (
            int(BankTier.ZERO_FEE),
            int(BankTier.REDUCED_FEE),
            int(BankTier.STANDARD_FEE),
        )
        for tier_id in required_tiers:
            if tier_id not in self.tier_fees:
                raise ValueError(f"tier_fees missing entry for BankTier({tier_id})")

    # --- Persona-indexed size tables ---
    #
    # These return the lognormal median for the single protection product
    # assigned to a given persona. Sigma comes from the instance-level
    # field corresponding to each product.

    def initial_balance_median(self, persona: str) -> float:
        return float(get_persona(persona).initial_balance)

    def courtesy_buffer_median_for_persona(self, persona: str) -> float:
        # Standard courtesy is a small discretionary cushion (~$50-$300).
        # Not a published product dimension; banks set it per-account.
        table = {
            STUDENT: 100.0,
            RETIRED: 150.0,
            SALARIED: 200.0,
            FREELANCER: 175.0,
            SMALLBIZ: 250.0,
            HNW: 300.0,
        }
        return float(table.get(persona, table[SALARIED]))

    def linked_buffer_median_for_persona(self, persona: str) -> float:
        # Effective cap on the linked savings sweep; in practice this is
        # just the available balance in the linked savings account.
        table = {
            STUDENT: 225.0,
            RETIRED: 500.0,
            SALARIED: 700.0,
            FREELANCER: 600.0,
            SMALLBIZ: 2_200.0,
            HNW: 10_000.0,
        }
        return float(table.get(persona, table[SALARIED]))

    def overdraft_limit_median_for_persona(self, persona: str) -> float:
        # OD LOC credit-line ceilings. SoFi / NerdWallet typical ranges.
        table = {
            STUDENT: 500.0,
            RETIRED: 1_000.0,
            SALARIED: 1_500.0,
            FREELANCER: 1_500.0,
            SMALLBIZ: 3_500.0,
            HNW: 7_500.0,
        }
        return float(table.get(persona, table[SALARIED]))

    def loc_apr_for_persona(self, persona: str) -> float:
        if persona in self.loc_apr:
            return float(self.loc_apr[persona])
        return float(self.loc_apr.get(SALARIED, 0.21))


DEFAULT_RULES = Rules()


@dataclass(frozen=True, slots=True)
class SetupParams:
    """
    Inputs to `initialize`: the static layout of the account universe.

    `merchant_internals` and `card_internals` carry the account IDs that
    should NOT receive personal overdraft protection:
      - merchants get a business-checking balance overlay
      - credit cards have their `overdrafts` field repurposed as a credit
        limit via `ClearingHouse.set_credit_limit`
    """

    accounts: list[str]
    account_indices: dict[str, int]
    hub_indices: set[int]
    persona_mapping: npt.NDArray[np.int64]
    persona_names: list[str]
    merchant_internals: frozenset[str] = frozenset()
    card_internals: frozenset[str] = frozenset()


def _sample_balances(
    rules: Rules,
    rng: Rng,
    *,
    num_accounts: int,
    persona_mapping: npt.NDArray[np.int64],
    persona_names: list[str],
) -> F64:
    """Persona-indexed personal checking starting balances."""
    balances: F64 = np.zeros(num_accounts, dtype=np.float64)
    persona_arr: I64 = np.asarray(persona_mapping, dtype=np.int64)
    sigma = float(rules.initial_balance_sigma)

    for persona_id, persona_name in enumerate(persona_names):
        mask: Bool = np.asarray(persona_arr == persona_id, dtype=np.bool_)
        count = int(np.count_nonzero(mask))
        if count <= 0:
            continue

        sampled = lognormal_by_median(
            rng.gen,
            median=rules.initial_balance_median(persona_name),
            sigma=sigma,
            size=count,
        )
        balances[mask] = np.asarray(sampled, dtype=np.float64)

    return balances


def _assign_protections(
    rules: Rules,
    rng: Rng,
    *,
    num_accounts: int,
    persona_mapping: npt.NDArray[np.int64],
    persona_names: list[str],
    excluded_idx: set[int],
) -> tuple[
    npt.NDArray[np.int8],  # protection_type
    F64,  # overdrafts   (LOC credit limit)
    F64,  # linked_buffers
    F64,  # courtesy_buffers
    dict[int, float],  # loc_apr  by account index
    set[int],  # loc_accounts (set of indices)
]:
    """
    Assign exactly one protection product per eligible internal account.

    Excluded accounts (hubs, merchant internals, credit cards) keep
    ProtectionType.NONE and zero buffers. For the rest, we draw a
    single multinomial outcome per account from the persona-specific
    mix, then size only the chosen buffer.

    Returns the four arrays plus LOC-specific metadata for the
    subset of accounts that end up with LOC protection.
    """
    protection = np.zeros(num_accounts, dtype=np.int8)
    overdrafts = np.zeros(num_accounts, dtype=np.float64)
    linked = np.zeros(num_accounts, dtype=np.float64)
    courtesy = np.zeros(num_accounts, dtype=np.float64)
    loc_apr: dict[int, float] = {}
    loc_accounts: set[int] = set()

    persona_arr: I64 = np.asarray(persona_mapping, dtype=np.int64)
    default_mix = rules.protection_mix.get(
        SALARIED,
        ProtectionMix(courtesy_p=0.40, linked_p=0.35, loc_p=0.10),
    )

    for persona_id, persona_name in enumerate(persona_names):
        persona_mask: Bool = np.asarray(persona_arr == persona_id, dtype=np.bool_)
        all_idx = np.asarray(np.flatnonzero(persona_mask), dtype=np.int64)

        # Drop excluded indices (hubs, merchants, credit cards).
        if excluded_idx:
            idx_list = cast(list[int], all_idx.tolist())
            eligible = np.asarray(
                [i for i in idx_list if i not in excluded_idx],
                dtype=np.int64,
            )
        else:
            eligible = all_idx

        eligible_count = int(eligible.size)
        if eligible_count <= 0:
            continue

        mix = rules.protection_mix.get(persona_name, default_mix)
        draws = rng.gen.random(eligible_count)

        c_cut = float(mix.courtesy_p)
        l_cut = c_cut + float(mix.linked_p)
        o_cut = l_cut + float(mix.loc_p)

        courtesy_mask = draws < c_cut
        linked_mask = (draws >= c_cut) & (draws < l_cut)
        loc_mask = (draws >= l_cut) & (draws < o_cut)

        courtesy_idx = eligible[courtesy_mask]
        linked_idx = eligible[linked_mask]
        loc_idx = eligible[loc_mask]

        if courtesy_idx.size > 0:
            sizes = lognormal_by_median(
                rng.gen,
                median=rules.courtesy_buffer_median_for_persona(persona_name),
                sigma=float(rules.courtesy_buffer_sigma),
                size=int(courtesy_idx.size),
            )
            courtesy[courtesy_idx] = np.asarray(sizes, dtype=np.float64)
            protection[courtesy_idx] = int(ProtectionType.COURTESY)

        if linked_idx.size > 0:
            sizes = lognormal_by_median(
                rng.gen,
                median=rules.linked_buffer_median_for_persona(persona_name),
                sigma=float(rules.linked_buffer_sigma),
                size=int(linked_idx.size),
            )
            linked[linked_idx] = np.asarray(sizes, dtype=np.float64)
            protection[linked_idx] = int(ProtectionType.LINKED)

        if loc_idx.size > 0:
            sizes = lognormal_by_median(
                rng.gen,
                median=rules.overdraft_limit_median_for_persona(persona_name),
                sigma=float(rules.overdraft_limit_sigma),
                size=int(loc_idx.size),
            )
            overdrafts[loc_idx] = np.asarray(sizes, dtype=np.float64)
            protection[loc_idx] = int(ProtectionType.LOC)

            apr = rules.loc_apr_for_persona(persona_name)
            for raw_idx in cast(list[int], loc_idx.tolist()):
                loc_apr[raw_idx] = apr
                loc_accounts.add(raw_idx)

    return protection, overdrafts, linked, courtesy, loc_apr, loc_accounts


def _assign_bank_tiers(
    rules: Rules,
    rng: Rng,
    *,
    num_accounts: int,
    excluded_idx: set[int],
) -> npt.NDArray[np.int8]:
    """
    Draw one BankTier per account from the population-level mix.

    Excluded accounts (hubs, merchants, credit cards) get ZERO_FEE so
    they never emit fees even if the protection_type check is bypassed.
    """
    tiers = np.full(num_accounts, int(BankTier.ZERO_FEE), dtype=np.int8)

    mix = rules.bank_tier_mix
    zero_cut = float(mix.zero_fee_p)
    reduced_cut = zero_cut + float(mix.reduced_fee_p)

    eligible_mask = np.ones(num_accounts, dtype=np.bool_)
    if excluded_idx:
        excluded_list = sorted(excluded_idx)
        eligible_mask[np.asarray(excluded_list, dtype=np.int64)] = False

    eligible_idx = np.asarray(np.flatnonzero(eligible_mask), dtype=np.int64)
    eligible_count = int(eligible_idx.size)
    if eligible_count == 0:
        return tiers

    draws = rng.gen.random(eligible_count)

    zero_mask = draws < zero_cut
    reduced_mask = (draws >= zero_cut) & (draws < reduced_cut)
    standard_mask = draws >= reduced_cut

    tiers[eligible_idx[zero_mask]] = int(BankTier.ZERO_FEE)
    tiers[eligible_idx[reduced_mask]] = int(BankTier.REDUCED_FEE)
    tiers[eligible_idx[standard_mask]] = int(BankTier.STANDARD_FEE)

    return tiers


def _sample_overdraft_fees(
    rules: Rules,
    rng: Rng,
    *,
    num_accounts: int,
    bank_tier: npt.NDArray[np.int8],
) -> F64:
    """
    Sample one overdraft-fee amount per account from its tier's lognormal.

    Zero-fee-tier accounts get 0.0. Other tiers draw from the
    tier-specific lognormal and clip to [floor, ceiling].
    """
    fees: F64 = np.zeros(num_accounts, dtype=np.float64)

    for tier_id, params in rules.tier_fees.items():
        tier_mask: Bool = np.asarray(bank_tier == tier_id, dtype=np.bool_)
        count = int(np.count_nonzero(tier_mask))
        if count <= 0:
            continue

        if params.median <= 0.0:
            # ZERO_FEE sentinel: these accounts emit no fee.
            fees[tier_mask] = 0.0
            continue

        sampled = lognormal_by_median(
            rng.gen,
            median=float(params.median),
            sigma=float(params.sigma),
            size=count,
        )
        arr = np.asarray(sampled, dtype=np.float64)
        arr = np.clip(arr, float(params.floor), float(params.ceiling))
        fees[tier_mask] = arr

    return fees


def _sample_loc_billing_days(
    rng: Rng,
    loc_accounts: set[int],
) -> dict[int, int]:
    """
    Uniform billing day in [1, 28] per LOC account.

    Clamped to 28 so every month has a valid billing date without
    month-length special cases.
    """
    out: dict[int, int] = {}
    if not loc_accounts:
        return out
    for idx in sorted(loc_accounts):
        out[idx] = rng.int(1, 29)
    return out


@dataclass(slots=True)
class ClearingHouse:
    """Manages account balances, protection products, and LOC interest tracking."""

    balances: F64
    overdrafts: F64
    linked_buffers: F64
    courtesy_buffers: F64
    account_indices: dict[str, int]
    hub_indices: set[int]
    external_indices: set[int]

    # Per-account protection product (ProtectionType value).
    protection_type: npt.NDArray[np.int8] = field(
        default_factory=lambda: np.zeros(0, dtype=np.int8)
    )

    # Per-account bank tier (BankTier value). Static over the account's
    # lifetime — real customers do change banks but not inside one sim run.
    bank_tier: npt.NDArray[np.int8] = field(
        default_factory=lambda: np.zeros(0, dtype=np.int8)
    )

    # Per-account overdraft fee in dollars, sampled once from the account's
    # tier lognormal. Zero for ZERO_FEE-tier accounts and for any account
    # that should not emit courtesy fees (hubs, merchants, credit cards).
    overdraft_fee_amount: F64 = field(
        default_factory=lambda: np.zeros(0, dtype=np.float64)
    )

    # LOC accounting state. Only populated for accounts where
    # protection_type == ProtectionType.LOC.
    loc_accounts: set[int] = field(default_factory=set)
    loc_apr: dict[int, float] = field(default_factory=dict)
    loc_billing_day: dict[int, int] = field(default_factory=dict)

    # Running dollar-seconds integral of max(0, -balance) on each LOC
    # account, reset to zero after each monthly billing event.
    loc_integral_seconds: dict[int, float] = field(default_factory=dict)

    # Timestamp of the last balance-affecting touch (or billing event)
    # for each LOC account. Lazily initialized on first touch so that
    # the ledger can be constructed before the replay window is known.
    loc_last_update: dict[int, datetime] = field(default_factory=dict)

    # ------------------------------------------------------------------
    # Snapshot / restore
    # ------------------------------------------------------------------

    def copy(self) -> "ClearingHouse":
        return ClearingHouse(
            balances=np.array(self.balances, copy=True, dtype=np.float64),
            overdrafts=np.array(self.overdrafts, copy=True, dtype=np.float64),
            linked_buffers=np.array(self.linked_buffers, copy=True, dtype=np.float64),
            courtesy_buffers=np.array(
                self.courtesy_buffers, copy=True, dtype=np.float64
            ),
            account_indices=dict(self.account_indices),
            hub_indices=set(self.hub_indices),
            external_indices=set(self.external_indices),
            protection_type=np.array(self.protection_type, copy=True, dtype=np.int8),
            bank_tier=np.array(self.bank_tier, copy=True, dtype=np.int8),
            overdraft_fee_amount=np.array(
                self.overdraft_fee_amount, copy=True, dtype=np.float64
            ),
            loc_accounts=set(self.loc_accounts),
            loc_apr=dict(self.loc_apr),
            loc_billing_day=dict(self.loc_billing_day),
            loc_integral_seconds=dict(self.loc_integral_seconds),
            loc_last_update=dict(self.loc_last_update),
        )

    def restore_from(self, other: "ClearingHouse") -> None:
        if self.account_indices != other.account_indices:
            raise ValueError(
                "cannot restore from ledger with different account indices"
            )
        if self.hub_indices != other.hub_indices:
            raise ValueError("cannot restore from ledger with different hub indices")
        if self.external_indices != other.external_indices:
            raise ValueError(
                "cannot restore from ledger with different external indices"
            )

        np.copyto(self.balances, other.balances)
        np.copyto(self.overdrafts, other.overdrafts)
        np.copyto(self.linked_buffers, other.linked_buffers)
        np.copyto(self.courtesy_buffers, other.courtesy_buffers)
        np.copyto(self.protection_type, other.protection_type)
        np.copyto(self.bank_tier, other.bank_tier)
        np.copyto(self.overdraft_fee_amount, other.overdraft_fee_amount)

        # Static-after-init config: overwrite defensively in case caller
        # mutated it. Clear-and-update preserves the same dict identity so
        # any cached references keep working.
        self.loc_accounts.clear()
        self.loc_accounts.update(other.loc_accounts)
        self.loc_apr.clear()
        self.loc_apr.update(other.loc_apr)
        self.loc_billing_day.clear()
        self.loc_billing_day.update(other.loc_billing_day)

        # Dynamic state: reset integrals and last-update timestamps. The
        # accumulator re-initializes loc_last_update on its next extend()
        # pass, so the scratch book starts fresh for each screening stage.
        self.loc_integral_seconds.clear()
        for idx in self.loc_accounts:
            self.loc_integral_seconds[idx] = 0.0
        self.loc_last_update.clear()

    # ------------------------------------------------------------------
    # Internal liquidity views
    # ------------------------------------------------------------------

    def _available_liquidity(self, idx: int) -> float:
        return float(
            self.balances.item(idx)
            + self.overdrafts.item(idx)
            + self.linked_buffers.item(idx)
            + self.courtesy_buffers.item(idx)
        )

    def available_to_spend(self, account: str) -> float:
        idx = self.account_indices.get(account)
        if idx is None:
            return 0.0
        if idx in self.hub_indices:
            return float("inf")
        return self._available_liquidity(idx)

    def available_cash(self, account: str) -> float:
        idx = self.account_indices.get(account)
        if idx is None:
            return 0.0
        if idx in self.hub_indices:
            return float("inf")
        return float(self.balances.item(idx))

    def overdraft_fee_for(self, account: str) -> float:
        """
        Per-account courtesy-overdraft fee in dollars.

        Zero for ZERO_FEE-tier accounts or any account that doesn't
        carry personal overdraft protection.
        """
        idx = self.account_indices.get(account)
        if idx is None:
            return 0.0
        return float(self.overdraft_fee_amount.item(idx))

    def bank_tier_for(self, account: str) -> BankTier:
        idx = self.account_indices.get(account)
        if idx is None:
            return BankTier.ZERO_FEE
        return BankTier(int(self.bank_tier.item(idx)))

    # ------------------------------------------------------------------
    # Credit-card hookup
    # ------------------------------------------------------------------

    def set_credit_limit(self, account: str, limit_value: float) -> None:
        """
        Convert an internal account into a credit-card-style liability.

        Reuses the `overdrafts` slot to hold the credit limit so the
        existing `_available_liquidity` sum treats card accounts as
        "zero cash + full credit available" at initialization.

        Defensively clears any LOC registration: credit cards have their
        own product (CC_INTEREST, CC_LATE_FEE) and should not accrue
        LOC interest on top.
        """
        idx = self.account_indices.get(account)
        if idx is None:
            return

        self.balances[idx] = 0.0
        self.overdrafts[idx] = float(limit_value)
        self.linked_buffers[idx] = 0.0
        self.courtesy_buffers[idx] = 0.0
        self.protection_type[idx] = int(ProtectionType.NONE)

        # Credit cards never emit overdraft fees.
        self.bank_tier[idx] = int(BankTier.ZERO_FEE)
        self.overdraft_fee_amount[idx] = 0.0

        self.loc_accounts.discard(idx)
        _ = self.loc_apr.pop(idx, None)
        _ = self.loc_billing_day.pop(idx, None)
        _ = self.loc_integral_seconds.pop(idx, None)
        _ = self.loc_last_update.pop(idx, None)

    # ------------------------------------------------------------------
    # LOC interest accrual
    # ------------------------------------------------------------------

    def flush_loc_integral(self, account_idx: int, now: datetime) -> None:
        """
        Advance the dollar-seconds integral for one LOC account up to `now`.

        Called before every balance change on an LOC account so the
        integral captures the negative-balance level that held over the
        period just ended. Safe to call repeatedly; idempotent when
        `now <= last_update`.
        """
        if account_idx not in self.loc_accounts:
            return

        last = self.loc_last_update.get(account_idx)
        if last is None:
            # First touch — just anchor the clock. No integral to add.
            self.loc_last_update[account_idx] = now
            return

        if now <= last:
            return

        delta_seconds = (now - last).total_seconds()
        current_negative = max(0.0, -float(self.balances.item(account_idx)))
        accumulated = self.loc_integral_seconds.get(account_idx, 0.0)
        self.loc_integral_seconds[account_idx] = (
            accumulated + current_negative * delta_seconds
        )
        self.loc_last_update[account_idx] = now

    def bill_loc_interest(self, account_idx: int, now: datetime) -> float:
        """
        Post one billing cycle of interest on an LOC account.

        Steps:
          1. Flush the integral up to `now`.
          2. Convert dollar-seconds × APR / seconds-per-year into dollars.
          3. Directly debit the account balance — banks always capitalize
             interest even if doing so breaches the credit limit.
          4. Reset the integral to zero and anchor the clock at `now`.

        Returns the rounded interest amount (0.0 if no accrual happened).
        Callers use the return value to decide whether to emit a
        Transaction row for the accrual.
        """
        if account_idx not in self.loc_accounts:
            return 0.0

        self.flush_loc_integral(account_idx, now)

        integral_seconds = self.loc_integral_seconds.get(account_idx, 0.0)
        apr = self.loc_apr.get(account_idx, 0.0)

        interest_raw = integral_seconds * apr / _SECONDS_PER_YEAR
        interest = round(max(0.0, float(interest_raw)), 2)

        if interest > 0.0:
            self.balances[account_idx] -= interest

        # Reset integral regardless; even if interest is zero we've
        # consumed this billing cycle.
        self.loc_integral_seconds[account_idx] = 0.0
        self.loc_last_update[account_idx] = now

        return interest

    # ------------------------------------------------------------------
    # Transfers
    # ------------------------------------------------------------------

    def try_transfer(self, src: str, dst: str, amount: float) -> bool:
        return self.try_transfer_with_reason(src, dst, amount).accepted

    def try_transfer_with_reason(
        self,
        src: str,
        dst: str,
        amount: float,
        *,
        channel: str | None = None,
        timestamp: datetime | None = None,
    ) -> TransferDecision:
        amt = float(amount)
        if amt <= 0.0 or not np.isfinite(amt):
            return _DECISION_INVALID

        acct_idx = self.account_indices
        ext_idx = self.external_indices
        hub_idx = self.hub_indices
        balances = self.balances

        src_idx = acct_idx.get(src)
        dst_idx = acct_idx.get(dst)

        src_ext = src_idx is None or src_idx in ext_idx
        dst_ext = dst_idx is None or dst_idx in ext_idx

        if src_ext and dst_ext:
            return _DECISION_EXT_TO_EXT

        # Flush LOC integrals on both sides BEFORE modifying any balance
        # so the accumulated dollar-seconds captures the pre-transfer
        # negative balance over the elapsed interval.
        if timestamp is not None:
            if src_idx is not None and src_idx in self.loc_accounts:
                self.flush_loc_integral(src_idx, timestamp)
            if dst_idx is not None and dst_idx in self.loc_accounts:
                self.flush_loc_integral(dst_idx, timestamp)

        # Inbound from external into a known internal account.
        if src_ext:
            if dst_idx is None:
                return _DECISION_EXT_TO_EXT
            balances[dst_idx] += amt
            return _DECISION_ACCEPTED

        # src is internal from here onward: src_ext was False, so by
        # construction src_idx is not None and not in ext_idx.
        assert src_idx is not None

        is_hub = src_idx in hub_idx
        is_liquidity_event = channel in LIQUIDITY_EVENT_CHANNELS
        if not is_hub:
            # Self-transfers must be funded by actual cash, not protection.
            if channel == SELF_TRANSFER:
                spendable = float(balances.item(src_idx))
            else:
                spendable = self._available_liquidity(src_idx)
            # Liquidity events (overdraft fees, LOC interest) bypass the
            # check: real banks always post these even when the account
            # has no room. The customer ends up over-limit, not spared.
            if not is_liquidity_event and spendable < amt:
                return _DECISION_INSUFFICIENT

        # Detect courtesy taps: a debit that ends with negative balance
        # on a COURTESY-protected account triggers a fee. Suppressed for
        # liquidity-event channels so the fee's own debit can't recurse.
        courtesy_tapped_amount: float | None = None

        def _check_courtesy_tap(new_balance: float) -> None:
            nonlocal courtesy_tapped_amount
            if is_hub:
                return
            if channel in LIQUIDITY_EVENT_CHANNELS:
                return
            if new_balance >= 0.0:
                return
            if int(self.protection_type.item(src_idx)) != int(ProtectionType.COURTESY):
                return
            courtesy_tapped_amount = -new_balance

        # Outbound to external.
        if dst_ext:
            if not is_hub:
                balances[src_idx] -= amt
                _check_courtesy_tap(float(balances.item(src_idx)))

            if courtesy_tapped_amount is not None:
                return TransferDecision(
                    accepted=True,
                    courtesy_tapped_idx=src_idx,
                    courtesy_tapped_amount=courtesy_tapped_amount,
                )
            return _DECISION_ACCEPTED

        # Internal transfer: dst_ext was False, so dst_idx is not None.
        assert dst_idx is not None

        if not is_hub:
            balances[src_idx] -= amt
            _check_courtesy_tap(float(balances.item(src_idx)))
        balances[dst_idx] += amt

        if courtesy_tapped_amount is not None:
            return TransferDecision(
                accepted=True,
                courtesy_tapped_idx=src_idx,
                courtesy_tapped_amount=courtesy_tapped_amount,
            )
        return _DECISION_ACCEPTED


# ---------------------------------------------------------------------------
# Construction
# ---------------------------------------------------------------------------


def _apply_merchant_seed(
    rules: Rules,
    rng: Rng,
    *,
    balances: F64,
    overdrafts: F64,
    linked: F64,
    courtesy: F64,
    protection_type: npt.NDArray[np.int8],
    bank_tier: npt.NDArray[np.int8],
    overdraft_fee_amount: F64,
    account_indices: dict[str, int],
    hub_indices: set[int],
    merchant_internals: frozenset[str],
) -> None:
    """
    Overwrite personal-checking seeds for internal merchants.

    Internal merchants default to the SALARIED persona (~$1.2k balance)
    because no owner maps to them in persona_map. That's a personal
    checking balance, not a small-business one. This overlay replaces
    the merchant's initial balance with a lognormal sample centered on
    `merchant_balance_median`, zeros out personal protection products,
    and clears bank-tier fee assignment — merchants use business credit
    lines we don't model here.
    """
    if not merchant_internals:
        return

    merchant_idx_list = [
        account_indices[acct]
        for acct in merchant_internals
        if acct in account_indices and account_indices[acct] not in hub_indices
    ]
    if not merchant_idx_list:
        return

    merchant_idx = np.asarray(merchant_idx_list, dtype=np.int64)
    seeded = lognormal_by_median(
        rng.gen,
        median=float(rules.merchant_balance_median),
        sigma=float(rules.merchant_balance_sigma),
        size=int(merchant_idx.size),
    )
    balances[merchant_idx] = np.asarray(seeded, dtype=np.float64)

    # Merchants carry no personal protection products.
    overdrafts[merchant_idx] = 0.0
    linked[merchant_idx] = 0.0
    courtesy[merchant_idx] = 0.0
    protection_type[merchant_idx] = int(ProtectionType.NONE)

    # Merchants never emit personal overdraft fees.
    bank_tier[merchant_idx] = int(BankTier.ZERO_FEE)
    overdraft_fee_amount[merchant_idx] = 0.0


def initialize(
    rules: Rules,
    rng: Rng,
    params: SetupParams,
) -> ClearingHouse:
    """Bootstrap a new ledger with seeded balances and single-product protection."""

    num_accounts = len(params.accounts)

    balances = _sample_balances(
        rules,
        rng,
        num_accounts=num_accounts,
        persona_mapping=params.persona_mapping,
        persona_names=params.persona_names,
    )

    if params.hub_indices:
        hub_idx = np.asarray(list(params.hub_indices), dtype=np.int64)
        balances[hub_idx] = 1e18

    # Exclude hubs, merchants, and credit cards from personal protection
    # and from fee-bearing bank-tier assignment.
    excluded_idx: set[int] = set(params.hub_indices)
    for acct in params.merchant_internals:
        idx = params.account_indices.get(acct)
        if idx is not None:
            excluded_idx.add(idx)
    for acct in params.card_internals:
        idx = params.account_indices.get(acct)
        if idx is not None:
            excluded_idx.add(idx)

    protection_type, overdrafts, linked, courtesy, loc_apr, loc_accounts = (
        _assign_protections(
            rules,
            rng,
            num_accounts=num_accounts,
            persona_mapping=params.persona_mapping,
            persona_names=params.persona_names,
            excluded_idx=excluded_idx,
        )
    )

    # Bank-tier and per-account fee sampling. Done before the merchant
    # overlay so the overlay can zero out merchant assignments cleanly.
    bank_tier = _assign_bank_tiers(
        rules,
        rng,
        num_accounts=num_accounts,
        excluded_idx=excluded_idx,
    )
    overdraft_fee_amount = _sample_overdraft_fees(
        rules,
        rng,
        num_accounts=num_accounts,
        bank_tier=bank_tier,
    )

    # Apply merchant business-checking overlay after protection and
    # tier assignment.
    _apply_merchant_seed(
        rules,
        rng,
        balances=balances,
        overdrafts=overdrafts,
        linked=linked,
        courtesy=courtesy,
        protection_type=protection_type,
        bank_tier=bank_tier,
        overdraft_fee_amount=overdraft_fee_amount,
        account_indices=params.account_indices,
        hub_indices=params.hub_indices,
        merchant_internals=params.merchant_internals,
    )

    # Monthly billing day per LOC account, drawn once at setup.
    loc_billing_day = _sample_loc_billing_days(rng, loc_accounts)

    # Pre-initialize integral bookkeeping for LOC accounts.
    loc_integral_seconds: dict[int, float] = {idx: 0.0 for idx in loc_accounts}

    external_indices = {
        idx for acct, idx in params.account_indices.items() if is_external(acct)
    }

    return ClearingHouse(
        balances=balances,
        overdrafts=overdrafts,
        linked_buffers=linked,
        courtesy_buffers=courtesy,
        account_indices=params.account_indices,
        hub_indices=params.hub_indices,
        external_indices=external_indices,
        protection_type=protection_type,
        bank_tier=bank_tier,
        overdraft_fee_amount=overdraft_fee_amount,
        loc_accounts=loc_accounts,
        loc_apr=loc_apr,
        loc_billing_day=loc_billing_day,
        loc_integral_seconds=loc_integral_seconds,
        loc_last_update={},
    )
