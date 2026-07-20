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

Vector storage is exact (capacity x element size); hash-map overheads
are approximated; the Router line (`infra.router~`) is derived from its
build inputs because its state is private.

### Compile-time budget (structs as of this writing, 64-bit)

Per person unless noted; the probe is the authority.

| pack | layout | ~bytes/person |
|---|---|---|
| people.pii | `pii::Record` ≈ 80 B POD (index-based names, fixed-width fields; no heap strings) | 80 |
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

## Who consumes what, when

"fold" = generation prologue + spending session + two-phase settlement.

| pack | fold | streaming exporter (during fold) | finisher (after fold) |
|---|---|---|---|
| accounts registry/lookup/ownership | opening book, validation, routing | standard, mule-ml | all |
| creditCards | card lifecycle | card-fraud | all |
| personas | spending market spans the table daily | — | aml family |
| portfolios terms | obligations burden | — | aml family |
| portfolios obligation stream | product schedule + burden prep (below) | — | — |
| router + ringInfra | every routed row | — | — |
| merchants | spending payees | card-fraud | standard, card-fraud |
| landlords, directory | blueprint construction only | — | standard, aml family |
| **people.pii** | **never** | mule-ml (at stream finish) | standard, aml family, card-fraud |
| **devices/ips records+usages** | **never** (router holds its own copies) | mule-ml (at stream finish) | standard, aml family, card-fraud |

## The cut line

- **Spine (stays resident):** accounts pack, creditCards, personas,
  portfolio terms, router + ringInfra, blueprint counterparty access.
  ~350–400 B/person.
- **Cold (export-only):** pii roster, device/IP records + usages,
  landlord/directory detail.
- **Window-scaled cache:** the obligation stream.

## R2.1 — release + seed-replay rebuild (delivered)

Mechanics (`pipeline/simulate.hpp`): `releaseExportOnlyPacks(world)`
frees `people.pii`, `infra.devices`, `infra.ips` (grep-verified: their
only consumers are `src/exporter/*`; the Router owns its routing
copies). `SimulationPipeline::rebuildWorldForExport()` replays
`buildWorldWith()` from a FRESH `Rng::fromSeed(seed)` — world-build
draws are a *prefix* of the run's shared sequential stream, so the
replay is byte-identical however far the fold advanced the shared RNG.
One code path serves the run build and the replay, so they cannot
drift.

`main.cpp` epoch flow: release after the streaming exporters bind;
fold; free the fold's world BEFORE the replay (two worlds never
coexist); rebuild only when a finisher needs the world.

| use case | packs released for the fold | rebuild before finisher |
|---|---|---|
| plain | yes | no (summary uses counts captured at build) |
| standard | yes | yes |
| card-fraud | yes | yes |
| mule-ml | **no** — its stream finish() reads devices/ips/pii (`addDeviceUsageRanges`/`addIpUsageRanges`/`writePartyRows`) | n/a |
| aml, aml-txn-edges | **no** — streaming `SharedContext` built from people/holdings at bind; copy-vs-alias audit pending | n/a |

Both deferrals move to R2.3. Safety audit done for the swapped cases:
`StreamingCardFraudExport` / `StreamingTransfersExport` dereference
Config pointers only in ctor/append/finish (all pre-swap);
`takeArtifacts()` moves accumulated state only.

## R2.2 — windowed obligation derivation

**R2.2.0 tie audit (delivered).** The stream's two consumers are
already window-shaped or fold-once:

- `transfers/channels/obligations/schedule.cpp` walks
  `between(active.start, active.endExcl)` per replay window — the
  product schedule source;
- `transfers/legit/ledger/burdens.cpp` aggregates
  `between(windowStart, windowEndExcl)` ONCE at spending prep into
  per-person monthly burden doubles.

So per-chunk derivation fits both — **but** `products.cpp` orders the
stream with `std::sort` on **timestamp alone**, and `std::sort` is
unstable: equal-timestamp events sit in implementation-pinned, not
specified, relative order. Sorting per-chunk subsequences is NOT
guaranteed to reproduce the full sort's tie order, so windowed
derivation is a safe refactor **only if the (timestamp) key is unique
in practice**. `pipeline::diagnostics::logObligationTieAudit`
(world_footprint.hpp, printed with the footprint under `make run-mem`)
counts equal-timestamp adjacencies in the built stream.

Decision tree, on measured output:

- **ties = 0** (expected if event timestamps carry per-person jitter):
  R2.2.1 replaces the retained stream with an on-demand generator that
  emits `[start, endExcl)` events in timestamp order — same values,
  same order, chunk-invariance + arch gates judge; the
  population x window-months pack disappears.
- **ties > 0**: the order must first be pinned to a total key
  (timestamp, then a content key). If the pinned order differs from
  today's unstable-sort order, that is a MODEL-VERSION change (goldens
  recaptured in a named commit, owner-gated) and R2.2.1 lands after it.

Owner: run `make run-mem ARGS="--population 200000 --days 730"` and
paste the footprint + tie-audit lines under Measured baselines.

## R2.3 — regenerable attribute view for streaming exporters

Give mule-ml's stream-finish reads and the aml family's SharedContext
a regenerable view instead of retained-pack pointers. This is the seam
R3's population sharding plugs into: a shard = a contiguous PersonId
range whose cold attributes are rebuilt on demand.

After R2, resident world ≈ spine only (~400 B/person): 1B people ≈
400 GB — still too big for one box; that is R3's population sharding,
and R2 is its prerequisite.

## Acceptance, every round

- `make test` green; NO golden file moves.
- `make run-mem` footprint before vs after shows the targeted pack gone
  from residency at the targeted phase.
- No CLI surface change; `--help` byte-identical.

## Measured baselines (owner-run, paste blocks here)

*(none recorded yet — first capture:
`make run-mem ARGS="--population 200000 --days 730"`; include the
world-footprint block AND the obligation tie-audit line)*
