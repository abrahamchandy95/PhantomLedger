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
  `ObligationStream::restrictTo(start, endExcl)` drops out-of-range
  events at append (chunk-sized scratch, never window-sized);
  `ObligationSynthesis::synthesize()` split into a shared per-person
  `emitPerson()` core (construction/emit order verbatim — draw-order-
  defining); new `ObligationSynthesis::generateWindow(people, runWindow,
  start, endExcl)` replays the core with scratch terms ledgers.
  Identity argument: content-keyed `personRng(seed, person)` makes each
  person independently replayable; in-range events arrive in global
  append order; the stable sort's tie groups are timestamp-independent
  ⇒ the result equals the materialized stream's `between()` slice
  byte-for-byte. No consumers rewired yet — nothing observable changes.

## R2.2.1b — consumer rewire + release (NEXT)

Wiring facts (verified): `Scheduler::generate` (schedule.cpp) drafts
per event in stream order from the product lane — chunk-walking the
pinned order feeds it identical draws. `buildMonthlyBurdens`
(burdens.cpp) reads only the FIRST 3 months (`kBurdenWindowMonths`),
order-insensitively. `makeProductSource` (window_sources.cpp) currently
drafts the ENTIRE window's product transactions into a
`PrecomputedCursorSource` at fold start (premiums + claims +
obligations merged; the source self-compacts as consumed) — the
obligation stream's last windowed-path consumer runs ONCE, at fold
start.

Scope for b: (1) `makeProductSource` obligations input switches to
`generateWindow` (full-range first, chunked cursoring after — premiums/
claims draw in per-person order, so their windowing is its own step);
(2) burden prep generates its 3-month slice; (3) `synthesize()` stops
materializing the stream once no consumer reads it — the 265 MB leaves
the world; the oracle path (ProductTxnEmitter::obligations) switches to
the same generator. Plumbing: the stage needs the ObligationSynthesis
config (SimulationPipeline::products_) and People — both flow through
existing seams. Gates: chunk-invariance, arch equivalence, all table
goldens — zero movement vs the recaptured baselines.

## Remaining after R2.2

- **R2.4 — prologue windowing** (the measured #1, 14.6 GB): design in
  place — aggregate consumers (payday sets, burdens) reduce to compact
  per-person structures; the replay consumer needs rows in window order
  (windowed generation or sequential disk spool). Verification
  checklist: post-bind reads of the obligations Snapshot's baseTxns
  span; paydayInbound consumers; income/routine generators' RNG regime
  (they share the sequential stream — the R2.2 ordering lesson applies).
- **R2.3b** — regenerable attribute view for mule-ml's stream-finish
  reads (the seam R3's shards plug into).
- **R3** — population sharding (design doc first).

## Acceptance, every round

`make test` green, NO golden movement (except explicitly green-lit
model rounds); `make run-mem` shows the targeted retention gone at the
targeted phase; no CLI surface change.
