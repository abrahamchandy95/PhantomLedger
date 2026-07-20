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

## Measured baselines (owner-run)

**200k/730d (pre-releases):** worldTotal 538 MB; prologue peak 14.6 GB.
**20k/730d full run (post R2.1→R2.4a):** worldTotal 54 MB; prologue
peak 4.65 GB (transients — see anatomy); fold peak 5.49 GB; candidate
spool 1143 MiB on disk; rebuild-for-export footprint byte-identical to
the original build (replay identity proven in production).

**Peak anatomy:** the prologue peak is set by TRANSIENTS — vector
reallocation doubling in `TxnStreams::add`'s two `addSortedView` paths
(old+new buffers coexist during growth; +988 MB ≈ 2 x 4.85M x 100 B at
`internal`). `SeededScreen::sorted` is a view (no copy). Steady-state
streams at 20k are ~1 GB.

## Delivered

- **R2.0** — `logWorldFootprint` + this doc (measurement first).
- **R2.1 + R2.3a** — cold-pack release + seed-replay world rebuild
  (pii + device/IP inventories; all use cases except mule-ml).
- **R2.2.0** — obligation tie audit (ties pervasive, day-granular).
- **MODEL: obligation order pin** (owner-approved, goldens recaptured):
  stable_sort ⇒ (timestamp, then generation order).
- **R2.2.1a** — windowed obligation generator (`restrictTo`,
  `emitPerson` core, `generateWindow` — equals the materialized slice).
- **R2.2.1b** — fold-time obligation release (both consumers finish at
  fold start; the population x window-months pack gone for the fold).
- **R2.4a** — payday-inbound release (single consumer; both engines).
- **R2.4b-1** — base-stream replay seam: activity-owned
  `BaseReplaySource` (stateless `postThrough` mirroring
  `advanceBookThrough`; caller keeps `RunState::baseIdx`) +
  `SpanReplaySource` adapter; `Snapshot.baseReplayOverride` is the
  plug-in point.
- **R2.4b-2 — both fold copies on disk:**
  - *ledger replay*: `BaseReplaySpool` (replay_spool.hpp, pipeline) —
    5-field records (source/target key, amount bits, channel,
    timestamp: exactly what `advanceBookThrough` uses), std::tmpfile,
    bounded read buffer; postThrough mirrors the original call for
    call (null-book no-consume, held-back row, fromIdx handshake).
    The windowed run spools `screened` after spending prep (its last
    RAM consumer), sets the override, frees the vector
    (`[mem] screenedSpooled`).
  - *base cursor*: `replayReady` rides the existing
    `BinaryCandidateSpool`/`BinarySpoolCursor` machinery — records are
    bit-identical round trips (test_spool_equivalence) and audit-order
    sorting satisfies the cursor's replay-order verification. Spooled
    and freed at source construction.
  Fold-resident base stream: ~1 GB at 20k / ~9.6 GB at 200k → two
  bounded read buffers + temp files. Judged by
  test_production_windowed (spooled pipeline vs resident-span oracle)
  + all goldens + session/window/thread gates.

## R2.4b-3 — prologue build transients (NEXT)

The remaining prologue wall is the BUILD itself: doubling reallocations
and dual sorted views while income/routines accumulate. Options, to be
decided on measurement after b-2 lands (`[mem:b]` lines): exact
pre-reserve from a prologue row estimate; build ONE view and derive the
other at spool time; or merge batches directly to disk. This is what
bounds the prologue at 100k+ populations (the standing "prologue
windowing ≥100k" item).

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
