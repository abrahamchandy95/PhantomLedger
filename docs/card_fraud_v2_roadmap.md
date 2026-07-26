# card-fraud-realism-v2 roadmap (V2-R)

**STATUS (2026-07-26): CODE-COMPLETE on the critical path.** Both
headline generation shortcuts are closed and measured, the exporter's
four full-window entity labels are withheld and quarantined, the
point-in-time feature contract is written and pinned, and prevalence is
gated per year. What remains is the owner's verification runbook and the
arc's wind-up commit.

**GOVERNING DIRECTIVE (owner, 2026-07-26): the deliverable is REALISTIC
DATA A GNN CAN BE TRAINED ON HONESTLY. Nothing on the critical path is
optional.** Success = no exported feature labels the corpus without
behavior, plus a stated contract for which columns a model may read.
Gates 1, 3, 4 and 5 of `docs/card_fraud_online_gnn.md`.

Owner rulings:
1. Distance-from-home fraud merchant selection — **APPROVED**.
2. **AMENDED BY OWNER:** the TF_GNN_v3 schema and the leak-closing
   logic are INDEPENDENT. Schema conformance is not a reason to leave a
   leak open — close what can be closed.
3. Prevalence calibration as its own serverless test.
4. Arc targets gate closure, not publication.
5. Population/density causality: researched, discretion exercised.
6. Zeroed-not-dropped for the leaking label columns: TF_GNN_v3 loading
   jobs map POSITIONALLY, so the columns stay and carry 0.

## The standing verdict, restated

The card-fraud corpus was a good PostgreSQL / TigerGraph /
temporal-loading artifact and not a credible online fraud benchmark.
Gates 1, 3, 4 and 5 now have code behind them. What is still missing for
a benchmark CLAIM is calibration of the fraud LEVEL against a named
issuer-side series (the arc measures separability and stability, not
level) and the depth items cut below. The README states this without
overclaiming.

## P0 CONFIRMED IN CODE — AND CLOSED

`src/transfers/fraud/typologies/unauthorized.cpp` — the card and
giftCardScam rails set `draft.destination = pickOne(rng,
ctx.billerAccounts)`, the legit-TRANSFER biller/hub pool, while
legitimate card purchases route through
`PaymentRouter::pickMerchantIndex` (payments.cpp:193) into the market
merchant CATALOG. Two disjoint destination populations on one export
edge ⇒ every fraud row landed on a merchant with zero legitimate card
rows. Structural, not a seed artifact.

**FIXED at b-2.** The biller pool survives only as the degradation path
when no catalogue, no eligible merchant or no home area is available.
**MEASURED: fraud-only-merchant row share 1.0 → 0.0000.**

## Landed, in order

### Carriers + fill (zero-golden)

`InjectorLegitCounterparties` and `IllicitContext` carry
`const entity::merchant::Catalog *merchants` and
`std::span<const GeoAreaId> homeAreas`; `CompromisePlan` carries
`homeArea`. The catalogue POINTER (not parallel key/area spans) is the
right carrier: each `Record` already has `counterpartyId`, `location`,
`footprint` and `weight`, and the modality split needs all four —
`Footprint::online` IS the card-not-present acceptance population, and
`weight` keeps fraud on the same merchant size distribution legitimate
selection uses. `pickAccount` returns the owner alongside the key so
`homeArea` resolves without a key→person reverse index.

All FOUR `legitCounterparties` call sites filled in one round (the
parity trap below).

### b-2 — the selection flip (MODEL-MOVING, four goldens re-pinned)

`unauthorized.cpp` draws modality first (`kCardNotPresentShare = 0.70`,
a DECLARED CHOICE — the anchored claim is the direction, not the number;
`giftCardScam` is always card-present), then selects: card-present from
the victim's distance-decayed pool over physical outlets using the
SHARED `cardPresentDecayScaleMiles(homeArea)` kernel; card-not-present
from the `Footprint::online` population by popularity. Weighted by the
catalogue's own `weight`. Own lane `{"fraud","unauth","merchant",seq}`.

