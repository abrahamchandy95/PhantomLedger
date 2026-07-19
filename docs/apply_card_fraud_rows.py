#!/usr/bin/env python3
"""Append the card-fraud use-case derivation rows to
docs/fraud_model_audit.md (card-fraud-2026-07).

Companion to exporter/card_fraud/{schema,derive,streaming}.hpp (rounds
T1/T2 of the card-fraud arc). Every content-keyed derivation the
exporter performs is a model value and gets a classed row here; the
geo rows land with the T3 finisher's script.

Safety contract: pure APPEND (no anchors to drift); refuses to run
twice (guards on the section heading); atomic temp+rename write.

Run from anywhere:  python3 docs/apply_card_fraud_rows.py
Delete this script after it has been applied (one-shot tool).
"""

import os
import sys
import tempfile

DOC = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "fraud_model_audit.md")

GUARD = "### U-1. card-fraud use-case view derivations"

SECTION = """

### U-1. card-fraud use-case view derivations (card-fraud-2026-07)

Export-time, content-keyed derivations for the `card-fraud` use case
(TigerGraph TF_GNN_v3 target; exporter/card_fraud/). None of these
touch the world model or any other use case's bytes: they are
deterministic functions of row/entity content (FNV-1a over fixed-width
fields, lane-salted; derive.hpp), so the corpus stream and every
existing golden are unaffected.

| Item | PL value | Class | Suggested source |
|---|---|---|---|
| Card view | channels {card_purchase, merchant}; merchant-channel (account-paid POS) rows interpreted as DEBIT-card transactions; ATO (p2p rail) excluded | CHOICE | IBM TabFormer mixes credit/debit cards |
| Card attribution | source Key in card registry -> that credit card (<=1 credit card/person); any other source -> the account's derived debit card; Card.is_fraud = card ever carried a flag-1 view row | CHOICE (label definition) | — |
| Identifier scheme | C/D/M = prefixed role.bank.number of the entity Key; P<person>; T<row_seq> (Payment_Transaction ids cross-reference the transactions table 1:1) | CHOICE | — |
| use_chip mix | Swipe .63 / Chip .26 / Online .11, content-keyed per row [Likely on the split — verify against the TabFormer "Use Chip" empirical mix at citation time] | CHOICE | IBM TabFormer (credit_card_transactions "Use Chip" column) |
| error model | incidence 2.0% of view rows; mix Insufficient Balance .40 / Bad PIN .20 / Technical Glitch .20 / Bad Card Number .08 / Bad Expiration .05 / Bad CVV .05 / Bad Zipcode .02; error-free rows carry the empty string [Likely on incidence and mix — verify against the TabFormer "Errors?" column] | CHOICE | IBM TabFormer (credit_card_transactions "Errors?" column) |
| train/val/test split | chronological .70/.15/.15 of window days (floored day boundaries); settlement-tail rows past the window end land in test | CHOICE | standard temporal GNN evaluation practice |
| Category fallback | non-catalog view destinations (the unauthorized rail draws biller accounts) become Merchant vertices with a content-keyed uniform category over the 10-category taxonomy; keyed by destination so mer_cat and Merchant_Assigned agree by construction | CHOICE | — |
| mer_cat granularity | the 10-category merchant taxonomy stands in for TabFormer's MCC codes | DEVIATES-BY-CHOICE (an MCC taxonomy would be its own model round) | — |
"""


def main() -> int:
    with open(DOC, encoding="utf-8") as fh:
        text = fh.read()

    if GUARD in text:
        print("REFUSING to edit: the card-fraud derivation section already "
              "exists; document untouched.", file=sys.stderr)
        return 1

    if not text.endswith("\n"):
        text += "\n"
    text += SECTION.lstrip("\n") if text.endswith("\n\n") else SECTION

    fd, tmp = tempfile.mkstemp(dir=os.path.dirname(DOC),
                               prefix=".fraud_model_audit.", suffix=".tmp")
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as fh:
            fh.write(text)
        os.replace(tmp, DOC)
    except BaseException:
        if os.path.exists(tmp):
            os.unlink(tmp)
        raise

    print(f"Appended the card-fraud derivation section to {DOC}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
