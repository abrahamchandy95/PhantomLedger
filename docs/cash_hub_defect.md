# ✅ CLOSED: the shared cash hub — `cash-hub-defect-2026-08`

**STATUS: CLOSED IN THE CUSTOMER-LEDGER PROJECTION.** Sections 1–5 preserve the
pre-fix diagnosis and measurements as a regression baseline. The implemented
disposition is recorded in section 6 and its executable gates in section 7.
This document lives in `docs/` deliberately: `CLAUDE.md` is git-ignored and a
previous citation kept only there did not survive a clone.

**Before the repair**, every ATM withdrawal was credited to a single randomly
chosen customer's checking account, which reported infinite liquidity and
exported as `inf`. **Now**, ATM rows use multiple ownerless, geographically
local `Bank::external` processor endpoints. Key-aware clearing debits only the
customer leg and never credits an endpoint balance; no customer is selected as
infrastructure, no hub flag or infinite balance exists, and non-finite CSV
values are rejected.

The endpoint key is the combined terminal/acceptor context available in the
current two-ended transaction schema. It is not asserted to be a terminal-level
general-ledger account. Real vault-cash and network-settlement GLs remain
outside this customer-ledger projection, avoiding both a fictitious customer
account and a new system-wide graph sink.

---

## 1. What is confirmed

### 1.1 The observed export

Driving the real `clearing::Ledger` and the real `exporter::csv::Writer` through
the exact production expression at
[aml/vertices.cpp:234](src/exporter/aml/vertices.cpp:234), the exported cell is:

```
A0000000011,0.0
A0000000012,inf
```

Reproduced end to end through `aml::exportAll` and
`aml_txn_edges::exportAll` at population 400: **4 of 1,674 `Account` rows and 4
of 2,089 `Internal_Account` rows carry a non-finite balance**, for example
`A0000000203,A0000000203,inf,2024-01-26 00:03:00,active,...`. The count is
exactly the hub count, `max(1, 0.01 × population)`, which is **700 rows at the
shipped default `--population 70000`** and 100 at the pop-10,000 acceptance
config.

The original minimal reproducer is retained at
[docs/cash_hub_inf_repro.cpp](docs/cash_hub_inf_repro.cpp) as a **historical
pre-fix snapshot**. It intentionally calls the removed `Ledger::createHub`
API and therefore does not build against the repaired tree; the executable
replacement is `tests/test_cash_boundaries.cpp`, which proves both that the
old infinite value is impossible to export and that boundary endpoints never
receive a balance mutation.

### 1.2 The measured super-node

Measured on the 23,866,506-row production export
(`MulePatternLearner/data/transactions.csv`, 499,409 distinct accounts):

| quantity | value |
|---|---|
| Distinct ATM destination accounts in the whole corpus | **1** (`A0000247513`) |
| Rows credited to it | **1,823,332 = 7.64% of the corpus** |
| ...of which `atm_withdrawal` | 1,230,944 |
| ...of which `cc_interest` | 352,271 |
| ...of which `cc_late_fee` | 239,313 |
| ...remainder | 791 subscription, 10 p2p, 2 camouflage_bill, 1 insurance_claim |
| Rows it sources | 144 |
| Distinct counterparties | **344,576 = 69.0% of all accounts** |
| Its rank by in-degree | **1st** |
| Next highest | `XM00000001` at 176,196, the external-unknown sentinel |

It is the highest-degree vertex in the graph and the only one in the top eight
that is not `X`-prefixed, meaning it is the only one a consumer cannot recognise
as non-customer infrastructure.

### 1.3 Money conservation is broken, and by a lot

Replaying every emitted row through a clone of the opening book and summing
per-slot cash deltas (which must be zero under conservation) on a
900-person / 731-day leg, seed `0xC0FFEE`, 668,247 rows, gross $85,682,895.37:

**Net cash created: +$36,248,870.40, or +42.31% of gross applied value.**

Decomposition, agreeing with the direct measurement to within one floating-point
quantum:

| source | rows | amount |
|---|---|---|
| The 9 seeded `1e18` roster hubs | 8,389 | +$5,050,718.05 |
| The 30 `fundingHubs`, `createHub`'d but **never seeded** (cash 0.00) | 35,319 | +$33,260,807.20 |
| Destroyed by `1e18` rounding on credits into seeded hubs | 120,568 | −$2,062,782.85 |

The larger contributor is the second row and it is arguably a **separate bug**:
`limits.cpp` calls `createHub` on every `fundingHubs` key but
`seedHubAccounts` only covers `plan.counterparties().hubAccounts`, so 30
accounts with a zero balance and a bypassed funding screen source $33.26M.

