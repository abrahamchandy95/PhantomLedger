# Closed: the shared cash hub (`cash-hub-defect-2026-08`)

Closed in the customer-ledger projection. The pre-fix diagnosis is kept as a
regression baseline; see [Implemented disposition](#implemented-disposition)
and [Regression gates](#regression-gates-shipped-with-the-fix). It lives in
`docs/` because `CLAUDE.md` is git-ignored and a citation kept only there was
lost on clone.

- Before: every ATM withdrawal credited one random customer's checking
  account, which reported infinite liquidity and exported as `inf`.
- Now: ATM rows go to many ownerless, local `Bank::external` processor
  endpoints. Key-aware clearing debits only the customer leg and never credits
  an endpoint. No customer is infrastructure, no hub flag or infinite balance
  exists, and the CSV writer rejects non-finite values.
- The endpoint key is the combined terminal/acceptor context the two-ended
  schema allows, not a terminal GL account. Real vault-cash and
  network-settlement GLs stay outside the projection: no fictitious customer
  account, no system-wide graph sink.

## Pre-fix findings

### The observed export

The real `clearing::Ledger` and `exporter::csv::Writer` through the production
expression at [aml/vertices.cpp:234](../src/exporter/aml/vertices.cpp#L234):

```
A0000000011,0.0
A0000000012,inf
```

End to end (`aml::exportAll`, `aml_txn_edges::exportAll`, pop 400): 4 of 1,674
`Account` and 4 of 2,089 `Internal_Account` rows non-finite, e.g.
`A0000000203,A0000000203,inf,2024-01-26 00:03:00,active,...`. That is the hub
count, `max(1, 0.01 × population)`: 700 at the default `--population 70000`,
100 at the pop-10,000 acceptance config.

[docs/cash_hub_inf_repro.cpp](cash_hub_inf_repro.cpp), the original
reproducer, calls the removed `Ledger::createHub` and no longer builds.
`tests/test_cash_boundaries.cpp` replaces it: `inf` cannot be exported and
boundary endpoints never get a balance mutation.

### The super-node

On the 23,866,506-row production export (`MulePatternLearner/data/transactions.csv`,
499,409 accounts):

| quantity | value |
|---|---|
| distinct ATM destination accounts | 1 (`A0000247513`) |
| rows credited to it | 1,823,332 (7.64%): 1,230,944 `atm_withdrawal`, 352,271 `cc_interest`, 239,313 `cc_late_fee`, 791 subscription, 10 p2p, 2 camouflage_bill, 1 insurance_claim |
| rows it sources | 144 |
| distinct counterparties | 344,576 (69.0% of accounts) |
| in-degree rank | 1st; 2nd is the external-unknown sentinel `XM00000001` at 176,196 |

The only top-eight vertex without an `X` prefix, so the only one a consumer
cannot recognise as infrastructure.

### Money conservation broken by 42%

Replaying every row through a clone of the opening book (900 people, 731 days,
seed `0xC0FFEE`, 668,247 rows, gross $85,682,895.37), per-slot cash deltas
should sum to zero. Net cash created: +$36,248,870.40, +42.31% of gross. Terms
(matching to one floating-point quantum):

| source | rows | amount |
|---|---|---|
| 9 seeded `1e18` roster hubs | 8,389 | +$5,050,718.05 |
| 30 `fundingHubs`, `createHub`'d, never seeded (cash 0.00) | 35,319 | +$33,260,807.20 |
| `1e18` rounding on credits into seeded hubs | 120,568 | −$2,062,782.85 |

The largest term is arguably a separate bug: `limits.cpp` runs `createHub` on
every `fundingHubs` key, but `seedHubAccounts` covers only
`plan.counterparties().hubAccounts`, leaving 30 zero-balance accounts with a
bypassed funding screen.

`Ledger::invalid` (the external-key void), once suspected, is not involved: 0
rows resolve to it. `limits.cpp` registers every record, `Bank::external`
included, and settlement uses `findAccount` then index-based `transferAt`,
never the key-based overload that forces external to `invalid`.

### The `1e18` quantum deletes most ATM cash

`kHubCash = 1e18`
([balance_book.hpp:141](../include/phantomledger/transactions/clearing/balance_book.hpp#L141))
has an ulp of 128.0: credits become `128 × round(amount / 128)`, and anything
≤ $64 vanishes (ties-to-even eats $64.00). Of 18 `kAtmAmounts`, 7 ($20, $40 ×3,
$60 ×3) credit $0.00; $80 to $160 credit $128; $200 and $300 credit $256. In
the corpus, 120,568 hub credits worth $3,697,214.85 book as $1,634,432.00;
108,093 rows (89.7%, $2,376,059.08) vanish.

### The hub owner's behaviour is distorted

`liquidity()` and `availableCash()` return infinity, so:

- the liquidity ratio clamps and the spending multiplier pins at `kCeiling = 1.10` all run;
- `selectPaymentRoute`'s "cash short, use the card" fallback never fires;
- owners skip salary, rent, revenue, ATM and subscriptions but not spending,
  cards or fraud selection (pop 900: 9 of 9 hold credit cards, 1 to 8 each; 18
  fraud rows in 4 rings touch one);
- their own activity makes most of the money: salary 30,288 rows / $28.9M,
  cash_deposit 5,270 / $4.6M, client_ach_credit 4,624 / $4.0M.

Nothing exported marks them: `entity::account::Flag` has
fraud/mule/victim/external/shell, no hub; the ledger's `hub` bit stays in
memory.

## The causal chain

Five independently verified links, each a possible cut point:

1. `selectHubAccounts` picks customers:
   [plans.cpp:67](../src/transfers/legit/blueprints/plans.cpp#L67) samples roster
   persons without replacement, then `census.ownership->primaryIndex(person)`.
   Count `clamp(population × fraction, 1, personCount)`; `fraction = 0.01`,
   defaulted in two structs, forwarded once, overridden nowhere in `src/` or
   `tests/`. The floor of 1 means no config disables it.
2. One key, four roles: [atm.cpp:220](../src/transfers/legit/routines/atm.cpp#L220)
   uses `hubAccounts.front()` as the ATM network; the same key is the
   cash-deposit source ([passes.cpp:127](../src/transfers/legit/ledger/passes.cpp#L127)),
   card `issuerAcct`, and head of `billerAccounts` (the hub vector, copied).
   Employer and landlord fallbacks are dead in production.
3. Infinite liquidity: [ledger.cpp:129](../src/transactions/clearing/ledger.cpp#L129)
   returns `std::numeric_limits<double>::infinity()` for `isHub(idx)`, as does
   `availableCash` (line 139). `decide()` skips the sufficiency test for a hub
   source; `applyTransfer` guards the debit on `!srcHub`, the credit not at all.
4. Both AML exporters write `roundMoney(finalBook->liquidity(rec.id))`
   ([aml/vertices.cpp:234](../src/exporter/aml/vertices.cpp#L234),
   [aml_txn_edges/vertices.cpp:179](../src/exporter/aml_txn_edges/vertices.cpp#L179));
   `roundMoney` (`std::round(x * 100.0) / 100.0`) passes infinity. `finalBook`
   is non-null in both engines and exporters, and the `hub` bit survives
   because `Ledger::clone` is the copy constructor and `flags_` a plain member.
5. [csv.cpp:44](../src/exporter/csv.cpp#L44): `std::to_chars` returns `std::errc{}`
   for infinity, so no throw; the trailing-zero fixup's `.eEnN` probe matches
   the `n` in `inf`, so nothing is appended. The cell is a bare `inf`.

Downstream, `TableMirror` mirrors every column as `text` and `COPY` uses
`FORMAT csv` unquoted, so `inf` loads silently. Cast to `double precision`,
PostgreSQL reads `Infinity`: a poisoned feature, not an abort. A `numeric`
target on PostgreSQL ≤ 13 would abort, an integer target always.

## Why nothing caught it

- No finiteness assertion in the 68-test suite
  (`grep -rn 'isfinite|isinf|std::isnan' tests/*.cpp` is empty); `balance` in
  `tests/` is only a `balance_book.hpp` include or a `BalanceRules` fixture.
- `test_pipeline_e2e` passed with the `inf` rows: `expectTable` checks only
  that a table exists and is non-empty.
- The golden pinned the defect: `tests/golden_tables_aml.md5` line 35 digests
  `aml_txn_edges_vertices_Account` (45,188 rows; column 7 is `balance`), so the
  fix had to red it.
- No test runs `--usecase aml`; `test_table_golden` covers standard,
  aml-txn-edges and card_fraud.
- Nothing tested hubs: `hubSet` has no hits in `tests/`, `hubAccounts` is
  unchecked plumbing, `gate_world.hpp` hardcodes `fraction = 0.01`.
- No conservation invariant; the bit-exact FNV book hash for cross-architecture
  equality passes any imbalance both legs share.
- Neither `CLAUDE.md` nor `docs/fraud_model_audit.md` mentioned the hub,
  `kHubCash` or `1e18`.

## What real systems do

Researched 2026-08-18.

- On-us withdrawal: debit the deposit liability, credit the bank's
  currency-and-coin asset. [Federal Reserve Regulation D](https://www.federalreserve.gov/frrs/regulations/section-2042-definitions.htm)
  counts proprietary-ATM currency as vault cash. Oracle FLEXCUBE: `ATM Cash GL`.
- Off-us: the credit is a network settlement GL (net position against the
  network), since no issuer cash moved; the acquirer posts to its
  `Acquirer Cash GL`. The [Federal Reserve Payments Study glossary](https://www.federalreserve.gov/paymentsystems/files/FRPS_2016_DFIPS_Glossary.pdf)
  separates on-us and foreign ATM activity.
- Cardinality: FLEXCUBE has one GL set per bank and a per-terminal
  cross-reference, many-to-one; its manual calls central the norm, fanning out
  per branch only across multiple switches. Settlement accounts shard per
  network and business line: single digits to low tens.
- So one cash account is fine. The faults: a depositor's checking account,
  infinite money, four unrelated roles. A real core has `ATM Cash GL`,
  `Acquirer Cash GL`, `Deposit Cash GL`, `Settlement GL` and two mandatory
  suspense GLs.
- Interchange runs opposite ways: on purchases the acquirer pays the issuer, on
  cash withdrawals the issuer pays the acquirer. Reusing the purchase direction
  for ATM rows inverts the sign.
- The fraud graph key is the terminal: ISO 8583 DE 41 Card Acceptor Terminal
  Identification (8 alphanumeric) plus DE 42 Card Acceptor Identification Code
  (15), unique only as a pair, one acceptor to many terminals. In ISO 20022 the
  ATM sends `catp.001 ATMWithdrawalRequest` itself. Issuers have it (from the
  processors' field maps): Galileo/SoFi expose DE41 as `terminal_id` and DE42 as
  `merchant_id`; Marqeta a `card_acceptor` object.

No published dataset of five inspected mints a monolithic cash node or models
an ATM terminal:

| dataset | cash counterparty |
|---|---|
| IBM AMLSim | `Branch` population (`numBranches = 1000`, `B`-prefixed), but dead code: `isNextStep` returns `false` in `CashInModel` and `CashOutModel` |
| PaySim | 34,749 merchants for 20,000 clients, deliberately hub-free |
| IBM AMLworld (NeurIPS 2023) | edge attribute (a Payment Format value): 18,069,965 of 176M rows, ~10.3% |
| Neo4j fraud reference | subtype label on a reified Transaction (`CashIn`, `CashOut`, `Payment`, `Debit`, `Transfer`) |
| Sparkov | no cash |

Super-node harm is documented:

- SALT-GNN (arXiv:2607.10131): stratified by recipient degree (HI-Small,
  HI-Medium, AMLSim-32k-5%), AML GNNs degrade in dense recipient contexts and
  aggregate F1 hides it; report by degree.
- GCNs score high-degree nodes better (Tang et al., CIKM 2020,
  arXiv:2006.15643), so a super-node flatters metrics.
- Graph convolution is Laplacian smoothing (Li, Han & Wu, AAAI 2018,
  arXiv:1801.07606): the hub is in every neighbour's receptive field at every
  layer.
- PageRank follows the in-degree power law (Litvak et al., arXiv:math/0607507):
  centrality features become "distance to the cash node".
- Modularity's resolution limit merges small communities (Fortunato &
  Barthélemy, PNAS 2007); a vertex adjacent to everything is the worst case for
  recovering rings by community detection.

Sizing: no current official US ATMs-per-capita figure. The World Bank (IMF
Financial Access Survey) stops at 2009: 425,010 ATMs, 172.76 per 100,000
adults. The only current count is commercial (Euromonitor via trade press):
451,500 in 2022, down from 470,000 in 2019. The Federal Reserve Payments Study
2022 reports 3.7 billion ATM withdrawals in 2021, average $156 (2018) → $198
(2021). Derived across vintages: ~8,200 withdrawals per ATM a year, ~135
terminals per 100,000 people.

## Downstream damage

- `MulePatternLearner/scripts/pipeline/pre_graph/temporal_flow_aggs.py` reads
  only `src_acct, dst_acct, amount, ts`, dropping `channel`, so `HAS_PAID`
  cannot tell cash-out from transfer.
- `mule_ml`'s `Transfer_Transaction.csv` has no channel column: an ATM
  withdrawal equals a P2P transfer byte for byte.
- `MulePatternLearner`'s GSQL queries have no `is_external` guard:
  `money_flow.gsql` computes `fan_in_ratio`, `pass_through_ratio`, `net_flow`
  over every `Account:a`; `weight_account_edges.gsql` links the hub to every
  counterparty weighted by count, so almost every pair clears
  `cluster_with_wcc.gsql`'s `min_link_weight = 2` and WCC makes one component,
  voiding the live `com_size` feature; `pagerank.gsql` sinks its mass into the
  hub, and `pagerank` is live too.

## Implemented disposition

Options B and C combined, without a new global target:

- Posting paths classify a `Bank::external` key before index resolution.
  Registered external entities stay valid references but never enter the
  internal map, are never seeded, never get a balance mutation. Unknown
  `Bank::internal` keys are rejected.
- ATMs are population-scaled external processor endpoints over customer
  home-area quantiles. Event-time relocation picks the area, the customer's four
  nearest endpoints form the choice set, and a draw-free hash gives a stable
  primary point with occasional nearby use, so the raw graph gets a distributed
cash-access layer, not a supernode. (`atm-spread-2026-09`: the four were
  first chosen per area by pool index, so a whole city shared its four
  lowest-numbered points; ties now break on a per-(person, area, rail) hash
  window, byte-identical where a rail has four points or fewer.)
- Cash deposits, billers, the card issuer, employers and landlords use distinct
  external roles and pools, never a roster customer.
- The retired population selection still runs and is discarded, as an
  entropy-compatibility burn only; it confers no status or behaviour.

With only `source` and `target` in the row contract, the cash-point key carries
both context and the external-boundary marker; the clearing rule, not a
terminal balance, carries the money. A future schema can split DE41/DE42 and
the GL projection without changing that.

Historical advice: move hub selection to its own `RngFactory` lane first,
re-pin once, then change the model, or pay a second re-pin and band
re-measurement.

### Why options B and C move goldens

Hub selection draws on the shared sequential RNG that then feeds the opening
book, the transaction factory, every income and routine pass, and fraud
planning. `selectHubAccounts` spends `k = hubCountFor(...)` bounded draws, and
Floyd's sampler runs `j` over `[n-k, n)` with `range = j+1`, so changing `k`
changes the first draw. Amplifiers: hubs skip opening-balance seeding
(`seedHubAccount` draw-free, `seedOwnedAccount` 4 to 11 draws); five emission
passes short-circuit on hub membership before their coin; subscriptions draw
one timestamp per sub per month; `Rng::bounded` returns 0 without consuming a
u64 when `range <= 1`, so a pool shrunk to 1 shifts the plan lane.

This breaks `merchant-churn-2026-07` rule 2 (a data-dependent draw count goes
last on its own lane): it depends on `populationCount` and sits first on the
shared stream. All four goldens move; `golden_tables_aml.md5` and
`golden_tables_card_fraud.md5` share one `transactions` digest, so the
exporter-only escape of `merchant-coordinates`, `merchant-ownership` and
`card-churn` is closed. `kTableCount = 43` holds.

### Option A: export-only containment (rejected as insufficient)

Public `Ledger::isHubAccount(const Key&)` (or a `reportedBalance` of
`totalLiquidity` without the hub short-circuit), empty balance instead of
`inf`, and an `is_hub` column. Suppress, not clamp: `0.0` invents an empty
account, and the finite `totalLiquidity` grows with row count, re-encoding the
degree leak. NULL is the repo's mask (`merchant-coordinates` rule 2: row
absence, never `0,0`). Moves only `aml_txn_edges_vertices_Account`, not
`golden_run.b2sum` (no lane, funding decision or draw count changes); `Ledger::liquidity` stays infinite in-simulation, since
clamping changes `decide()` and every later draw. Fixes no edge: 1,823,332 rows
still hit one node with 69% of accounts as neighbours. A capability, not a
repair, and its audit row must lead with that.

### Option B: named external system counterparties (boundary semantics adopted)

New `entities/counterparties/system_accounts.hpp` on the
`institutional_accounts.hpp` pattern: constexpr keys at a reserved serial base;
`kCashNetwork`, `kBranchVault` as `Role::processor, Bank::external`;
`kCardIssuerReceivable` as `Role::business, Bank::external`. No taxonomy
change: `bankFeeCollectionKey` and `bankOdLocKey` are already
`Role::business, Bank::external` with reserved high serials via the external
path, and `Role::processor`/`Role::platform` are `externalOnly` (`XS`, `XP`).
Relaxing `Role::account`'s `internalOnly` would cost a predicate row, a layout
constant, a layout-table row and an exporter label row.

- Trap: `validateTransactionAccounts` throws on an endpoint missing from the
  `Lookup`, so register via `addAccounts(..., external = true)` or every ATM,
  subscription and issuer row aborts the run.
- Latent trap: `seedHubAccount` skips owner `invalidPerson`, so `kHubCash`
  never reaches an external hub (harmless while the external path masks cash
  reads).
- Keep `rng.choiceIndices` as a `[[maybe_unused]]` burn so the blueprint lane
  and entity prefix hold.
- Free win: `exporter::common::isExternalKey` keys on `Bank::external`, so
  `TransactionEdgeClassifier::observe` moves ~1.8M rows from
  `Account_Send`/`Receive` to the Counterparty edges with no exporter edit, and
  the `inf` row disappears.

### Option C: sharded cash-point layer (distributed transaction layer adopted)

A draw-free, area-anchored `cash_points.hpp` registry of external
`Role::processor` accounts, sized per area over the population-independent
71-city catalogue, resolved by hashing account key and home area with month
rotation. No new uniforms, so the shared streams stay byte-identical and
`golden_run.b2sum` rows should hold, only the digest moving. It restores structure: the top vertex loses 1,230,944 rows,
cash-out becomes a many-terminal bipartite layer, and `distance(home, cash-out
point)` turns local, matching the step-2 merchant geography.

- Historical, not implemented: the first review treated `Fraud::structuring` as
  cash deposits (per `isCurrency`), but generator and README define it as
  victim-to-ring split payments under a reporting threshold; routing ATM or
  depository endpoints into it would invent a cash leg. The tag/comment
  mismatch is a separate model-version decision.
- The layer is 100% legitimate, so "touches a terminal ⇒ not a mule" runs near
  precision 1.0. One memorisable id becomes a clean bipartite type: a stronger
  shortcut.

### Fixes independent of the option

1. The 30 unseeded `fundingHubs`: the largest money-creation term ($33.26M).
2. `unauthorized.cpp:589`, fraud's one unguarded `billerAccounts` read:
   `pickOne` on an empty span calls `choiceIndex(0)`, which throws (the other
   three consumers guard).
3. Delete first, as they widen every grep: `LegitCounterparties::hubAccounts`
   (3 writes, 0 reads), `CounterpartyAccess::isHub(const Key&)`,
   `CounterpartyAccess::firstHub()` (no callers).

## Regression gates shipped with the fix

`tests/test_cash_boundaries.cpp`, on the real replay and a generated two-year
gate world, proves:

- an external withdrawal endpoint's deliberately poisoned slot stays untouched
  while the customer is debited once; an external cash-depository source
  credits only the customer;
- external-to-external, unknown-internal, unknown-external and invalid-key
  postings reject as unbooked;
- every opening and replayed cash, protection and liquidity value is finite,
  with no `1e18` sentinel;
- settled check deposits and both crypto ramps change only the customer leg;
  wrong kind/direction and generic-endpoint bypasses reject;
- every ATM target is registered, `Bank::external`, ownerless, not
  customer-owned, and from the configured cash-point catalog;
- empty custom cash-service pools fall back to registered
  ATM/depository/check/crypto/biller endpoints with no `unbooked` drops;
- a 300-person / 730-day world has at least two ATM targets, none above 95% of
  ATM rows;
- `csv::Writer` throws on a non-finite double.

The production-window, architecture, thread, chunk and spool equivalence tests
stay the broader parity gates. All four baselines (`golden_run.b2sum` and the
standard, AML and card-fraud table digests) were refreshed after these passed; a second live PostgreSQL run matched every digest, and write/readback,
derived-readback and resume tests passed on the same isolated cluster.

### The gates that must accompany the fix

The pre-fix design, kept as rationale; its hub-bit preconditions were superseded
once no hub existed. Bands measured at the legs, never derived, each with a
disarm that reds it.

- Precondition: `postedBook != nullptr`, the hub bit set on the exported book,
  marked count > 0. A hand-off dropping `flags_` would mark nothing and pass
  (`device-fanout-2026-08` rule 1: how `inf` shipped green).
- Finiteness and agreement: every balance finite or empty; empty count equals
  `is_hub` count. Disarm by reverting suppression and by marking a random
  non-hub.
- Concentration band: top-1 destination share, unfiltered (with a measured floor,
  so a vanished defect fails loudly) and with hubs dropped (a measured band on
  the share removed defeats "mark everything").
- Print, never band, the fraud lift on hub membership: selection is uniform over
  persons, but gate legs have 1 to 9 hub entities, so a lift band is a row-level
  binomial over an entity-level coin (`device-fanout-2026-08` rule 2, sub-gate
  G's error).
- No ceiling at 1.0: collapsed keys read 1.00 by construction
  (`merchant-selection-2026-08` rule 6).
- Conservation: replay through a clone of the opening book; the per-slot delta
  sums to zero or exactly the declared cash sink. Its absence hid 42%.

After B or C, re-measure (do not widen) about 37 constants in
`test_card_endpoint_graph` (four-significant-figure ones like 0.3526, 0.4944,
11.40 were measured) and `test_econ_wiring`'s two drift-parity `checkBand`s.
Its ATM row floor and `$20` lattice check are structural tripwires; keep them.
`test_card_merchant_graph`'s sub-gate G is hub-free. `test_membership` justifies
a keying choice by "the biller pool is tiny (the hub accounts)", which sharding
invalidates.

## Rules this round produced

1. A digest golden pins whatever it is given, including `inf`. Pair each digest
   pin with a domain predicate; finiteness is the cheapest.
2. A synthetic sink must not be drawn from the population it serves (the fourth
   case, after the merchant-ownership register, the residential-proxy address
   and the enumeration probe pool). Here 69% of accounts neighboured a depositor
   still eligible as victim or mule.
3. An infinity is a sentinel and must not cross an export boundary. Infinite
   liquidity is a fair in-simulation "never rejects"; it broke only when an
   exporter read it as a quantity (compare `ts == 0` and
   `device_risk_score = -1`). Give the ledger a separate reporting
   accessor.
4. `std::to_chars` succeeds on infinity: an `errc` says the value could be
   formatted, not that it should exist.
5. When two passes configure one concept, assert they cover the same set
   (`createHub` covered `fundingHubs`, `seedHubAccounts` did not: $33.26M, more
   than the documented hubs made).
6. `1e18` has a $128 quantum, deleting 7 of 18 ATM denominations and 89.7% of
   hub-credit rows. A saturating balance needs a flag, not a magnitude.
7. Central is right, customer-owned is wrong. Real cores centralise the ATM cash
   GL, so per-machine ledger shards model nothing real; the fraud-bearing
   terminal/acceptor pair lives on the transaction, not the posting. Separate
   the money leg from the context leg before choosing a cardinality.

## Remaining limitations

- No separate DE41 terminal or DE42 acceptor fields, on-us/off-us class,
  cash-point coordinates or ATM interchange-fee leg; the `XS…` endpoint combines
  terminal and acceptor at the customer-ledger boundary.
- The clearing contract also covers `check_deposit`, `crypto_ramp_out` and
  `crypto_ramp_in`; see [customer_ledger_boundaries.md](customer_ledger_boundaries.md).
- Consumers that discard channel and external type must exclude typed boundary
  and cash-point nodes from customer-flow WCC and PageRank, or use a future
  explicit cash-point edge.
- Terminal density (13.5 per 10,000 people, minimum two) is a declared anchor
  from the commercial US count, not an official series; calibrate before
  claiming absolute throughput.
- The pre-fix `inf` was reproduced in-process (historical). The repaired tree
  was written to and read back from an isolated PostgreSQL 17 cluster; every
  cell passed the finite-value guard and digests matched on a second run.
- The conservation figure is one leg, one seed (0xC0FFEE, pop 900, 1991-01-01,
  731 days); +42.31% moves with population and window, as hub count scales with
  population and credit mix does not. `--population 70000` was not measured.
- Draw-count claims come from reading code. `seedOwnedAccount`'s 4 to 11 is
  exact per branch, but its mean over the golden's 20 hub accounts was not
  measured, so the lane shift is bounded, not known.
- Every existing behavioural band re-ran after the repair and passed unwidened.
- No consumer of the AML `balance` column was found. The TigerGraph loader
  reads only `cf_*` card-fraud tables and no balance; whatever loads `aml` or
  `aml_txn_edges` is outside this repo and the loader's. Grep that consumer
  before sizing (the `cf_Is_Merchant` lesson).
- The terminal sizing anchor is class S, uncited, and candidates disagree ~8x:
  a US-population share of ~470,000 terminals gives ~700 at pop 500,000;
  1,230,944 ATM rows over realistic throughput gives ~85 (the known 1.7x volume
  shortfall plus throughput variance). Pick the density anchor, print
  throughput, and do not tune ATM volume to close the gap.
