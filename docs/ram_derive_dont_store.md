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
| post R2.4b-3 | **2839 MB** | **5130 MB** | `replayReady=0` on every prologue line; output identical again; **the peak moved INTO the phase-A fold** (`screenedSpooled` 3680 → `phaseA` 5130) |

**Peak anatomy (post b-3):** the prologue now carries one view
(`screened`, 481 MB steady, plus its reallocation-doubling transients in
`TxnStreams::add` ⇒ the 2839 MB floor). The run peak accrues INSIDE the
phase-A fold: +1449 MB above the spooled baseline, in buffers the design
says are bounded (`preStage_` compacts per settled span; the
accumulator's pending queue drains per chunk; phase B's stages erase
consumed prefixes). Unmeasured suspects: the 3-month generation chunks
(~2.3M rows staged per `Session::advance` at this scale), `mergeSorted`'s
old+new coexistence in `stagePreRows`, the pending queue's 136-B
`QueuedItem`s, and settled batches awaiting `takeSettledBefore`.
R2.4c.0 instruments exactly these.

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
- **R2.4b-2** — both fold copies on disk: `screened` via
  `BaseReplaySpool` (5-field records, exact `advanceBookThrough`
  mirror), `replayReady` via `BinaryCandidateSpool`/`BinarySpoolCursor`
  (bit-identical records, audit-order verified). Fold-resident base
  stream → two bounded buffers + temp files.
- **R2.4b-3** — prologue single-view build:
  `TxnStreams::deferReplayView()` (windowed composition only; the
  monolithic oracle keeps both views); replay order derived ONCE at
  spool time via `sortForReplay` — equal to the retired incremental
  merge because `fundsLess` (auditKey) is total for every
  output-affecting purpose (rows comparing equal are byte-identical,
  the S10 re-pin). Measured: prologue 4654 → 2839 MB, run peak
  5219 → 5130 MB, output identical.

## R2.4c — fold residency (CURRENT)

The run peak now accrues inside the phase-A fold, not the prologue.

1. **R2.4c.0 — inventory (this round).** Per-generation-span
   `phaseA:gen` probe in the windowed driver (`preStage` /
   `prePending` / `preSettled` rows via
   `ChronoReplayAccumulator::{pendingRows,settledRows}`), plus
   `sessionPrepared` / `baseSpooled` timeline probes in windowed_run so
   session construction and the one-shot replay sort are separable from
   the fold's own growth. Diagnostic-only, `mem`-gated.
2. Measurement decides the lever. Candidates, all residency-only with
   zero CLI surface: a smaller default generation chunk (the
   window-invariance gates prove output does not depend on chunking —
   the RNG-lane design exists precisely so window boundaries cannot
   move draws); exact reserves or bounded batch merges in
   `stagePreRows`; tighter settled-batch draining if `preSettled`
   dominates.

The monolithic oracle keeps both views and full spans (library-level;
untouched).

## Remaining after R2.4

- **R2.2.1c** — retire the obligation materialization (3-month burden
  slice at build; both product emitters on `generateWindow`).
- **R2.3b** — regenerable attribute view for mule-ml's stream-finish
  reads (the seam R3's shards plug into).
- **R3** — population sharding (design doc first; post-R2 spine ≈
  400 B/person ⇒ 1B ≈ 400 GB on one box — sharding is the answer).

## Acceptance, every round

`make test` green, NO golden movement (except explicitly green-lit
model rounds); `make run-mem` shows the targeted retention gone at the
targeted phase; no CLI surface change.
