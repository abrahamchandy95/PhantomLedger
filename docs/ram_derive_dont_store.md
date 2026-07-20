# RAM R2 — derive, don't store

The transaction corpus already streams. What still scales with
population is everything the run RETAINS: the world packs, the
whole-window obligation precompute, and — the measured standout — the
generation prologue's base stream. R2 makes retained state regenerable
or windowed without moving a golden byte; changes that WOULD move bytes
go through the model-version pipeline with explicit owner gates.

Standing constraints (owner directives): byte identity per refactor
round; no new CLI args/knobs/refusals; `mem`-topic diagnostics only.

## Measured baselines (owner-run)

**200k / 730d (pre-releases):** worldTotal 538 MB (~2823 B/person;
obligations 265), prologue peak 14.6 GB (screened 4.8 GB + replayReady
4.8 GB + paydayInbound 1.4 GB + transients). Obligation ties: 5.79M of
5.79M events.

**20k / 730d FULL RUN (post R2.1/R2.2.1b/R2.3a/R2.4a, standard):**

```
worldTotal 54.1 MB  ->  export-only packs freed
prologue:  income 1201 | splitters 1524 | rent 1868 | subs 2681
           | atm 3665 | internal 4653 MB peak
           paydayInbound=0 from rent onward        (R2.4a working)
obligationsReleased at fold start                  (R2.2.1b working)
phaseA/B peak 5488 MB; candidate spool 1143 MiB ON DISK
rebuild for export: footprint byte-identical to the original build
                    (54.11 MB, 581365 events)      (replay identity
                                                    proven in prod)
run: 18.18M rows, 24 spans streamed, digest stable
```

**Peak anatomy (the surprise):** the prologue peak is set by
TRANSIENTS, not steady state. `SeededScreen::sorted` is exonerated — it
wraps the span, no copy. The +812/+985/+988 MB steps are **vector
reallocation doubling** in `TxnStreams::add`'s two `addSortedView`
paths: inserting past capacity reallocates `screened_`/`replayReady_`
to ~2x while old+new buffers coexist (+988 ≈ 2 x 4.85M rows x 100 B —
exact match at `internal`). Steady-state streams at 20k are only
~1 GB; the 4.65 GB peak is doubling transients stacked on them.

## Delivered

- **R2.0** — `logWorldFootprint` + this doc (measurement first).
- **R2.1 + R2.3a** — release + seed-replay rebuild: pii + device/IP
  inventories freed for the whole fold (all use cases except mule-ml);
  `rebuildWorldForExport()` = fresh `Rng::fromSeed(seed)` replay,
  byte-identity now verified in production output above.
- **R2.2.0** — tie audit (ties pervasive; day-granular due dates).
- **MODEL: obligation order pin** (owner-approved, goldens recaptured):
  stable_sort ⇒ (timestamp, then generation order). Tie order only.
- **R2.2.1a** — windowed generator machinery (`restrictTo`,
  `emitPerson` core, `generateWindow` — equals the materialized slice
  byte-for-byte).
- **R2.2.1b** — fold-time obligation release (both consumers finish at
  fold start; 265 MB@200k gone for the fold, all use cases).
- **R2.4a** — payday-inbound release (single consumer
  `addSplitDeposits`; freed + collection stopped right after, both
  engines; verified in the 20k log: zero from `rent` onward).

## R2.4b — the base-stream program (NEXT, staged)

Verified consumer facts: `screened`'s binding consumer is
`DayDriver::advanceLedgerToDay(PreparedRun::LedgerReplay …)` — every
day, whole run, but **monotone forward**; prep consumers
(`prepareMarket` payday sets, `prepareObligations` burdens) are
one-pass aggregates; prologue `SeededScreen`s walk it monotone forward
too (`seedIdx_`). `replayReady` feeds `PrecomputedCursorSource`, which
already self-compacts during the fold.

Stages, in order:

1. **R2.4b-1 — cursor seam (behavioral no-op):** activity-owned
   forward-cursor abstraction replaces the raw spans in
   `PreparedRun::LedgerReplay` (and the obligations Snapshot's
   `baseTxns` hand-off), with the in-memory vector adapter as the only
   implementation. All gates green, zero movement.
2. **R2.4b-2 — spool the fold's copies:** after the prologue completes,
   write `screened` (timestamp order) to a sequential binary spool
   (BinaryCandidateSpool pattern — explicit files, never OS paging),
   free the vector, feed the cursor from disk; same for the replay
   source. Fold-resident base stream -> cursor buffers (~1 GB@200k
   freed for the fold).
3. **R2.4b-3 — kill the build transients:** the prologue peak itself
   (doubling reallocations + dual sorted views). Options, decided by
   measurement after b-2: exact pre-reserve from a prologue row
   estimate; or build ONE view and derive the other at spool time; or
   merge batches directly to disk. This is what bounds the prologue at
   100k+ populations (the standing "prologue windowing ≥100k" item).

## Remaining after R2.4

- **R2.2.1c** — retire the obligation materialization (3-month burden
  slice at build; both product emitters on `generateWindow`).
- **R2.3b** — regenerable attribute view for mule-ml (R3's shard seam).
- **R3** — population sharding (design doc first).

## Diagnostics notes

- The tie-audit line now reads "(order pinned: …)" — the pre-pin
  wording was stale after the model round.
- `phaseA`/`phaseB` `live:` figures are CUMULATIVE stream counts (the
  candidates live in the disk spool, posted rows stream out); their
  `~MB` is what those rows WOULD cost resident, not what is resident.

## Acceptance, every round

`make test` green, NO golden movement (except explicitly green-lit
model rounds); `make run-mem` shows the targeted retention gone at the
targeted phase; no CLI surface change.
