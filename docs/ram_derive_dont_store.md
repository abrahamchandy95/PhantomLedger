# RAM R2 — derive, don't store

The transaction corpus already streams. What still scales with
population is everything the run RETAINS: the world packs, the
whole-window obligation precompute, and — the measured standout — the
generation prologue's base stream. R2 makes retained state regenerable
or windowed without moving a golden byte; changes that WOULD move bytes
go through the model-version pipeline with explicit owner gates.

Standing constraints (owner directives): byte identity per refactor
round (regenerate by replaying the same procedures with the same seed —
never re-keyed draws); no new CLI args/knobs/refusals; `mem`-topic
diagnostics only.

## Measured baseline (owner-run, 2026-07-20, 200k people / 730 days)

```
[mem]   worldTotal             538.46 MB  (~2823 B/person)
        of which portfolios.obligations 265.17, portfolios.terms 55.93,
        infra.router~ 56.79, infra.devices 53.67, holdings.accounts 39.03
[mem] obligation stream: 5792713 events, 5792041 equal-timestamp adjacencies
[mem:b] routines:done  peakRSS= 14638.4 MB   screened=48.4M (~4800 MB)
                                             replayReady=48.4M (~4800 MB)
                                             paydayInbound=14.6M (~1448 MB)
```

Readings: (1) the prologue base stream (14.6 GB) dwarfs the world
(538 MB); (2) obligation ties are pervasive — the order had to be
pinned before windowed derivation; (3) world estimates held up.

## Delivered

- **R2.0** — `logWorldFootprint` + this doc (measurement first).
- **R2.1 + R2.3a — release + seed-replay rebuild.** pii + device/IP
  inventories freed for the whole fold; `rebuildWorldForExport()`
  replays from a fresh `Rng::fromSeed(seed)` (world-build draws are a
  prefix of the shared stream ⇒ byte-identical). Released for
  plain/standard/card-fraud/aml/aml-txn-edges (audited copy-at-bind);
  NOT mule-ml (stream finish reads the packs → R2.3b).
- **R2.2.0 — tie audit**: ties pervasive (day-granular due dates).
- **MODEL: obligation order pin** (owner-approved, goldens recaptured):
  `stable_sort` ⇒ pinned total order (timestamp, then generation
  order). Tie order only; no model numbers.
- **R2.2.1a — windowed generator machinery**: `restrictTo` +
  per-person `emitPerson()` core + `generateWindow()` — equals the
  materialized `between()` slice byte-for-byte (content-keyed
  `personRng`, global append order, stable-sort tie independence).
- **R2.2.1b — fold-time obligation release**: both consumers finish at
  fold start (burden prep's 3-month slice; `makeProductSource`'s
  one-pass drafting), so `runWindowedErased` frees
  `portfolios.obligations()` right after the product source is built
  (`[mem] obligationsReleased`) — 265 MB gone for the fold, all use
  cases. Stage windowed entry points take mutable Holdings for exactly
  this release.
- **R2.4a — payday-inbound release** (zero output change): verified
  single consumer — `addSplitDeposits`, immediately after income; only
  income rows match the channel filter, so nothing later contributes.
  `TxnStreams::releasePaydayInbound()` (frees + stops collecting) is
  called right after the splitters run, in BOTH engines (shared
  passes). ~1.4 GB at 200k/730d, freed for the rest of the prologue and
  the whole fold. The memlog lines after `routines:splitters` now show
  paydayInbound=0 — that is the release, visible in the log timeline.

## R2.4b — the screened stream (NEXT design decision)

Verification results for the remaining two prologue copies:

- **`screened` CANNOT simply be released**: its consumers are
  `prepareMarket` (one aggregation pass → per-person payday sets),
  `prepareObligations` (Snapshot spans it), and — the binding one —
  `DayDriver::advanceLedgerToDay(PreparedRun::LedgerReplay …)`, which
  walks the span EVERY DAY across the whole run to advance the screen
  ledger. The consumption is **monotone forward** (day by day), so the
  resident span can become a forward cursor over a sequentially spooled
  stream (the BinaryCandidateSpool pattern — explicit files, never OS
  paging): write `screened` to disk at prologue time in timestamp
  order, stream it back per day. API change in the activity layer
  (LedgerReplay span → cursor seam) + the Snapshot's span consumers
  move to the prep aggregates they already produce. ~4.8 GB at
  200k/730d.
- **`replayReady`** already self-compacts: `PrecomputedCursorSource`
  erases consumed prefixes during the fold. Front-loaded ~4.8 GB at
  fold start; the same disk-spool seam removes it (write replay-sorted
  rows to disk, cursor them back), reusing R2.4b's machinery.

Order of work: cursor seam design (activity layer) → spooled screened
stream → spooled replay source. Gates: session/window/thread/arch
equivalence + all goldens; `[mem:b]` lines bounded by cursor buffers.

## Remaining after R2.4

- **R2.2.1c** — retire the obligation materialization entirely (shrink
  `synthesize()` to the window-independent 3-month burden slice; switch
  both product emitters to `generateWindow`; plumbing via
  `SimulationPipeline::products_`).
- **R2.3b** — regenerable attribute view for mule-ml's stream-finish
  reads (the seam R3's shards plug into).
- **R3** — population sharding (design doc first).

## Acceptance, every round

`make test` green, NO golden movement (except explicitly green-lit
model rounds); `make run-mem` shows the targeted retention gone at the
targeted phase; no CLI surface change.
