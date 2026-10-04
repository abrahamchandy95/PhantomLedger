# PhantomLedger: manual review roadmap

Review the codebase bottom-up, so each file comes after everything it depends
on.

## Method

1. One station per sitting (one to three sessions each), in order.
2. Orient with `graphify query "<station topic>"` and
   `graphify explain "<concept>"`; skim `graphify-out/GRAPH_REPORT.md` once
   first. [simulation.md](simulation.md) says the same in prose.
3. Read a station's tests last, as the spec of what is load-bearing.
4. Fix while reading only if byte-neutral: `make test` green, zero movement in
   `golden_run.b2sum`, `golden_tables.md5`, `golden_tables_aml.md5`,
   `golden_tables_card_fraud.md5`. Moving a golden is a model change; route it
   through a named model version.
5. Check constants both ways: every magic number traces to a row in
   `docs/fraud_model_audit.md`, and every row to wired code (grep it;
   `boost-cycle-retire-2026-07` found a documented constant with no consumer).

## Lenses for every file

- Determinism: one sequential `Rng &` in fixed call order; reordering two
  drawing calls changes output. Relocatable generation (products, family,
  fraud) draws only from content-keyed lanes
  (`RngFactory{seed}.rng({"products", "full_schedule"})`). Samplers use the
  house Box-Muller lognormal and fixed draw patterns (e.g. exactly 2 uniforms
  per call); no `std::` distributions, no rejection sampling on keyed paths.
- Ordering: `transactions::Comparator` (fundsTransfer scope) is the only replay
  order, `detail::auditKey` the only row identity.
- Frozen bytes: taxonomy enum values and names, encoding renderers,
  `csv::Writer` output, exporter schema headers and stems. Renaming any is a
  model change.
- Money: `roundMoney`/`cents` at the sampler; clearing rejects `amount <= 0`.
- Colocation: single-consumer data lives with its consumer; each stage hands
  off one owned product type; callers pass the world, not its derivations; DRY
  is for repeated logic, not similar shape.
- C++ pitfalls hit here: member-init and designated-initializer order must
  match declarations; Clang rejects some nested-class forward uses; `-UNDEBUG`
  keeps asserts in release builds.

## Stations

### 1. Primitives (~29 files): `primitives/`, `src/primitives/`

`random/pcg64.hpp` → `rng.hpp` → `seed.hpp` → `factory.hpp` → `distributions/`
(uniform, normal, lognormal, gamma, beta, poisson, binomial, cdf, alias) →
`time/` (constants, calendar, almanac, window) → `hashing/` →
`crypto/blake2b.hpp` → `io/callback_streambuf.hpp` → `postgres/` (connection,
txn_readback) → `utils/`, `validate/`, `concurrent/`, `tokens/`.
Check: draw counts stable per outcome (else keyed determinism breaks),
calendar edges, blake2b vectors. Gates: `test_pcg64`, `test_rng`, `test_seed`,
`test_math`, `test_cdf`, `test_calendar`, `test_validate`.

### 2. Taxonomies, encoding, identifiers (~36 files): `taxonomies/`, `encoding/`, `entities/identifiers.hpp`

All frozen output. Check: enum ↔ name exhaustiveness (`fraudTypeName`),
`toIndex` bijectivity, channel tag bytes (`channels::Fraud` 0x70 to 0x7D),
`isCurrency` membership, key-to-text renderers. Gates: `test_channels`,
`test_ids`, `test_counterparties`.

### 3. Math models (~8 files): `math/`

`amounts.hpp` (channel/merchant amounts), `paycheck.hpp`, `momentum.hpp`,
`dormancy.hpp`, `seasonal.hpp`, `evolution.hpp`, `timing.hpp`, `counts.hpp`. Every constant is doc-anchored
(L-2/L-3): two-way check with care. Gates: goldens (indirect), `test_spending`.

### 4. Entity records (~24 files): `entities/`

Plain data: people, accounts, cards, merchants, landlords, counterparties,
identity/pii, behaviors, `infra/` (router, devices, ipv4), `products/`
(portfolio, obligation streams, terms ledgers). Check: the Router's per-person
device/IP state is mutable and order-dependent, so product and family
generation snapshot pristine copies. Gates: `test_ownership_invariant`
(partly), downstream gates.

### 5. World synthesis (~60 files): `synth/`, `src/synth/`

Build order (table of contents: `pipeline/stages/entities.cpp`): `people/`
(`fraud.hpp` ring profile; `make.hpp` repeat victimization p .10) →
`accounts/` → `personas/` → `pii/` (pools, samplers, geonames, correlate,
sharing, membership) → `merchants/`, `landlords/`, `counterparties/`, `cards/`
→ `infra/` (devices, ips, rings) → `products/` (terms for mortgage, auto_loan,
student_loan, tax, insurance; sampling; obligations) → `family/`. Check: call
order defines output (each consumes the shared RNG); products use their own
content-keyed seed. Gates: `test_ownership_invariant`, table goldens.

### 6. Relationships (~11 files): `relationships/`, `src/relationships/`

Family (links, partition, support, builder), social (communities, sampler,
builder). Check: partition determinism, household topology (L-4/L-10 depend on
it).

### 7. Transactions and clearing (~12 files): `transactions/`, `src/transactions/`

`record.hpp` (row layout, Comparator, auditKey), `draft.hpp`, `factory.hpp`
(device/IP routing draws), `clearing/` (ledger, balance_book, screening,
protection, liquidity): the total order, row identity, settle/reject rules.
Gates: `test_order_ties`, `test_postgres` (ordered read-back identity),
settlement invariants.