`Ledger::invalid`, the external-key void, is **not** the culprit and the earlier
framing that suspected it is corrected here: 0 rows resolve to `invalid` on
either leg, because `limits.cpp` registers every registry record including
`Bank::external` counterparties, and settlement resolves with `findAccount` then
calls the index-based `transferAt`. The key-based overload that forces external
to `invalid` is never used by settlement.

### 1.4 The `1e18` quantum silently deletes most ATM cash

`kHubCash = 1e18` at
[balance_book.hpp:141](include/phantomledger/transactions/clearing/balance_book.hpp:141)
is exactly representable and its ulp is exactly **128.0**. Every credit is
therefore quantised to `128 × round(amount / 128)`, and **every credit at or
below $64 vanishes entirely** (ties-to-even eats exactly $64.00).

The ATM ladder sits squarely in the vanishing range. Of the 18 entries in
`kAtmAmounts`, **7 ($20, $40 ×3, $60 ×3) credit $0.00 to the hub no matter how
often they fire**; $80 through $160 all credit exactly $128; $200 and $300
credit $256. On the real corpus, 120,568 rows credit a seeded hub for a true
$3,697,214.85, the book records $1,634,432.00, and **108,093 of those rows
(89.7%, $2,376,059.08) vanish without trace**.

### 1.5 The hub owner's behaviour is distorted too

The user's instinct that this "makes someone unnecessarily rich" is correct, and
the effect is broader than the balance column. Because `liquidity()` and
`availableCash()` both return infinity:

- The liquidity ratio clamps, so the spending multiplier **pins permanently at
  `kCeiling = 1.10`**: hub owners spend at the maximum multiplier for the whole
  run.
- `selectPaymentRoute`'s "cash short, use the card" fallback **can never fire**
  for them.
- They are excluded from salary, rent, revenue, ATM and subscriptions, but
  **not** from spending, cards or fraud selection. Measured at pop 900: 9 of 9
  hub persons hold credit cards (1 to 8 each), and 18 fraud rows across 4 rings
  touch a hub person.
- Their own ordinary consumer activity is what creates most of the money above:
  salary 30,288 rows / $28.9M, cash_deposit 5,270 / $4.6M,
  client_ach_credit 4,624 / $4.0M.

No exported flag distinguishes them. `entity::account::Flag` has
fraud/mule/victim/external/shell and **no hub member**; the ledger's `hub` bit
lives only in memory and never reaches a registry record or any export.

---

## 2. The causal chain, five links

Each link is independently verified, and each is a candidate cut point.

1. **`selectHubAccounts` picks customers.**
   [plans.cpp:67](src/transfers/legit/blueprints/plans.cpp:67) samples roster
   *person* indices without replacement, then dereferences
   `census.ownership->primaryIndex(person)` to get an account key. Count is
   `clamp(population × fraction, 1, personCount)` with `fraction = 0.01`
   defaulted in two structs, forwarded once, and **never overridden anywhere in
   `src/` or `tests/`**. Because the count is floored at 1, no configuration
   value can turn this off.
2. **One key carries four production roles.**
   [atm.cpp:220](src/transfers/legit/routines/atm.cpp:220) takes
   `hubAccounts.front()` as the ATM network. The same key is the cash-deposit
   source ([passes.cpp:127](src/transfers/legit/ledger/passes.cpp:127)), the
   card `issuerAcct`, and the head of `billerAccounts` (which *is* the hub
   vector, copied whole). Two further roles, employers and landlords, exist as
   fallbacks and are dead in production.
3. **The ledger gives hubs infinite liquidity.**
   [ledger.cpp:129](src/transactions/clearing/ledger.cpp:129) returns
   `std::numeric_limits<double>::infinity()` when `isHub(idx)`. `availableCash`
   at line 139 does the same. `decide()` skips the sufficiency test entirely for
   a hub source, and `applyTransfer` guards the source debit on `!srcHub` while
   the destination credit has no hub branch at all.
4. **Both AML exporters read that value as the balance.**
   [aml/vertices.cpp:234](src/exporter/aml/vertices.cpp:234) and
   [aml_txn_edges/vertices.cpp:179](src/exporter/aml_txn_edges/vertices.cpp:179)
   write `roundMoney(finalBook->liquidity(rec.id))`. `roundMoney` is
   `std::round(x * 100.0) / 100.0`, which passes infinity through unchanged.
   `finalBook` is non-null in **both** engines and **both** exporters, and the
   `hub` bit survives every hand-off because `Ledger::clone` is the copy
   constructor and `flags_` is a plain member.
