# card-fraud-realism-v2 roadmap (V2-R)

**STATUS (2026-07-27): ROUND 8 LANDED; the export is materially safer,
but it is not yet a public benchmark.** Both headline generation
shortcuts are closed and measured, the exporter's four full-window
entity labels are withheld and quarantined, the point-in-time feature
contract is written and pinned, and prevalence is gated per year.
ROUND 4 re-pointed the whole gate harness at the PRODUCTION population;
ROUND 5 rebuilt the one claim that round falsified; ROUND 6 fixed the
operator session on victim-AUTHORIZED pushes; ROUND 7 closed the
remaining device-ID shortcut, enforced membership intervals, and added
transaction-time device/IP edges with a closed vertex universe and no
asymmetric Party-ownership topology. ROUND 7 also repaired legitimate
credit-card session routing and the autopay/due-date clock. Its
end-to-end trace rejected a tempting but false “fix”: late-injected fraud
cannot simply be pointed at an already-serviced credit-card liability.
ROUND 8 replaced the `use_chip` content hash with CAUSAL entry mode read
off the destination's acceptance environment plus the dated US EMV
terminal mix (exporter-only; `error` remains the open hash half).
Remaining benchmark gates are explicit below: integrating credit-card
fraud into lifecycle servicing, effective card lifecycles and reissues,
era/concept drift in the fraud process, delayed labels, level
calibration, and an executable GSQL/training/evaluation path.

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
7. **HISTORICAL ROUND 6 scope ruling:** fix the authorized rails; SIZE
   and DECLARE the `FD` device-render leak on the card/ato rails rather
   than closing it in that round. **SUPERSEDED BY ROUND 7:** all device
   owner types now render through one opaque, fixed-width `D` namespace.

## The standing verdict, restated

The card-fraud corpus was a good PostgreSQL / TigerGraph /
temporal-loading artifact and not a credible online fraud benchmark.
Gates 1, 3, 4 and 5 now have code behind them. What is still missing for
a benchmark CLAIM is calibration of the fraud LEVEL against a named
issuer-side series (the arc measures separability and stability, not
level), effective instrument lifecycles, era-varying fraud mechanisms,
delayed-verdict evaluation, and the executable GSQL/training/evaluation
implementation. The README and online-GNN contract state this without
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

**The modality decision drives DESTINATION SELECTION — and since
ROUND 8 the export READS it back off the destination.** See the
`use_chip` history in the leak inventory below.

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

**PROVENANCE OF THE NUMBERS IN THIS SECTION (ROUND 4):** every count
above was read off the JOINERLESS harness world. ROUND 4 flipped the
harness to the production join cohort, which RE-ROLLS the corpus (see
below), so the row counts, merchant counts and base rate move. What does
NOT move is the verdict: both bounds are held by the b-2 destination
MECHANISM rather than by a seed, and both were re-verified at 0.0000.
The gates now PRINT their own world shape, so a future reader never has
to guess which population a recorded number came from.

### ROUND 1 — label leakage (EXPORTER-ONLY; three table goldens re-pin)

Four full-window entity verdicts rode the feature graph:
`Card.is_fraud` ("this card ever carried a flagged row"),
`Party.is_fraud`, `Device.is_blocked`, `IP.is_blocked`. A GNN given any
of them scores ~100% and learns nothing about behavior.

Per ruling 6 the columns are RETAINED and written as 0
(`kLabelWithheld` in `src/exporter/card_fraud/export.cpp`), and the
investigative content moves to a dedicated table,
`card_fraud."cf_Ground_Truth_Label"` — `(entity_type, entity_id,
label)`, positives only, joinable 1:1 to the vertex tables, pointed at
by no edge and loaded by no TF_GNN_v3 job. The one supervised target in
the graph is `Payment_Transaction.is_fraud`, anchored to its own row's
timestamp for offline supervision and never admitted as an input
feature. Production-like delayed verdict availability remains an open
benchmark gate.

Gates: `tests/test_pipeline_e2e.cpp` (serverless — every cell of the
four columns renders 0, and whenever the window produced flagged card
rows the overlay carries the cards they touched);
`tests/test_table_golden.cpp` (the same at the production config, pop
10000, against live PostgreSQL); `docs/card_fraud_postgres_acceptance.sql`
(overlay↔vertex join integrity and label vocabulary). This round moved
the table count 34 → 35; ROUND 7's two event-time session edges move the
current contract to **37**.

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
| STREAM PREFIX | the five streamed transaction tables — transaction vertex, card/merchant edges, and transaction-time device/IP edges — must be a byte-exact prefix |
| IDENTICAL | world-derived tables (Party and the PII layer) cannot move at all |
| GROWING SET | `Card`, `Merchant`, `Device`, `IP`, and the geo chain: the row SET may grow, a row may never change |

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
stopped riding `L`); fraud rides `card_purchase` with merchant-POS share
< 0.05; at least two typologies with the unauthorized family > 0.30;
fraud tickets exceed legitimate ones; episode size bounded; the
aggregate rate inside a wide plausibility band.