### 8. Spending engine (~70 files): `activity/`, `src/activity/`

Three sittings: `income/` (selection, timestamps, revenue catalog/profiles/draw;
L-10 anchors) and `recurring/` (rent, growth); `spending/market/` (census,
paydays, commerce, cards), `spending/spenders/`, `spending/obligations/`,
`spending/liquidity/`; `spending/dynamics/` (momentum AR(1), dormancy, paycheck
boost, monthly evolution), `spending/actors/`, `spending/simulator/` (driver,
day loop, warm start), `spending/routing/`. Check: the [mathematical models](simulation.md#mathematical-models) and dynamics constants
against code and doc; day loop independent of thread count (partitioned work,
per-person draw lanes). Gates: `test_spending`, `test_session_vs_simulator`,
`test_thread_invariance`.

### 9. Legit transfers (~65 files): `transfers/legit/`, `transfers/channels/` and their `src/`

Sittings: `blueprints/` (plans, paydays) then `ledger/` (passes, whose order is
the output contract; streams; screenbook; limits; burdens; card_config);
`routines/` (paychecks, atm, internal, subscriptions, credit_cards, spending +
spending_session, relatives, `family/`); `channels/` (government
cohorts/benefits; credit-card lifecycle, cycle, statement, dispute;
subscriptions; obligation schedule and delinquency; insurance premiums and
claims). Check: screened-stream lifetime (obligations hold a span into it);
delinquency/cure parameters against the F-5-adjacent doc blocks. The Reg E gap
is known and owner-gated (doc F-4 C3). Gates: `test_arch_equivalence`,
`test_production_windowed`, window gates.

### 10. Fraud engine (~30 files): `transfers/fraud/`, `src/transfers/fraud/`

`behavior.hpp` → `rings.cpp` → `playbook.hpp` (17 playbooks, weights sum 1.00;
F-3) → `schedule.*` → `typologies/` (dispatch, then each of 9; `unauthorized.*`
holds card/ATO/gift-card; `typologies/amounts.hpp` is the cited sampler home)
→ `camouflage.*` → `engine.*` → `injector.*`. Check: budget denominator is
flag-1 rows only; F = pL/(1−p), p = .0012 of count; structuring stays ≤ $9,950
(never files CTRs); fixed draw patterns; F-1 to F-7 row by row. Gates:
`test_fraud_amounts`, `test_unauthorized_keyed`, fraud denominators in table
goldens.

### 11. Pipeline and windowed engine (~35 files): `pipeline/`, `src/pipeline/`

`data.hpp`/`result.hpp` (stage products), `stages/entities`, `stages/infra`,
`stages/products`; `stages/transfers/` (orchestrator, the retained-corpus
reference path: read, don't touch; windowed_run, production; windowed_driver,
the Phase A/B fold; window_sources, product_replay, binary_spool,
fraud_emission, ledger_replay); `chunk/` (schedule, sink, async_sink, flush),
`batch/cogen`, `acceptance/fingerprint`, `invariants.hpp`. Check: the
stage-product rule; cursor-source isolation (pristine routers, dedicated
lanes); the Phase A realized-count → fraud-budget boundary; spool
byte-identity. Gates: `test_window_invariance`, `_bisect`,
`test_chunk_invariance`, `test_spool_equivalence`, `test_resume`,
`test_arch_equivalence`, `test_production_windowed`, `test_fingerprint`,
`test_scale_soak`.

### 12. Exporters (~45 files): `exporter/`, `src/exporter/`

`csv.hpp` (COPY-payload renderer) → `common/` (framework, table +
TableCapture, render, hashing, minhash, pii_render) → `sinks/` (golden, PG
mirror) → `schema.hpp` (kernel + kLedger) → each exporter with its colocated
schema: `standard/`, `mule_ml/`, `aml/` (`sar.hpp` is the world-form entry),
`aml_txn_edges/` (StreamProducts, streaming, derived bundle, labels),
`card_fraud/`, `mule_temporal/`, then `econ/` (era reference tables for every
use case). Check: PostgreSQL only, no file paths; one render into
`TableTarget{pg, capture}`; use-case exporters stay separate by design (AHA: do
not unify); derived-bundle parity between readback (windowed) and corpus
(reference) builders. Gates: `test_table_golden`, `test_run_golden`,
`test_pipeline_e2e`, `test_sink`, `test_golden`, `test_minhash_parity`,
`test_pg_readback`, `test_derived_readback`, `test_postgres`,
`test_schedule`.

### 13. App shell and build (~10 files): `app/`, `src/app/`, CMake

options → parsers → cli (teaching `die()`s) → setup → progress → main (parse →
resolveBackend → runWindowedStream); root `CMakeLists.txt` (source-list audit,
`-UNDEBUG`); `tests/CMakeLists.txt` (gate roster, retirement notes). Check: CLI
stays `--usecase --population --days --seed --start`; env stays `PL_PG`,
`PL_THREADS`, `PL_LOG_*` and test-infra variables.

### 14. Docs and baselines (two sittings)

Re-read `docs/fraud_model_audit.md` end to end with the code fresh, checking
every table row doc → code; this is where the review pays off. Then check the
golden baselines, `README.md` and `docs/simulation.md` for drift.

## Pace

One session each for Primitives through Entity records, App shell and Docs;
two for World synthesis, Legit transfers, Fraud engine, Pipeline and Exporters;
three for Spending engine. About 18 sessions at 30-45 files: a month of daily
reading covers the tree.
