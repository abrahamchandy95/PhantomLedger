# Counterparty hubs in the mule-temporal corpus: measured, researched, proposed

Status: research only, 25 September 2026. No generator code had changed when
the measurements below were taken.

**Update, 25 September 2026:** proposed change 3 (lender and insurer pools)
has shipped, together with the mortgage routing fix and a reserved key for the
external-unknown catch-all, as the institutional-providers-2026-09 amendment
in `docs/fraud_model_audit.md`. Proposed change 2 has shipped as the
unknown-counterparty-2026-09 amendment, realistic by channel (owner decision):
paid checks are keyed by the payee's bank from the FDIC Summary of Deposits
share table, the rest of the unidentified slot pays identified remote
merchants, P2P with no usable contact goes to a named P2P platform, and a
funeral pays a funeral home in the decedent's city; the catch-all is retired
and receives no row. Its replacements are small in rows but not in degree:
the largest bank's check-payee hub is paid by about 40% of customers, so MPL's
hub handling must cover the check hubs and the largest remote merchants (the
amendment's corpus-movement note gives the figures). Proposed change 4 has
shipped as the atm-spread-2026-09 amendment: ties at each person's four-point
cut are broken by a per-person hash instead of the terminal index, so every
terminal in a city is used; at pop 200,000, before the affordability screen,
the busiest ATM falls from about 298,000 to about 44,000 withdrawals a year.
Proposed change 5 has shipped as the counterparty-sizes-2026-09 amendment:
employers follow the SUSB 2022 firm-size distribution plus government
payrolls and landlords the RHFS 2021 property-size distribution plus the NMHC
Top-50 owners, both thinned to the population, so at pop 200,000 there are
89,231 employers and 66,658 landlords (not about 480 and 240) and the busiest
private employer pays about 1,700 people. The "38,000 to 41,000 payroll
credits" per employer in the table below is about 4.3 times what the
pre-change code could emit (about 308 payees and 9,100 credits a year); it is
registered there as unreconciled and should not be quoted as the baseline.
Proposed change 6, outlets and category frequency, has shipped as the
outlets-frequency-2026-09 amendment: each physical chain brand is expanded
into outlet accounts that share a brand label (at pop 500,000, 1,068 chains
and 3,346 added outlets, the largest 51), and the favourite visit law keeps
its Zipf ranks but lets the category decide which favourite holds which rank,
so grocery and restaurant visits roughly double and biller-category card
visits fall from 27% to 12%. The flat 4.7 payments per payer below is gone,
but the amendment also shows that outlets do not shrink the typical physical
merchant: at pop 500,000 the median grocery or restaurant outlet expects about
2,300 payments a year, not the few hundred a real bank of that size would see,
because the catalogue is sized per customer rather than per resident, and more
accounts cross 2,048, not fewer.
Everything measured below is the pre-change corpus.

MulePatternLearner (MPL) found that its per-account context query aborted on
accounts with more than 2,048 visible payments. About 5,600 accounts cross that
line by the end of 2024, the largest has 3.8 million payments, and roughly 95%
of accounts pay at least one of them. This note answers whether that is a
PhantomLedger generation problem, what reality looks like, and what to change.

**Short answer: partly.** Most hubs are realistic in kind (large card
merchants, payroll, Social Security, the IRS), and a real bank's graph has them
too, so MPL must keep handling hubs whatever the generator does. But the seven
largest hubs, and the ATM terminals, are artifacts of how PhantomLedger routes
money, and they are what make the tail so extreme.

## What was measured

Source: the TigerGraph graph loaded from the `--usecase mule-temporal
--population 200000 --days 366 --seed 42` run (snapshot
`phantomledger_2024_seed42_snapshot_20260919`), read with read-only
interpreted queries on 25 September 2026. The graph has 317,840 internal
deposit accounts, 434,783 internal card accounts and 35,660 external accounts.
No internal account has more than 1,967 payments; every hub is external.

Hubs at the 2025-01-01 cutoff (the MPL hub registry, full visibility), by what
they are:

| Hub class | Hubs | Notes |
|---|---:|---|
| Card merchants | 4,384 | Catalogue merchants; the largest takes 341,126 payments |
| Payers only (employers, SSA, clients) | 566 | About 480 employers at 38,000 to 41,000 payroll credits each |
| Other external deposit payees | 392 | 160 subscription billers at about 47,000 each, the IRS, landlords |
| Cash points (ATMs, depositories) | 227 | Busiest four ATMs about 250,000 withdrawals each |
| Unknown-type digital payees | 25 | Includes the catch-all below |
| Population-wide singletons | 5 | Catch-all, servicer, insurers, auto lender |
| Bank posting sinks | 3 | Card issuer, fee collection, overdraft line |

Reach, from a 1/256 hash sample of 2,897 internal accounts: 94.3% of deposit
accounts and 99.9% of card accounts pay at least one card-merchant hub.
Removing every artifact below would not change that figure.

### The artifacts, largest first

| Account | Payments | Mean | Distinct payers | Cause in source |
|---|---:|---:|---:|---|
| External-unknown catch-all (`XM00000001`) | 3,814,933 | $220 | 196,989 deposit (62%) | `PaymentRouter::emitExternal` in `src/activity/spending/routing/payments.cpp`: the 5% "external unknown" spending share, P2P with no usable contact, and funeral payments all go to one key, which also collides with catalogue merchant 1 |
| Card issuer | 2,316,826 | $37 | 316,051 card accounts (73%) | `Session::accrueInterest` and `Session::postLateFee` in `src/transfers/channels/credit_cards/session.cpp` book interest and late fees as payments to `issuerAccount` |
| Insurer (premiums in, 6,866 claims out at $7,923) | 1,808,916 | $288 | 165,812 | One `Insurance::autoCarrier` key for everyone (`src/synth/products/terms/insurance.cpp`) |
| Student-loan servicer, also receiving every mortgage | 1,430,365 | $1,282 | 130,524 | Flagged "SUSPECTED DEFECT" in `src/synth/products/terms/mortgage.cpp`: mortgages route to `Lending::studentServicer` |
| Life insurer | 1,074,202 | $37 | 93,238 | One `Insurance::lifeCarrier` key |
| Auto lender | 714,971 | $669 | 75,215 | One `Lending::autoLoan` key |
| Bank fee collection | 361,991 | $41 | 47,237 | `bankFeeCollectionKey()` in `src/transfers/legit/ledger/posting.cpp` |
| Busiest ATM terminals (4) | about 250,000 each | $125 | about 25,000 each | `buildNearbyPoints` in `src/transfers/legit/blueprints/plans.cpp`: every resident of a city sits at the city centroid, so all terminals in the city tie on distance and the index tie-break gives everyone the same four |
| Overdraft line of credit | 113,386 | $65 | 14,656 | `bankOdLocKey()` in the same file |

Smaller but related: payroll picks one of about 480 employers uniformly, so
every employer has about 1,500 employees; rent picks one of 240 landlords,
about 320 tenant accounts each; subscriptions spread uniformly over 160 billers;
and every card merchant gets about 4.7 payments per paying account per year
whatever its size or category.

## What reality looks like

`[P]` means the primary document was read; `[S]` means only a secondary summary
or search snippet was seen.

### Fees and interest have no counterparty

- ISO 20022 defines Account Management (ACMT) as "operations on one account"
  between the servicing institution and the account owner, with charges
  (CHRG), interest (INTR) and fees (FEES) sub-families, and card-specific
  PMNT/CCRD/CHRG and PMNT/CCRD/INTR codes. [ISO 20022 External Code Sets,
  October 2023](https://www.iso20022.org/sites/default/files/media/file/BTC_ExternalCodeListDescription_October2023.doc) `[P]`.
  Danske Bank maps "Bank's fee" and "Interest debit" to these codes
  ([appendix v1.3, 2023](https://danskeci.com/-/media/pdf/danskeci-com/iso-20022-xml/banktransactioncode_appendix.pdf)) `[P]`.
- In camt.053, RelatedParties is optional, and the Dutch implementation guide
  fills debtor and creditor only for credit transfers and direct debits
  ([Betaalvereniging IG camt.053 v1.1](https://www.betaalvereniging.nl/wp-content/uploads/2026/03/IG-Bank-to-Customer-Statement-CAMT-053-v1-1.pdf)) `[P]`.
- A service charge debits the customer account and credits the service-charge
  income GL ([Oracle FLEXCUBE Accounting Entries, 2021](https://docs.oracle.com/cd/F44734_01/PDF/UserManual/Accounting%20Entries%20User%20Manual.pdf)) `[P]`.
  Card interest and fees are income lines on the call report
  ([FFIEC 051 Schedule RI instructions](https://www.fdic.gov/system/files/2024-08/2021-12-051-ri.pdf)) `[P]`.
- Aggregators label these as categories (Plaid `BANK_FEES_INTEREST_CHARGE`,
  `OVERDRAFT_FEES`; MX "Fees & Charges"), not as payments to a merchant
  ([Plaid taxonomy](https://plaid.com/documents/transactions-personal-finance-category-taxonomy.csv),
  [MX categories](https://docs.mx.com/api-reference/platform-api/reference/categories)) `[P]`.
- Published bank transaction graphs (DNB: [Johannessen and Jullum,
  2023](https://arxiv.org/pdf/2307.13499); Rabobank: [Bonato and Szava,
  2025](https://arxiv.org/pdf/2509.10715)) have customer and external
  counterparty nodes, no bank or GL node `[P]`. An IBM patent removes
  "super nodes" such as utilities before graph analysis
  ([US12093245B2](https://patents.google.com/patent/US12093245B2/en)) `[P]`.
- Volume check: about one late fee per general-purpose card account per year,
  and 48% of active accounts revolve, so about 6 to 7 postings per active card
  per year ([CFPB Credit Card Market Report,
  2023](https://files.consumerfinance.gov/f/documents/cfpb_consumer-credit-card-market-report_2023.pdf)) `[P]`.
  23% of banked consumers paid an overdraft fee in a year, heavily
  concentrated ([CFPB, December 2023](https://files.consumerfinance.gov/f/documents/cfpb_overdraft-nsf-report_2023-12.pdf)) `[P]`.
  The generator's counts are plausible; the shared counterparty is not.

### Unidentified counterparties are null, not one account

Enrichment vendors match about 90% to 97% of transactions (Spade "at least
95%", [docs](https://docs.spade.com/reference/understanding-enriched-data) `[P]`;
Ntropy 96.75% on hard cases, [2024](https://www.ntropy.com/blog/better-loyalty-us) `[P]`).
The rest keep a raw descriptor with a null merchant (MX sets `merchant_guid`
to null, [docs](https://docs.mx.com/api-reference/more-apis/data-enhancement-off-platform/) `[P]`).
A 5% unidentified share is realistic; one account receiving all of it has no
real analogue. Funeral payments go to identifiable funeral homes.

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

Auto claims run about 4.2% collision and 4.0% comprehensive per insured vehicle
year at $5,489 and $2,306 (ISO via Triple-I `[S]`), and many are paid to body
shops or lienholders rather than the customer.

Social Security and IRS really are single originators in ACH data: Treasury
uses fixed descriptors ("SOC SEC", "TAX REF") and the company name "IRS TREAS
310" ([Fiscal Service Green Book](https://fiscal.treasury.gov/files/reference-guidance/green-book/greenbook-full.pdf),
[refund FAQ](https://fiscal.treasury.gov/payments-from-government/direct-deposit/faq-tax-refund)) `[P]`.
Those two singletons are correct.

### ATMs are far less busy

- 3.4 billion US ATM withdrawals in 2024, average $210
  ([Federal Reserve Payments Study](https://www.federalreserve.gov/paymentsystems/frps_cy2015_24_topline.htm) `[P]`),
  over 520,000 to 540,000 ATMs (ATMIA `[S]`): about 6,400 withdrawals per ATM
  per year, 18 a day.
- Cardtronics averaged 757 withdrawals per ATM per month in 2019
  ([10-K](https://www.sec.gov/Archives/edgar/data/1671013/000167101320000008/catm-201901231x10k.htm) `[P]`).
- Bank of America has 14,893 ATMs for about 69 million clients, about 4,600
  clients per ATM ([10-K 2024](https://www.sec.gov/Archives/edgar/data/70858/000007085825000139/bac-20241231.htm) `[P]`).
- Debit cardholders make 1.9 ATM transactions a month, 58% at their own bank's
  ATMs ([PULSE 2024](https://www.pulsenetwork.com/public/insights-and-news/news-release-2024-debit-issuer-study/) `[P]`).

For a 200,000-customer bank that suggests roughly 50 to 150 own ATMs (plus
others' ATMs carrying about 42% of withdrawals), 10,000 to 40,000 withdrawals
per ATM per year, and low thousands of distinct customers per ATM. The
generator's busiest terminals take about 680 withdrawals a day from 12.5% of
the bank, 25 to 70 times too busy. The 13.5 terminals per 10,000 people is
itself close to the all-ATM density of 17 to 20 per 10,000 adults (IMF FAS
2009, ATMIA); the problem is that only four per city are ever used.

### Merchants: realistic volume, wrong shape

- Issuers see merchants at outlet level. ISO 8583 DE42 is the card acceptor
  ID and DE41 the terminal; Visa assigns each outlet a location and online
  merchants use their principal place of business
  ([Visa Merchant Data Standards Manual, 2026](https://usa.visa.com/dam/VCOM/download/merchants/visa-merchant-data-standards-manual.pdf) `[P]`).
  Brand-level nodes exist only after enrichment (Plaid `merchant_entity_id`
  is brand level, store number separate, [docs](https://plaid.com/docs/api/products/transactions/) `[P]`).
- Consumers make about 48 payments a month, 65% on cards, so about 375 card
  payments a year ([2025 Diary of Consumer Payment Choice](https://www.frbservices.org/binaries/content/assets/crsocms/news/research/2025-diary-of-consumer-payment-choice.pdf) `[P]`).
- Krumme et al. (2013) measured a median 64 distinct merchants in six months,
  the top merchant taking about 13% of a person's visits, and Zipf 0.80 over
  rank ([Scientific Reports](https://arxiv.org/pdf/1305.1120) `[P]`). The
  generator already uses that Zipf exponent.
- Walmart shoppers make about 65 trips a year (Numerator `[S]`); FMI reports
  1.6 grocery trips per person per week
  ([2026](https://www.fmi.org/newsroom/news-archive/view/2026/05/20/fmi-s-signature-research-examines-the-evolving-physical-store-experience) `[P]`).
- No public fitted in-degree distribution for card merchants was found; B2B
  payment networks show power-law tails with exponent about 2.6
  ([arXiv 1711.07677](https://arxiv.org/pdf/1711.07677) `[P]`).

So card-merchant hubs of 2,000 to 340,000 payments are plausible for one bank
of this size: a single supermarket outlet at 10% market share already gives
tens of thousands a year, and a national online brand would give millions.
What is off is the flat 4.7 payments per paying account for every merchant.
Real frequency depends on category: roughly 30 to 65 a year at a primary
grocery or supercenter, 10 to 30 for a gas brand, 1 to 3 for electronics or
furniture (judgment anchored on the figures above; only grocery is measured).

## Proposed changes

Ordered by how much unrealism each removes per unit of cost. Change 3 has
shipped (institutional-providers-2026-09) and so has change 2
(unknown-counterparty-2026-09, which deviates from the text below where the
amendment says so). Change 1 shipped as neither option below: owner decision,
the postings are kept and their contra is retyped as a bank-owned income GL
per posting kind (bank-gl-2026-09), because a fee or interest charge really is
a double-entry posting whose credit leg is an income GL. Changes 4
(atm-spread-2026-09) and 5 (counterparty-sizes-2026-09) have shipped too; the
change 5 amendment reconciles this note's metro-scale counts with the
national sample PhantomLedger draws. Change 6 (outlets and category
frequency) has shipped as outlets-frequency-2026-09. Every change must follow the binding rules in this repository: new
draws on their own `RngFactory` lane so the shared stream does not shift,
re-pin the goldens once per model round, and pair each digest pin with a
domain predicate.

1. **Bank-originated postings get no counterparty.** Card interest, card late
   fees, overdraft fees and overdraft line-of-credit postings stop being
   payments to three shared accounts (2.79 million rows). Either the
   mule-temporal exporter omits them from `Payment_Transaction` (they stay in
   the audit ledger), or it exports them as single-account postings with only
   a `Transaction_From_Account` edge. The second keeps a useful signal
   (overdraft and NSF events are concentrated in few accounts) but needs a
   matching MPL change, because MPL's context query currently treats a
   payment with no recipient as invalid.
2. **Retire the external-unknown catch-all.** Route the 5% unidentified
   spending share through the existing external tail-merchant pool, send P2P
   with no usable contact to the external person/family pool (or drop the
   event), and give funeral payments a small funeral-home pool placed by city.
   This also removes the collision with catalogue merchant 1.
3. **Replace population-wide lenders and insurers with pools.** Fix the
   mortgage routing defect already flagged in `mortgage.cpp`, then draw each
   loan or policy's provider once at origination from a market-share table
   (mortgage servicers, five federal student servicers plus private lenders,
   auto lenders, auto, home and life insurers) using the shares above. Keep
   SSA and the IRS as single accounts.
4. **Spread ATM use across a city's terminals.** Give each person a stable
   within-city offset (or break equal-distance ties with a per-person hash
   instead of the terminal index) so that every terminal in a city is used.
   Target: busiest terminal at or below about 40,000 withdrawals a year.
5. **Heavy-tailed employers and landlords.** Size employers from the SUSB
   distribution (thousands of small employers, a few very large ones) and
   landlords from the RHFS ownership mix (mostly one or two tenants, a few
   large managers), instead of uniform picks over 480 and 240.
6. **Larger, optional: outlets and category frequency.** Implement the
   organization/outlet/endpoint split already designed in
   [`data/commerce/README.md`](../../data/commerce/README.md) so physical
   chains present one account per outlet, and make visit frequency depend on
   category instead of a flat 4.7 per payer.

## What this means for MulePatternLearner

Changes 2 to 5 remove or shrink the largest hubs and the false two-hop links
through them (62% of deposit accounts currently share the catch-all). Change 1,
as shipped, keeps its hubs and types them instead: the four bank income GLs
are `account_type = gl`, `is_external` False, and MPL should drop or separate
their edges rather than treat them as customer neighbours. They do
not remove the need for hub handling: realistic card merchants, ATMs at 10,000
to 40,000 withdrawals a year, large employers, SSA and the IRS all stay above
2,048 payments, and about 94% of deposit accounts would still touch one. MPL's
hub registry and history-withheld stubs remain the right design.

Adopting any change means regenerating the 2024 corpus (about 63 minutes and
13.7 GB RSS last time), loading a new TigerGraph snapshot, and re-running MPL
preparation and the hub registry under a new dataset id. The mule-temporal use
case has no golden of its own, but `tests/golden_run.b2sum` and the
`tests/golden_tables*.md5` files will move.

## Not verified

A current split of bank-owned versus independent ATMs; volumes at the busiest
individual terminals; distinct ATMs per customer; transaction-count shares by
merchant (sold by panel vendors); a fitted merchant in-degree for card
networks; monthly versus semiannual premium payment shares; the FSA servicer
table (timed out). Where a figure above rests on arithmetic from mixed years,
it is an estimate, not a measurement.
