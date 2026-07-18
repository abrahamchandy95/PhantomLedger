#!/usr/bin/env python3
"""Anchored merge edit for docs/fraud_model_audit.md — retire the
boost-cycle half of the F-4 cycle-amounts row (boost-cycle-retire-2026-07).

Companion to the kFraudBoostCycle deletion in math/amounts.hpp. The
constant was defined but never wired: no fraud::Typology enumerator, no
channels::Fraud tag, no playbook phase, no sampler consumed it. Retiring
it is byte-neutral (zero consumers => zero golden movement); the doc row
keeps the historical value as an audit trail.

Safety contract (apply_c4.py pattern): every anchor is verified to occur
EXACTLY ONCE before anything is applied; on any failure the doc is left
untouched. The write is atomic (temp file + rename).

Run from anywhere:  python3 docs/apply_boost_cycle_retire.py
Delete this script after it has been applied (one-shot tool).
"""

import os
import sys
import tempfile

DOC = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "fraud_model_audit.md")

# (name, exact old substring, replacement) — anchors include the trailing
# newline so a row can never be matched mid-line.
EDITS = [
    (
        "F-4 cycle/boost-cycle amounts row",
        "| Cycle / boost-cycle amounts | LN($600, .25) / LN($500, .20) "
        "| CHOICE | — |\n",
        "| Cycle amount | LN($600, .25). The boost-cycle companion "
        "LN($500, .20) was RETIRED (boost-cycle-retire-2026-07): the "
        "constant existed in code but was never wired — no Typology "
        "enumerator, no fraud channel, no playbook phase, no sampler "
        "consumed it | CHOICE | — |\n",
    ),
]


def main() -> int:
    with open(DOC, encoding="utf-8") as fh:
        text = fh.read()

    failures = []
    for name, old, _ in EDITS:
        count = text.count(old)
        if count != 1:
            failures.append(
                f"  {name}: anchor occurs {count} time(s); need exactly 1")
    if failures:
        print("REFUSING to edit; document untouched:", file=sys.stderr)
        print("\n".join(failures), file=sys.stderr)
        return 1

    for _, old, new in EDITS:
        text = text.replace(old, new, 1)

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

    print(f"Applied {len(EDITS)} edit(s) to {DOC}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
