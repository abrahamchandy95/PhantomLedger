# Debugging PhantomLedger

PhantomLedger is **silent by default**: a plain `make run` prints only
warnings and errors (plus the CLI's own progress output). Every
diagnostic surface is opt-in and reachable through `make`, so debugging
never requires remembering environment variables.

There are three debugging surfaces, from coarsest to finest:

1. the **determinism harness** — `make test` and the golden baselines;
2. **runtime diagnostics** — the topic/level logger and the RAM probes;
3. **corpus probes** — SQL against the PostgreSQL output.

---

## 1. The determinism harness

`make test` builds and runs the CTest suite. The suite's spine is
byte-identity: the same `(seed, config)` must produce bit-identical
output on a fixed toolchain, pinned by four baselines:

| Baseline file | Pins |
|---|---|
| `tests/golden_run.b2sum` | the streamed transaction-corpus digest |
| `tests/golden_tables.md5` | standard-use-case PostgreSQL table content |
| `tests/golden_tables_aml.md5` | aml-txn-edges table content (fraud-dense config) |
| `tests/golden_tables_card_fraud.md5` | card-fraud table content (same fraud-dense config; its corpus digest must equal the aml section's — the use-case-invariance pin) |

**When a golden test fails**, triage in this order:

1. Read the failure output — it names the section and the first
   diverging line, so you know *which* output moved, not just *that*
   something moved.
2. Decide: is the divergence **intended** (a deliberate model change) or
   **unintended** (a refactor that was supposed to be byte-identical)?
   Refactor rounds must show ZERO golden movement; if one moves a
   golden, the refactor has a bug — do not recapture.
3. For deliberate model changes only: recapture every affected baseline
   in one named commit that describes the model change. Never mix a
   recapture with a refactor.

Architecture-level determinism has its own dedicated gates — run them
directly when you suspect a path divergence rather than a model change:

```sh
ctest --test-dir build -R arch_equivalence      # monolithic vs windowed
ctest --test-dir build -R production_windowed   # production runWindowed() API
ctest --test-dir build -R chunk_invariance      # output invariant across chunk strategies
ctest --test-dir build -R thread_invariance     # output invariant across thread counts
```

`test_scale_soak` is skipped in the default run (multi-hour); it is the
full-scale soak for ordering/tie audits.

---

## 2. Runtime diagnostics

### Make targets

| Target | Level | What you get |
|---|---|---|
| `make run` | warn | silence — warnings and errors only |
| `make run-info` | info | run lifecycle: plan budgets, run totals, end-of-run stats dumps |
| `make run-debug` | debug | per-day detail: day timing, window advances, warm-start |
| `make run-trace` | trace | everything the logger can say |
| `make run-mem` | info, `mem` only | RAM reporting (pre-flight estimates, world footprint, per-stage peak RSS), nothing else |

All accept `ARGS="..."` (forwarded to the binary) and, except `run-mem`,
`TOPICS=...` — a comma-separated topic filter (default `all`):

```sh
make run-info  ARGS="--usecase card-fraud --population 20000 --days 730"
make run-debug ARGS="--population 2000 --days 60" TOPICS=spending,liquidity
make run-mem   ARGS="--population 70000 --days 365"
```

Log lines are formatted `[HH:MM:SS] [LEVEL] [topic] file:line message`
on stderr.

### Topics

| Topic | Covers | Reach for it when |
|---|---|---|
| `sim` | run lifecycle: plan budgets (`targetTotalTxns`, person-days, active spenders), day-loop timing, run totals | overall volume looks wrong; runs are slow; you want the plan the simulator committed to |
| `spending` | the emission-funnel stats dump: per-channel/persona attempts vs emitted, route misses, ledger rejections with reasons, count and liquidity-multiplier distributions, per-day snapshots | transaction counts or channel mix are off; you need to see *where* in the funnel volume is lost |
| `routing` | payment channel/slot routing decisions | channel mix drifts from the configured CDF |
| `clearing` | ledger screening: balance-gated rejections and their reasons | rejection spikes, overdraft storms, cure/retry behavior |
| `liquidity` | liquidity-multiplier inputs and outputs | spending looks suppressed or inflated around paydays |
| `entities` | world synthesis | population, registry, or counterparty-pool issues |
| `mem` | RAM observability: the planner's **pre-flight** reserve estimate (retained corpus in monolithic mode, bounded staging in windowed mode), the one-shot **world footprint** report after the world build (per-pack resident bytes — the RAM R2 measurement, see `docs/ram_derive_dont_store.md`), plus `[mem]` peak-RSS lines across the world build (worldEntities/worldProducts/worldInfra), the batch settlement stages (buildLegit → mergeProducts → preFraudSettle → fraudInject → postFraudSettle), and the windowed phases (windowedPrologue/phaseA/phaseB) | RAM planning; deciding whether a config needs the windowed streaming path; leak hunting |

### Raw environment variables

The make targets wrap two env vars, useful when running the binary
outside make (CI, profilers, debuggers):

```sh
PL_LOG_LEVEL=debug PL_LOG_TOPICS=spending,liquidity ./build/phantomledger ...
```

- `PL_LOG_LEVEL` = `trace | debug | info | warn | error | off`
  (default `warn`; unrecognized values fall back to `warn`).
- `PL_LOG_TOPICS` = comma-separated topic names, or `all`. When set,
  only the listed topics are enabled; unset means all topics.

Two more env vars matter for runs generally: `PL_PG` overrides the
PostgreSQL connection (unset/empty uses the code default
`dbname=phantomledger`; see README Usage), and `PL_FILE_ONLY=1` is test
infrastructure only (serverless corpus-digest escape).

`PL_LOG_EVERY_N(level, topic, n, ...)` exists in code for rate-limited
hot-path sites; and `kCompileMinLevel` in
`include/phantomledger/diagnostics/logger.hpp` is the compile-time
floor — tighten it to strip DEBUG/TRACE call sites from a release
binary entirely.

### Adding a topic

1. Add the enumerator before `kCount` in `diagnostics::Topic`
   (`include/phantomledger/diagnostics/logger.hpp`).
2. Name it in `Logger::topicName` (`src/diagnostics/logger.cpp`).
3. Document it in the topics table above.

---

## 3. Playbooks — what to run when

**"Transaction volume looks wrong."**
`make run-info TOPICS=sim` first: compare the planned budget
(`Plan built: targetTotalTxns=…`) with the finishing totals. If the plan
is right but the output is low, `make run-debug TOPICS=spending,liquidity`
and read the funnel dump: attempts vs emitted per channel, route misses,
ledger rejections, and the liquidity-multiplier distribution tell you
which stage is eating the volume.

**"Too many rejections / overdraft storm."**
`make run-debug TOPICS=clearing,spending` — rejection reasons come from
the clearing book; the spending dump shows which personas/channels are
affected.

**"How much RAM will this config take?"**
`make run-mem` — the pre-flight line predicts the corpus reserve before
anything allocates; the **world footprint** block then shows which
world packs hold the resident bytes (per pack, MB, and B/person); each
stage line reports measured peak RSS and the live transaction rows it
holds. If peak RSS is dominated by the posted corpus, the windowed
streaming path (bounded staging + file-backed spool) is the mitigation;
if it is dominated by world packs, that is the RAM R2 program —
`docs/ram_derive_dont_store.md` maps each pack to its consumers and its
derive-don't-store stage.

**"A golden diverged after my change."**
Section 1 above. Refactor ⇒ fix the refactor; model change ⇒ recapture
in a named commit.

**"The two architectures might disagree."**
Run the four dedicated gates listed in section 1; their failure output
includes per-channel histograms, drop maps, and the first differing row.

**"Fraud rates look off."**
That is corpus QA, not logging: probe PostgreSQL (section 4) and compare
against the pinned measurements in `docs/fraud_model_audit.md` — that
document is the authority on the fraud model; it changes only through
its merge-script protocol.

**"The run dies before generating anything."**
A reachable PostgreSQL server is required; the run fails fast when
`PL_PG` points nowhere. That is by design.

---

## 4. Corpus probes (SQL)

The streamed ledger is shared by every use case:

```sql
SELECT * FROM transactions ORDER BY row_seq;
```

Canonical QA probes (card-fraud examples; adapt schema/prefix per
use case — see README Export Formats for the schema map):

```sql
-- fraud share of the card view (order ~0.1%; the external anchor is
-- being re-pinned to an issuer-side BY-NUMBER rate — see the roadmap)
SELECT count(*) FILTER (WHERE is_fraud = '1')::numeric / count(*)
FROM card_fraud."cf_Payment_Transaction";

-- use_chip mix
SELECT use_chip, count(*) FROM card_fraud."cf_Payment_Transaction"
GROUP BY use_chip;
```

Measured values worth knowing are recorded in
`docs/fraud_model_audit.md`, not here — one authority per number.
