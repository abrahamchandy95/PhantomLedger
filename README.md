# PhantomLedger

PhantomLedger generates a synthetic retail-bank ledger (customers, accounts, everyday payments and realistic fraud) and writes it to PostgreSQL. Legitimate accounts show milder forms of every fraud behavior, so mules differ in degree, not in kind.

## Build

Needs CMake 3.23 or later, a C++23 compiler (GCC 13+, Clang 17+ or MSVC 19.38+), Git (CMake fetches [`faker-cxx`](https://github.com/cieslarmichal/faker-cxx) v4.3.2), the PostgreSQL client library (`libpq` and headers), and a reachable, writable PostgreSQL database for runs.

```sh
make build        # Release build into build/
```

## Run

```sh
PL_PG='dbname=phantomledger' ./build/phantomledger [options]
```

| Option | Default | Meaning |
|---|---|---|
| `--usecase NAME` | `standard` | What to produce (one of the use cases below). |
| `--start YYYY-MM-DD` | `2025-01-01` | First simulated day. |
| `--days N` | `365` | Number of simulated days. |
| `--population N` | `70000` | Number of simulated people. |
| `--seed N` | `0xDEADBEEF` | Random seed. |
| `--help`, `-h` | | Print usage. |

- `PL_PG` is the PostgreSQL connection string, such as `'host=... port=... dbname=...'` (default `dbname=phantomledger`). With no server answering, the run stops before generating anything.
- Output goes only to that database, never to files: the raw ledger to the `transactions` table, and each use case's tables to its own PostgreSQL schema (`public` for `standard`, otherwise the use-case name with underscores, such as `card_fraud`). The same seed and options rewrite identical content.
- `make run ARGS="..."` builds, then runs the binary with those options; `make run-help` prints `--help`.

## Use cases

### `standard`

The general bank view: customers, accounts, phones, emails, devices, IPs, merchants and aggregated payment flows. [Details](docs/exports.md#standard).

```sh
PL_PG='dbname=phantomledger' ./build/phantomledger --usecase standard --start 2024-01-01 --days 365 --population 70000 --seed 42
```

### `mule-ml`

Account-level mule detection: one row per account with its mule label and identity, the transfers, and account-to-device and account-to-IP usage. [Details](docs/exports.md#mule-ml).

```sh
PL_PG='dbname=phantomledger' ./build/phantomledger --usecase mule-ml --start 2024-01-01 --days 365 --population 70000 --seed 42
```

### `aml`

Anti-money-laundering: customers, accounts, counterparties, watchlist matches, suspicious-activity reports and name and address matching keys. [Details](docs/exports.md#aml).

```sh
PL_PG='dbname=phantomledger' ./build/phantomledger --usecase aml --start 2024-01-01 --days 365 --population 70000 --seed 42
```

### `aml-txn-edges`

The AML data with one record per transaction instead of aggregated flows, plus derived account features (PageRank, communities, distance to a mule, degrees). [Details](docs/exports.md#aml-txn-edges).

```sh
PL_PG='dbname=phantomledger' ./build/phantomledger --usecase aml-txn-edges --start 2024-01-01 --days 180 --population 20000 --seed 42
```

### `card-fraud`

Card payments for scoring each transaction at its own time: timestamped payments with their device and IP sessions, cards, merchants and places. The whole window must lie in 1990 to 2024. [Details](docs/exports.md#card-fraud), [feature contract](docs/card_fraud_feature_contract.md).

```sh
PL_PG='dbname=phantomledger' ./build/phantomledger --usecase card-fraud --start 1999-01-01 --days 1070 --population 50000 --seed 42
```

### `mule-temporal`

Time-ordered mule detection: payments, Zelle transfers and changes in who owns or uses which account, device, IP and address, in one sequence with pseudonymous IDs. [Details](docs/mule_temporal.md).

```sh
PL_PG='dbname=phantomledger' ./build/phantomledger --usecase mule-temporal --start 2024-01-01 --days 366 --population 200000 --seed 42
```

## Tests

```sh
make test                                                    # build, then run every test
ctest --test-dir build -R test_postgres --output-on-failure  # one test
PL_TEST_PG='dbname=phantomledger_test' make test             # PostgreSQL tests on a scratch database
```

Tests that need PostgreSQL skip when no server answers. They use `PL_TEST_PG` (default `dbname=phantomledger`) and overwrite its tables, so point it at a scratch database.

## Documentation

- [docs/simulation.md](docs/simulation.md): how the world, personas, banking, payments and fraud are modeled, with sources.
- [docs/exports.md](docs/exports.md): what each use case writes.
- [docs/mule_temporal.md](docs/mule_temporal.md), [docs/card_fraud_feature_contract.md](docs/card_fraud_feature_contract.md), [docs/card_fraud_online_gnn.md](docs/card_fraud_online_gnn.md): use-case contracts.
- [docs/debugging.md](docs/debugging.md): every make target and variable, diagnostics, golden baselines.
- [docs/fraud_model_audit.md](docs/fraud_model_audit.md): the authority for every model number.
- [docs/era_data_provenance.md](docs/era_data_provenance.md): the embedded 1990 to 2024 economic data.