A flat national draw would have traded the merchant shortcut for a
DISTANCE shortcut — legitimate card-present spend is distance-decayed,
so uniformly-placed fraud would be separable by miles-from-home alone.
Hence the same kernel, not a new one.

**The modality decision drives DESTINATION SELECTION only. It is not
exported.** See the `use_chip` correction below.

### The attacker-IP shortcut (MODEL-MOVING, same re-pin)

`injector.cpp` wrote `network::Ipv4::pack(198, 51, 100, …)` —
TEST-NET-2 — for every unauthorized event, while legitimate IPs come
from `randomIpv4` (first octet 11–222, full 32-bit spread). Collision
probability ≈ 1 in 14,000,000: **a /24 lookup labelled unauthorized
fraud essentially perfectly.** Now `network::randomIpv4(rng)`.
Deliberate ring infrastructure sharing — real behavioral signal — is
untouched.

*Process note, kept deliberately:* an earlier round declared this claim
stale because a repo-wide grep for the dotted-quad strings
(`192.0.2` / `198.51.100` / `203.0.113` / `TEST-NET`) found only the
audit doc. The defect was written in INTEGER OCTET form. **Grep the
constructor, not the rendered literal, before calling an audit claim
stale.**

### c — the baselines (zero-golden, THE PROOF)

- `tests/test_card_merchant_overlap.cpp` — the audit's SQL, serverless.
  Fraud-only-merchant share **0.0000** (was ≈ 1.0).
- `tests/test_card_baselines.cpp` — gate 5. Ranks merchants by empirical
  fraud rate, sweeps the threshold: **recall at precision ≥ 0.90 =
  0.0000** against a gate of < 0.25, pure-fraud-merchant share 0.0000.
  Base rate 0.254% over 42,516 card rows across 254 merchants.
  *Read the printed `best precision 0.25 (lift 98×)` correctly: at this
  base rate any merchant with 4 rows and 1 fraud scores 0.25. It is a
  small-cell artifact, not signal — precision never approaches 0.90,
  which is why recall@0.90 is zero.*
  This is also the gate that catches a REPLACEMENT shortcut if the
  selection ever regresses.

### ROUND 1 — label leakage (EXPORTER-ONLY; three table goldens re-pin)

Four full-window entity verdicts rode the feature graph:
`Card.is_fraud` ("this card ever carried a flagged row"),
`Party.is_fraud`, `Device.is_blocked`, `IP.is_blocked`. A GNN given any
of them scores ~100% and learns nothing about behavior.

Per ruling 6 the columns are RETAINED and written as 0
(`kLabelWithheld` in `src/exporter/card_fraud/export.cpp`), and the
investigative content moves to a 35th table,
`card_fraud."cf_Ground_Truth_Label"` — `(entity_type, entity_id,
label)`, positives only, joinable 1:1 to the vertex tables, pointed at
by no edge and loaded by no TF_GNN_v3 job. The one supervised target in
the graph is `Payment_Transaction.is_fraud`, observable at its own row's
timestamp.

Gates: `tests/test_pipeline_e2e.cpp` (serverless — every cell of the
four columns renders 0, and whenever the window produced flagged card
rows the overlay carries the cards they touched);
`tests/test_table_golden.cpp` (the same at the production config, pop
10000, against live PostgreSQL); `docs/card_fraud_postgres_acceptance.sql`
(overlay↔vertex join integrity and label vocabulary). Table count
34 → 35 in all three.

**Observed on the re-pin:** only the card_fraud section diverged;
exactly four vertex tables changed with IDENTICAL row counts, plus the
new overlay (114 rows at pop 10000 / 60d); the stream golden held at
197,199 rows. Exporter-only, as declared.

### ROUND 2 — the point-in-time contract (gate 4, zero-golden)

`docs/card_fraud_feature_contract.md` classifies every exported column
FEATURE-SAFE / TARGET / PROHIBITED / USE-WITH-CARE, and states the split
and evaluation rules.

`tests/test_card_point_in_time.cpp` pins it with a **truncation
experiment**: one world, exported twice through the production path —
once over every settled row, once over only rows before a mid-window
cutoff `T` (exactly what a model scoring at `T` could have known). Every
feature-safe value present in the score-time export must be
byte-identical in the full-window export. Three enforcement classes:

