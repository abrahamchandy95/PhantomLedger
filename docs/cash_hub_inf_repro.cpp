// Minimal reproducer for `cash-hub-defect-2026-08` (docs/cash_hub_defect.md).
//
// Reproduces the exact production expression that puts the literal text `inf`
// into the exported account balance column:
//
//   src/exporter/aml/vertices.cpp:234
//   src/exporter/aml_txn_edges/vertices.cpp:179
//       row.balance = roundMoney(finalBook->liquidity(rec.id));
//
//   src/exporter/csv.cpp:140  Writer::cell(double) -> writeDoubleTo
//
// `Ledger::liquidity` returns +infinity for a hub-flagged slot
// (src/transactions/clearing/ledger.cpp:129), `roundMoney` is
// std::round(x * 100) / 100 which passes infinity through unchanged, and
// std::to_chars renders infinity as "inf" while RETURNING std::errc{}, so
// csv.cpp's throw never fires. The trailing-zero fixup then probes the
// rendered text for ".eEnN" and matches the 'n' in "inf", so no ".0" is
// appended either.
//
// EXPECTED OUTPUT
//   ordinary account  liquidity=0
//   HUB account       liquidity=inf
//
//   --- exact bytes the exporter would write ---
//   A0000000011,0.0
//   A0000000012,inf
//
// BUILD (from the repo root, after a normal cmake build so build/*.a exist).
// The include and -isystem flags are lifted from build/compile_commands.json:
//
//   c++ -std=c++23 -DNDEBUG -Iinclude \
//       -isystem "$(brew --prefix libpq)/include" \
//       docs/cash_hub_inf_repro.cpp -o /tmp/cash_hub_inf_repro \
//       build/libpl_export.a build/libpl_ledger.a \
//       build/libpl_primitives.a build/libpl_world.a
//   /tmp/cash_hub_inf_repro
//
// This file is documentation, NOT part of the test suite. The real gate this
// defect needs is described in docs/cash_hub_defect.md, section "The gates that
// must accompany the fix": a finiteness-and-agreement check with a precondition
// asserting the hub bit actually survived the book hand-off.

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"

#include <iostream>
#include <sstream>

using namespace PhantomLedger;

int main() {
  const auto ordinary =
      entity::makeKey(entity::Role::account, entity::Bank::internal, 11);
  const auto hub =
      entity::makeKey(entity::Role::account, entity::Bank::internal, 12);

  clearing::Ledger ledger;
  ledger.initialize(2);
  ledger.addAccount(ordinary, 0);
  ledger.addAccount(hub, 1);

  // What the hub wiring does: src/transfers/legit/ledger/limits.cpp:122-124
  // calls createHub for every key in plans.cpp's selectHubAccounts result,
  // and those keys are roster PERSONS' primary deposit accounts.
  ledger.createHub(1);

  const double ordBal =
      primitives::utils::roundMoney(ledger.liquidity(ordinary));
  const double hubBal = primitives::utils::roundMoney(ledger.liquidity(hub));

  std::ostringstream oss;
  {
    exporter::csv::Writer w{oss};
    w.cell(std::string_view{"A0000000011"}).cell(ordBal).endRow();
    w.cell(std::string_view{"A0000000012"}).cell(hubBal).endRow();
  }

  std::cout << "ordinary account  liquidity=" << ordBal << "\n";
  std::cout << "HUB account       liquidity=" << hubBal << "\n\n";
  std::cout << "--- exact bytes the exporter would write ---\n" << oss.str();
  return 0;
}
