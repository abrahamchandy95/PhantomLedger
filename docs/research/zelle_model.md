# Research basis for the temporal Zelle corpus

Research checked 17 September 2026. The implemented scenario uses **2024 consumer behavior** and a published bank sending-limit profile. It is not an estimate that every account-to-account transfer uses Zelle, nor a reconstruction of every bank's historical rules.

## Evidence and parameters

| Evidence | Denominator / scope | Use in generation |
|---|---|---|
| 30.3278% reported Zelle use in 2024; 33.2103% in 2025 | All consumers; preceding 12 months, Atlanta Fed table 1 | Context only; not an account enrollment rate or payment share. |
| **31.51632894%** in 2024 | Banked respondents with an observed Zelle response and national individual weight | Probability that a synthetic consumer profile belongs to the sending-user cohort. |
| **55.62245487%** in 2024 | Electronic P2P payments by banked past-year Zelle users, national day-of-week weights | Conditional rail choice for an eligible payment by a sending-user profile, before limits. |
| 151 million enrolled consumer and small-business accounts; 3.6 billion payments in 2024 | Network accounts and annual payments, respectively | Scale context; not unique people and not used as a population adoption ratio. |
| Small businesses sent or received over 500 million payments in 2024 | Business-involved network payments | Supports business participation; does not identify a business adoption probability. |
| Consumer: **$3,500 / rolling 24 hours**, **$20,000 / rolling 30 days**; business: **$15,000 / 24 hours**, **$60,000 / 30 days** | Wells Fargo published maximum sending limits, accessed September 2026 | One explicit synthetic bank-policy profile, for internal and external senders. Actual bank limits can vary. |