| Class | Requirement |
|---|---|
| STREAM PREFIX | the three streamed transaction tables must be a byte-exact prefix |
| IDENTICAL | world-derived tables (Party, the PII layer) cannot move at all |
| GROWING SET | `Card`, `Merchant`, the geo chain: the row SET may grow, a row may never change |

The gate is not a tautology — it would have **failed** on the
pre-round-1 `Card.is_fraud`, whose value flipped 0 → 1 the moment a
future flagged row arrived. `cf_Ground_Truth_Label` is excluded by
design, and the test PRINTS how much it moves across the cutoff as
standing evidence for the quarantine.

`Party.created_at`'s prohibition is formally LIFTED: H3's single
membership path writes `membership.joinTs(p)`.

### ROUND 3 — the prevalence suite (gate 3, zero-golden)

`tests/test_card_prevalence.cpp` — four whole calendar years
(1991–1994, N=300) on the card view.

GATED: per-year fraud RATE stability (spread < 4× — `F = pL/(1−p)`
rides the realized candidate count `L`, so a fan-out means some budget
stopped riding `L`); per-year fraud amounts flat in CALIBRATION dollars
(spread < 2.5× — the only gate proving U-6 class F scaling reaches the
card fraud rail); fraud rides `card_purchase` with merchant-POS share
< 0.05; at least two typologies with the unauthorized family > 0.30;
fraud tickets exceed legitimate ones; episode size bounded.

PRINTED, NEVER GATED: per-year card-view ROW COUNTS. H4's
real-consumption ramp raises them while the documented small-N
liquidity drain (U-9 ADDENDUM, ~27% in second-year gate legs) lowers
them; at N=300 the two are not separable. **Do not gate what the
harness cannot resolve** — the same reasoning that subsumed H4's
recession-direction gate.

IBM TabFormer's observed 0.11675% is a NAMED COMPARATOR with a wide
plausibility band (0.02%–2%) and the ratio is printed, not pinned.
PhantomLedger is TabFormer-SHAPED, not calibrated to it.

### Layering verified

`PL_LINT_ALLOWED_transfers` includes `activity` and the entities vocab,
so the injector may read the merchant catalogue and the shared decay
curve directly — one curve, two consumers. No relocation round.

## THE PARITY TRAP (must survive into every later round)

`legitCounterparties` has **FOUR** call sites:

| Site | Role |
|---|---|
| `src/pipeline/simulate.cpp:95` | monolith reference oracle |
| `src/pipeline/stages/transfers/windowed_run.cpp:285` | PRODUCTION windowed engine |
| `tests/window_leg_support.hpp:313` | the gate harness every v2 gate runs on |
| `tests/test_membership.cpp:626` | H3 membership gate (still defaulted — harmless, it exercises ring fraud, not the card rails) |

The two ENGINES must pass IDENTICAL arguments or
`test_arch_equivalence` / `test_production_windowed` diverge. The gate
harness must be filled too, or every gate measures a carrier-free world
and passes vacuously.

## CUT from the critical path (polish, registered not forgotten)

- **b′ — compromise incidence scaling.** The Bettencourt tilt on victim
  selection: P(compromise) ∝ homeAreaPopulation^(β−1), β ≈ 1.15,
  normalized so total prevalence is unchanged (the budget law still
  owns how much fraud exists). One function, one named constant, one
  named lane. Must land AFTER a home-area-only baseline, which is why
  that baseline is NOT in `test_card_baselines` today — with no tilt in
  the world it would measure nothing.
- **Transaction-time device/IP edges.** The attacker's device and IP
  exist on the corpus row (`public.transactions.device_id`,
  `.ip_address` — feature-safe there) but are not transaction-time
  edges in the card graph; `Has_IP`/`Has_Device` are window-wide party
  associations.
- **Exporting the real card-present modality** into `use_chip` (see the
  correction below).
- **Compromise-state effective dates.** Likely SMALLER than it looks:
  H3 delivered membership intervals, closure and join anchors; H4
  delivered era-varying volume. Re-scope when reached.
