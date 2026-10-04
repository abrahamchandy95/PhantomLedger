# Counterparty hubs in the mule-temporal corpus: measured, researched, proposed

Research note, 25 September 2026, on the pre-change corpus. All six
[proposed changes](#proposed-changes) have since shipped.

MulePatternLearner (MPL)'s per-account context query aborted on accounts with
over 2,048 visible payments. By the end of 2024 about 5,600 accounts cross
that line, the largest with 3.8 million payments, and about 95% of accounts
pay at least one. Is this a generation problem, what is real, what to change?

Partly. Most hubs are realistic in kind (large card merchants, payroll,
Social Security, the IRS), so MPL must handle hubs regardless. The seven
largest hubs and the ATM terminals are routing artifacts and make the tail
extreme.

## What was measured

The TigerGraph graph of the `--usecase mule-temporal --population 200000
--days 366 --seed 42` run (snapshot
`phantomledger_2024_seed42_snapshot_20260919`), read-only interpreted queries,
25 September 2026: 317,840 internal deposit, 434,783 internal card and 35,660
external accounts. No internal account exceeds 1,967 payments; every hub is
external.

Hubs at the 2025-01-01 cutoff (MPL hub registry, full visibility):

| Hub class | Hubs | Notes |
|---|---:|---|
| Card merchants | 4,384 | Catalogue merchants; the largest takes 341,126 payments |
| Payers only (employers, SSA, clients) | 566 | About 480 employers at 38,000 to 41,000 payroll credits each (unreconciled; see change 5) |
| Other external deposit payees | 392 | 160 subscription billers at about 47,000 each, the IRS, landlords |
| Cash points (ATMs, depositories) | 227 | Busiest four ATMs about 250,000 withdrawals each |
| Unknown-type digital payees | 25 | Includes the catch-all below |
| Population-wide singletons | 5 | Catch-all, servicer, insurers, auto lender |
| Bank posting sinks | 3 | Card issuer, fee collection, overdraft line |

In a 1/256 hash sample of 2,897 internal accounts, 94.3% of deposit and 99.9%
of card accounts pay at least one card-merchant hub; removing the artifacts
below would not change that.

### The artifacts, largest first

| Account | Payments | Mean | Distinct payers | Cause in source |
|---|---:|---:|---:|---|
| External-unknown catch-all (`XM00000001`) | 3,814,933 | $220 | 196,989 deposit (62%) | `PaymentRouter::emitExternal` in `src/activity/spending/routing/payments.cpp` sends the 5% "external unknown" spending share, P2P with no usable contact and funeral payments to one key, which also collides with catalogue merchant 1 |
| Card issuer | 2,316,826 | $37 | 316,051 card accounts (73%) | `Session::accrueInterest` and `Session::postLateFee` in `src/transfers/channels/credit_cards/session.cpp` book interest and late fees as payments to `issuerAccount` |
| Insurer (premiums in, 6,866 claims out at $7,923) | 1,808,916 | $288 | 165,812 | One `Insurance::autoCarrier` key for everyone (`src/synth/products/terms/insurance.cpp`) |
| Student-loan servicer, also receiving every mortgage | 1,430,365 | $1,282 | 130,524 | "SUSPECTED DEFECT" in `src/synth/products/terms/mortgage.cpp`: mortgages route to `Lending::studentServicer` |
| Life insurer | 1,074,202 | $37 | 93,238 | One `Insurance::lifeCarrier` key |
| Auto lender | 714,971 | $669 | 75,215 | One `Lending::autoLoan` key |
| Bank fee collection | 361,991 | $41 | 47,237 | `bankFeeCollectionKey()` in `src/transfers/legit/ledger/posting.cpp` |
| Busiest ATM terminals (4) | about 250,000 each | $125 | about 25,000 each | `buildNearbyPoints` in `src/transfers/legit/blueprints/plans.cpp`: residents sit at the city centroid, so terminals tie on distance and the index tie-break gives everyone the same four |
| Overdraft line of credit | 113,386 | $65 | 14,656 | `bankOdLocKey()` in the same file |

Also uniform: payroll over about 480 employers (about 1,500 employees each),
rent over 240 landlords (about 320 tenant accounts each), subscriptions over
160 billers, and about 4.7 payments per paying account a year at every card
merchant, whatever its size or category.

## What reality looks like

`[P]` primary document read; `[S]` only a secondary summary or snippet seen.

### Fees and interest have no counterparty

- ISO 20022 Account Management (ACMT) is "operations on one account" between
  institution and owner, with charges (CHRG), interest (INTR) and fees (FEES)
  sub-families and card codes PMNT/CCRD/CHRG and PMNT/CCRD/INTR
  ([ISO 20022 External Code Sets, October 2023](https://www.iso20022.org/sites/default/files/media/file/BTC_ExternalCodeListDescription_October2023.doc)) `[P]`;
  Danske Bank maps "Bank's fee" and "Interest debit" to them
  ([appendix v1.3, 2023](https://danskeci.com/-/media/pdf/danskeci-com/iso-20022-xml/banktransactioncode_appendix.pdf)) `[P]`.
- camt.053 RelatedParties is optional; the Dutch guide fills debtor and
  creditor only for credit transfers and direct debits
  ([Betaalvereniging IG camt.053 v1.1](https://www.betaalvereniging.nl/wp-content/uploads/2026/03/IG-Bank-to-Customer-Statement-CAMT-053-v1-1.pdf)) `[P]`.
- A service charge debits the customer and credits the service-charge income
  GL ([Oracle FLEXCUBE Accounting Entries, 2021](https://docs.oracle.com/cd/F44734_01/PDF/UserManual/Accounting%20Entries%20User%20Manual.pdf)) `[P]`;
  card interest and fees are call-report income lines
  ([FFIEC 051 Schedule RI instructions](https://www.fdic.gov/system/files/2024-08/2021-12-051-ri.pdf)) `[P]`.
- Aggregators treat them as categories, not merchant payments (Plaid
  `BANK_FEES_INTEREST_CHARGE`, `OVERDRAFT_FEES`; MX "Fees & Charges";
  [Plaid taxonomy](https://plaid.com/documents/transactions-personal-finance-category-taxonomy.csv),
  [MX categories](https://docs.mx.com/api-reference/platform-api/reference/categories)) `[P]`.
- Published bank transaction graphs have customer and external counterparty
  nodes, no bank or GL node (DNB: [Johannessen and Jullum, 2023](https://arxiv.org/pdf/2307.13499);
  Rabobank: [Bonato and Szava, 2025](https://arxiv.org/pdf/2509.10715)) `[P]`.
  An IBM patent removes "super nodes" such as utilities before graph analysis
  ([US12093245B2](https://patents.google.com/patent/US12093245B2/en)) `[P]`.
- Volume: about one late fee per general-purpose card account a year and 48%
  of active accounts revolving give about 6 to 7 postings per active card a
  year ([CFPB Credit Card Market Report, 2023](https://files.consumerfinance.gov/f/documents/cfpb_consumer-credit-card-market-report_2023.pdf)) `[P]`;
  23% of banked consumers paid an overdraft fee in a year, heavily
  concentrated ([CFPB, December 2023](https://files.consumerfinance.gov/f/documents/cfpb_overdraft-nsf-report_2023-12.pdf)) `[P]`.
  The generator's counts are plausible; the shared counterparty is not.

### Unidentified counterparties are null, not one account

Enrichment vendors match about 90% to 97% of transactions (Spade "at least
95%", [docs](https://docs.spade.com/reference/understanding-enriched-data) `[P]`;
Ntropy 96.75% on hard cases, [2024](https://www.ntropy.com/blog/better-loyalty-us) `[P]`);
the rest keep a raw descriptor and a null merchant (MX `merchant_guid` null,
[docs](https://docs.mx.com/api-reference/more-apis/data-enhancement-off-platform/) `[P]`).
A 5% unidentified share is realistic; one account receiving all of it is not.
Funeral payments go to identifiable funeral homes.

### Lenders and insurers are many firms, not one

| Category | Real concentration | Holders | Typical payment |
|---|---|---|---|
| Mortgage servicers | Largest 7.3%, top 10 54.1% of Agency UPB, Q4 2023 ([FSOC 2024](https://home.treasury.gov/system/files/261/FSOC-2024-Nonbank-Mortgage-Servicing-Report.pdf) `[P]`) | 59.7% of owned homes have a mortgage ([ACS 2024](https://www.census.gov/newsroom/press-releases/2025/acs-1-year-estimates.html) `[P]`) | Median $1,520 ([FHFA NMDB, Q1 2024](https://www.fhfa.gov/news/news-release/fhfa-releases-data-visualization-dashboard-for-nmdb-outstanding-residential-mortgage-statistics) `[P]`) |
| Federal student servicers | Five servicers; Nelnet about 31% of 45M borrowers ([Nelnet 10-K 2024](https://www.sec.gov/Archives/edgar/data/1258602/000125860225000014/nni-20241231.htm) `[P]`) | 17% of adults owe, 57% of them required to pay in 2024 ([Fed SHED](https://www.federalreserve.gov/publications/2025-economic-well-being-of-us-households-in-2024-higher-education-and-student-loans.htm) `[P]`) | About $203 ([Experian](https://www.experian.com/blogs/ask-experian/research/average-student-loan-payments/) `[P]`) |
| Auto lenders | Largest about 6%; captives 31%, banks 25%, credit unions 20% ([Experian 2025](https://www.experianplc.com/newsroom/press-releases/2025/banks-experience-market-share-rebound-for-new-and-used-vehicle-f) `[P]`) | 34.7% of families ([SCF 2022](https://www.federalreserve.gov/publications/files/scf23.pdf) `[P]`) | New $746, used $528, Q4 2024 (Experian `[P]`) |
| Auto insurers | State Farm 18.9%, Progressive 16.7%, top 10 76% ([NAIC](https://content.naic.org/sites/default/files/research-actuarial-property-casualty-market-share.pdf) `[P]`) | About 85% of motorists insured | $1,282 per vehicle per year ([NAIC 2023](https://content.naic.org/article/naic-releases-2023-auto-insurance-database-average-premium-supplement) `[P]`) |
| Home insurers | State Farm 18.7%, top 10 62.5% (NAIC `[P]`) | About 80% of mortgages escrow insurance | $1,569 per year (Triple-I `[S]`) |
| Life insurers | Largest 8.7%, top 10 45.8% ([NAIC](https://content.naic.org/sites/default/files/publication-msr-lb-life-fraternal.pdf) `[P]`) | 51% own any life insurance (LIMRA `[S]`) | |
| Card issuers | Chase 21.9%, top 10 82.5% of purchase volume ([Nilson 2025](https://www.globenewswire.com/news-release/2025/03/06/3038338/0/en/jp-morgan-tops-nilson-report-ranking-of-us-credit-card-issuers.html) `[P]`) | 78% of adults ([CFPB 2025](https://files.consumerfinance.gov/f/documents/cfpb_consumer-credit-card-market-report_2025.pdf) `[P]`) | |
| Landlords | Individuals own 70.2% of rental properties; 85.6% of properties are single-unit (RHFS 2021, [Census/HUD](https://www.census.gov/content/dam/Census/library/visualizations/2021/econ/2021-RHFS-Infographic-tagged.pdf) `[P]`) | About 35% of households rent | Median gross rent $1,487 (ACS 2024 `[P]`) |
| Employers | 30.6% of workers at firms with 10,000+ employees, 16.2% at firms under 20 ([SUSB 2022](https://www2.census.gov/programs-surveys/susb/tables/2022/us_naicssector_large_emplsize_2022.xlsx) `[P]`) | | |

Auto claims: about 4.2% collision and 4.0% comprehensive per insured vehicle
year, at $5,489 and $2,306 (ISO via Triple-I `[S]`), many paid to body shops
or lienholders rather than the customer.

Social Security and the IRS really are single ACH originators: Treasury uses
fixed descriptors ("SOC SEC", "TAX REF") and company name "IRS TREAS 310"
([Fiscal Service Green Book](https://fiscal.treasury.gov/files/reference-guidance/green-book/greenbook-full.pdf),
[refund FAQ](https://fiscal.treasury.gov/payments-from-government/direct-deposit/faq-tax-refund)) `[P]`.
Those two singletons are correct.

### ATMs are far less busy

- 3.4 billion US ATM withdrawals in 2024, average $210
  ([Federal Reserve Payments Study](https://www.federalreserve.gov/paymentsystems/frps_cy2015_24_topline.htm) `[P]`),
  over 520,000 to 540,000 ATMs (ATMIA `[S]`): about 6,400 per ATM a year, 18
  a day.
- Cardtronics: 757 withdrawals per ATM a month in 2019
  ([10-K](https://www.sec.gov/Archives/edgar/data/1671013/000167101320000008/catm-201901231x10k.htm) `[P]`).
- Bank of America: 14,893 ATMs, about 69 million clients, about 4,600 clients
  per ATM ([10-K 2024](https://www.sec.gov/Archives/edgar/data/70858/000007085825000139/bac-20241231.htm) `[P]`).
- Debit cardholders: 1.9 ATM transactions a month, 58% at their own bank
  ([PULSE 2024](https://www.pulsenetwork.com/public/insights-and-news/news-release-2024-debit-issuer-study/) `[P]`).

So a 200,000-customer bank has roughly 50 to 150 own ATMs (others' ATMs carry
about 42% of withdrawals), 10,000 to 40,000 withdrawals per ATM a year, and
low thousands of customers per ATM. The generator's busiest terminals take
about 680 a day from 12.5% of the bank: 25 to 70 times too busy. Its 13.5
terminals per 10,000 people is near the all-ATM density of 17 to 20 per
10,000 adults (IMF FAS 2009, ATMIA); the fault is that only four per city
are used.

### Merchants: realistic volume, wrong shape

- Issuers see outlets: ISO 8583 DE42 is the card acceptor ID, DE41 the
  terminal; Visa gives each outlet a location, and online merchants use
  their principal place of business
  ([Visa Merchant Data Standards Manual, 2026](https://usa.visa.com/dam/VCOM/download/merchants/visa-merchant-data-standards-manual.pdf) `[P]`).
  Brand-level nodes appear only after enrichment (Plaid `merchant_entity_id`
  is brand level, store number separate, [docs](https://plaid.com/docs/api/products/transactions/) `[P]`).
- About 48 payments a month per consumer, 65% on cards: about 375 card
  payments a year ([2025 Diary of Consumer Payment Choice](https://www.frbservices.org/binaries/content/assets/crsocms/news/research/2025-diary-of-consumer-payment-choice.pdf) `[P]`).
- Krumme et al. (2013): median 64 distinct merchants in six months, top
  merchant about 13% of a person's visits, Zipf 0.80 over rank
  ([Scientific Reports](https://arxiv.org/pdf/1305.1120) `[P]`); the generator
  already uses that exponent.
- Walmart shoppers make about 65 trips a year (Numerator `[S]`); FMI: 1.6
  grocery trips per person a week
  ([2026](https://www.fmi.org/newsroom/news-archive/view/2026/05/20/fmi-s-signature-research-examines-the-evolving-physical-store-experience) `[P]`).
- No public fitted in-degree distribution for card merchants was found; B2B
  payment networks have power-law tails, exponent about 2.6
  ([arXiv 1711.07677](https://arxiv.org/pdf/1711.07677) `[P]`).

Card-merchant hubs of 2,000 to 340,000 payments are plausible for one bank
this size: a supermarket outlet at 10% share gives tens of thousands a year,
a national online brand millions. The flat 4.7 payments per paying account is
what is wrong. Real frequency depends on category: roughly 30 to 65 a year at
a primary grocery or supercenter, 10 to 30 for a gas brand, 1 to 3 for
electronics or furniture (judgment anchored on the figures above; only
grocery is measured).

## Proposed changes

Ordered by unrealism removed per unit of cost. Each shipped as an amendment in
`docs/fraud_model_audit.md`, under this repository's binding rules: new draws
on their own `RngFactory` lane so the shared stream does not shift, goldens
re-pinned once per model round, each digest pin paired with a domain
predicate.

| # | Proposed | Shipped |
|---|---|---|
| 1 | Bank-originated postings get no counterparty. Card interest, card late fees, overdraft fees and overdraft line-of-credit postings (2.79 million rows) stop paying three shared accounts. Either the mule-temporal exporter omits them from `Payment_Transaction` (they stay in the audit ledger), or exports single-account postings with only a `Transaction_From_Account` edge; the second keeps a signal (overdraft and NSF events concentrate in few accounts) but needs an MPL change, since MPL's context query rejects a payment with no recipient. | bank-gl-2026-09, neither option (owner decision): postings stay, their contra retyped as a bank-owned income GL per posting kind, since a fee or interest charge is a double-entry posting whose credit leg is an income GL. |
| 2 | Retire the external-unknown catch-all. Route the 5% unidentified spending share through the external tail-merchant pool, send P2P with no usable contact to the external person/family pool (or drop it), and give funerals a small funeral-home pool placed by city. Also ends the collision with catalogue merchant 1. | unknown-counterparty-2026-09, realistic by channel (owner decision), deviating from the proposal where the amendment says so: paid checks keyed by the payee's bank from the FDIC Summary of Deposits share table; the rest of the unidentified slot pays identified remote merchants; P2P with no usable contact goes to a named P2P platform; a funeral pays a funeral home in the decedent's city. The catch-all is retired (no row). Its replacements are small in rows, not degree: the largest bank's check-payee hub is paid by about 40% of customers, so MPL's hub handling must cover check hubs and the largest remote merchants (figures in the amendment's corpus-movement note). |
| 3 | Replace population-wide lenders and insurers with pools. Fix the mortgage routing defect flagged in `mortgage.cpp`, then draw each loan or policy's provider once at origination from a market-share table (mortgage servicers, five federal student servicers plus private lenders, auto lenders, auto, home and life insurers) with the shares above. Keep SSA and the IRS single. | institutional-providers-2026-09, with the mortgage fix and a reserved key for the external-unknown catch-all. |
| 4 | Spread ATM use across a city's terminals: a stable within-city offset per person, or equal-distance ties broken by a per-person hash instead of the terminal index. Target: busiest terminal at or below about 40,000 withdrawals a year. | atm-spread-2026-09: the per-person hash at each person's four-point cut, so every terminal in a city is used. At pop 200,000, before the affordability screen, the busiest ATM falls from about 298,000 to about 44,000 withdrawals a year. |
| 5 | Heavy-tailed employers and landlords: employers sized from SUSB (thousands of small, a few very large), landlords from the RHFS ownership mix (mostly one or two tenants, a few large managers), not uniform picks over 480 and 240. | counterparty-sizes-2026-09: SUSB 2022 firm sizes plus government payrolls, RHFS 2021 property sizes plus the NMHC Top-50 owners, both thinned to the population. At pop 200,000, 89,231 employers and 66,658 landlords (not about 480 and 240); the busiest private employer pays about 1,700 people. It reconciles this note's metro-scale counts with PhantomLedger's national sample, and registers the "38,000 to 41,000 payroll credits" per employer above as unreconciled: about 4.3 times what the pre-change code could emit (about 308 payees and 9,100 credits a year), so not a baseline. |
| 6 | Larger, optional: outlets and category frequency. Implement the organization/outlet/endpoint split designed in [`data/commerce/README.md`](../../data/commerce/README.md), one account per physical chain outlet, and make visit frequency depend on category instead of a flat 4.7 per payer. | outlets-frequency-2026-09: chain brands become outlet accounts sharing a brand label (pop 500,000: 1,068 chains, 3,346 added outlets, the largest 51). The favourite-visit law keeps its Zipf ranks, but the category picks which favourite holds which rank: grocery and restaurant visits roughly double, biller-category card visits fall from 27% to 12%, and the flat 4.7 is gone. Outlets do not shrink the typical physical merchant: at pop 500,000 the median grocery or restaurant outlet expects about 2,300 payments a year, not a real bank's few hundred, because the catalogue is sized per customer, not per resident; more accounts cross 2,048, not fewer. |

## What this means for MulePatternLearner

- Changes 2 to 5 remove or shrink the largest hubs and the false two-hop
  links through them (62% of deposit accounts shared the catch-all).
- Change 1 as shipped keeps its hubs but types them: the four bank income GLs
  are `account_type = gl`, `is_external` False. MPL should drop or separate
  their edges, not treat them as customer neighbours.
- Hub handling stays necessary: realistic card merchants, ATMs at 10,000 to
  40,000 withdrawals a year, large employers, SSA and the IRS all exceed
  2,048 payments, and about 94% of deposit accounts would still touch one.
  MPL's hub registry and history-withheld stubs remain the right design.
- Any change means regenerating the 2024 corpus (about 63 minutes and 13.7 GB
  RSS last time), loading a new TigerGraph snapshot, and re-running MPL
  preparation and the hub registry under a new dataset id. Mule-temporal has
  no golden of its own, but `tests/golden_run.b2sum` and the
  `tests/golden_tables*.md5` files move.

## Not verified

The current split of bank-owned versus independent ATMs; volumes at the
busiest terminals; distinct ATMs per customer; merchant transaction-count
shares (sold by panel vendors); a fitted card-network merchant in-degree;
monthly versus semiannual premium payment shares; the FSA servicer table
(timed out). Figures from arithmetic across mixed years are estimates.
