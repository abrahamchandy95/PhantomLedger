# Customer-ledger boundary flows

Status: implemented for cash withdrawal, cash deposit, settled check deposit,
and bank-visible crypto fiat ramps.

## The accounting boundary

PhantomLedger is a customer-account projection, not a bank general ledger and
not a blockchain asset ledger. A real bank balances these events against vault
cash, check collection, network settlement, correspondent, or custody books.
Those books are outside this projection. The model therefore posts only the
customer leg:

| Event | Recorded context | Customer-book mutation |
|---|---|---|
| `atm_withdrawal` | external ATM terminal/acceptor | debit customer only |
| `cash_deposit` | external cash depository | credit customer only |
| `check_deposit` | external check-capture/collection point | credit customer only |
| `crypto_ramp_out` | external crypto service venue | debit customer only |
| `crypto_ramp_in` | the same venue class | credit customer only |

“Only the customer leg” does not mean an infinite source or sink. Boundary
keys are registered for referential integrity, but they are ownerless,
`Bank::external`, absent from the internal ledger index, never seeded, and
never balance-mutated. External-to-external events reject.

The current transaction schema requires both a source and a target. Its
external key is therefore an observed service/capture context, not the
beneficiary account and not a proxy balance. A future nullable-counterparty
schema may omit that context when the source data did not observe it; using an
invalid key today would break validation, sorting, CSV/PostgreSQL readback, and
the graph exporters.

## The bank's income ledgers

Fee and interest postings are the one place the projection books a bank-side
leg. The contra of a charge or of debit interest is fully determined (the
bank's income GL for that posting kind), so the book carries four internal,
ownerless `Role::ledger` accounts (`entities/holdings/general_ledger.hpp`):
card interest income, card fee income, deposit fee income and credit-line
interest income. They are never seeded and never debited, so each one's
balance is the income posted to it. They are not boundary contexts: they are
bank-owned accounts, typed as such by every exporter (see the bank-gl-2026-09
amendment in `docs/fraud_model_audit.md`).

## Enforced contracts

Each boundary record carries a `boundary::Policy` with a kind and allowed flow
direction. Clearing derives the required contract from the channel and rejects
all of the following:

- ATM cash moving inbound;
- a cash depository or check-capture point used as an outbound payee;
- a check point used for cash or crypto;
- a typed boundary used with an unrelated generic channel;
- a generic external counterparty attempting to impersonate any typed
  boundary; and
- unknown internal or external keys.

This validation is shared by all behavioral modules rather than reimplemented
inside each generator.

## Behavioral modules

### Cash withdrawals

ATM behavior retains its round-note amount lattice and affordability screen.
Terminals are geographically distributed. At event time the customer's
relocation-aware area resolves to that customer's own nearest four points; a
stable primary is used most of the time. Every resident of an area sits at its
centroid, so all of the area's own points tie on distance. Points in strictly
nearer distance groups are always kept, and the group that straddles the
four-point cut is split by a draw-free window keyed by (person, area, rail),
so every point in a city is used instead of the four lowest-numbered ones.
A world with four or fewer points per rail selects exactly the former list.
The same selection serves cash deposits and check capture, each rail on its
own hash domain (amendment atm-spread-2026-09 in `docs/fraud_model_audit.md`).
No customer account is ever selected as cash infrastructure.

### Cash and check deposits

Business cash takings remain part of non-payroll revenue because that module
decides their frequency and amount; their endpoint now uses the same local,
relocation-aware depository lookup. A dedicated deposits routine adds
conservative household cash and settled paper-check credits on isolated random
lanes. Cash remains on a nominal bill lattice. A `check_deposit` row represents
the posted credit after capture/collection, not the image-scan instant.

The Federal Reserve's 2021 study reported 5.2 billion consumer checks with an
average value of $1,249, and 52% of checks were deposited as images. Those are
system-level context, not a direct per-customer frequency calibration. Federal
Reserve Check 21 services explicitly distinguish forward collection, image
cash letters, returns, and the bank of first deposit. Check-return events and
funds-availability holds remain a declared extension.

Sources:

- [Federal Reserve Payments Study, 2021 detailed data](https://www.federalreserve.gov/paymentsystems/frps-dfips-cy-2021.htm)
- [Federal Reserve Financial Services — Check 21](https://www.frbservices.org/financial-services/check/check21.html)
- [Federal Reserve Financial Services — Remote Deposit Capture](https://www.frbservices.org/resources/financial-services/check/reference-guide/forward-return-types/rdc.html)

### Crypto transfers

This is intentionally a USD ramp model, not a native-token transfer model.
`crypto_ramp_out` records fiat leaving a customer deposit account for a venue;
`crypto_ramp_in` records fiat returning. Accepted ramp-outs build a private
per-account acquisition-value inventory inside the generator, and ramp-ins are
capped by the remaining prior inventory. The module therefore cannot invent an
inbound cash-out without a modeled prior outflow. It does not claim to model
wallet-to-wallet recipients, token quantities, market gains/losses, fees, or
transaction hashes.

There is no activity before 2013. The default modern adopter ceiling is 9%,
with much lower monthly bank-touch probabilities and adoption concentrated
toward recent years. This is conservative against the Federal Reserve's 2025
household results: 10% reported any cryptocurrency use, 9% bought or held it,
and 2% used it for a financial transaction.

Source: [Federal Reserve, Economic Well-Being of U.S. Households in 2025 — Banking and Credit](https://www.federalreserve.gov/publications/2026-economic-well-being-of-us-households-in-2025-banking.htm)

## Graph use

The removed defect used a real customer `Account` as a shared cash hub, which
made customer-only WCC and PageRank collapse. Boundary points are instead
external `Counterparty` entities. Customer-account algorithms must remain
typed: exclude Counterparty/boundary vertices from an account-only projection.
A heterogeneous graph may deliberately retain shared terminals, capture
points, and venues because co-use of a real service is legitimate context, but
it must not reinterpret those vertices as customer accounts or balance edges.

The executable regression gate verifies that generated boundary endpoints are
external, ownerless, typed, registered, balance-inert, directionally valid,
locally selected where physical geography applies, and replay without
`unbooked` drops. It also verifies that total crypto ramp-in per account never
exceeds accepted ramp-out value.

