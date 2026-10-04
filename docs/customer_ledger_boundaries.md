# Customer-ledger boundary flows

Status: implemented for cash withdrawal, cash deposit, settled check deposit,
and bank-visible crypto fiat ramps.

## The accounting boundary

PhantomLedger projects customer accounts; it is neither a bank general ledger
nor a blockchain asset ledger. The bank's side of these events (vault cash,
check collection, network settlement, correspondent or custody books) is out
of scope, so only the customer leg is posted:

| Event | Recorded context | Customer-book mutation |
|---|---|---|
| `atm_withdrawal` | external ATM terminal/acceptor | debit customer only |
| `cash_deposit` | external cash depository | credit customer only |
| `check_deposit` | external check-capture/collection point | credit customer only |
| `crypto_ramp_out` | external crypto service venue | debit customer only |
| `crypto_ramp_in` | the same venue class | credit customer only |

Boundary keys are not infinite sources or sinks: they are registered for
referential integrity but ownerless, `Bank::external`, outside the internal
ledger index, never seeded and never balance-mutated. External-to-external
events are rejected.

The schema requires a source and a target, so the external key is the observed
service or capture context, not the beneficiary account or a proxy balance. A
future nullable-counterparty schema may omit an unobserved context; an invalid
key today would break validation, sorting, CSV/PostgreSQL readback and the
graph exporters.

## The bank's income ledgers

Fee and interest postings are the one bank-side leg, because their contra (the
bank's income GL for that posting kind) is fully determined. Four internal,
ownerless `Role::ledger` accounts (`entities/holdings/general_ledger.hpp`)
take card interest income, card fee income, deposit fee income and credit-line
interest income. Never seeded or debited, each balance equals the income
posted to it. They are bank-owned accounts, not boundary contexts, and every
exporter types them so (bank-gl-2026-09 amendment in
`docs/fraud_model_audit.md`).

## Enforced contracts

Each boundary record carries a `boundary::Policy` (kind and allowed
direction). Clearing derives the required contract from the channel and, for
every behavioral module, rejects:

- inbound ATM cash;
- a cash depository or check-capture point as an outbound payee;
- a check point used for cash or crypto;
- a typed boundary on an unrelated generic channel;
- a generic external counterparty impersonating a typed boundary;
- unknown internal or external keys.

## Behavioral modules

### Cash withdrawals

ATM amounts keep the round-note lattice and affordability screen. Terminals
are spread geographically; at event time the customer's relocation-aware area
resolves to that customer's nearest four, with a stable primary most of the
time. Residents sit at the area centroid, so the area's own points tie on
distance: nearer distance groups are always kept, and the group straddling the
four-point cut is split by a draw-free window keyed by (person, area, rail),
so every point in a city gets used, not the four lowest-numbered. Four or
fewer points per rail reproduce the former list. Cash deposits and check
capture use the same selection, each rail on its own hash domain
(atm-spread-2026-09 amendment in `docs/fraud_model_audit.md`). No customer
account is ever cash infrastructure.

### Cash and check deposits

Business cash takings stay in non-payroll revenue (which sets frequency and
amount) but use the same local, relocation-aware depository lookup. A deposits
routine adds conservative household cash and settled paper-check credits on
isolated random lanes. Cash stays on a nominal bill lattice. A
`check_deposit` row is the posted credit after capture and collection, not the
image-scan instant. Check returns and funds-availability holds are a declared
extension.

Context, not per-customer calibration: the Federal Reserve's 2021 study
counted 5.2 billion consumer checks averaging $1,249, 52% deposited as images.
Federal Reserve Check 21 services distinguish forward collection, image cash
letters, returns and the bank of first deposit. Sources:

- [Federal Reserve Payments Study, 2021 detailed data](https://www.federalreserve.gov/paymentsystems/frps-dfips-cy-2021.htm)
- [Federal Reserve Financial Services: Check 21](https://www.frbservices.org/financial-services/check/check21.html)
- [Federal Reserve Financial Services: Remote Deposit Capture](https://www.frbservices.org/resources/financial-services/check/reference-guide/forward-return-types/rdc.html)

### Crypto transfers

A USD ramp model, not a native-token model. `crypto_ramp_out` is fiat leaving
a customer deposit account for a venue; `crypto_ramp_in` is fiat returning.
Accepted ramp-outs build a private per-account acquisition-value inventory, and
ramp-ins are capped by what remains, so no cash-out appears without a prior
outflow. Not modeled: wallet-to-wallet recipients, token quantities, market
gains or losses, fees, transaction hashes.

No activity before 2013. The default modern adopter ceiling is 9%, with much
lower monthly bank-touch probabilities and adoption weighted toward recent
years: conservative against the Federal Reserve's 2025 household survey (10%
used any cryptocurrency, 9% bought or held it, 2% used it for a financial
transaction). Source:
[Federal Reserve, Economic Well-Being of U.S. Households in 2025: Banking and Credit](https://www.federalreserve.gov/publications/2026-economic-well-being-of-us-households-in-2025-banking.htm)

## Graph use

A removed defect used a real customer `Account` as a shared cash hub, which
collapsed customer-only WCC and PageRank. Boundary points are now external
`Counterparty` entities. Exclude Counterparty/boundary vertices from
account-only algorithms. A heterogeneous graph may keep shared terminals,
capture points and venues (co-use of a real service is legitimate context) but
must not treat them as customer accounts or balance edges.

The regression gate checks that boundary endpoints are external, ownerless,
typed, registered, balance-inert, directionally valid, locally selected where
geography applies, and replay without `unbooked` drops, and that each
account's crypto ramp-in total never exceeds its accepted ramp-out value.
