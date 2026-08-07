#pragma once

#include "phantomledger/entities/holdings/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"

#include <cstdint>
#include <limits>
#include <span>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::transfers::legit::routines::paychecks {

/*
  THIS PASS IS WHAT KEEPS A PERSON'S NON-PRIMARY DEPOSIT ACCOUNTS SOLVENT, and
  until `device-fanout-2026-08` it did not have to.

  Every credit a person receives — salary, benefits, revenue, insurance, tax
  refunds, family transfers, chargebacks — lands on the PRIMARY account. That
  was harmless while the primary was also the only account anything was ever
  spent from. The instrument round broke the symmetry: `depositRoute` now
  spreads merchant-channel spend across EVERY deposit account the person owns,
  chosen by a draw-free Zipf hash, while income still arrives in one place.

  MEASURED CONSEQUENCE, four legs, before this pass was widened: secondary
  deposit accounts ran net-NEGATIVE in 83.2% / 83.3% / 84.8% / 84.7% of cases,
  mean net -1,390 / -3,615 / -2,753 / -3,232 against primaries at +28,239 /
  +9,911 / +23,673 / +13,327; and 31.8% / 40.3% / 35.2% / 37.5% of secondaries
  NEVER RECEIVED A SINGLE CREDIT for the whole window. A drained secondary is
  not cosmetic: a transfer that would take it below its protection buffer is
  REJECTED outright, so the rows routed to it are lost, and the liquidity
  multiplier averages cash across all of a person's deposit slots, so one dead
  secondary suppresses that person's ENTIRE spend rate.

  THE SIZE WAS NEVER THE PROBLEM — THE COVERAGE WAS. The old fraction band
  (0.10-0.35) already straddles break-even: the merchant channel is the only
  outflow that spreads, and it is 0.2442 / 0.2592 / 0.2343 / 0.2366 of a
  deposit account's total outflow, so a secondary carrying share `s` of the
  spread needs only `s * ~0.244` of income to balance. What starved the model
  was a 30% inclusion coin: seven in ten multi-account people had a spread they
  never funded.
 */

/// One person's deposit-splitting plan.
struct Split {
  /* EVERY non-primary deposit account, not one. The spend law spreads across
   * all of them, so funding one and starving the rest reproduces the defect
   * at a smaller scale. Emission rotates over this pool draw-free, so a
   * person with three accounts funds all three across their paydays without
   * emitting more rows than a person with two. */
  std::vector<entity::Key> secondaries;

  /* The share of each paycheck forwarded, DERIVED rather than drawn — see
   * `planSplitters`. */
  double fraction = 0.0;

  /* THE OWNER'S DEATH INSTANT, EXCLUSIVE, and it is carried because the
   * posting lag can cross it.
   *
   * A split posts 5-30 minutes AFTER the credit that triggers it. Every
   * income emitter already stops at death, so the CREDIT is always inside the
   * person's life — but the lagged self-transfer need not be, and
   * `test_membership` asserts flatly that the dead move no money between
   * their own accounts. The defect predates this round; it simply could not
   * fire while the pass reached 30% of people through the salary channel
   * alone, and widening coverage surfaced it at 3 rows.
   *
   * `int64 max` means "no death in or before this window". */
  std::int64_t eligibleUntilExcl = std::numeric_limits<std::int64_t>::max();
};

using SplitsByPrimary = std::unordered_map<entity::Key, Split>;

/// Phase 1 — planning. TAKES NO RANDOMNESS AT ALL, which is the whole point
/// of the shape.
///
/// It used to draw twice per eligible person off the SHARED routine lane: a
/// 30% inclusion coin and a fraction uniform. `addSplitDeposits` runs FIRST in
/// the routine order, ahead of rent, subscriptions, ATM, internal transfers
/// and the entire spending fold, and its draw count depends on how many people
/// hold a second account — data. That is the exact shape CLAUDE.md
/// `merchant-churn-2026-07` rule 2 forbids, and it is why widening coverage
/// could not simply be done: it would have shifted every rent amount, every
/// subscription pick and every spending decision in the corpus through a
/// mechanism that has nothing to do with paychecks.
///
/// Both draws are now gone rather than merely relocated. Inclusion is
/// universal, and the fraction is DERIVED from the same instrument weight law
/// the spending router uses, so the planner is a pure function of world state
/// — which also means the two halves cannot drift apart when the Zipf exponent
/// or the account-count law moves.
[[nodiscard]] SplitsByPrimary
planSplitters(const blueprints::LegitBlueprint &plan,
              const entity::account::Ownership &ownership,
              const entity::account::Registry &registry);

/// Phase 2 — emission. For every payday-inbound credit whose target is a
/// primary account in `splits`, emit one follow-up self-transfer.
///
/// ALSO TAKES NO RANDOMNESS, and that is not tidiness — it is what keeps the
/// two engines in lockstep. Its row count depends on how many people have a
/// split and how many paydays they have, so a sequential lane would have made
/// the posting lag depend on a row's POSITION in the emission stream; the
/// monolith sees the whole window at once and the windowed engine sees one
/// window at a time, so the same paycheck got different lags and
/// `test_arch_equivalence` red with a semantic divergence. Relocating the
/// stream to its own seed does not help — any stream restarts per window. The
/// lag is a hash of (primary, paycheck timestamp) instead, which is a pure
/// function of world state and therefore prefix-identical by construction.
[[nodiscard]] std::vector<transactions::Transaction>
emitSplitTransfers(const transactions::Factory &txf,
                   const SplitsByPrimary &splits,
                   std::span<const transactions::Transaction> existingTxns);

} // namespace PhantomLedger::transfers::legit::routines::paychecks