PRINTED, NEVER GATED: per-year card-view ROW COUNTS. H4's
real-consumption ramp raises them while the documented small-N
liquidity drain (U-9 ADDENDUM, ~27% in second-year gate legs) lowers
them; at N=300 the two are not separable. **Do not gate what the
harness cannot resolve** — the same reasoning that subsumed H4's
recession-direction gate.

**ALSO PRINTED, NEVER GATED — and this one was a GATE until ROUND 4
falsified it.** A per-year "fraud amounts are flat in CALIBRATION
dollars, spread < 2.5×" check shipped in this round, described here as
*the only gate proving U-6 class F scaling reaches the card fraud rail*.
That description was wrong, and so was the gate. `unauthorized.cpp`
applies `priceScale` only to the CONTINUOUS samplers (`cardFraudSpend`,
`atoDrainAmount`); the DENOMINATION samplers (`cardTestCharge`,
`giftCardScamAmount`) are FIXED-NOMINAL by owner-approved CHOICE
(authority U-6). The card view mixes all three, so deflating the
COMBINED mean and asserting flatness asserts the OPPOSITE of U-6 — and
`test_econ_wiring`'s `isScaledFraudRow()` already excluded this same
rail for this same reason, meaning the two gates contradicted each other
and this one was wrong. Independently, the statistic was under-powered:
42–92 draws from lognormal(σ = 1.2) give CV = 1.79/√n ≈ 19–28% on a
per-year mean, so a 2.5× max/min envelope over four years sits inside
sampling variation. It is now PRINTED and DECOMPOSED — per-year
fixed-nominal-lattice vs CPI-scaled counts and means, nominal and
deflated spreads side by side, plus a class-F clamp-ceiling ratio — so
ROUND 5's replacement could be sized from data. Full account: authority
U-13 ADDENDUM 2.

IBM TabFormer's observed 0.11675% is a NAMED COMPARATOR with a wide
plausibility band (0.02%–2%) and the ratio is printed, not pinned.
PhantomLedger is TabFormer-SHAPED, not calibrated to it.

### ROUND 4 — the harness WORLD SHAPE (zero-golden, harness-only)

Every gate in this arc runs on `pltest::runLeg`, and `GateWorld`
defaulted `withJoinCohort = false` → `identity.windowDays = 0` → a
population with NO join cohort. Production does the opposite
unconditionally (`SimulationPipeline::buildEntities`, simulate.cpp:168).
So every band in sections c and ROUND 3 above was calibrated against a
world the generator never emits. The default is now TRUE; the flag
survives only as a bisect knob and no gate ships with it false.

Not a 3% perturbation: `joinDays`, `dob` and `timeline` draw on isolated
lanes, so the shared stream's draw SEQUENCE is untouched — but the
joiners' ages, transition dates and death dates move, and every lane
that READS those attributes consumes the shared stream a different
NUMBER of times as soon as one transition crosses a window boundary.
Downstream of the first such read the corpus is a fresh realization.

**GOLDEN IMPACT: ZERO, and it was MEASURED rather than assumed.** The
V3 re-pin was committed before the flip was written, so the four
baselines predate it; the golden gates recapture only when a baseline is
absent, so the run that followed compared post-flip digests against
pre-flip pinned baselines and passed. Do not re-capture those baselines
casually — that would retroactively make the invariant unfalsifiable.

**BAND OUTCOME: nothing widened, nothing re-centred.** One red appeared
and it was the mis-specified flatness check above — diagnosed with the
nominal-spread discriminator shipped alongside it, then reclassified
rather than loosened. Each gate now PINS `joiners > 0` as its own
precondition and prints its world shape, so an anti-shortcut baseline
can never again be measured against a population production does not
generate. Authority U-13 and its two addenda.

### ROUND 5 — class F on the card rail (zero-golden, new gate)

`tests/test_card_class_f.cpp` restores the coverage ROUND 4 withdrew.
Two era legs at N=900 — 1991/1461d and 2019/730d — comparing the **75th
percentile** of non-lattice card-fraud amounts, deflated by each row's
own year's `priceScale`.

Four design decisions, each one a direct answer to how the previous
attempt failed:

| Failure of the old gate | What this one does instead |
|---|---|
| Mixed three amount families into one deflated mean | Excludes the RESOLVABLE lattice (gift-card rows, by `FraudType`) outright |
| Could not filter card-test probes (same `FraudType` as the spend) | Uses a QUANTILE, which a $5-bounded contaminant at the bottom of the axis cannot reach — and GATES that the probe share stays below it |
| Asked a heavy-tailed sample to resolve a NULL effect (flatness) | Asks a CROSS-ERA question with a ~1.8× effect, the shape that already works for the ring rail |
| Hand-picked a 2.5× envelope that turned out to be noise | Sizes the band as ±3σ of the REALIZED two-leg quantile SE (analytic CV = 1.635/√n) |

