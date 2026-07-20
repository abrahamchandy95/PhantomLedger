# PhantomLedger — manual file-by-file review roadmap

A dependency-ordered reading plan for reviewing the whole codebase by
hand. Reviewed bottom-up, every file is read AFTER everything it depends on, so
nothing is mysterious when you reach it.

## Method

1. **One station per sitting.** Each station below is a coherent
   subsystem sized for one to three sessions. Do them in order.
2. **Orient before reading.** Run `graphify query "<station topic>"`
   (and `graphify explain "<concept>"` for anything unfamiliar) to get
   the scoped subgraph; skim `graphify-out/GRAPH_REPORT.md` once before
   station 1. The README's architecture sections cover the same ground
   in prose.
3. **Read the station's tests LAST, as the spec.** Every station lists
   its protecting gates; reading them after the sources tells you which
   properties are load-bearing.
4. **Fix-while-reading is safe if byte-neutral.** Any cleanup you make
   must leave `make test` green with ZERO movement on
   `golden_run.b2sum` / `golden_tables.md5` / `golden_tables_aml.md5`.
   If a change moves a golden, it changed the model — stop and route it
   through a named model version instead.
5. **Model constants get the two-way check.** Code → doc: every magic
   number should trace to a row in `docs/fraud_model_audit.md`.
   Doc → code: every row should trace to WIRED code (grep the constant;
   the boost-cycle-retire-2026-07 round found a documented constant
   with zero consumers — that failure mode is real).

## Standing review lenses (apply to every file)

- **Determinism.** The single sequential `Rng &` is threaded in a fixed
  call order — reordering two calls that both draw from it is an
  output-changing bug even if the logic is "equivalent." Relocatable
  generation (products, family, fraud) must draw only from content-keyed
  lanes (`RngFactory{seed}.rng({"products", "full_schedule"})`).
  Samplers use the house Box-Muller lognormal and fixed draw patterns
  (e.g. exactly 2 uniforms per call) — never `std::` distributions,
  never rejection sampling in keyed paths.
- **Ordering.** `transactions::Comparator` (fundsTransfer scope) is the
  ONLY replay order; `detail::auditKey` is the only row identity. Any
  sort by anything else is a bug.
- **Frozen byte surfaces.** Taxonomy enum values and name strings,
  encoding renderers, `csv::Writer` output, exporter schema headers and
  stems — these ARE the golden bytes. Renames here are model changes.
- **Money.** Amounts go through `roundMoney`/`cents` at the sampler;
  clearing rejects `amount <= 0`.
- **Structure (colocation doctrine).** Single-consumer data lives with
  its consumer; each pipeline stage hands off ONE owned product type;
  callers pass the world, not its derivations; DRY applies to repeated
  logic, never to coincidentally similar shape.
- **C++ pitfalls the project has hit.** Member-init order must match
  declaration order; designated initializers must match field order;
  Clang rejects some nested-class forward uses; `-UNDEBUG` keeps asserts
  live in release builds.

## Stations

### 1. Primitives (~29 files) — `primitives/`, `src/primitives/`

Read order: `random/pcg64.hpp` → `rng.hpp` → `seed.hpp` →
`factory.hpp` → `distributions/` (uniform, normal, lognormal, gamma,
beta, poisson, binomial, cdf, alias) → `time/` (constants, calendar,
almanac, window) → `hashing/` → `crypto/blake2b.hpp` →
`io/callback_streambuf.hpp` → `postgres/` (connection, txn_readback) →
`utils/`, `validate/`, `concurrent/`, `tokens/`.

Check: draw-count stability of every distribution (a branch that
consumes a different number of uniforms per outcome breaks keyed
determinism); calendar edge cases; blake2b vectors.
Gates: `test_pcg64`, `test_rng`, `test_seed`, `test_math`, `test_cdf`,
`test_calendar`, `test_validate`.

### 2. Taxonomies + encoding + identifiers (~36 files) — `taxonomies/`, `encoding/`, `entities/identifiers.hpp`

Every enum VALUE and name STRING here is output; treat the whole
station as a frozen surface. Check enum ↔ name switch exhaustiveness
(e.g. `fraudTypeName`), `toIndex` bijectivity, channel tag bytes
(`channels::Fraud` 0x70–0x7D), `isCurrency` membership, and the
encoding renderers that turn keys into table text.
Gates: `test_channels`, `test_ids`, `test_counterparties`.

