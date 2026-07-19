# RAM R2 — derive, don't store

The transaction corpus already streams: the windowed engine stages a
bounded window, spools Phase-A candidates to disk, and COPYs settled
rows out, so corpus size never bounds RAM. What still scales with
population is the **world** — every pack `buildWorld()` materializes
stays resident from build until the vertex exporters finish. R2 makes
the export-only packs *regenerable* instead of *retained*, without
moving a single golden byte.

Standing constraints (owner directives):

- **Byte identity.** Every R2 round is a refactor: `make test` green,
  ZERO golden movement. Re-keying attribute derivation onto per-entity
  RNG lanes would change draw sequences and therefore bytes — that is a
  model change and is out of scope. R2 regenerates by *replaying the
  same procedures with the same seed*, so values are identical and only
  residency changes.
- **No new surface.** No CLI args, no knobs, no refusals. Purely
  internal restructure plus `mem`-topic diagnostics.

## Measurement first (R2.0 — delivered)

`pipeline::diagnostics::logWorldFootprint` (world_footprint.hpp) prints
a per-pack estimate once, right after the world build:

```sh
make run-mem ARGS="--population 200000 --days 730"
```

```
[mem] world footprint (estimated resident bytes; ...):
[mem]   people.pii            ...
[mem]   portfolios.obligations ...
[mem]   worldTotal            ...  (~B/person at N people)
```

Vector storage is exact (capacity x element size); hash-map overheads
are approximated (16 B node overhead + one bucket pointer per entry);
the Router line (`infra.router~`) is derived from its build inputs
because its state is private. Decisions below should be re-checked
against measured output at a representative population before each
implementation round.

### Compile-time budget (structs as of this writing, 64-bit)

Per person unless noted. These are sizeof-derived approximations — the
probe is the authority.

| pack | layout | ~bytes/person |
|---|---|---|
| people.pii | `pii::Record` ≈ 80 B POD (index-based names, fixed-width phone/email/SSN — already compacted; no heap strings) | 80 |
| people.personas | `behavior::Persona` 72 B + 1 B assignment | 73 |
| people.roster | 1 B flags + ring topology (fraud participants only) | ~2 |
| holdings.accounts | 24 B record + ~56 B lookup node + 12 B ownership, x ~1.5 accounts/person | ~140 |
| holdings.creditCards | 40 B terms + ~64 B byKey node (x card share) + 4 B byPerson | ~75 |
| portfolios.terms | insurance + installment terms maps (holders only) | ~50 |
| **portfolios.obligations** | **48 B x events; events ≈ persons x window-months x products — the only pack that scales with the WINDOW** | ~2,300 at 2 yrs |
| infra.devices / infra.ips | records + usages + byPerson pools | ~350 |
| infra.router~ | account->owner map + its own copies of both byPerson pools + sticky indexes | ~350 |
| cps.* | merchants/landlords/directory (sublinear counts) | ~10 |

Ballpark: **~1.1 KB/person + ~1.2 KB/person/window-year**. At 150M
people over 2 years that is ~500 GB resident; at 1B it is impossible.
This is the wall R2 removes.

## Who consumes what, when

From `src/app/main.cpp` and the fold composition
(`windowed_run.cpp`); "fold" = generation prologue + spending session +
two-phase settlement.

| pack | fold | streaming exporter (during fold) | finisher (after fold) |
|---|---|---|---|
| accounts registry/lookup/ownership | opening book, validation, routing | standard, mule-ml | all |
| creditCards | card lifecycle | card-fraud | all |
| personas | spending market spans the table daily | — | aml family |
| portfolios terms | obligations burden | — | aml family |
| portfolios obligation stream | product schedule source | — | — |
| router + ringInfra | every routed row | — | — |
| merchants | spending payees | card-fraud | standard, card-fraud |
| landlords, directory | blueprint construction only | — | standard, aml family |
| **people.pii** | **never** | mule-ml (at stream finish) | standard, aml family, card-fraud |
| **devices/ips records+usages** | **never** (router holds its own copies) | mule-ml (at stream finish) | standard, aml family, card-fraud |
| ringPlans | injector inputs at fraud boundary | — | — |

## The cut line

