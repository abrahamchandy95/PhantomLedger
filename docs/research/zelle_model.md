# Research basis for the temporal Zelle corpus

Checked 17 September 2026. Models 2024 consumer behavior and one published bank's sending limits. Not a claim that every account-to-account transfer uses Zelle, nor each bank's historical rules.

## Evidence and parameters

| Evidence | Denominator or scope | Use in generation |
|---|---|---|
| 30.3278% used Zelle in 2024; 33.2103% in 2025 | All consumers, past 12 months (Atlanta Fed table 1) | Context only; not enrollment or payment share |
| 31.51632894% in 2024 | Banked respondents with a Zelle response and national individual weight | Probability a consumer profile is in the sending-user cohort |
| 55.62245487% in 2024 | Electronic P2P payments by banked past-year Zelle users, national day-of-week weights | Zelle choice for an eligible payment by a sending user, before limits |
| 151 million enrolled consumer and small-business accounts; 3.6 billion payments in 2024 | Network accounts; annual payments | Scale context; not people, not an adoption ratio |
| Over 500 million small-business payments sent or received in 2024 | Business-involved payments | Supports business participation; no adoption probability |
| Consumer $3,500 / rolling 24 hours, $20,000 / rolling 30 days; business $15,000 / 24 hours, $60,000 / 30 days | Wells Fargo published maximum sending limits, accessed September 2026 | One synthetic bank policy for internal and external senders; real limits vary |

Sources: [Atlanta Fed survey and public data](https://www.atlantafed.org/research-and-data/surveys/survey-and-diary-of-consumer-payment-choice), [2025 tables with historical series](https://www.atlantafed.org/-/media/Project/Atlanta/FRBA/Documents/banking/consumer-payments/survey-diary-consumer-payment-choice/2025/dcpc2025-tables-for-web.xlsx), [Zelle 2024 results](https://www.zelle.com/press-releases/zelle-shatters-records-1-trillion-sent-single-year), [Wells Fargo limits and eligibility](https://www.wellsfargo.com/help/online-banking/zelle-faqs/).

## Reproduce the consumer estimates

Download the [2024 public ZIP](https://www.atlantafed.org/-/media/Project/Atlanta/FRBA/Documents/banking/consumer-payments/survey-diary-consumer-payment-choice/2024/2024-diary-of-consumer-payment-choice.zip) and run (standard library only):

```sh
python3 docs/research/zelle_2024.py /path/to/2024-diary-of-consumer-payment-choice.zip
```

[zelle_2024_calibration.json](zelle_2024_calibration.json) holds the source checksum, filters, weighted sums and estimates, no respondent records.

- Sending-user propensity: weighted mean of `zelle_adopt` over individuals with `bnk_acnt_adopt=1`, a 0/1 `zelle_adopt` and nonmissing `ind_weight`: 1,482 of 4,968. It measures past-year use to purchase or pay someone, not recipient-only enrollment.
- Payment choice: transactions joined to individuals on `id` and to days on `(id,diary_day)`; actual payments on diary days 1 to 3 (not day 0, not the supplemental sample lacking national weights) by banked past-year Zelle users, with `p2p_type` 1 to 4, `pi` in 6/7/10/11 (bank-account-number payment, online bill payment, mobile payment apps, account-to-account transfer) and nonmissing national `dow_weight`. Weighted share with `mobile_app=2` (Zelle): 98 of 186. These are clustered survey observations, not 186 people: a synthetic baseline, not a precise or fraud-specific estimate.
- The published "mobile apps" instrument share is not the Zelle share: it counts stored app balances, while the questionnaire records the chosen app separately. Some respondents report inconsistent Zelle funding methods; the generator has neither funding codes nor app balances.

## Model contract and explicit assumptions

1. Sending users. A seed-keyed stable draw per profile. A known consumer's accounts share the draw and the rolling budget; business profiles are separate. Ownerless external accounts are separate unobserved profiles; no Party is invented.
2. Rail choice. A second draw, keyed on seed, endpoints, timestamp and event ordinal, never on fraud verdicts, ring IDs or future events. It covers P2P, family gifts and support, P2P rent and comparable simulated fraud flows; not cash, cards, checks, self-transfers, tagged ACH, unsupported purposes or payroll.
3. Eligibility. Consumer deposit, family, business and landlord (a small-business profile) accounts; not credit, brokerage, processors, platforms, employers or undifferentiated corporate clients. Businesses reuse the consumer estimates as an explicit proxy, since the network totals give no business-population denominators; not a measured business rate.
4. External means another bank, not a foreign bank or a nonparticipant. Eligible ownerless family, business and landlord accounts are assumed domestic at participating banks. Known non-US owner residency is excluded, conservatively: there is no account-jurisdiction field, and residency only approximates Zelle's legal eligibility. Production needs real account jurisdiction and bank and product eligibility. No measured interbank share is claimed; foreign remittances are not Zelle.
5. Recipients need not be senders. A Zelle payment gives both endpoints distinct opaque handles and token bindings at first observed use, sequenced before the payment: enrollment complete by settlement, recipient-only users included. No pending invitations, 14-day wait, expiry or later conversion to sender ([recipient enrollment](https://www.wellsfargo.com/help/online-banking/zelle-faqs/)).
6. Limits. One profile for all modeled banks. Amounts must be present, positive and within both rolling windows, summed in cents. Receiving spends no budget. Over-limit payments stay ordinary, at original amount and time: no hidden split, retry or balance change. Budget history is earlier accepted Zelle sends only; pre-window spending is unknown, so the first 30 days are a warm-up. No dynamic risk limits, exceptions or real bank policy histories.
7. Ledger untouched. Amounts, timings, counterparty mix and fraud incidence stay. Unidentified non-Zelle rails are `unknown`, not invented ACH, wire or RTP shares; tagged ACH, cash, card, check and internal activity keep their broad rail. Interbank clearing never adds a second payment vertex ([Zelle small-business product brief](https://www.zelle.com/sites/default/files/2025-12/02_Zelle%20SMB%20Product%20Brief%20Q425.pdf)).
8. Fixed in time. Probabilities do not vary by year. The start year must be 2019 or later, the survey's first Zelle year. Caps are scenario constraints, not asserted 2024 or 2019 rules. Participation is bank-integrated throughout, though the standalone Zelle app stopped transfers in March 2025 ([Zelle's app transition](https://www.zelle.com/blog/were-evolving-how-consumers-send-money-zelle)).

The observed Zelle share follows eligible opportunities, sender activity, counterparties and limits; it is not forced to 31.52%, 55.62% or their product. Not fitted: demographics, sender activity correlation, network homophily, amounts (the simulator's, not the network's average payment). Keep these limitations with the dataset.