### 3. Math models (~8 files) — `math/`

`amounts.hpp` (channel/merchant amount catalog), `paycheck.hpp`,
`momentum.hpp`, `dormancy.hpp`, `seasonal.hpp`, `evolution.hpp`,
`timing.hpp`, `counts.hpp`. Every constant is doc-anchored (L-2/L-3);
run the two-way check here with special care.
Gates: goldens (indirect), `test_spending` (dynamics behavior).

### 4. Entity records (~24 files) — `entities/`

Plain data: people, accounts, cards, merchants, landlords,
counterparties, identity/pii, behaviors, `infra/` (router, devices,
ipv4), `products/` (portfolio, obligation streams, terms ledgers).
Check: the Router's sticky per-person device/IP state (it is MUTABLE
and order-dependent — the reason product/family generation snapshot
pristine copies).
Gates: `test_ownership_invariant` (partly), downstream gates.

### 5. World synthesis (~60 files) — `synth/`, `src/synth/`

Read in pipeline build order (`pipeline/stages/entities.cpp` is the
table of contents): `people/` (incl. `fraud.hpp` ring profile,
`make.hpp` repeat-victimization p .10) → `accounts/` → `personas/` →
`pii/` (pools, samplers, geonames, correlate, sharing, membership) →
`merchants/`, `landlords/`, `counterparties/`, `cards/` → `infra/`
(devices, ips, rings) → `products/` (terms: mortgage / auto_loan /
student_loan / tax / insurance; sampling; obligations) → `family/`.
Check: the ORDER of build calls is output-defining (each consumes the
shared RNG); products use their own content-keyed seed.
Gates: `test_ownership_invariant`, table goldens.

### 6. Relationships (~11 files) — `relationships/`, `src/relationships/`

Family (links, partition, support, builder) and social (communities,
sampler, builder). Check: partition determinism, household topology
constraints (the doc's L-4/L-10 blocks lean on them).

### 7. Transactions + clearing (~12 files) — `transactions/`, `src/transactions/`

`record.hpp` (row layout + Comparator + auditKey), `draft.hpp`,
`factory.hpp` (device/IP routing draws), `clearing/` (ledger,
balance_book, screening, protection, liquidity). This is the heart:
the total order, the row identity, and the settle/reject rules.
Gates: `test_order_ties`, `test_postgres` §4, settlement invariants.

### 8. Spending engine (~70 files) — `activity/`, `src/activity/`

The largest station; take it in three sittings:
(a) `income/` (selection, timestamps, revenue catalog/profiles/draw —
L-10 anchors live here) and `recurring/` (rent, growth);
(b) `spending/market/` (census, paydays, commerce, cards) and
`spending/spenders/`, `spending/obligations/`, `spending/liquidity/`;
(c) `spending/dynamics/` (momentum AR(1), dormancy, paycheck boost,
monthly evolution), `spending/actors/`, `spending/simulator/` (driver,
day loop, warm start), `spending/routing/`.
Check: README "Market Simulator" math vs code; dynamics constants vs
doc; the day loop's thread-count independence (work is partitioned,
draws are per-person lanes).
Gates: `test_spending`, `test_session_vs_simulator`,
`test_thread_invariance`.

### 9. Legit transfers (~65 files) — `transfers/legit/`, `transfers/channels/`, `src/transfers/legit/`, `src/transfers/channels/`

(a) `blueprints/` (plans, paydays) then `ledger/` (passes — the
pass ORDER is the output contract; streams; screenbook; limits;
burdens; card_config);
(b) `routines/` (paychecks, atm, internal, subscriptions,
credit_cards, spending + spending_session, relatives, `family/`);
(c) `channels/` (government cohorts/benefits, credit_cards lifecycle /
cycle / statement / dispute, subscriptions, obligations schedule +
delinquency, insurance premiums/claims).
Check: screened-stream lifetime (obligations hold a span into it);
delinquency/cure parameters vs the doc's F-5-adjacent blocks; Reg E
gap is KNOWN and owner-gated (doc F-4 C3).
Gates: `test_arch_equivalence`, `test_production_windowed`,
window gates.

