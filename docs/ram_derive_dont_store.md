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
[mem:b] income                 peakRSS= 10154.6 MB  screened=14.6M rows
[mem:b] routines:done          peakRSS= 14638.4 MB  screened=48.4M (~4800 MB)
                                                    replayReady=48.4M (~4800 MB)
                                                    paydayInbound=14.6M (~1448 MB)
[mem] windowedPrologue   peakRSS= 14638.4 MB
```

Readings:

1. **The prologue dwarfs everything.** The world is 538 MB; the base
   stream (income + base routines, precomputed for the WHOLE window)
   peaks at **14.6 GB** — 48.4M rows retained as `screened` PLUS a full
   `replayReady` copy PLUS the payday-inbound view. Population x
   window-scaled: ~73 GB/1M people at 730 days. This is the #1 wall.
2. **Obligation ties are pervasive** (5.79M of 5.79M): timestamps are
   day-granular, so the unstable sort's within-day order is
   unspecified-but-pinned. Windowed obligation derivation CANNOT be a
   zero-golden refactor.
3. World-side estimates held up (obligations 265 MB as predicted;
   ~2.8 KB/person total at 2 years).

## Delivered

- **R2.0** — `logWorldFootprint` + this doc (measurement first).
- **R2.1 + R2.3a — release + seed-replay rebuild.**
  `releaseExportOnlyPacks(world)` frees pii + device/IP inventories for
  the whole fold; `rebuildWorldForExport()` replays `buildWorldWith()`
  from a fresh `Rng::fromSeed(seed)` (world-build draws are a prefix of
  the shared stream ⇒ byte-identical); main.cpp frees the fold's world
  before the replay so two worlds never coexist. Released for: plain,
  standard, card-fraud, aml, aml-txn-edges (audits: standard/cf bind no
  cold packs; aml family's SharedContext and ShellStats OWN their data —
  sets/vectors/maps copied at bind; append paths read only the batch).
  NOT released for mule-ml: its stream finish() reads devices/ips/pii
  (`addDeviceUsageRanges`/`addIpUsageRanges`/`writePartyRows`) → R2.3b.
- **R2.2.0 — obligation tie audit** (`logObligationTieAudit`): verdict
  above.

## R2.4 — prologue windowing (NEXT: design-first, the measured #1)

Problem: `TxnStreams` retains the whole-window base stream three ways —
`screened` (the session/market view), `replayReady` (the driver's
cursor source), `paydayInbound` (cure discovery) — because downstream
consumers were written against full-window views:

- `prepareMarket` → `buildPaydaysByPerson(baseTxns, …)` — aggregates
  the stream ONCE into per-person payday-day lists (compact).
- `prepareObligations` → Snapshot **spans** `streams.screened()` for
  the session's lifetime.
- The windowed driver's base cursor consumes `takeReplayReady()` —
  already a forward, window-ordered walk.
- Future-inbound cure discovery reads the payday-inbound view.

Design direction (to verify, then implement in stages): the stream's
*aggregate* consumers already reduce to compact per-person structures
(payday sets, burdens) — derive those in a single generation pass and
DROP the rows; the *replay* consumer needs rows only in window order —
generate income/routine rows per generation chunk (their generators are
calendar-driven), or spool them to disk sequentially (the
BinaryCandidateSpool pattern: explicit files, never OS paging) if
cross-window state makes windowed generation order-fragile. The open
question to settle first: what the session actually reads through the
obligations Snapshot's `baseTxns` span DURING the fold (if prep-only,
the span can point at a dropped-after-prep buffer; if per-day, that
access needs a compact replacement).

Verification checklist before any code: (1) every read of
`Snapshot.baseTxns` after `SessionBundle::make`; (2) every consumer of
`streams.paydayInbound`; (3) whether income/routine generators can
emit a chunk `[start,end)` with identical draws (they share the
sequential RNG stream with the blueprint — the R2.2 ordering lesson
applies in full). Acceptance: all goldens byte-identical; window/
thread/session-equivalence and arch gates green; `[mem:b]` prologue
lines bounded by chunk, not window.

## PARKED (owner-gated): obligation order pin + R2.2.1

Pinning the stream to a total order (timestamp, then generation order
via `std::stable_sort`) makes it deterministic AND chunk-reproducible;
then R2.2.1 replaces the 265 MB whole-window precompute with a windowed
generator as a pure refactor. Because ties are pervasive, the pin
REORDERS same-day events ⇒ RNG draw order shifts in product-row
generation ⇒ bytes move: a MODEL-VERSION round (all four goldens
recaptured in one named commit). Parked until the owner explicitly
green-lights it. No model numbers change — only tie order.

## Remaining after R2.4

- **R2.3b** — regenerable attribute view for mule-ml's stream-finish
  reads (the seam R3's shards plug into).
- **R3** — population sharding (design doc first; post-R2 spine ≈
  400 B/person ⇒ 1B ≈ 400 GB on one box; sharding is the answer).

## Acceptance, every round

`make test` green, NO golden movement (except explicitly green-lit
model rounds); `make run-mem` shows the targeted retention gone at the
targeted phase; no CLI surface change.
