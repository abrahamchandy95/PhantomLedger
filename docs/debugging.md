# Debugging PhantomLedger

`make run` prints only warnings, errors and CLI progress. Diagnostics are
opt-in through `make`: the determinism harness, runtime logging, and SQL corpus
probes.

## Build targets and variables

| target | does |
|---|---|
| `make build` | configure + build (Release by default) |
| `make test` | build + run the CTest suite |
| `make run` | build + run the binary (silent: warnings and errors only) |
| `make run-help` | build + print `--help` |
| `make run-fast` | incremental build, then run (skips reconfigure) |
| `make run-info`, `run-debug`, `run-trace`, `run-mem` | run with diagnostics ([runtime diagnostics](#runtime-diagnostics)) |
| `make rebuild` | clean + build |
| `make clean` | remove the build directory |

| variable | default | meaning |
|---|---|---|
| `CONFIG` | `Release` | CMake build type (`Debug`, `Release`, `RelWithDebInfo`). |
| `BUILD_DIR` | `build` | Out-of-tree build directory. |
| `TESTS` | `ON` | `PL_BUILD_TESTS`: build the C++ tests. |
| `BIN` | `phantomledger` | Binary name for the `run` targets. |
| `ARGS` | *(empty)* | CLI arguments for the `run` targets; the only variable that forwards the CLI. |
| `TOPICS` | `all` | Comma-separated topic filter for the diagnostics targets. |

```sh
make build CONFIG=Debug
make test BUILD_DIR=build-debug CONFIG=Debug
make run ARGS="--usecase standard --days 120 --population 200000"
```

Layout (LLVM-style): `include/phantomledger/<layer>/…` mirrors
`src/<layer>/…` (the project prefix appears once), and a module is reviewed
as the folder pair. Top-level folders are the dependency layers, enforced by
the configure-time include-layer lint. Configure also fails if a `src/*.cpp`
is not registered in `CMakeLists.txt`.

## The determinism harness

`make test` runs the CTest suite. The same `(seed, config)` must give
bit-identical output on a fixed toolchain, pinned by four baselines:

| baseline | pins |
|---|---|
| `tests/golden_run.b2sum` | streamed transaction-corpus digest |
| `tests/golden_tables.md5` | standard use-case tables |
| `tests/golden_tables_aml.md5` | aml-txn-edges tables (fraud-dense config) |
| `tests/golden_tables_card_fraud.md5` | card-fraud tables (same config); its corpus digest must equal the aml one (use-case invariance) |

When a golden fails, the output names the section and first diverging line.
A refactor must move no golden: fix the refactor, never recapture. A model
change recaptures every affected baseline in one named commit describing it,
never mixed with a refactor.

If the architectures may diverge, run the dedicated gates; failures print
per-channel histograms, drop maps and the first differing row:

```sh
ctest --test-dir build -R arch_equivalence      # monolithic vs windowed
ctest --test-dir build -R production_windowed   # production runWindowed() API
ctest --test-dir build -R chunk_invariance      # across chunk strategies
ctest --test-dir build -R thread_invariance     # across thread counts
```

`test_scale_soak` (multi-hour ordering/tie soak) skips unless `PL_SOAK=1`.
Knobs: `PL_SOAK_POP` (default 10000), `PL_SOAK_DAYS` (365), `PL_SOAK_SEED`
(20260724), `PL_SOAK_THREADS` (0 = machine).

## Runtime diagnostics

| target | level | output |
|---|---|---|
| `make run` | warn | warnings and errors |
| `make run-info` | info | plan budgets, run totals, end-of-run stats dumps |
| `make run-debug` | debug | day timing, window advances, warm start |
| `make run-trace` | trace | everything |
| `make run-mem` | info, `mem` only | pre-flight estimates, world footprint, per-stage peak RSS |

All take `ARGS="..."`; all but `run-mem` take `TOPICS=a,b` (default `all`):

```sh
make run-info  ARGS="--usecase card-fraud --population 20000 --days 730"
make run-debug ARGS="--population 2000 --days 60" TOPICS=spending,liquidity
make run-mem   ARGS="--population 70000 --days 365"
```

Lines go to stderr as `[HH:MM:SS] [LEVEL] [topic] file:line  message`. The
topic is a logger column, not a message prefix: filter with `TOPICS=`, not
grep.

| topic | covers |
|---|---|
| `sim` | plan budgets (`targetTotalTxns`, person-days, active spenders), day-loop timing, run totals (wrong volume, slow runs) |
| `spending` | funnel dump: attempts vs emitted per channel/persona, route misses, ledger rejections with reasons, count and liquidity-multiplier distributions, daily snapshots (where the funnel loses volume) |
| `routing` | channel/slot routing (mix drifting from the configured CDF) |
| `clearing` | balance-gated rejections and reasons (overdraft storms, cure/retry) |
| `liquidity` | liquidity-multiplier inputs and outputs (payday suppression or inflation) |
| `entities` | world synthesis: population, registry, counterparty pools |
| `mem` | pre-flight reserve (retained corpus when monolithic, bounded staging when windowed); one-shot world footprint after the world build (per-pack resident bytes, see [ram_derive_dont_store.md](ram_derive_dont_store.md)); peak RSS per world-build stage (worldEntities/worldProducts/worldInfra), base-stream composition, batch settlement (buildLegit → mergeProducts → preFraudSettle → fraudInject → postFraudSettle) and windowed phases (windowedPrologue/phaseA/phaseB). For RAM planning, the windowed-path decision, leak hunting |

Outside `make` (CI, profilers, debuggers), set the variables the targets wrap:

```sh
PL_LOG_LEVEL=debug PL_LOG_TOPICS=spending,liquidity ./build/phantomledger ...
```

- `PL_LOG_LEVEL`: `trace | debug | info | warn | error | off`; default and
  fallback `warn`.
- `PL_LOG_TOPICS`: comma-separated topics or `all`; unset means all.
- `PL_PG`: PostgreSQL connection; unset or empty uses `dbname=phantomledger`
  ([README](../README.md#run)).
- `PL_FILE_ONLY=1`: test infrastructure only (serverless corpus-digest escape;
  `aml-txn-edges` cannot run this way).

In code:

- `PL_LOG_INFO(mem, "peak {:.1f} MB", mb)`: bare topic, `std::format` string
  (`{}`, not printf `%s`). A malformed format string drops the line.
- `PL_LOG_EVERY_N(level, topic, n, ...)` rate-limits hot paths; it takes
  qualified `Level::`/`Topic::` enumerators.
- `kCompileMinLevel` (`include/phantomledger/diagnostics/logger.hpp`) is the
  compile-time floor; raise it to strip DEBUG/TRACE sites from a release build.
- New topic: add it before `kCount` in `diagnostics::Topic` (same header), name
  it in `Logger::topicName` (`src/diagnostics/logger.cpp`), add it to the table
  above.

## Playbooks

| symptom | do this |
|---|---|
| Volume looks wrong | `make run-info TOPICS=sim`: compare `Plan built: targetTotalTxns=…` with the totals. Plan right, output low: `make run-debug TOPICS=spending,liquidity` and find the stage losing volume in the funnel dump. |
| Rejections or overdraft storm | `make run-debug TOPICS=clearing,spending`: reasons from the clearing book; affected personas and channels from the spending dump. |
| RAM for a config | `make run-mem`: the pre-flight line predicts the corpus reserve before allocation; the footprint block gives each pack's MB and B/person; stage lines give peak RSS and live rows. Corpus-dominated: use the windowed path (bounded staging, file-backed spool). Pack-dominated: RAM R2; [ram_derive_dont_store.md](ram_derive_dont_store.md) maps each pack to its consumers and stage. |
| A golden diverged | See [the determinism harness](#the-determinism-harness). |
| Architectures may disagree | Run the four dedicated gates. |
| Fraud rates look off | Corpus QA: [probe PostgreSQL](#corpus-probes-sql) and compare with `docs/fraud_model_audit.md`, the fraud-model authority, which changes only under its authority rule. |
| Run dies before generating | PostgreSQL must be reachable; the run fails fast, by design, when `PL_PG` points nowhere. |

## Corpus probes (SQL)

The streamed ledger, shared by every use case:

```sql
SELECT * FROM transactions ORDER BY row_seq;
```

Card-fraud probes; adapt schema and prefix per use case ([exports.md](exports.md)
has the schema map):

```sql
-- fraud share of the card view (order ~0.1%; uncalibrated until re-pinned to
-- an issuer-side by-number rate, see docs/fraud_model_audit.md)
SELECT count(*) FILTER (WHERE is_fraud = '1')::numeric / count(*)
FROM card_fraud."cf_Payment_Transaction";

-- use_chip mix
SELECT use_chip, count(*) FROM card_fraud."cf_Payment_Transaction"
GROUP BY use_chip;
```

Only `cf_Payment_Transaction.is_fraud` is a label. Card and Party `is_fraud`
and Device and IP `is_blocked` are always 0, kept for positional loading.

Measured values live in `docs/fraud_model_audit.md`, one authority per number.
