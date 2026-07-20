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
[mem]   people.roster            0.23 MB      [mem]   cps.merchants     0.40 MB
[mem]   people.pii              14.50 MB      [mem]   cps.landlords     0.01 MB
[mem]   people.personas         13.92 MB      [mem]   cps.directory     0.33 MB
[mem]   holdings.accounts       39.03 MB      [mem]   infra.devices    53.67 MB
[mem]   holdings.creditCards    16.66 MB      [mem]   infra.ips        21.79 MB
[mem]   portfolios.terms        55.93 MB      [mem]   infra.ringPlans   0.03 MB
[mem]   portfolios.obligations 265.17 MB      [mem]   infra.router~    56.79 MB
[mem]   worldTotal             538.46 MB  (~2823 B/person)
[mem] obligation stream: 5792713 events in window,
      5792041 equal-timestamp adjacencies
[mem:b] routines:done          peakRSS= 14638.4 MB  screened=48.4M (~4800 MB)
                                                    replayReady=48.4M (~4800 MB)
                                                    paydayInbound=14.6M (~1448 MB)
```

Readings: (1) the prologue base stream (14.6 GB) dwarfs the world
(538 MB) — the #1 wall (R2.4); (2) obligation ties are pervasive —
day-granular timestamps — so the order had to be pinned before any
windowed derivation; (3) world estimates held up.

## Delivered

- **R2.0** — `logWorldFootprint` + this doc (measurement first).
- **R2.1 + R2.3a — release + seed-replay rebuild.**
  `releaseExportOnlyPacks(world)` frees pii + device/IP inventories for
  the whole fold; `rebuildWorldForExport()` replays `buildWorldWith()`
  from a fresh `Rng::fromSeed(seed)` (world-build draws are a prefix of
  the shared stream ⇒ byte-identical); main.cpp frees the fold's world
  before the replay. Released for plain/standard/card-fraud/aml/
  aml-txn-edges (audited copy-at-bind); NOT for mule-ml (stream finish
  reads devices/ips/pii → R2.3b).
- **R2.2.0 — tie audit** (`logObligationTieAudit`): ties pervasive.
- **MODEL: obligation order pin** (owner-approved, goldens recaptured):
  `ObligationStream::sort()` = `std::stable_sort` ⇒ pinned total order
  (timestamp, then generation order). No model numbers changed; tie
  order only. Prerequisite for everything below.
- **R2.2.1a — windowed generator machinery** (zero behavior change):
  `ObligationStream::restrictTo(start, endExcl)` (chunk-sized scratch
  at append); `synthesize()` split into shared per-person `emitPerson()`
  (construction/emit order verbatim — draw-order-defining);
  `ObligationSynthesis::generateWindow(people, runWindow, start,
  endExcl)` replays the core with scratch terms ledgers. Content-keyed
  `personRng(seed, person)` + global append order + stable-sort tie
  independence ⇒ the result equals the materialized stream's
  `between()` slice byte-for-byte, from the same code path.
- **R2.2.1b — fold-time release** (zero output change): in the windowed
  path the stream's last consumers both run at fold start —
  `makeProductSource` drafts the whole window's product transactions in
  ONE pass over the events, and burden prep consumed its 3-month slice
  during session preparation. `runWindowedErased` therefore releases
  `portfolios.obligations()` immediately after the product source is
  built (`[mem] obligationsReleased` marks it): the 265 MB
  population x window-months pack is gone for the run's longest phase,
  every use case. Holdings became mutable through the stage's windowed
  entry points for exactly this one release (documented at the
  signature); the finisher-time world rebuild re-materializes the
  stream transiently where a use case takes the world back.

## R2.2.1c — retire the materialization (NEXT in the R2.2 line)

What remains for "never materialized at all": `synthesize()` still
builds the full stream at world build, because three consumers read it
before the release point — burden prep (first 3 months only,
order-insensitive), the windowed product source (one pass), and the
monolithic oracle's `ProductTxnEmitter::obligations`. Scope: shrink
`synthesize()`'s materialization to the 3-month burden slice (window-
independent — kills the window-months term at build time too, and in
the transient finisher rebuild); switch the windowed product source and
the oracle emitter to `generateWindow` (plumbing: TransferStage needs
the `ObligationSynthesis` config from `SimulationPipeline::products_`
plus People through `makeProductSource`/`mergeProducts`; the gate
harness passes the default-constructed synthesis it already mirrors).
Layering note: burdens stay untouched precisely because the 3-month
slice remains materialized — the transfers layer never needs the
pipeline-layer generator.

## Remaining after R2.2

- **R2.4 — prologue windowing** (the measured #1, 14.6 GB): aggregate
  consumers (payday sets, burdens) reduce to compact per-person
  structures; the replay consumer needs rows in window order (windowed
  generation or sequential disk spool). Verify first: post-bind reads
  of the obligations Snapshot's baseTxns span; paydayInbound consumers;
  income/routine generators' RNG regime (they share the sequential
  stream — the R2.2 ordering lesson applies).
- **R2.3b** — regenerable attribute view for mule-ml's stream-finish
  reads (the seam R3's shards plug into).
- **R3** — population sharding (design doc first).

## Acceptance, every round

`make test` green, NO golden movement (except explicitly green-lit
model rounds); `make run-mem` shows the targeted retention gone at the
targeted phase; no CLI surface change.