Sources: [Atlanta Fed survey and public data](https://www.atlantafed.org/research-and-data/surveys/survey-and-diary-of-consumer-payment-choice), [2025 tables with historical series](https://www.atlantafed.org/-/media/Project/Atlanta/FRBA/Documents/banking/consumer-payments/survey-diary-consumer-payment-choice/2025/dcpc2025-tables-for-web.xlsx), [Zelle 2024 results](https://www.zelle.com/press-releases/zelle-shatters-records-1-trillion-sent-single-year), [Wells Fargo limits and eligibility](https://www.wellsfargo.com/help/online-banking/zelle-faqs/).

## Reproduce the consumer estimates

Download the [2024 public ZIP](https://www.atlantafed.org/-/media/Project/Atlanta/FRBA/Documents/banking/consumer-payments/survey-diary-consumer-payment-choice/2024/2024-diary-of-consumer-payment-choice.zip), then run:

```sh
python3 docs/research/zelle_2024.py /path/to/2024-diary-of-consumer-payment-choice.zip
```

The script uses Python's standard library. [zelle_2024_calibration.json](zelle_2024_calibration.json) records the source checksum, filters, weighted sums and estimates; it contains no respondent records.

For sending-user propensity, filter individual records to `bnk_acnt_adopt=1`, a 0/1 `zelle_adopt` response, and nonmissing `ind_weight`. Compute the weighted mean of `zelle_adopt`: **1,482 positive responses among 4,968 respondents**. The question concerns use to purchase or pay someone in the preceding year. It does not measure recipient-only enrollment.

For payment choice, join transaction records to individual records by `id` and day records by `(id,diary_day)`. Keep actual payments on diary days 1–3, banked past-year Zelle users, `p2p_type` 1–4, `pi` in 6/7/10/11, and nonmissing national `dow_weight`. These instruments are bank-account-number payment, online bill payment, mobile payment apps and account-to-account transfer. Compute the weighted share with `mobile_app=2` (Zelle): **98 of 186 recorded payments**. Exclude day 0 and the supplemental sample without national weights. These are clustered survey observations, not 186 independent people; the estimate is a synthetic baseline, not a precision forecast or a fraud-specific estimate.

Do not use the published table's “mobile apps” instrument share as Zelle share: that table's app category covers stored app balances, while the transaction questionnaire separately records the chosen app. Some respondents report inconsistent Zelle funding methods; the generator retains neither those funding codes nor an app balance.

## Model contract and explicit assumptions

1. A seed-keyed, stable draw chooses sending-user profiles. Accounts with the same known consumer owner share this draw and the rolling sending budget. Business profiles are separate. Ownerless external accounts represent separate unobserved customer profiles; no Party ownership is fabricated.
2. A second draw chooses Zelle for an eligible electronic P2P opportunity. It uses seed, endpoints, timestamp and event ordinal. Fraud verdicts, ring IDs and future events never enter the draw. P2P, family gifts/support, P2P rent and comparable simulated fraud flows share this rule. Cash, cards, checks, self-transfers, explicitly tagged ACH and unsupported purposes are excluded. Payroll is not relabeled as Zelle.
3. Eligible product roles are consumer deposit, family, business and landlord accounts. Credit, brokerage, processors, platforms, employers and undifferentiated corporate clients are excluded. A landlord is treated as a small-business profile. Business sending propensity and payment choice use the **consumer estimates as an explicit proxy**, because the cited network totals do not supply business-population denominators. This is not a measured business adoption rate.
4. **External means another bank**, not a foreign bank or a nonparticipant. This scenario assumes eligible ownerless family/business/landlord accounts are domestic accounts at participating banks. Known non-US owner residency is conservatively excluded because there is no account-jurisdiction field; residency is an imperfect proxy, not Zelle's legal eligibility test. A production adapter needs actual account jurisdiction and bank/product eligibility. The dataset does not claim a measured interbank share or model foreign remittances as Zelle.
5. A recipient need not be in the past-year sending cohort. When an eligible settled payment chooses Zelle, both endpoints receive distinct opaque network handles and token bindings at first observed use. Registration is visible before the payment in the common sequence domain. This models enrollment completed by settlement, including recipient-only users. It does not simulate pending invitations, a 14-day wait, expired invitations or later recipient conversion into active senders. [The recipient enrollment process](https://www.wellsfargo.com/help/online-banking/zelle-faqs/) explains the distinction.
6. The same published maximum-dollar profile applies to all modeled banks. Amounts must be present and positive and fit both rolling windows; sums are evaluated in cents. A receiving account does not spend its sending budget. Over-limit candidates remain ordinary payments with their original amount and time; there is no hidden split, retry or altered ledger balance. Earlier accepted Zelle sends are the only budget history. Pre-window spending is unknown, so the first 30 days are a budget warm-up. Dynamic risk limits, special exceptions and real bank-specific policy histories are not modeled.
7. Existing ledger amounts, timings, counterparty mix and fraud incidence remain intact. Unidentified non-Zelle rails are `unknown`, rather than invented ACH/wire/RTP shares. Tagged ACH, cash, card, check and internal activity retain their broad rail. Zelle interbank settlement can itself use a clearing rail; it must not create a second payment vertex. [Zelle small-business product brief](https://www.zelle.com/sites/default/files/2025-12/02_Zelle%20SMB%20Product%20Brief%20Q425.pdf).
8. Probabilities are pinned to this research scenario across supported simulation dates. They do not automatically change by year. The temporal use case requires a start year of at least 2019, the first year in the survey's Zelle series. Current sending caps are explicit scenario constraints, not asserted 2024 or 2019 rules. The standalone Zelle app ceased transfers in March 2025; this scenario uses bank-integrated participation throughout. [Zelle's app transition](https://www.zelle.com/blog/were-evolving-how-consumers-send-money-zelle).

The observed Zelle share will depend on the ledger's eligible opportunities, sending-user activity, counterparties and limits. It is not forced to 31.52%, 55.62%, or their product across all transfers. Population demographics, sender activity correlation and network homophily are not fitted by this model. Amount distributions are inherited from the simulator, not calibrated to the network's average payment. These limitations should remain visible with the dataset.