The key property: a quantile of a scale family scales *exactly* with the
scale, so `Q_p` reads the price level cleanly and `p` is free to be
chosen above the contamination. **When a mixture cannot be separated, do
not deflate the aggregate — pick a statistic the unresolvable component
cannot reach.**

And the gate **checks its own power**: it computes the FIXED-NOMINAL
null (≈0.55) and the DOUBLE-SCALED null (≈1.81) from the realized price
scales and FAILS as UNDER-POWERED unless its own band excludes both. An
undersized leg reports that fact instead of passing vacuously, and the
repair is a larger leg, never a tighter band.

N=900 is deliberate: the card rail is sparse and the power arithmetic is
not satisfiable at N=300, which is precisely the trap the withdrawn gate
fell into.

**CORRECTION, from the gate's own first run.** N=900 was ALSO justified
here as "clears the ~833 ring threshold, so solo and ring card spends
are both exercised." That is FALSE, and wrong on mechanism rather than
merely on count: the very first run printed `ring 0` in both legs.
`buildCompromisePlans` excludes ring participants and ring victims, so
**the unauthorized card rail is ring-free BY DESIGN at every
population**, and its class-F population is `txnFraudSolo` only. N=900
stands on the power arithmetic alone. The ring/other counters are
RETAINED as a **TRIPWIRE** — nonzero means a later round routed a
different fraud family onto this rail — and are documented as one so
nobody deletes them as dead code. **Audit the justification you wrote
against the gate's own printed output before calling a round done.**

### ROUND 6 — who operated the row (MODEL-MOVING, four goldens re-pin)

What was then the last declared generation defect in this arc.
`unauthorized.cpp`
stamped the ATTACKER's device and IP onto every emitted row, including
the two victim-AUTHORIZED rails where the victim is the one transacting:
the gift-card victim walks into a store and buys the cards, and the
impostor-push victim wires or app-pushes their own money. The row
asserted the opposite of its own typology.

