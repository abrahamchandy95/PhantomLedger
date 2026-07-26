#!/usr/bin/env python3
"""
merge_v2_addendum_2026_07.py — card-fraud-realism-v2 FOLLOW-UP merge.

The first merge ran before rounds 2-3 existed and before a wrong claim
was caught, so two things are stale in the repo:

  docs/fraud_model_audit.md  U-10's last row asserts that b-2 made
                             `use_chip` model-backed on the fraud rail.
                             IT DOES NOT. This appends a U-10 ADDENDUM
                             that corrects the record and adds the
                             point-in-time contract and prevalence
                             suite as BUILT.
  README.md                  says the point-in-time contract and
                             prevalence calibration still remain. Both
                             shipped; what actually remains is LEVEL
                             calibration against a named issuer series.

Atomic (temp + os.replace) and guarded: a second run is a no-op.

    python3 merge_v2_addendum_2026_07.py
    git diff -- README.md docs/fraud_model_audit.md
    rm merge_v2_addendum_2026_07.py
"""

from __future__ import annotations

import os
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
AUDIT = ROOT / "docs" / "fraud_model_audit.md"
README = ROOT / "README.md"

U10_MARKER = "### U-10."
ADDENDUM_MARKER = "#### U-10 ADDENDUM."

ADDENDUM = """
#### U-10 ADDENDUM. Rounds 2-3 as built, and one correction to this arc's own record (card-fraud-2026-07l)

Recorded after the point-in-time contract and the prevalence suite
landed. The first row CORRECTS a claim made in U-10 above.

| PL value | Class | Suggested source |
|---|---|---|
| CORRECTS U-10 — `use_chip` IS NOT CAUSAL: U-10's closing row states that b-2 made `use_chip` model-backed on the fraud rail. That is WRONG. The card-present / card-not-present modality decision drives DESTINATION SELECTION inside generation and is never exported; `derive::useChipFor` remains an FNV content hash of the row (Swipe .63 / Chip .26 / Online .11) for fraud and legitimate rows alike. It is point-in-time SAFE (deterministic in the row, carries no future) but MECHANISM-FREE. The feature contract classes it USE WITH CARE alongside `error` and `gender`. Exporting the real modality is REGISTERED | MEASUREMENT (code fact; supersedes the U-10 row) | include/phantomledger/exporter/card_fraud/derive.hpp |
| THE POINT-IN-TIME CONTRACT AS BUILT (gate 4, docs/card_fraud_feature_contract.md): every exported column is classified FEATURE-SAFE / TARGET / PROHIBITED / USE-WITH-CARE, with the splitting rules stated. Pinned by a TRUNCATION EXPERIMENT — one world exported twice through the production path, once over every settled row and once over only rows before a mid-window cutoff T — requiring every feature-safe value present in the score-time export to be BYTE-IDENTICAL in the full-window export. FOUR comparison classes: STREAM PREFIX (the three streamed transaction tables), IDENTICAL (world-derived), KEYED (first column a unique entity id with attributes after it — this is the class that catches a full-window entity label), LINE SUBSET (pure edge tables). Not a tautology: it would have FAILED on the pre-round-1 `Card.is_fraud` | INVARIANT (contract) + MEASUREMENT (executable) | tests/test_card_point_in_time.cpp |
| GATE-DESIGN LAW, learned by getting it wrong: the first version of that gate keyed EVERY growing table by its first column and reported 244 phantom "changed" rows in Party_Has_Card. Nothing had changed — a party owns a credit card plus a derived debit card per account, so the first column is not a unique key there, and `artifacts.cards` being a std::map over entity::Key means a newly-seen account inserts BEFORE an existing card and reorders that party's rows. CHOOSE THE COMPARISON BY THE TABLE'S KEY SHAPE, and make the gate SELF-CHECK it: the keyed comparison now verifies first-column uniqueness and says "classify as a LINE SUBSET table" instead of inventing a leak | INVARIANT (review law) | tests/test_card_point_in_time.cpp |
| LEAK QUANTIFIED IN THE CORPUS ITSELF: at the 300-person 365-day 1991 gate world the ground-truth overlay carries 8 ever_fraud cards at the score-time cutoff vs 12 over the full window. Those 4 cards would have rendered `Card.is_fraud = 1` on transactions that occurred BEFORE they were ever defrauded — the leak, measured, in PhantomLedger's own output rather than argued from principle | MEASUREMENT (observed) | tests/test_card_point_in_time.cpp |
| THE PREVALENCE SUITE AS BUILT (gate 3): a single aggregate fraud rate stopped being informative once H4 made activity volume era-varying, so prevalence is measured per YEAR, per CHANNEL, per TYPOLOGY, per AMOUNT and per EPISODE on the card view over four whole calendar years (1991-1994, N=300). GATED: per-year RATE stability (spread < 4x — F = pL/(1-p) rides the realized candidate count, so a fan-out means a budget stopped riding L); per-year fraud amounts flat in CALIBRATION dollars (spread < 2.5x — the only gate proving U-6 class F scaling reaches the card fraud rail); fraud rides card_purchase with merchant-POS share < 0.05; at least two typologies with the unauthorized family > 0.30; fraud tickets exceed legitimate ones; episode size bounded | MEASUREMENT (statistical tolerance) + INVARIANT | tests/test_card_prevalence.cpp |
| PRINTED, NEVER GATED (a standing law, not a concession): per-year card-view ROW COUNTS. H4's real-consumption ramp raises them across the era while the documented small-N liquidity drain (U-9 ADDENDUM, ~27% in second-year gate legs) lowers them; at N=300 the two are not separable. DO NOT GATE WHAT THE HARNESS CANNOT RESOLVE — the same reasoning that subsumed H4's recession-direction gate | CHOICE (declared) + MEASUREMENT (harness resolution) | U-9 ADDENDUM lineage |
| PREVALENCE COMPARATOR, and the honest limit of this arc: IBM TabFormer observed 28,471 fraud rows in 24,386,900 (0.11675%). PhantomLedger is TabFormer-SHAPED, not calibrated to it, so the aggregate gate is a WIDE plausibility band (0.02%-2%) with the ratio PRINTED rather than pinned. The arc closed SEPARABILITY and STABILITY; it did not calibrate the LEVEL. Calibrating card-fraud prevalence and the CNP share against a named issuer-side series (Nilson / FTC) is REGISTERED and is what still stands between this corpus and a published benchmark claim | MEASUREMENT (named comparator) + CHOICE (declared scope limit) | IBM TabFormer released artifact |
| ROUND-1 RE-PIN AS OBSERVED: only the card_fraud golden section diverged; exactly four vertex tables changed (cf_Card, cf_Party, cf_Device, cf_IP) with IDENTICAL row counts, plus the new cf_Ground_Truth_Label at 114 rows; the standard and fraud sections stayed digest-identical and the STREAM golden held at 197,199 rows. Exporter-only, exactly as declared — the byte-identity harness confirmed the classification instead of being told it | MEASUREMENT (observed) | tests/test_table_golden.cpp |
"""