5. **The CSV writer serialises it silently, and skips its own guard by
   accident.** [csv.cpp:44](src/exporter/csv.cpp:44) calls `std::to_chars`,
   which returns `std::errc{}` for infinity, so the throw is not taken. The
   trailing-zero fixup probes for `.eEnN`, which **matches the `n` in `inf`**,
   so no `.0` is appended and nothing is quoted. The cell is the bare three
   characters `inf`.

Then, downstream, `TableMirror` creates every mirrored column as **`text`**, and
the `COPY` is `FORMAT csv` with the token unquoted, so `inf` lands in PostgreSQL
as a three-character string with no error. If a consumer later copies that text
into a `double precision` column, PostgreSQL **accepts** it as `Infinity`
(float8 input recognises the special values), so the downstream failure mode is
a silently poisoned feature, not a load abort. A `numeric` target on PostgreSQL
13 or earlier would abort instead, and an integer target always would.

---

## 3. Why nothing caught it

This is the reusable part.

- **Zero finiteness assertions exist in the 68-test suite.**
  `grep -rn 'isfinite|isinf|std::isnan' tests/*.cpp` returns nothing. Every
  `balance` hit in `tests/` is a `balance_book.hpp` include or a `BalanceRules`
  fixture, never an exported value.
- **`test_pipeline_e2e` renders the `inf` rows today and passes**, because
  `expectTable` checks only that the table stem exists and is non-empty.
- **The digest golden pins the defect.** `tests/golden_tables_aml.md5` line 35
  digests `aml_txn_edges_vertices_Account` at 45,188 rows, and column 7 of that
  header is `balance`. The pinned md5 therefore already encodes the `inf` text,
  and **fixing the bug will red that golden**. A digest cannot tell you a value
  is absurd; it tells you only that it has not changed.
- **The plain `aml` use case is covered by no golden at all.** No test in the
  repo ever runs `--usecase aml`; `test_table_golden`'s three sections are
  standard, aml-txn-edges and card_fraud.
- **No test asserts anything about hub accounts.** `hubSet` appears zero times
  in `tests/`. `hubAccounts` appears only as unchecked harness plumbing, and
  `gate_world.hpp` merely hardcodes `fraction = 0.01`.
- **No conservation invariant exists anywhere.** The only book-wide check is a
  bit-exact FNV hash used for cross-architecture equality, which is satisfied by
  any imbalance so long as both legs produce the same one.
- **Neither `CLAUDE.md` nor `docs/fraud_model_audit.md` mentions the hub,
  `kHubCash` or `1e18` anywhere** before this round.

---

## 4. What real systems do, and why it reframes the fix

Researched 2026-08-18. This is the part that decides which option is right, and
it contradicts both obvious designs in opposite directions.

### 4.1 A centralised cash GL is CORRECT. The customer account is not.