**THE DEFERRAL RATIONALE WAS FALSIFIED BY READING THE CONSTRUCTION.**
The recorded reason for deferring (audit OPEN ITEMS #4) was that a fix
would have to call `infra::Router` from the fraud planner, advancing
that victim's STICKY device index and perturbing legitimate routing in
later windows — the exact path `test_arch_equivalence` and
`test_production_windowed` compare byte-for-byte. That cost is **already
being paid by the existing code**: every unauthorized row has
`draft.ringId = -1` (so the SharedInfra branch is skipped), a
customer-session channel (`cardPurchase` / `p2p` / `externalUnknown` —
none is payday-inbound and none appears in `isExternallyInitiated`), and
`draft.source = plan.victimAccount`. So `transactions::Factory::make`
already resolves `ownerOf(victim) → routeDeviceFor / routeIpFor` on the
plan's own rng lane and writes the victim's session onto the row. The
two overwrite lines were **discarding a correct value that had already
been computed**.

The fix is therefore to stop overwriting on the authorized rails. It
consumes **no new randomness**, advances **no new sticky state**, adds
**no carrier**, and touches neither engine's plumbing — it lives entirely
inside `unauthorized::generate`, which both engines share.

**AND THE OBVIOUS "SAFER" FIX WOULD HAVE OPENED A NEW SHORTCUT.** The
registered design was a read-only non-advancing pick —
`devicesByPerson[person].front()`. That hands every authorized-rail row
pool slot 0 while the SAME victim's legitimate rows follow the sticky
index, so `device != that person's current device` becomes the
replacement label. Letting the router's own result stand is what makes
the scam row's session indistinguishable, by construction, from a
legitimate row of the same victim. **The non-advancing read was the
worse option, and only reading the routing path showed it.**

Gate: `tests/test_unauthorized_keyed.cpp` gains a Router-backed leg
asserting the rail-conditional session — authorized rows carry the
victim's own device/IP and NOT the attacker's; card/ato rows KEEP the
attacker session (a tripwire against over-applying the fix). Both
authorized rails are pinned present (gift-card 4 rows, impostor 3), so
neither half can pass vacuously. The impostor rail had no unit coverage
before this round.

**HISTORICAL ROUND 6 FINDING — THE DEVICE `ownerId` RENDER CHECK WAS A
LEAK.**
Carried beside this item was the question of whether the magic
`0xACE00000` ownerId with `OwnerType::ring` is visible downstream, on
the hypothesis that `renderDeviceId` might hash the key. **It does not
hash.** `exporter/common/render.hpp` switches on `ownerType` and the
`ring` branch writes the literal `encoding::kFraudDevice` layout
(`{"FD", 4}`), so an attacker device renders `FD2900000768` while a
person device renders `D<customer>_<slot>`. `writeSessionCells` puts that
string in `public.transactions.device_id` on every ledger row. This is a
DETERMINISTIC label, not a 1-in-14M coincidence like TEST-NET-2 — a
strictly stronger version of the shortcut this arc already closed once.

Per the historical owner ruling 7 it was SIZED AND DECLARED in ROUND 6,
not closed: the authorized-rail fix removed it from the gift-card and
impostor rows, while card/ato correctly retained the attacker session.
**ROUND 7 SUPERSEDES THAT DISPOSITION.** The defect was exporter-side
rendering, so all owner types now share the same opaque rendering rather
than changing the underlying session semantics.

**ROUND 6 GOLDEN IMPACT (historical): all four re-pin, for two different
reasons.** The
authorized-rail rows change `device_id` and `ip_address`, so the corpus
stream moves: `golden_run.b2sum`, `golden_tables.md5` and
`golden_tables_aml.md5` move on those columns. `golden_tables_card_fraud.md5`
moves **only through the corpus-stream digest it also pins** — every
`cf_*` table should be byte-identical, because `Payment_Transaction`
carries no device/IP column and `Device`/`IP`/`Has_Device`/`Has_IP` are
built from `world.infra.devices|ips` (entity synthesis), not from
transaction sessions. `use_chip` and `error` hash timestamp, endpoints
and amount only, so they do not move either. **That asymmetry is the
checkable prediction of this round.** The RNG stream is untouched, so
row counts, amounts, timestamps and destinations must all be unchanged.

### ROUND 7 — causal session graph and lifecycle closure

ROUND 7 is a combined model/export repair. It closes several ways an
otherwise accurate temporal model could teach a GNN the generator's
implementation instead of payment behavior.

1. **Membership is now an interval, not a join-only predicate.**
   `VictimPopulation::member()` requires the case timestamp to be inside
   `[joinTs, death + 120-day settlement)`. Authorized scams additionally
   require the victim to be alive; card and ATO cases may still occur
   during the declared estate-settlement tail, but never after account
   closure. Planning also requires the entire sampled case span to fit
   before the earliest victim/payee horizon, so a case that starts one
   second before closure is rejected rather than compressed into an
   artificial velocity burst; post-horizon chargebacks are suppressed.
   The card-graph stream independently resolves the owner of each owned
   endpoint and excludes view rows outside that owner's membership
   interval. Neither the raw generated fraud rail nor the online feature
   graph now carries an owned endpoint across its boundary.

2. **A false stolen-card carrier fix was rejected.** Before this round,
   every unauthorized card case used the victim's primary deposit
   account, so the card exporter classified every positive as a derived
   debit card and no modeled credit card carried fraud. A first pass
   swapped in the issued credit-card key. The end-to-end trace showed
   why that was not a fix: fraud is planned only after
   `CardCycleDriver` has closed and serviced legitimate cycles, so the
   late-injected liability purchase bypassed statements, payments,
   interest, and lifecycle screening. The swap was reverted and a gate
   now rejects unserviced credit-liability sources. Honest credit-card
   fraud requires moving planning into the lifecycle and remains OPEN.
   Independently, the router's owner map now includes credit-card keys,
   which repairs the pre-existing empty device/IP session on legitimate
   credit-card purchases.

3. **Device identifiers are role-neutral.** `FD`, `LD`, and the
   person-key-shaped rendering are retired. Every assigned
   `devices::Identity` is pseudonymized by a stable repository-owned
   digest and rendered as one fixed-width `D` identifier. Owner type,
   prefix, width, and numeric range can no longer label attacker,
   customer, or shared infrastructure. This is a stable pseudonym, not a
   cryptographic security boundary; models must treat it as categorical.
   The rail semantics from ROUND 6 remain: card/ato use the attacker
   session, authorized scams use the victim's routed session.

4. **The graph now carries the session that happened at the
   transaction.** `Transaction_Uses_Device.csv` and
   `Transaction_Uses_IP.csv` append one timestamped edge for each
   assigned session endpoint. They join `T<row_seq>` to the exact
   device/IP observed on that row and are stream-prefix tables under the
   point-in-time contract. `Has_Device` and `Has_IP` are POPULATED as of
   attacker-infra-2026-07 (they were header-only here for four rounds):
   they carry the associations the institution has ON FILE, with declared
   partial coverage, so missing adjacency is weak evidence rather than an
   attacker-role bit. World-derived, never stream-derived.

5. **Observed endpoints close the vertex universe.** Exogenous attacker
   devices and IPs are not required to belong to the synthesized
   customer/ring infrastructure roster. `cf_Device` and `cf_IP` are now
   the union of that roster and every endpoint observed on a card-view
   transaction, while preserving the roster's withheld
   flag/blacklist-overlay facts. Consequently, “edge endpoint absent
   from its vertex table” is no longer a deterministic fraud label.
   Withholding all static Party→Device/IP rows also closes the
   complementary “endpoint has no Party edge” shortcut without inventing
   false ownership for an exogenous attacker.
   The two new edges bring the card-fraud export from **35 to 37
   tables**.

6. **Card-payment time uses one clock.** The due cutoff is constructed
   once at 17:00 on the resolved due date. Autopay posts at noon on that
   due calendar day, manual on-time/late samples are constrained to the
   same cutoff, and a late fee posts at 10:00 on the following day. The
   old path added a 12-hour lag to an already-timed due timestamp, which
   made autopay systematically late and distorted long-horizon
   delinquency behavior. `tests/test_card_payment_timing.cpp` pins
   autopay, manual timing, weekend resolution, and late-fee ordering.

**GOLDEN/SCHEMA IMPACT:** model-moving and exporter-moving. Legitimate
credit-card sessions, membership filtering, payment timestamps, and
role-neutral device rendering can move corpus and use-case digests; the
two new edge tables and header-only static infrastructure edges move the
table manifest. Re-pin only after the membership, endpoint-integrity,
point-in-time-prefix, debit-only lifecycle guard, payment-timing, and
engine-parity gates are green.

### ROUND 8 — causal `use_chip` (EXPORTER-ONLY; card-fraud table golden re-pins)

The registered "exporting the real card-present modality" item, landed
as use-chip-causal-2026-07. Before this round `derive::useChipFor` was
an FNV content hash of the row (Swipe .63 / Chip .26 / Online .11) for
fraud and legitimate rows alike: point-in-time safe, mechanism-free,
and INCOHERENT with the graph — a physical grocery outlet could render
"Online Transaction", a 1994 row could render "Chip Transaction", and
the hash could contradict the merchant-geography structure the model
also sees.

**THE MECHANISM WAS ALREADY IN THE WORLD; the export just refused to
read it.** Entry mode is a property of the ACCEPTANCE ENVIRONMENT, and
the acceptance environment is exactly the `Footprint` axis both sides
of generation already partition destinations on: legitimate selection
splits card-present picks (distance-decayed physical pools) from online
picks (the `Footprint::online` national CDF) in
`payments.cpp::pickMerchantIndex`, and the fraud rails make the same
split per-case in `unauthorized.cpp::pickMerchantDestination`. So the
DESTINATION CARRIES the modality decision, and the exporter can derive
it causally with NO row-schema change, NO new carrier, NO new
randomness, and NO generation impact — the ROUND 6 lesson ("check
whether the value is already on the row") applied to the exporter.

The derivation (`derive.hpp`, fed by the same catalog index that
resolves `mer_cat`):

- catalog `Footprint::online` destination → **"Online Transaction"**
  (the CNP acceptance population, for fraud and legit rows alike);
- non-catalog view destination (the fraud rails' degraded biller
  fallback — remote-billed hub accounts by construction) → "Online
  Transaction", DECLARED CHOICE;
- physically-located outlets (localOutlet / regionalOutlet /
  nationalService — every non-online footprint gets a real GeoArea in
  `placeGeography`) → card-present, split **Chip/Swipe by the dated US
  EMV terminal mix** (`chipShareBasisPoints`): zero before 2012, low
  single digits through the October 2015 network liability shift, 0.65
  in 2019, frozen at 0.90 outside coverage — the era-scale freeze
  convention. Values are a DECLARED CHOICE shaped by the EMVCo US
  chip-share series ([Likely] — owner verifies). The per-row draw stays
  content-keyed on `kUseChipLane`, so byte-identical rows derive
  identically on every toolchain.

**THE ANTI-SHORTCUT READING, stated before anyone asks.** This
deliberately makes `use_chip` correlate with the label — fraud is
CNP-majority (`kCardNotPresentShare = 0.70`) while legitimate spend is
CNP-minority (~0.11 mode roll) — and that is REAL signal, the same
class of honest correlation as b-2's distance decay. It opens no NEW
structural shortcut because the modality was ALREADY visible to a graph
model through merchant geography (online merchants are geography-free
vertices); the round makes the flat feature AGREE with the structure
instead of contradicting it. The merchant-ID baseline gate
(`test_card_baselines`) still holds the ceiling on destination-derived
separability.

Gate: `tests/test_card_use_chip.cpp` — two 300-person legs, 1991 and
2019. GATED: the coherence pin (Online ⟺ geography-free destination — a
regression barrier that fails the moment a hash returns); ZERO chip
rows in the pre-EMV 1991 leg (sharp, sampling-free); chip AND swipe
present in 2019 with the realized chip share inside [0.60, 0.70] behind
a ≥1,000-row power precondition that FAILS as under-powered (ROUND 5's
law); both modalities and labels populated per leg; the EMV table's
fixed points pinned at compile time. PRINTED, NOT GATED: fraud-vs-legit
CNP shares (the modeled direction realizes over CASES and the gift-card
rail is card-present by construction — instrument first, band later).

Support: `LegResult` now carries a copy of the leg's merchant catalogue
(`window_leg_support.hpp`, additive field) so exporter-derivation gates
resolve footprints against the records the generator selected from.

**SCOPE LIMITS, declared:** entry mode only — `error` remains a content
hash (authorization attempts are unmodelled) and is the open half of
online-GNN gate 4. The chip/swipe split is a presentation-layer
terminal-technology mix, not per-card/terminal adoption state (that is
the card-lifecycle gate). The legitimate CNP share (`kCardPresentShare
= 0.89`) is still era-flat — its dated version belongs to the
fraud-process era-drift gate, and payments.cpp now documents that this
constant shapes the exported entry-mode mix directly.

**GOLDEN IMPACT: exporter-only, ONE baseline.**
`golden_tables_card_fraud.md5` re-pins (`Payment_Transaction.use_chip`
changes value distribution). The corpus stream golden
(`golden_run.b2sum`), `golden_tables.md5` and `golden_tables_aml.md5`
MUST NOT move — no generation code changed except a payments.cpp
comment — and the card-fraud section's corpus-stream digest must still
equal the fraud section's. A moving stream digest here is a defect in
the round, not a re-pin. The point-in-time contract is unaffected:
use_chip = f(row content, static footprint, row year) is deterministic
in the row, so STREAM PREFIX holds by construction and
`test_card_point_in_time` re-verifies it.

### Layering verified

`PL_LINT_ALLOWED_transfers` includes `activity` and the entities vocab,
so the injector may read the merchant catalogue and the shared decay
curve directly — one curve, two consumers. No relocation round.
(ROUND 8 adds no edge: the exporter already read
`entities/counterparties/merchants.hpp` for the category index, and the
footprint rides the same record.)

## THE PARITY TRAP (must survive into every later round)

`legitCounterparties` has **FOUR** call sites:

| Site | Role |
|---|---|
| `src/pipeline/simulate.cpp:95` | monolith reference oracle |
| `src/pipeline/stages/transfers/windowed_run.cpp:285` | PRODUCTION windowed engine |
| `tests/window_leg_support.hpp:313` | the gate harness every v2 gate runs on |
| `tests/test_membership.cpp:626` | membership gate, now filled with the same persona carrier as production |

The two ENGINES must pass IDENTICAL arguments or
`test_arch_equivalence` / `test_production_windowed` diverge. The gate
harness must be filled too, or every gate measures a carrier-free world
and passes vacuously.

**THE SAME TRAP HAS A WORLD-SHAPE FORM (ROUND 4).** Filling the carriers
identically is not enough if the two sides build DIFFERENT POPULATIONS.
`test_arch_equivalence` reported a settlement-shaped "SEMANTIC
divergence" for exactly this reason, and the diagnostics blamed the
wrong layer for a full round. Both sides now measure the cohort off the
carrier and pin equality BEFORE any corpus comparison.

**ROUND 6 IS THE TRAP'S COUNTEREXAMPLE, WORTH KEEPING.** Not every fix
needs a carrier. The session fix required no new argument at any of the
four sites, because the value it needed was already being computed
inside `transactions::Factory` and thrown away. **Before adding a
carrier, check whether the value is already on the row** — the parity
trap is a cost, and the cheapest way to pay it is not to incur it.
**ROUND 8 is the exporter-side instance of the same lesson:** the
modality was already on the row (through the destination), so the
causal export needed no generation change at all.

## CUT from the critical path (polish, registered not forgotten)

- **b′ — compromise incidence scaling.** The Bettencourt tilt on victim
  selection: P(compromise) ∝ homeAreaPopulation^(β−1), β ≈ 1.15,
  normalized so total prevalence is unchanged (the budget law still
  owns how much fraud exists). One function, one named constant, one
  named lane. Must land AFTER a home-area-only baseline, which is why
  that baseline is NOT in `test_card_baselines` today — with no tilt in
  the world it would measure nothing.
- **~~Transaction-time device/IP edges.~~ LANDED in ROUND 7.**
  `Transaction_Uses_Device` and `Transaction_Uses_IP` carry the exact
  row session with `edge_unix_time`. `Has_IP`/`Has_Device` were called
  header-only compatibility tables here; both are POPULATED as of
  attacker-infra-2026-07. **And "LANDED" was doing a lot of work in this
  bullet:** the edges shipped while the endpoints they pointed at were
  minted one per compromise, so the layer was structurally inert — an
  exported edge being implemented is not the same as the entity behind it
  being modelled.
- **Credit-card fraud lifecycle integration.** Unauthorized positives
  remain derived-debit backed. Planning must move before card-cycle
  finalization so a compromised issued card affects statements,
  payments, interest, chargebacks, limits, and later spending. A late
  source-key substitution is explicitly test-rejected.
- **~~Exporting the real card-present modality~~ LANDED as ROUND 8**
  (`use_chip` reads the destination footprint + the dated EMV terminal
  mix; `tests/test_card_use_chip.cpp`). Still registered from that
  design: a MODELED authorization-outcome mechanism to replace the
  `error` hash, and a time-varying legitimate CNP share.
- **Effective card lifecycles and reissues.** Membership intervals are
  now enforced, but cards are still effectively static instruments over
  the owner's tenure: expiry, replacement after compromise, renewal,
  product migration, and multiple-card histories are not modeled.
- **Fraud-process era/concept drift.** H4 delivered era-varying
  legitimate volume, not an era-varying adversary. Compromise rates,
  CNP/payment-method mix, authentication, device reuse, and fraud
  strategy need dated mechanisms so a decades-long benchmark does not
  repeat one 2020-shaped process backward through time. (ROUND 8's
  dated EMV mix covers the PRESENTATION layer only.)
- **Delayed labels and executable evaluation.** The current target is
  emitted on the transaction row. A benchmark must reveal
  reports/chargebacks/verdicts only after their modeled delays and ship
  the GSQL feature query, training code, temporal splits, baselines, and
  evaluation harness that enforce score-before-update.
- **Level calibration** of card-fraud prevalence against a named
  issuer-side series (Nilson / FTC), including the CNP share.
- **~~Device `ownerId` render check and `FD` remediation.~~ CLOSED in
  ROUND 7.** ROUND 6 confirmed the literal role prefix; ROUND 7 replaced
  every role-specific layout with one stable opaque `D` namespace.
- **~~e — the two-era card-rail class-F leg.~~ LANDED as ROUND 5**
  (`tests/test_card_class_f.cpp`). The coverage gap opened by the
  withdrawn flatness gate is closed.
- **f — promote the class-F CLAMP CEILING from print to gate.**
  `cardFraudSpend` clamps to `[1.0 × priceScale, 5000 × priceScale]`;
  `test_card_prevalence` prints `max fraud amount / (5000 ×
  priceScale(year))` per year. An UNSCALED clamp would push that ratio
  above 1.0 in the early era — a sharp, sampling-free signal, unlike a
  mean-flatness envelope. Gate it at ≤ 1.0 once a run confirms the
  observed values. NOTE its one weakness before shipping it: the clamp
  binds rarely (P(draw > $2,663 in 1991) ≈ 0.0017), so at a few hundred
  rows the gate has real specificity but only partial POWER — it is a
  cheap complement to ROUND 5, not a substitute.
- **g — `postCloseWorkers >= 1` coverage fragility in
  `test_persona_wiring`.** PRE-EXISTING, surfaced by the ROUND 4 audit,
  not introduced by it: an existence gate whose expectation is ~3 reads
  0 on roughly 5% of ANY re-roll. **The repair if it fires is a longer
  window, NEVER a lower floor** — a floor of 0 asserts nothing.
- **~~The `FD` device render (ROUND 6, owner-deferred).~~ CLOSED in
  ROUND 7.** The chosen solution changes every owner type together:
  stable opaque digest, common prefix, common width, common numeric
  domain. This intentionally moves AML/mule renderings as well as
  attacker rows, so it belongs in the ROUND 7 re-pin.

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
| `use_chip` / `error` are export-time hashes, not causal | Confirmed, and **a claim made earlier in this arc was WRONG**: b-2 did NOT make `use_chip` model-backed — until ROUND 8 the modality decision drove destination selection and was never exported | **use_chip CLOSED (round 8)** — entry mode reads the destination's acceptance footprint plus the dated EMV terminal mix; the feature contract reclassifies it FEATURE-SAFE (`test_card_use_chip`). **error remains OPEN** — still a content hash, classed USE WITH CARE, the remaining half of online-GNN gate 4 |
| Raw `public.transactions` exposes ground truth + TEST-NET IPs | **BOTH CONFIRMED.** The TEST-NET claim DOES reproduce — the earlier "did not reproduce" reading grepped the dotted-quad string and missed `Ipv4::pack(198, 51, 100, …)` | IP shortcut **CLOSED**. The raw ledger's `is_fraud`/`ring_id`/`fraud_type` columns are the generator's own ground truth and stay — that table is the corpus, not the feature graph; the contract names `ring_id`/`fraud_type` PROHIBITED for features |
| The arc's own gates measured a joinerless population | **CONFIRMED (ROUND 4)** — `GateWorld` defaulted `withJoinCohort = false`, so no gate in this arc had ever run on the shipped world shape | **CLOSED (round 4)** — default inverted, `joiners > 0` pinned as a precondition in all four behavioural gates, world shape printed on every leg line. One band was falsified by the flip and WITHDRAWN rather than widened |
| No gate proved U-6 class F scaling reaches the CARD rail | **CONFIRMED (ROUND 4)** — the gate that claimed to was mis-specified and under-powered, and withdrawing it left the coverage at zero | **CLOSED (round 5)** — `test_card_class_f.cpp`, a cross-era deflated-quantile gate that sizes its own band and fails as UNDER-POWERED rather than passing vacuously |
| The attacker session rides victim-AUTHORIZED rows | **CONFIRMED (ROUND 6)**, and the recorded deferral rationale was FALSE: `transactions::Factory` already routed the victim's own device/IP onto these rows and `unauthorized.cpp` overwrote it, so the sticky-index cost the deferral feared was already being paid | **CLOSED (round 6)** — the overwrite is now skipped on the two authorized rails. No new draws, no new carrier, no plumbing change. The registered "non-advancing `front()` pick" alternative was REJECTED as a replacement shortcut |
| Device `ownerId` carries a magic `0xACE00000` prefix — does it reach the corpus? | **CONFIRMED A LEAK (ROUND 6).** The old `OwnerType::ring` branch wrote literal `FD…`, while person/shared devices used distinguishable layouts | **CLOSED (round 7)** — every assigned identity now renders through the same stable opaque, fixed-width `D` namespace. Tests reject role prefixes and width/range distinctions; raw IDs remain categorical only |
| Exogenous session endpoints are absent from `cf_Device` / `cf_IP` | **CONFIRMED (ROUND 7 audit).** Attacker endpoints can be generated outside the synthesized infrastructure roster, so vertex absence was a structural label once event-time edges were introduced | **CLOSED (round 7)** — the vertex universe is the union of roster endpoints and endpoints observed on card-view rows; every session edge endpoint is pinned present |
| Exogenous endpoints have no `Has_Device` / `Has_IP` Party edge | **CONFIRMED (ROUND 7 adversarial review).** Populating those tables only for customer-owned endpoints turns Party adjacency into an attacker-role bit | **CLOSED (round 7) → REOPENED AND RE-CLOSED DIFFERENTLY (attacker-infra-2026-07).** The round-7 disposition treated the asymmetry as a fact about the world; it was a fact about the GENERATOR. The withholding also had a cost nobody priced: Party is TF_GNN_v3's only path to Device/IP, so empty ownership tables left every endpoint vertex isolated and the layer inert. Re-closed by removing the asymmetry — partial registry coverage (`infra::enrollment`, ~72%/~61%) puts legitimate rows on un-recorded endpoints, and victim-endpoint + residential-proxy fraud puts fraud on recorded ones. Residual "not on file ⇒ fraud" precision **0.027 at 2.9x lift**, gated by `test_card_endpoint_graph` |
| **Attacker endpoints are never reused across victims** | **CONFIRMED (attacker-infra-2026-07), and it was the largest open defect in the use case.** `buildCompromisePlans` minted `Identity{ring, 0xACE00000 + seq, 0}` and a fresh `randomIpv4` per compromise, so cross-victim endpoint sharing was ZERO BY CONSTRUCTION — the reason to model card fraud as a graph at all. **Four gates stayed green through it**, because every one asserted endpoints were PRESENT and none asserted they were SHARED | **CLOSED (attacker-infra-2026-07)** — endpoints come from campaign-scoped operator infrastructure with tiling tenure chains and a Pareto case load. Measured: **74–82% of attacker devices seen by >1 victim, mean 5–9, max 37–40**; zero out-of-tenure attribution. Draw-count-preserving on the planner lane, so corpus row counts are UNMOVED (189,035 → 189,035) and only the device/IP columns moved. `test_card_endpoint_graph` gates degree distribution, pool sizing, the shortcut size, and the point-in-time hard zero; confirmed non-vacuous by disarming reuse (12 checks red, mean fan-out collapses to 1.03) |
| Card positives exercise modeled credit cards | **FALSE.** Every compromise uses the primary deposit account, so every positive exports as a derived debit card. The attempted Round 7 key swap bypassed already-closed statement servicing | **OPEN, with guard** — `test_card_prevalence` requires debit-backed positives and zero late-injected liability sources until fraud planning is integrated into `CardCycleDriver` |

## Law compliance

NO new CLI; NO runtime path selection; NO network; embedded data only
(the scaling law adds a constant, not a table; ROUND 8's EMV curve is
likewise an embedded constant table). New randomness on a NAMED lane.
b-2 and the IP fix are model-moving (four-golden re-pin); round 1 is
exporter-only (three TABLE goldens; a moving stream golden would be a
defect, and it did not move); the fill, c, and rounds 2–5 are
zero-golden — rounds 4 and 5 touch only test-only code, so a moving
golden there is a defect rather than a re-pin. **ROUND 6 is
model-moving** (four-golden re-pin) but consumes NO new randomness, so
row counts, amounts, timestamps and destinations must be unchanged and
only the two session columns may move — a moving amount or count there
is a defect, not a re-pin. **ROUND 7 is deliberately model- and
export-moving:** payment timestamps, sessions, membership-visible rows,
device renderings, the endpoint vertex sets, two new edge tables, and
the header-only static infrastructure edges can move. It adds no runtime
network or path selection and no unscoped randomness; its opaque ID
algorithm is repository-owned and deterministic. **ROUND 8 is
exporter-only, ONE-baseline:** only `golden_tables_card_fraud.md5`
re-pins; the corpus stream and the standard/aml table goldens must not
move, it consumes no randomness (content-keyed hash retained for the
chip/swipe tiebreak only), and it adds no CLI, no network, no runtime
path selection.
