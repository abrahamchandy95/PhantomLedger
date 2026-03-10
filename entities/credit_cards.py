from dataclasses import dataclass

import numpy as np

from common.math import lognormal_by_median
from common.random import Rng, SeedBank
from common.validate import (
    require_float_between,
    require_float_ge,
    require_float_gt,
    require_int_ge,
)
from entities.accounts import AccountsData


def _cc_account_id(i: int) -> str:
    return f"L{i:09d}"


@dataclass(frozen=True, slots=True)
class CreditCardIssuanceRequest:
    base_seed: int
    accounts: AccountsData
    persona_for_person: dict[str, str]


@dataclass(frozen=True, slots=True)
class CreditIssuancePolicy:
    # --- card ownership by persona ---
    card_owner_student_p: float = 0.25
    card_owner_salaried_p: float = 0.70
    card_owner_retired_p: float = 0.55
    card_owner_freelancer_p: float = 0.65
    card_owner_smallbiz_p: float = 0.80
    card_owner_hnw_p: float = 0.92

    # --- merchant purchase paid by credit (conditional on having a card) ---
    cc_share_merchant_student: float = 0.55
    cc_share_merchant_salaried: float = 0.70
    cc_share_merchant_retired: float = 0.55
    cc_share_merchant_freelancer: float = 0.65
    cc_share_merchant_smallbiz: float = 0.75
    cc_share_merchant_hnw: float = 0.80

    # --- card economics at issuance ---
    apr_median: float = 0.22
    apr_sigma: float = 0.25
    apr_min: float = 0.08
    apr_max: float = 0.36

    limit_sigma: float = 0.65
    limit_student_median: float = 800.0
    limit_salaried_median: float = 3000.0
    limit_retired_median: float = 2500.0
    limit_freelancer_median: float = 4000.0
    limit_smallbiz_median: float = 7000.0
    limit_hnw_median: float = 15000.0

    # --- billing cycle at issuance ---
    cycle_day_min: int = 1
    cycle_day_max: int = 28

    # --- autopay mode issued per card ---
    autopay_full_p: float = 0.40
    autopay_min_p: float = 0.10

    def owner_probability(self, persona: str) -> float:
        match persona:
            case "student":
                return float(self.card_owner_student_p)
            case "retired":
                return float(self.card_owner_retired_p)
            case "freelancer":
                return float(self.card_owner_freelancer_p)
            case "smallbiz":
                return float(self.card_owner_smallbiz_p)
            case "hnw":
                return float(self.card_owner_hnw_p)
            case _:
                return float(self.card_owner_salaried_p)

    def merchant_credit_share(self, persona: str) -> float:
        match persona:
            case "student":
                return float(self.cc_share_merchant_student)
            case "retired":
                return float(self.cc_share_merchant_retired)
            case "freelancer":
                return float(self.cc_share_merchant_freelancer)
            case "smallbiz":
                return float(self.cc_share_merchant_smallbiz)
            case "hnw":
                return float(self.cc_share_merchant_hnw)
            case _:
                return float(self.cc_share_merchant_salaried)

    def limit_median(self, persona: str) -> float:
        match persona:
            case "student":
                return float(self.limit_student_median)
            case "retired":
                return float(self.limit_retired_median)
            case "freelancer":
                return float(self.limit_freelancer_median)
            case "smallbiz":
                return float(self.limit_smallbiz_median)
            case "hnw":
                return float(self.limit_hnw_median)
            case _:
                return float(self.limit_salaried_median)

    def validate(self) -> None:
        probability_fields: tuple[tuple[str, float], ...] = (
            ("card_owner_student_p", self.card_owner_student_p),
            ("card_owner_salaried_p", self.card_owner_salaried_p),
            ("card_owner_retired_p", self.card_owner_retired_p),
            ("card_owner_freelancer_p", self.card_owner_freelancer_p),
            ("card_owner_smallbiz_p", self.card_owner_smallbiz_p),
            ("card_owner_hnw_p", self.card_owner_hnw_p),
            ("cc_share_merchant_student", self.cc_share_merchant_student),
            ("cc_share_merchant_salaried", self.cc_share_merchant_salaried),
            ("cc_share_merchant_retired", self.cc_share_merchant_retired),
            ("cc_share_merchant_freelancer", self.cc_share_merchant_freelancer),
            ("cc_share_merchant_smallbiz", self.cc_share_merchant_smallbiz),
            ("cc_share_merchant_hnw", self.cc_share_merchant_hnw),
            ("autopay_full_p", self.autopay_full_p),
            ("autopay_min_p", self.autopay_min_p),
        )
        for name, value in probability_fields:
            require_float_between(name, value, 0.0, 1.0)

        require_float_gt("apr_median", self.apr_median, 0.0)
        require_float_ge("apr_sigma", self.apr_sigma, 0.0)
        require_float_gt("apr_min", self.apr_min, 0.0)
        require_float_gt("apr_max", self.apr_max, 0.0)
        if float(self.apr_max) < float(self.apr_min):
            raise ValueError("apr_max must be >= apr_min")

        require_float_gt("limit_sigma", self.limit_sigma, 0.0)

        limit_fields: tuple[tuple[str, float], ...] = (
            ("limit_student_median", self.limit_student_median),
            ("limit_salaried_median", self.limit_salaried_median),
            ("limit_retired_median", self.limit_retired_median),
            ("limit_freelancer_median", self.limit_freelancer_median),
            ("limit_smallbiz_median", self.limit_smallbiz_median),
            ("limit_hnw_median", self.limit_hnw_median),
        )
        for name, value in limit_fields:
            require_float_gt(name, value, 0.0)

        require_int_ge("cycle_day_min", self.cycle_day_min, 1)
        require_int_ge("cycle_day_max", self.cycle_day_max, self.cycle_day_min)


