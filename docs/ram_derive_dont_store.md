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
| post R2.4b-2 | 4654 MB | **5219 MB** | fold adds +565; **digest, row count and book hash identical to the resident-span run** — spool byte-identity proven end-to-end; spooling cost ~2 s in 751 s |

**Peak anatomy:** the prologue peak is set by TRANSIENTS — vector
reallocation doubling in `TxnStreams::add`'s two `addSortedView` paths
(old+new buffers coexist during growth; +988 MB ≈ 2 x 4.85M x 100 B at
`internal`) stacked on ~1 GB of steady-state streams (two 481 MB
views). `SeededScreen::sorted` is a view (no copy). After b-2 the
prologue is the run's RAM floor.

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
  stream → two bounded buffers + temp files. Measured above.

## R2.4b-3 — prologue single-view build (NEXT)

The prologue is now the floor (4.65 GB at 20k ⇒ ~46 GB at 200k). The
chosen mechanism, from the measured anatomy:

1. **Stop maintaining `replayReady` during accumulation.** Both views
   end on disk anyway (b-2); only `screened` is consumed DURING the
   prologue (SeededScreens, spending prep). `TxnStreams::add` keeps
   merging the timestamp view only; at spool time the replay-ready
   order is derived ONCE — copy `screened`, `sortForReplay`, spool,
   free. Halves the steady stream cost and removes half the doubling
   transients; the one-shot sort transient replaces a prologue-long
   resident vector. BYTE-IDENTITY WATCH: today `replayReady` is built
   by incremental stable merges of per-batch `fundsLess`-sorted adds;
   a one-shot `sortForReplay` over the same rows yields the same order
   ONLY because `fundsLess` (auditKey) is total for every
   output-affecting purpose (rows comparing equal are byte-identical —
   the S10 re-pin). State this in the round and let
   production_windowed + goldens judge.
2. Measure; if the remaining view's doubling transients still bound
   the peak, follow with an exact reserve or batch-to-disk merge.

The monolithic oracle keeps both views (library-level; untouched).

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