### 10. Fraud engine (~30 files) — `transfers/fraud/`, `src/transfers/fraud/`

Read: `behavior.hpp` → `rings.cpp` → `playbook.hpp` (17 playbooks,
weights sum 1.00 — doc F-3) → `schedule.*` → `typologies/` (dispatch,
then each of the 9; `unauthorized.*` carries card/ATO/gift-card;
`typologies/amounts.hpp` is the cited sampler home) → `camouflage.*` →
`engine.*` → `injector.*`.
Check: budget denominator = flag-1 rows only; F = pL/(1−p) with
p = .0012 of COUNT; structuring stays ≤ $9,950 (never files CTRs);
fixed draw patterns in every sampler; doc F-1…F-7 row-by-row.
Gates: `test_fraud_amounts`, `test_unauthorized_keyed`, fraud
denominators in table goldens.

### 11. Pipeline + windowed engine (~35 files) — `pipeline/`, `src/pipeline/`

(a) `data.hpp`/`result.hpp` (the stage products), `stages/entities`,
`stages/infra`, `stages/products`;
(b) `stages/transfers/`: orchestrator (the retained-corpus REFERENCE
path — read, don't touch), windowed_run (production), windowed_driver
(Phase A/B fold), window_sources, product_replay, binary_spool,
fraud_emission, ledger_replay;
(c) `chunk/` (schedule, sink, async_sink, flush), `batch/cogen`,
`acceptance/fingerprint`, `invariants.hpp`.
Check: the stage-product rule; cursor-source isolation (pristine
routers + dedicated lanes); the Phase A realized-count → fraud-budget
boundary; spool byte-identity.
Gates: the whole windowed suite (`test_window_invariance`, `_bisect`,
`test_chunk_invariance`, `test_spool_equivalence`, `test_resume`,
`test_arch_equivalence`, `test_production_windowed`,
`test_fingerprint`, `test_scale_soak`).

### 12. Exporters (~45 files) — `exporter/`, `src/exporter/`

Read: `csv.hpp` (the COPY-payload renderer) → `common/` (framework,
table + TableCapture, render, hashing, minhash, pii_render) →
`sinks/` (golden, PG mirror) → `schema.hpp` (kernel + kLedger) → then
one exporter at a time with its colocated schema: `standard/`,
`mule_ml/`, `aml/` (incl. `sar.hpp` world-form entry point), and
`aml_txn_edges/` (StreamProducts, streaming, derived bundle, labels).
Check: PostgreSQL-only (no file paths anywhere); one render into
`TableTarget{pg, capture}`; the four exporters stay separate by design
(AHA — do not "unify" them); derived-bundle parity between readback
(windowed) and corpus (reference) builders.
Gates: `test_table_golden`, `test_run_golden`, `test_pipeline_e2e`,
`test_sink`, `test_golden`, `test_minhash_parity`, `test_pg_readback`,
`test_derived_readback`, `test_postgres`, `test_schedule`.

### 13. App shell + build (~10 files) — `app/`, `src/app/`, CMake

options → parsers → cli (teaching die()s) → setup → progress →
main (parse → resolveBackend → runWindowedStream). Then root
`CMakeLists.txt` (source-list audit, `-UNDEBUG`) and
`tests/CMakeLists.txt` (gate roster + retirement notes).
Check: surface stays `--usecase --population --days --seed --start`;
env only `PL_PG`/`PL_THREADS`/`PL_LOG_*` + test-infra vars.

### 14. Docs + baselines (2 sittings)

Re-read `docs/fraud_model_audit.md` end-to-end with the code fresh in
mind — this is where the review pays off; run the doc → code direction
of the two-way check on every table row. Then the golden baselines
themselves (`golden_run.b2sum`, `golden_tables{,_aml}.md5`) and
`README.md` for drift against what you just read.

## Suggested pace

Stations 1–4 are quick (interfaces and tables): 1 session each.
Stations 5, 9 are 2 sessions; station 8 is 3; stations 10–12 are 2
each; 13–14 are 1 each. Roughly 18 sessions at 30–45 files per
session; at one session per day, a month of steady reading covers the
entire tree in dependency order.