DEFAULT_CREDIT_ISSUANCE_POLICY = CreditIssuancePolicy()


@dataclass(frozen=True, slots=True)
class CreditCardData:
    card_accounts: list[str]
    card_for_person: dict[str, str]  # person -> L...
    owner_for_card: dict[str, str]  # L... -> person

    apr_by_card: dict[str, float]
    limit_by_card: dict[str, float]
    cycle_day_by_card: dict[str, int]
    autopay_mode_by_card: dict[str, int]  # 0 none, 1 min, 2 full


@dataclass(frozen=True, slots=True)
class _IssuedCard:
    account_id: str
    owner: str
    apr: float
    limit: float
    cycle_day: int
    autopay_mode: int


def empty_credit_cards() -> CreditCardData:
    return CreditCardData([], {}, {}, {}, {}, {}, {})


def _sample_autopay_mode(
    policy: CreditIssuancePolicy,
    gen: np.random.Generator,
) -> int:
    u = float(gen.random())
    if u < float(policy.autopay_full_p):
        return 2
    if u < float(policy.autopay_full_p) + float(policy.autopay_min_p):
        return 1
    return 0


def _issue_card_for_person(
    policy: CreditIssuancePolicy,
    *,
    person_id: str,
    persona: str,
    card_number: int,
    gen: np.random.Generator,
) -> _IssuedCard | None:
    if float(gen.random()) >= float(policy.owner_probability(persona)):
        return None

    card = _cc_account_id(card_number)

    apr = lognormal_by_median(
        gen,
        median=float(policy.apr_median),
        sigma=float(policy.apr_sigma),
    )
    apr = max(float(policy.apr_min), min(float(policy.apr_max), float(apr)))

    limit = lognormal_by_median(
        gen,
        median=float(policy.limit_median(persona)),
        sigma=float(policy.limit_sigma),
    )
    limit = max(200.0, float(limit))

    cycle_day = int(
        gen.integers(int(policy.cycle_day_min), int(policy.cycle_day_max) + 1)
    )

    return _IssuedCard(
        account_id=card,
        owner=person_id,
        apr=float(apr),
        limit=float(limit),
        cycle_day=int(cycle_day),
        autopay_mode=_sample_autopay_mode(policy, gen),
    )


def attach_credit_cards(
    policy: CreditIssuancePolicy,
    rng: Rng,
    request: CreditCardIssuanceRequest,
) -> tuple[AccountsData, CreditCardData]:
    """
    Create credit card liability accounts (prefix L) and attach them to persons.

    Caller decides whether the feature is enabled.
    """
    policy.validate()

    persons = sorted(request.accounts.person_accounts)
    if not persons:
        return request.accounts, empty_credit_cards()

    seedbank = SeedBank(request.base_seed)

    new_accounts = list(request.accounts.accounts)
    new_person_accounts: dict[str, list[str]] = {
        person_id: list(acct_ids)
        for person_id, acct_ids in request.accounts.person_accounts.items()
    }
    new_acct_owner = dict(request.accounts.acct_owner)

    card_accounts: list[str] = []
    card_for_person: dict[str, str] = {}
    owner_for_card: dict[str, str] = {}

    apr_by_card: dict[str, float] = {}
    limit_by_card: dict[str, float] = {}
    cycle_day_by_card: dict[str, int] = {}
    autopay_mode_by_card: dict[str, int] = {}

    issued_count = 0

    for person_id in persons:
        persona = request.persona_for_person.get(person_id, "salaried")
        gen = seedbank.generator("cc_issue", person_id)

        issued = _issue_card_for_person(
            policy,
            person_id=person_id,
            persona=persona,
            card_number=issued_count + 1,
            gen=gen,
        )
        if issued is None:
            continue

        issued_count += 1

        new_accounts.append(issued.account_id)
        new_person_accounts.setdefault(person_id, []).append(issued.account_id)
        new_acct_owner[issued.account_id] = person_id

        card_accounts.append(issued.account_id)
        card_for_person[person_id] = issued.account_id
        owner_for_card[issued.account_id] = person_id
        apr_by_card[issued.account_id] = issued.apr
        limit_by_card[issued.account_id] = issued.limit
        cycle_day_by_card[issued.account_id] = issued.cycle_day
        autopay_mode_by_card[issued.account_id] = issued.autopay_mode

    # Preserve global RNG advancement expectations.
    _ = rng.float()

    updated_accounts = AccountsData(
        accounts=new_accounts,
        person_accounts=new_person_accounts,
        acct_owner=new_acct_owner,
        fraud_accounts=request.accounts.fraud_accounts,
        mule_accounts=request.accounts.mule_accounts,
        victim_accounts=request.accounts.victim_accounts,
    )

    cards = CreditCardData(
        card_accounts=card_accounts,
        card_for_person=card_for_person,
        owner_for_card=owner_for_card,
        apr_by_card=apr_by_card,
        limit_by_card=limit_by_card,
        cycle_day_by_card=cycle_day_by_card,
        autopay_mode_by_card=autopay_mode_by_card,
    )

    return updated_accounts, cards