- **Spine (stays resident):** accounts pack, creditCards, personas,
  portfolio terms, router + ringInfra, blueprint counterparty access.
  Compact, keyed, consumed every settlement day. ~350–400 B/person.
- **Cold (export-only):** pii roster, device/IP records + usages,
  landlord/directory detail. Never read by the fold.
- **Window-scaled cache (the standout):** the obligation stream is a
  whole-window precompute consumed strictly via
  `between(start, endExcl)` — already window-shaped, retained anyway.

## R2.1 — release + seed-replay rebuild (DELIVERED, awaiting gates)

Mechanics (`pipeline/simulate.hpp`):

- `releaseExportOnlyPacks(world)` swaps empty packs into
  `people.pii`, `infra.devices`, `infra.ips`, freeing their storage.
  Verified safe: the fold's only consumers of person infra are the
  Router's private copies, and grep confirms the three packs are read
  exclusively by `src/exporter/*`.
- `SimulationPipeline::rebuildWorldForExport()` replays
  `buildWorldWith()` from a FRESH `Rng::fromSeed(seed)`. World-build
  draws are a *prefix* of the run's shared sequential stream (the
  stream starts at the seed and `buildWorld()` is its first consumer),
  so the replay is byte-identical no matter how far the fold advanced
  the shared RNG. One code path (`buildWorldWith`) serves both the run
  build and the replay, so they cannot drift.

`main.cpp` epoch flow (release after the streaming exporters bind,
rebuild only when a finisher needs the world; the two worlds never
coexist — the fold's world is freed before the replay):

| use case | packs released for the fold | rebuild before finisher |
|---|---|---|
| plain (no use-case exporter) | yes | no (summary uses counts captured at build) |
| standard | yes | yes (`exportEntities` reads pii/devices/ips) |
| card-fraud | yes | yes (geo/PII layer reads pii; device/IP tables) |
| mule-ml | **no** | n/a (world retained) |
| aml, aml-txn-edges | **no** | n/a (world retained) |

Why the two deferrals (both move to R2.3):

- **mule-ml** dereferences the packs at *stream finish*:
  `addDeviceUsageRanges` / `addIpUsageRanges` fold entity-scale usage
  ranges into the edge tables, and `writePartyRows` reads the pii
  roster — all inside the fold's `sink.finish()`.
- **aml family** builds its streaming `SharedContext` from
  `people`/`holdings` at bind time; whether that context copies or
  aliases the packs needs its own audit before any release.

Safety audit done for the swapped cases: `StreamingCardFraudExport`
and `StreamingTransfersExport` dereference their Config pointers only
in the constructor and `append()`/`finish()` — all before the epoch
swap; `takeArtifacts()` moves accumulated state only. The summary
line's People/Accounts counts are captured immediately after the build.

Residency effect at fold time (standard/card-fraud/plain): pii +
device/IP inventories (~430 B/person, ~40% of the non-obligation
world) are absent for the entire fold — the run's longest phase. Cost:
one extra world build before vertex export (the `[phase] rebuild *`
lines measure it).

## Remaining stages

2. **R2.2 — windowed obligation derivation.** Generate the obligation
   schedule per generation chunk instead of precomputing the whole
   window (the product source already consumes it windowed). Removes
   the population x window-months term entirely. The chunk boundary
   must not change event content or order — chunk-invariance and arch
   gates judge.
3. **R2.3 — per-shard attribute access for streaming exporters.** Give
   mule-ml's stream-finish reads and the aml family's SharedContext a
   regenerable attribute view instead of retained-pack pointers. This
   is the seam R3's population sharding plugs into: a shard = a
   contiguous PersonId range whose cold attributes are rebuilt on
   demand.

After R2, resident world ≈ spine only (~400 B/person): 1B people ≈
400 GB — still too big for one box, which is exactly what R3's
population sharding addresses; R2 is its prerequisite.

## Acceptance, every round

- `make test` green; NO golden file moves (run, tables, aml,
  card-fraud).
- `make run-mem` footprint before vs after shows the targeted pack gone
  from residency at the targeted phase, with `worldTotal` and phase
  peak-RSS lines down accordingly.
- No CLI surface change; `--help` byte-identical.

## Measured baselines (owner-run, paste blocks here)

*(none recorded yet — first capture should be
`make run-mem ARGS="--population 200000 --days 730"` before R2.2
lands)*
