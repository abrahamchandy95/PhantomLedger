#!/usr/bin/env python3
"""Reproduce Zelle calibration from the Atlanta Fed 2024 public ZIP (stdlib only).
Usage: python3 docs/research/zelle_2024.py /path/to/2024-diary-of-consumer-payment-choice.zip
Prints aggregate JSON; never copies respondent records into the repository.
"""
import csv
import hashlib
import io
import json
import math
import sys
import zipfile
from pathlib import Path

SOURCE = 'https://www.atlantafed.org/-/media/Project/Atlanta/FRBA/Documents/banking/consumer-payments/survey-diary-consumer-payment-choice/2024/2024-diary-of-consumer-payment-choice.zip'

def number(row, key):
    try:
        return float(row[key])
    except (ValueError, KeyError):
        return math.nan

def estimate(rows, weight, positive):
    denominator = sum(number(r, weight) for r in rows)
    numerator = sum(number(r, weight) for r in rows if positive(r))
    return dict(n=len(rows), positive_n=sum(positive(r) for r in rows),
                weighted_numerator=numerator, weighted_denominator=denominator,
                probability=numerator / denominator)

archive = Path(sys.argv[1])
with zipfile.ZipFile(archive) as z:
    def read(level):
        with z.open(f'zip content/dcpc_2024_{level}level_public.csv') as f:
            return list(csv.DictReader(io.TextIOWrapper(f)))
    people, days, transactions = read('ind'), read('day'), read('tran')
people_by_id = {r['id']: r for r in people}
days_by_id = {(r['id'], r['diary_day']): r for r in days}
assert len(days_by_id) == len(days)
banked = [r for r in people if number(r, 'bnk_acnt_adopt') == 1
          and number(r, 'zelle_adopt') in (0, 1)
          and math.isfinite(number(r, 'ind_weight'))]
payments = []
for t in transactions:
    if number(t, 'payment') != 1 or number(t, 'diary_day') not in (1, 2, 3):
        continue
    d = days_by_id[(t['id'], t['diary_day'])]
    p = people_by_id[t['id']]
    if not math.isfinite(number(d, 'dow_weight')):
        continue
    if number(p, 'bnk_acnt_adopt') != 1 or number(p, 'zelle_adopt') != 1:
        continue
    if number(t, 'p2p_type') not in (1, 2, 3, 4) or number(t, 'pi') not in (6, 7, 10, 11):
        continue
    payments.append(dict(t, dow_weight=d['dow_weight']))
result = dict(
    source=SOURCE, sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),
    period='October 2024 diary; adoption question refers to preceding 12 months',
    banked_past_year_zelle_senders=estimate(banked, 'ind_weight', lambda r: number(r, 'zelle_adopt') == 1),
    electronic_p2p_choice_given_banked_sender=estimate(payments, 'dow_weight', lambda r: number(r, 'mobile_app') == 2),
    definitions=dict(pi=[6, 7, 10, 11], p2p_type=[1, 2, 3, 4], mobile_app_zelle=2,
                     diary_days=[1, 2, 3], excluded='missing national weights; day 0; nonpayments; missing adoption; nonbanked'))
print(json.dumps(result, indent=2))