README_EDITS: list[tuple[str, str]] = [
    (
        "merchant-ID-only baseline is now gated in the suite "
        "(`test_card_baselines`).\nWhat remains before an online-benchmark "
        "claim is the point-in-time feature\ncontract and per-year "
        "prevalence calibration; follow the causal feature, split,\nmetric, "
        "and minimum-realism gates in",
        "merchant-ID-only baseline is gated in the suite "
        "(`test_card_baselines`), which\ncolumns a model may read is a "
        "written and tested contract\n([docs/card_fraud_feature_contract.md]"
        "(docs/card_fraud_feature_contract.md),\npinned by a truncation "
        "experiment in `test_card_point_in_time`), and per-year\nprevalence, "
        "channel, typology, amount and episode bands are gated by\n"
        "`test_card_prevalence`. What remains before an online-benchmark "
        "claim is\ncalibrating the fraud LEVEL against a named issuer-side "
        "series; follow the\ncausal feature, split, metric, and "
        "minimum-realism gates in",
    ),
]


def fail(message: str) -> None:
    print(f"REFUSED: {message}", file=sys.stderr)
    raise SystemExit(1)


def write_atomic(path: Path, text: str) -> None:
    handle = tempfile.NamedTemporaryFile(
        "w", encoding="utf-8", dir=str(path.parent), delete=False
    )
    try:
        handle.write(text)
        handle.flush()
        os.fsync(handle.fileno())
    finally:
        handle.close()
    os.replace(handle.name, path)


def merge_audit() -> str:
    text = AUDIT.read_text(encoding="utf-8")
    if ADDENDUM_MARKER in text:
        return "audit: U-10 ADDENDUM already present, skipped"
    if U10_MARKER not in text:
        fail(f"{AUDIT} does not contain {U10_MARKER!r} — run the first "
             f"merge script before this one")
    write_atomic(AUDIT, text.rstrip("\n") + "\n" + ADDENDUM)
    return "audit: U-10 ADDENDUM appended"


def merge_readme() -> str:
    text = README.read_text(encoding="utf-8")
    applied = 0
    skipped = 0
    for index, (old, new) in enumerate(README_EDITS, start=1):
        if new in text and old not in text:
            skipped += 1
            continue
        count = text.count(old)
        if count != 1:
            fail(
                f"README edit {index}: anchor found {count} times "
                f"(expected exactly 1). Anchor starts: {old[:70]!r}"
            )
        text = text.replace(old, new, 1)
        applied += 1
    if applied:
        write_atomic(README, text)
    return f"README: {applied} edit(s) applied, {skipped} already present"


def main() -> None:
    for path in (AUDIT, README):
        if not path.is_file():
            fail(f"missing {path}")
    print(merge_audit())
    print(merge_readme())
    print("done — verify the diff, then delete this script")


if __name__ == "__main__":
    main()