- **Level calibration** of card-fraud prevalence against a named
  issuer-side series (Nilson / FTC), including the CNP share.
- Device `ownerId` render check.

## Population, density, and urban scaling (adopted, pending b′)

`geo_data.hpp` carries POPULATION only — `GeoArea.landAreaKm2` was
never populated and preserving that 0 is part of the byte-neutral embed
contract, so **true DENSITY is not computable today**.
Population-as-urbanicity is already wired on the legitimate side:
`cardPresentDecayScaleMiles` log-interpolates the card-present radius
between a 4-mile dense-urban and a small-town scale, explicitly because
land area is missing (PROVISIONAL, debt recorded). b-2 now reuses that
same curve for fraud.

The law worth wiring: **Bettencourt, Lobo, Helbing, Kühnert & West,
"Growth, innovation, scaling, and the pace of life in cities," PNAS
104(17):7301–7306 (2007)** — socioeconomic quantities scale
superlinearly with city population, Y ∝ N^β with β ≈ 1.15 (serious
crime among them); infrastructure scales sublinearly. Per capita that is
N^0.15 ⇒ ≈2.2× across the catalogue's 8.48M-vs-50k range: material,
nowhere near separating.

Declared limits: Bettencourt measured metropolitan serious crime, NOT
payment-card fraud — the law supplies the FUNCTIONAL FORM and the
application is a CHOICE. Real card-fraud geography is dominated by
card-not-present activity, only weakly coupled to the victim's home.
The catalogue rows are municipal CORE populations, not metro
populations.

Adopted scope: ONE insertion point (compromise incidence, b′).
Declined and registered: density-based category mix (needs BLS CES
anchors plus land area), urbanicity-varying cash/ATM share (already
registered as a cash-share era model), any true density model (blocked
until the Census Gazetteer round).

**THE ANTI-SHORTCUT CONDITION.** Adding a real geographic tilt makes
home-area population correlate with the label. That is realistic, but
this arc exists because an artifactual correlation already solved the
task once. If a home-area-only baseline ever solves the task, the
exponent is wrong — not the gate.

## Leak inventory, final dispositions

| Audit claim | Verified state | Disposition |
|---|---|---|
| `Party.created_at` unusable until membership-time consistency is fixed | **ALREADY CLOSED** — `card_fraud/export.cpp` writes `membership.joinTs(p)` (H3's single membership path) | STALE; prohibition LIFTED by the feature contract |
| `Card.is_fraud`, `Party.is_fraud`, `Device.is_blocked`, `IP.is_blocked` are full-window labels | Confirmed | **CLOSED (round 1)** — columns retained for positional loading, written 0; verdicts moved to `cf_Ground_Truth_Label` |
| `use_chip` / `error` are export-time hashes, not causal | Confirmed, and **a claim made earlier in this arc was WRONG**: b-2 did NOT make `use_chip` model-backed. `derive::useChipFor` is still an FNV content hash of the row (Swipe .63 / Chip .26 / Online .11) for fraud and legitimate rows alike; the modality decision drives destination selection and is never exported | Both are point-in-time SAFE (deterministic in the row) but MECHANISM-FREE. The feature contract classes them USE WITH CARE. Exporting the real modality is registered above |
| Raw `public.transactions` exposes ground truth + TEST-NET IPs | **BOTH CONFIRMED.** The TEST-NET claim DOES reproduce — the earlier "did not reproduce" reading grepped the dotted-quad string and missed `Ipv4::pack(198, 51, 100, …)` | IP shortcut **CLOSED**. The raw ledger's `is_fraud`/`ring_id`/`fraud_type` columns are the generator's own ground truth and stay — that table is the corpus, not the feature graph; the contract names `ring_id`/`fraud_type` PROHIBITED for features |

## Law compliance

NO new CLI; NO runtime path selection; NO network; embedded data only
(the scaling law adds a constant, not a table). New randomness on a
NAMED lane. b-2 and the IP fix are model-moving (four-golden re-pin);
round 1 is exporter-only (three TABLE goldens; a moving stream golden
would be a defect, and it did not move); the fill, c, and rounds 2–3
are zero-golden.
