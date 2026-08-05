# RAM R2 — derive, don't store

The transaction corpus already streams. What still scales with
population is everything the run RETAINS: the world packs, the
whole-window obligation precompute, and — the measured standout — the
generation prologue's base stream. R2 makes retained state regenerable,
windowed, or disk-spooled without moving a golden byte; changes that
WOULD move bytes go through the model-version pipeline with explicit
owner gates.

Standing constraints (owner directives): byte identity per refactor
round; no new CLI args/knobs/refusals; `mem`-topic diagnostics only;
explicit spool files, never OS paging.

## Measured baselines (owner-run, 20k/730d standard unless noted)

| milestone | prologue peak | run peak | notes |
|---|---|---|---|
| pre-releases (200k) | 14.6 GB | — | worldTotal 538 MB |
| post R2.1→R2.4a | 4653 MB | 5488 MB | fold adds +835 over prologue |
| post R2.4b-2 | 4654 MB | 5219 MB | digest, row count and book hash identical to the resident-span run — spool byte-identity proven end-to-end; spooling cost ~2 s in 751 s |
| post R2.4b-3 | **2839 MB** | 5130 MB | `replayReady=0` on every prologue line; output identical; the peak moved INTO the phase-A fold |
| post R2.4c.0 | 2841 MB | **4494 MB** | probe verdict below; the −636 MB peak delta vs the previous (code-identical) run is allocator/OS variance — peaks are transient-set, not retention-set |

**R2.4c.0 probe verdict:** none of the fold's designed buffers carries
the growth — `preStage` tops out ~100 MB (per-span compaction works),
`prePending` stays under ~2k rows, `preSettled` always drains to the
spool. Phase-A growth (+~700 MB over the spooled baseline) accrues in
the first three generation spans and then goes FLAT: transient
high-water (session per-window output, `stagePreRows` merge
coexistence, allocator retention), not accumulation. There is no
retention lever left in the fold worth a round — **R2.4 is banked.**

**Peak anatomy (current):** the prologue carries one stream view
(`screened`, 481 MB steady at 20k/730d, plus `TxnStreams::add`'s
reallocation-doubling transients ⇒ the 2.8 GB floor); the fold adds
bounded staging plus flat transients. Both parts scale with
population × days (rows), not with population alone.

## Delivered

- **R2.0** — `logWorldFootprint` + this doc (measurement first).
- **R2.1 + R2.3a** — cold-pack release + seed-replay world rebuild
  (pii + device/IP inventories; all use cases except mule-ml); replay
  identity verified in production output.
- **R2.2.0** — obligation tie audit (ties pervasive, day-granular).
- **MODEL: obligation order pin** (owner-approved, goldens recaptured):
  stable_sort ⇒ (timestamp, then generation order).
- **R2.2.1a** — windowed obligation generator (`restrictTo`,
  `emitPerson` core, `generateWindow` — equals the materialized slice).
- **R2.2.1b** — fold-time obligation release (both consumers finish at
  fold start).
- **R2.4a** — payday-inbound release (single consumer; both engines).
- **R2.4b-1** — base-stream replay seam (`BaseReplaySource` +
  `SpanReplaySource`; `Snapshot.baseReplayOverride` plug-in point;
  caller keeps `RunState::baseIdx`).
- **R2.4b-2** — both fold copies on disk (`BaseReplaySpool` 5-field
  records mirroring `advanceBookThrough`; `BinaryCandidateSpool` reuse
  for the base cursor).
- **R2.4b-3** — prologue single-view build (`deferReplayView`; replay
  order derived once at spool time — equal by fundsLess totality, S10).
  Measured: prologue 4654 → 2839 MB, run peak 5219 → 5130 MB.
- **R2.4c.0** — fold-residency inventory (`phaseA:gen` per-span probe,
  `sessionPrepared`/`baseSpooled` timeline probes,
  `ChronoReplayAccumulator::{pendingRows,settledRows}`). Verdict above;
  probes stay as permanent instrumentation.
- **R2.2.1c** — obligation materialization retired: `synthesize()`
  retains ONLY the burden slice [window.start, +kBurdenWindowMonths x
  30 days) — `buildMonthlyBurdens` (both call sites keyed to the window
  start) is the stream's sole resident reader, and the obligation
  `Scheduler` consumes strictly `between(window)`. Both product
  emitters (`ProductTxnEmitter::obligations` — shared by the oracle's
  `ProductReplay::merge` and the windowed `makeProductSource`) now
  derive the whole-window stream transiently via `generateWindow`
  (byte-identical: pinned total order ⇒ independent tie groups ⇒ the
  restricted replay equals the materialized `between()` slice; the
  scheduler's draws follow the events). Plumbing:
  `TransferStage::obligationSynthesis(...)` wires the pipeline's OWN
  synthesis config (the object that built the world's terms) into both
  engines; `mergeProducts`/`makeProductSource` take People. The
  population x window-months precompute is gone from the world — at
  29-year windows this pack alone would have been ~390 MB at 20k.

## Remaining

- **R2.3b** — regenerable attribute view for mule-ml's stream-finish
  reads (the seam R3's shards plug into).
- **R2.5 — prologue windowing (design round FIRST).** The prologue's
  resident `screened` stream scales with person-days: measured ~0.55 KB
  per row at peak (steady + merge transients). Against a 10,592-day
  horizon that put the prologue at ~38 GB for 20k people and ~95 GB for
  50k — i.e. **~179 B per person-day** — so windowing the prologue
  (generation AND its whole-stream aggregations: market paydays,
  burdens, SeededScreens) was the prerequisite for that run at scale.

  **RETARGETED 2026-08 — the current target is ~100,000 people × ~1,096
  days (2022-01-01 through 2024-12-31, the latest 3 full years inside
  the era lock).** That is 109.6M person-days. Two extrapolations, both
  LINEAR and both UNMEASURED at this configuration:

  | basis | rate | 100k × 1,096d |
  |---|---|---|
  | prologue `screened` stream (this doc, 2026-07-20) | 179 B/person-day | **~19.6 GB** |
  | total peak RSS (`loc-accrual-perf-2026-08`: 13.2 GB at 50k × 730d) | 362 B/person-day | **~39.7 GB** |

  **The second number exceeds a 32 GB box, and it is the one that
  binds.** `loc-accrual-perf-2026-08` rule 7 already states memory — not
  time — is the constraint at the target config. Treat both figures as
  ESTIMATES: neither was measured at 100k × 1,096d, and linear
  extrapolation across both a 2x population and a 1.5x horizon is
  exactly the kind of derivation this repository's own rules say to
  distrust ("a gate on the answer is not a gate on the cost"). Measure
  before planning around either:

  ```
  make run-mem ARGS="--usecase card-fraud --population 50000 --days 1096 --start 2022-01-01"
  ```

  Then double the population and confirm the slope before committing to
  100k. If the total peak does land near 40 GB, the options are prologue
  windowing (this item), population sharding (R3), or a shorter horizon
  — and that is an owner decision, not an implementation detail.
- **R3** — population sharding (design doc first; post-R2 spine ≈
  400 B/person ⇒ 1B ≈ 400 GB on one box — sharding is the answer).

## Acceptance, every round

`make test` green, NO golden movement (except explicitly green-lit
model rounds); `make run-mem` shows the targeted retention gone at the
targeted phase; no CLI surface change.