- **On-us withdrawal** (the bank's own machine): DEBIT the customer deposit
  liability, CREDIT the bank's own currency-and-coin asset. [Federal Reserve
  Regulation D](https://www.federalreserve.gov/frrs/regulations/section-2042-definitions.htm)
  defines vault cash to include currency held at proprietary ATMs, which
  establishes that an on-us dispense reduces a balance-sheet asset of the
  issuer. Oracle FLEXCUBE names the account `ATM Cash GL`.
- **Off-us withdrawal** (another bank's machine): the credit side is neither
  cash nor a nostro. It is a **network settlement GL** carrying a net position
  against the interchange network, because none of the issuer's own currency
  left the building. The acquirer posts separately to its own `Acquirer Cash GL`.
  The [Federal Reserve Payments Study
  glossary](https://www.federalreserve.gov/paymentsystems/files/FRPS_2016_DFIPS_Glossary.pdf)
  likewise distinguishes on-us and foreign ATM activity rather than treating
  all terminals as one depositor account.
- **Cardinality, the direct answer.** FLEXCUBE uses two levels: one set of GLs
  per *bank*, and a per-*terminal* cross-reference row that points at a GL.
  Terminals are many-to-one onto GLs, and the manual states that the normal case
  is **centralised**, with per-branch fan-out appearing only when multiple
  switches are involved. Settlement accounts shard per network and per business
  line, so single digits to low tens.

So the fault is not that there is one cash account. **The fault is that the one
cash account is a depositor's checking account, that it reports infinite money,
and that four unrelated economic roles share it.** A real core has distinct
`ATM Cash GL`, `Acquirer Cash GL`, `Deposit Cash GL`, `Settlement GL` and two
mandatory suspense GLs.

Worth encoding while here: **interchange flows in the opposite direction for
cash withdrawals versus purchases.** On a purchase the acquirer pays the issuer;
on a cash withdrawal the issuer pays the acquirer. A generator reusing its
purchase fee direction for ATM rows has the sign inverted.

### 4.2 The graph key a real fraud model sees is the terminal, and it exists

An ATM is keyed by a **pair** of ISO 8583 fields: DE 41 Card Acceptor Terminal
Identification (8 alphanumeric) and DE 42 Card Acceptor Identification Code (15
alphanumeric), one acceptor to many terminals, unique only as a pair. The ISO
20022 equivalent is the `catp.*` family, where `catp.001 ATMWithdrawalRequest`
is sent by the ATM itself, so terminal identity is intrinsic to the message.

Crucially this is **present in issuer-side data**, confirmed against an issuer
processor's own field map rather than inferred: Galileo/SoFi exposes DE41 as
`terminal_id` and DE42 as `merchant_id`, and Marqeta exposes a `card_acceptor`
object. So a terminal layer is not inventing resolution an issuer lacks.

### 4.3 No published AML or fraud dataset uses a monolithic cash node

Across five inspected sources, zero mint one and zero model an ATM terminal:

| dataset | cash counterparty strategy |
|---|---|
| IBM AMLSim | shards onto a `Branch` population (`numBranches = 1000`), rendered `B`-prefixed. **But its cash is dead code**: `isNextStep` returns `false` unconditionally in both `CashInModel` and `CashOutModel`, so it documents an intention it never exercised. |
| PaySim | shards onto 34,749 merchants for 20,000 clients, deliberately hub-free |
| IBM AMLworld (NeurIPS 2023) | cash is an **edge attribute**, one value of Payment Format; 18,069,965 of 176M rows, about 10.3% |
| Neo4j fraud reference | subtype label on a reified Transaction node (`CashIn`, `CashOut`, `Payment`, `Debit`, `Transfer`) |
| Sparkov | omits cash entirely |

### 4.4 The super-node harm is documented, not speculative

- **SALT-GNN** (arXiv:2607.10131) stratifies AML GNN evaluation by recipient
  degree across HI-Small, HI-Medium and AMLSim-32k-5%, finds consistent
  degradation in dense recipient contexts, and states that **aggregate F1 hides
  it**. A degree-stratified report is therefore mandatory, not optional.
- **GCNs are biased toward high-degree nodes** and score them more accurately
  (Tang et al., CIKM 2020, arXiv:2006.15643), so a synthetic super-node does not
  merely degrade the model, it earns flattering metrics that mask the distortion.
- **Graph convolution is Laplacian smoothing** (Li, Han & Wu, AAAI 2018,
  arXiv:1801.07606): a node adjacent to a degree-N hub has that hub in its
  receptive field at every layer.
- **PageRank provably follows the same power law as in-degree** (Litvak et al.,
  arXiv:math/0607507), so a monolithic cash node becomes the top-centrality
  vertex by construction and any centrality feature degenerates into "distance
  to the cash node".
- **Community detection distorts**: modularity's resolution limit merges small
  well-defined communities (Fortunato & Barthélemy, PNAS 2007), and a vertex
  adjacent to everything is the pathological input. This is fatal if fraud rings
  are meant to be recoverable by community detection.

### 4.5 Sizing, if a terminal layer is ever built

There is **no current official US ATMs-per-capita statistic**. The World Bank
indicator sourced from the IMF Financial Access Survey reports the US only
through 2009 (425,010 ATMs, 172.76 per 100,000 adults) and is null for
2010 onward. The only current count located is commercial: Euromonitor via trade
press, 451,500 in 2022, down from a 470,000 peak in 2019. The **withdrawal**
count is official: the Federal Reserve Payments Study 2022 triennial reports
3.7 billion ATM withdrawals in 2021, average value rising from $156 (2018) to
$198 (2021). Derived by arithmetic across sources of different vintage, so flag
it as derived rather than cited: roughly 8,200 withdrawals per ATM per year and
about 135 terminals per 100,000 people.

---

## 5. Where the downstream damage lands

The generator's shape is only half the problem. Nothing downstream can filter it.

- `MulePatternLearner/scripts/pipeline/pre_graph/temporal_flow_aggs.py` requires
  only `src_acct, dst_acct, amount, ts` and **discards the `channel` column that
  is present in the export**, so `HAS_PAID` cannot distinguish cash-out from a
  genuine transfer.
- `mule_ml`'s `Transfer_Transaction.csv` has **no channel column at all**, so an
  ATM withdrawal is byte-indistinguishable from a P2P transfer.
- `MulePatternLearner`'s GSQL feature queries have **no `is_external` guard**:
  `money_flow.gsql` computes `fan_in_ratio`, `pass_through_ratio` and `net_flow`
  over `Account:a` unconditionally; `weight_account_edges.gsql` gives the hub an
  undirected edge to every counterparty with weight equal to the transaction
  count, so nearly every pair clears `cluster_with_wcc.gsql`'s
  `min_link_weight = 2` and **WCC merges the population into one component**,
  making `com_size` (a live model feature) meaningless; `pagerank.gsql` sinks its
  mass into that one node, and `pagerank` is a live model feature too.

---

## 6. Implemented disposition (and the three options considered)

The repair combines the useful parts of options B and C without installing a
new global target:

- Every posting path now classifies a `Bank::external` key before ledger-index
  resolution. Registered external entities remain valid transaction references
  but do not enter the internal account map, are never seeded, and never receive
  a balance mutation. Unknown `Bank::internal` keys are rejected rather than
  being mistaken for an external boundary.
- ATM withdrawals select from population-scaled external processor endpoints
  placed over customer home-area quantiles. Event-time relocation chooses the
  area, the customer's own four nearest endpoints form the local choice set,
  and a draw-free hash gives each customer a stable primary point with
  occasional nearby use. Thus the raw transaction graph has a distributed
  cash-access layer rather than a customer or system-wide supernode.
  (Amended by atm-spread-2026-09: the four were first chosen per area with a
  pool-index tie-break, so every resident of a city shared its four
  lowest-numbered points. Ties at the cut are now broken by a per-(person,
  area, rail) hash window, and worlds with four or fewer points per rail are
  byte-identical to the previous selection.)
- Cash deposits, billers, the card issuer, employers, and landlords use
  distinct external roles/pools; none falls back to a roster customer.
- The retired population selection is still executed and discarded solely as
  an entropy-compatibility burn. It confers no account status or behavior.

Because the row contract has only `source` and `target`, the chosen cash-point
key carries both transaction context and the external-boundary marker. The
clearing rule—not a terminal balance—carries the money semantics. A future
schema can separate ISO 8583 DE41/DE42 and the centralized GL projection without
changing that invariant.

**Historical implementation-order recommendation.** Move hub
selection onto its own `RngFactory` lane **first**, re-pin the goldens **once**,
and only then change the hub model. Doing it the other way forces a second full
re-pin and a second full band re-measurement.

### Why golden movement is unavoidable for options B and C

Hub selection sits on **the shared sequential RNG stream**, the same `Rng`
reference that then feeds the opening book, the transaction factory, every
income and routine pass, and fraud planning. `selectHubAccounts` spends exactly
`k = hubCountFor(...)` bounded draws, and because Floyd's sampler runs `j` over
`[n-k, n)` with `range = j+1`, **changing the hub count changes the value of the
first draw, not just the last**. Five further channels amplify it: hub accounts
are excluded from opening-balance seeding (`seedHubAccount` is draw-free while
`seedOwnedAccount` spends 4 to 11 draws); five emission passes short-circuit
before their coin on hub membership; subscription scheduling draws one timestamp
per sub per month; and `Rng::bounded` returns 0 **without consuming a u64** when
`range <= 1`, so sharding a pool down to size 1 silently shifts the plan lane.

**This is the repo's own `merchant-churn-2026-07` rule 2 being violated as
written**: a draw whose count depends on data must be last on its own isolated
lane, and this one depends on `populationCount` and sits first on the shared
stream.

**All four goldens move** on options B and C. `golden_tables_aml.md5` and
`golden_tables_card_fraud.md5` share a byte-identical `transactions` digest, so
the exporter-only escape used by `merchant-coordinates`, `merchant-ownership`
and `card-churn` is **not available here**. `kTableCount = 43` does not move.

### Option A: export-only containment (rejected as insufficient)

Add a public `Ledger::isHubAccount(const Key&)` (or a `reportedBalance` that
returns `totalLiquidity` with no hub short-circuit), write the balance as empty
rather than `inf`, and append an `is_hub` column to the account tables so a
consumer can filter.

Suppress rather than clamp: `0.0` invents an empty account, which is a lie, and
the finite `totalLiquidity` is monotone in the hub's row count, so it re-encodes
the degree leak as a numeric outlier. NULL is this repo's established mask
(`merchant-coordinates` rule 2: row absence is the `has_coordinates` mask, never
`0,0`).

**`golden_run.b2sum` cannot move**: nothing touches an rng lane, a funding
decision or a draw count. `Ledger::liquidity` keeps returning infinity
in-simulation deliberately, because clamping it changes `decide()` and therefore
every downstream draw. Exactly one goldened row moves,
`aml_txn_edges_vertices_Account`.

**It does not fix the edges.** All 1,823,332 rows still point at one node and
69% of accounts remain its 1-hop neighbours. This is a capability, not a repair,
and the audit row must say so in its first line or "we shipped a marker" will be
mistaken for "we fixed the hub".

### Option B: named external system counterparties (boundary semantics adopted)

New `entities/counterparties/system_accounts.hpp` on the
`institutional_accounts.hpp` pattern: constexpr keys at a reserved serial base,
`kCashNetwork` and `kBranchVault` as `Role::processor, Bank::external`, and
`kCardIssuerReceivable` as `Role::business, Bank::external`.

**No taxonomy change is needed, and this is the key discovery.** `Role::account`
is `internalOnly`, but the precedent for a non-customer infrastructure
counterparty **already exists**: `bankFeeCollectionKey` and `bankOdLocKey` are
already minted as `Role::business, Bank::external` with reserved high serials
and registered through the external path. `Role::processor` and `Role::platform`
are already `externalOnly` with `XS` and `XP` layouts. Relaxing `internalOnly`
instead would cost a predicate row, a layout constant, a layout-table row and an
exporter label row, versus zero on this route.

Two traps to respect. `validateTransactionAccounts` **throws** on any
transaction whose endpoint is absent from the account `Lookup`, so an external
hub must still be registered via `addAccounts(..., external = true)` or every
ATM, subscription and issuer row aborts the run. And `seedHubAccount` skips
records whose owner is `invalidPerson`, so `kHubCash` would never be written to
an external hub; harmless because the external-key path masks cash reads, but it
is a latent trap.

Containment lever: keep the `rng.choiceIndices` call as a `[[maybe_unused]]`
frozen legacy burn so the blueprint lane spends identical draws and the entity
prefix does not shift.

Free structural win: `exporter::common::isExternalKey` keys on `Bank::external`,
so `TransactionEdgeClassifier::observe` re-routes about 1.8M rows from
`Account_Send`/`Receive` to the Counterparty edge tables **with no exporter
edit**, and the internal-account row carrying `inf` disappears entirely.

### Option C: sharded cash-point layer (distributed transaction layer adopted)

A draw-free, area-anchored `cash_points.hpp` registry of external
`Role::processor` accounts, sized per area over the population-independent
71-city catalogue, resolved by a hash of the account key and home area with
month rotation. Zero new uniforms, so the shared streams stay byte-identical and
`golden_run.b2sum` rows should be unmoved with only the digest moving.

This is the option that actually restores graph structure: the top vertex loses
1,230,944 rows, cash-out becomes a many-terminal bipartite layer, and
`distance(home, cash-out point)` becomes local and consistent with the step-2
merchant geography.

**Historical risk hypothesis, not implemented.** The first review treated
`Fraud::structuring` as cash deposits because `isCurrency` describes it that
way. The generator and README actually define those rows as victim-to-ring
split payments below a reporting threshold. Threading ATM/depository endpoints
into that typology would therefore invent a cash leg and silently change its
meaning. The tag/comment mismatch remains a separate model-version decision;
this cash-boundary repair does not resolve it by guessing.

Secondary risk in the same family: the layer is 100% legitimate, so "touches a
terminal implies not a mule" runs near precision 1.0. Today that oracle is one
memorisable account id; after sharding it becomes a clean bipartite type, which
generalises better and is therefore a **stronger** shortcut.

### Independent fixes to ship regardless of which option is chosen

1. **The 30 unseeded `fundingHubs`.** `createHub` without `seedHubAccounts`
   coverage leaves them at cash 0.00 with a bypassed funding screen, sourcing
   $33.26M. This is the single largest money-creation contributor.
2. **`unauthorized.cpp:589`** is the one unguarded `billerAccounts` read in the
   fraud layer. `pickOne` on an empty span calls `choiceIndex(0)`, which throws.
   The other three fraud consumers all guard.
3. **Three dead or write-only members** to delete first, since they widen every
   grep for no reason: `LegitCounterparties::hubAccounts` (3 writes, 0 reads),
   `CounterpartyAccess::isHub(const Key&)` and `CounterpartyAccess::firstHub()`
   (both zero callers).

---

## 7. Regression gates shipped with the fix

`tests/test_cash_boundaries.cpp` exercises the real replay and a generated
two-year gate world. It proves:

- a registered external withdrawal endpoint leaves its deliberately poisoned
  ledger slot untouched while the customer is debited exactly once;
- a registered external cash-depository source credits only the customer;
- external-to-external, unknown-internal, unknown-external, and invalid-key
  postings reject as unbooked;
- every opening and replayed cash/protection/liquidity value is finite and no
  `1e18` sentinel exists;
- settled check deposits and both crypto ramp directions mutate only the
  customer leg, while wrong kind/direction and generic-endpoint bypasses reject;
- every generated ATM target is registered, `Bank::external`, ownerless,
  absent from the customer-owned set, and selected from the configured
  cash-point catalog;
- deliberately empty custom cash-service pools resolve to registered fallback
  ATM/depository/check/crypto/biller endpoints and produce no `unbooked`
  replay drops;
- a 300-person / 730-day deterministic gate produces at least two observed ATM
  targets and no target may own more than 95% of ATM rows; and
- `csv::Writer` throws on a non-finite double instead of serializing `inf`.

The existing production-window, architecture, thread, chunk, and spool
equivalence tests remain the parity gates for the broader pipeline. The bullets
below are the pre-fix gate design retained as review rationale; their hub-bit
preconditions were superseded by the stronger invariant that the hub mechanism
does not exist.

All four deterministic baselines were intentionally refreshed after those
semantic gates passed: `golden_run.b2sum` and the standard, AML, and card-fraud
PostgreSQL table digests. A second live PostgreSQL run matched every recaptured
digest, and the PostgreSQL write/readback, derived-readback, and resume tests
also passed against the same isolated test cluster.

Every band below was to be **measured at the legs, never derived**, and every one
needs a disarm that reds it.

- **A precondition first, or the whole gate is vacuous.** Assert
  `postedBook != nullptr`, that the hub bit is actually set on the book the
  export reads, and that the marked count is greater than zero. A hand-off that
  clones or restores the book without `flags_` would otherwise mark zero
  accounts and every check below would pass on an empty set. This is
  `device-fanout-2026-08` rule 1, and it is exactly how a literal `inf` shipped
  behind a green suite.
- **Finiteness and agreement.** Every balance cell parses as finite or empty,
  and the empty count equals the `is_hub` count. Disarm by reverting the
  suppression, and separately by marking a random non-hub account.
- **A concentration band, not a presence assert.** Compute the top-1
  destination's share of all rows twice, unfiltered and after dropping hub
  destinations. Assert a measured floor on the unfiltered share too, so the gate
  fails loudly if the defect silently disappears and the check stops meaning
  anything. Then assert a measured band on the *share of rows removed*, which is
  what defeats the cheap "mark everything" disarm.
- **Do not band the fraud lift on hub membership, print it.** Hub selection is
  uniform over persons and so label-independent by construction, but hub counts
  at gate legs are 1 to 9 **entities**. A lift band there is a row-level
  binomial over an entity-level coin, which is `device-fanout-2026-08` rule 2
  and sub-gate G's exact error.
- **Never set a ceiling at 1.0.** Collapsing the keys back into one reads 1.00
  by construction, so a band at the analytic value is a check that cannot fail.
  That is `merchant-selection-2026-08` rule 6.
- **A conservation invariant.** Replay the corpus through a clone of the opening
  book and assert the summed per-slot delta is zero, or is exactly the declared
  cash-sink amount. None exists today, which is why a 42% imbalance was
  invisible.

Bands known to need **re-measuring rather than widening** after options B or C:
roughly 37 constants in `test_card_endpoint_graph` (the four-significant-figure
ones such as 0.3526, 0.4944, 11.40 are the tell that they were measured), and
the two drift-parity `checkBand`s in `test_econ_wiring`. `test_econ_wiring`'s
ATM row floor and `$20` lattice check are structural and survive a correct
refactor, which makes them the right tripwires to keep. `test_card_merchant_graph`'s
sub-gate G is hub-free and needs no re-measurement. `test_membership` carries a
standing comment justifying a keying choice on the grounds that "the biller pool
is tiny (the hub accounts)", and that rationale is invalidated by sharding.

---

## 8. Rules this round produced

1. **A DIGEST GOLDEN PINS WHATEVER IT WAS GIVEN, INCLUDING AN ABSURDITY.**
   `golden_tables_aml.md5` encodes the literal `inf` today, so the correct fix
   reds a green golden. A byte pin answers "has this changed", never "is this
   sane". **Pair every digest pin with at least one predicate on the value's
   domain**, and finiteness is the cheapest such predicate there is.
2. **A SYNTHETIC SINK MUST NOT BE DRAWN FROM THE POPULATION IT SERVES.** This is
   the fourth instance of the disjointness rule, after the merchant-ownership
   register, the residential-proxy address and the enumeration probe pool. Here
   it is at its worst: the sink is a *customer*, so 69% of accounts became the
   1-hop neighbours of a depositor, and that depositor was still eligible for
   victim and mule selection.
3. **AN INFINITY IS A SENTINEL AND MUST NOT CROSS AN EXPORT BOUNDARY.** Infinite
   liquidity is a legitimate in-simulation device for "never rejects". It became
   a defect only when an exporter read the sentinel as a quantity. The repo
   already knows this shape (`ts == 0` is a sentinel, not an instant;
   `device_risk_score = -1` is unavailable, not clean). **Give the ledger a
   separate reporting accessor, so the sentinel cannot be read as data.**
4. **`std::to_chars` SUCCEEDS ON INFINITY, SO AN `errc` CHECK IS NOT A
   VALIDITY CHECK.** `csv.cpp`'s throw was never going to fire, and the `.eEnN`
   probe for the trailing-zero fixup matched the `n` in `inf`, so the one piece
   of code that inspected the rendered text waved it through. **A formatter's
   error code tells you it could format the value, not that the value should
   exist.**
5. **A SEEDING WALK AND A FLAGGING WALK OVER DIFFERENT KEY SETS IS A SILENT
   HOLE.** `createHub` covered `fundingHubs`; `seedHubAccounts` did not. The
   result was 30 zero-balance accounts with a bypassed funding screen sourcing
   $33.26M, which is more money than the documented hubs created. **When two
   passes configure the same concept, assert they cover the same set.**
6. **`1e18` IS NOT A LARGE NUMBER, IT IS A NUMBER WITH A $128 QUANTUM.** Using a
   huge float as "effectively infinite balance" silently deletes every credit at
   or below $64, which is 7 of the 18 ATM denominations and 89.7% of hub-credit
   rows. **A saturating balance needs a flag, not a magnitude.**
7. **CENTRALISED IS RIGHT, CUSTOMER-OWNED IS WRONG, AND THE TWO GOT
   CONFLATED.** Real cores genuinely do centralise the ATM cash GL, so the
   instinct to shard per machine at the ledger level models something no core
   system does. The graph key that carries the fraud signal is the ISO 8583
   terminal and acceptor pair, which lives on the *transaction*, not on the
   ledger posting. **Separate the money leg from the context leg before choosing
   a cardinality.**

---

## 9. Remaining limitations and historical qualifications

- The transaction record does not yet carry separate ISO 8583 DE41 terminal
  and DE42 acceptor fields, on-us/off-us classification, cash-point coordinates,
  or an ATM interchange-fee leg. The current `XS…` endpoint deliberately
  combines terminal/acceptor identity at the customer-ledger boundary.
- The shared clearing contract now also covers `check_deposit`,
  `crypto_ramp_out`, and `crypto_ramp_in`; the full rationale and calibration
  live in [customer_ledger_boundaries.md](customer_ledger_boundaries.md).
- Consumers that discard channel and external-entity type can still misuse any
  shared counterparty. They must exclude typed boundary/cash-point nodes from
  customer-flow WCC and PageRank, or use a future explicit cash-point edge.
- The terminal density (13.5 per 10,000 people, minimum two) is a declared
  synthetic anchor based on the commercial U.S. terminal-count estimate noted
  above, not a current official series. It needs calibration before claims
  about absolute terminal throughput.

- The pre-fix `inf` measurement was reproduced in-process and is retained as
  historical evidence. The repaired tree was additionally written to and read
  back from an isolated PostgreSQL 17 cluster during the fix; all table cells
  passed the new finite-value guard and the recaptured table digests matched on
  a second run.
- The conservation figure is **one leg and one seed** (0xC0FFEE, pop 900,
  1991-01-01, 731 days). The +42.31% share will move with population and window,
  because the hub count scales with population while the credit mix does not.
  The shipped default `--population 70000` was not measured.
- Every draw-count claim in the golden-movement analysis is derived by reading
  the code, not by measuring a delta. The 4-to-11 range for `seedOwnedAccount`
  is exact per branch, but the realized mean over the golden's 20 hub accounts
  was not measured, so the total shared-lane shift is bounded, not known.
- Every existing executable behavioral band was re-run after the repair and
  passed without widening its thresholds.
- **No downstream consumer of the AML `balance` column was located.**
  `tf_gnn_loader_v2` consumes only the `cf_*` card-fraud tables and has no
  reference to an account balance. Whatever loads schema `aml` or
  `aml_txn_edges` is outside both repositories, so the real blast radius of the
  text `inf` is unknown. **Grep that consumer before sizing this fix**, per the
  `cf_Is_Merchant` lesson.
- A terminal-layer sizing anchor is **class S uncited**, and the two candidate
  anchors disagree by roughly 8x: US population share of about 470,000 terminals
  implies ~700 at pop 500,000, while dividing 1,230,944 ATM rows by a realistic
  per-terminal throughput implies ~85. The gap is the known 1.7x volume
  shortfall plus throughput variance. Pick the density anchor, print throughput,
  and **do not tune ATM volume to close it.**
