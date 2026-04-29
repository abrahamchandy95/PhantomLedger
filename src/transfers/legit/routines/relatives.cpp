#include "phantomledger/transfers/legit/routines/relatives.hpp"

#include "phantomledger/relationships/family/builder.hpp"
#include "phantomledger/relationships/family/network.hpp"
#include "phantomledger/transfers/family/graph_config.hpp"
#include "phantomledger/transfers/family/transfer_config.hpp"
#include "phantomledger/transfers/legit/routines/family/allowances.hpp"
#include "phantomledger/transfers/legit/routines/family/grandparent_gifts.hpp"
#include "phantomledger/transfers/legit/routines/family/inheritance.hpp"
#include "phantomledger/transfers/legit/routines/family/parent_gifts.hpp"
#include "phantomledger/transfers/legit/routines/family/runtime.hpp"
#include "phantomledger/transfers/legit/routines/family/siblings.hpp"
#include "phantomledger/transfers/legit/routines/family/spouse.hpp"
#include "phantomledger/transfers/legit/routines/family/support.hpp"
#include "phantomledger/transfers/legit/routines/family/tuition.hpp"

#include <utility>

namespace PhantomLedger::transfers::legit::routines::relatives {

namespace family_relg = ::PhantomLedger::relationships::family;
namespace family_cfg = ::PhantomLedger::transfers::family;
namespace family_rt = ::PhantomLedger::transfers::legit::routines::family;

namespace {

// =============================================================================
// (1) Persona view — borrows from `plan.personas.pack`.
//
// The pack carries `behavior::Assignment::byPerson` which is a
// dense `vector<personas::Type>`. We just expose a span over it.
// Returning a span (not a copy) keeps the call zero-allocation.
// =============================================================================

[[nodiscard]] std::span<const ::PhantomLedger::personas::Type>
personasView(const blueprints::LegitBuildPlan &plan) {
  if (plan.personas.pack == nullptr) {
    return {};
  }
  const auto &assignment = plan.personas.pack->assignment;
  return std::span<const ::PhantomLedger::personas::Type>{assignment.byPerson};
}

// =============================================================================
// (2) Population size — read from the persona table.
//
// `plan.persons` is a curated subset (active persons), but the
// graph indexes on the *entire* PersonId space (1..personCount).
// We size to `assignment.byPerson.size()` to match.
// =============================================================================

[[nodiscard]] std::uint32_t
totalPersonCount(const blueprints::LegitBuildPlan &plan) {
  if (plan.personas.pack == nullptr) {
    return 0;
  }
  return static_cast<std::uint32_t>(
      plan.personas.pack->assignment.byPerson.size());
}

// =============================================================================
// (3) Window — copied from blueprint.
// =============================================================================

[[nodiscard]] ::PhantomLedger::time::Window
windowFromPlan(const blueprints::LegitBuildPlan &plan) {
  return ::PhantomLedger::time::Window{
      .start = plan.startDate,
      .days = plan.days,
  };
}

// =============================================================================
// (4) Graph construction — runs once per legit-build call.
//
// Per-stage RNG derivation lives in `family::build`; we just pass
// the base seed through.
// =============================================================================

[[nodiscard]] family_relg::Graph
buildGraph(const family_cfg::GraphConfig &graphCfg,
           std::span<const ::PhantomLedger::personas::Type> personas,
           std::uint32_t personCount, std::uint64_t baseSeed) {
  const family_relg::BuildInputs inputs{
      .personas = personas,
      .personCount = personCount,
      .baseSeed = baseSeed,
  };
  return family_relg::build(graphCfg, inputs);
}

// =============================================================================
// (5) Per-person amount-multiplier extraction.
//
// Persona objects live in `behavior::Table::byPerson` as full
// structs. The routines only care about `cash.amountMultiplier`,
// so we pre-extract a flat `vector<double>` once (lifetime tied
// to `generateFamilyTxns`) and span over it. This costs one
// allocation of personCount doubles, but every routine that
// reads the multiplier saves a struct-field deref per call.
// =============================================================================

[[nodiscard]] std::vector<double>
extractAmountMultipliers(const blueprints::LegitBuildPlan &plan) {
  std::vector<double> out;
  if (plan.personas.pack == nullptr) {
    return out;
  }
  const auto &table = plan.personas.pack->table.byPerson;
  out.reserve(table.size());
  for (const auto &persona : table) {
    out.push_back(persona.cash.amountMultiplier);
  }
  return out;
}

// =============================================================================
// (6) Runtime assembly.
//
// Pure-data wiring. Every field comes from blueprint/plan/graph
// already in scope — this function does no work, just plumbing.
// Splitting it out keeps `generateFamilyTxns` free of struct-init
// noise.
// =============================================================================

[[nodiscard]] family_rt::Runtime assembleRuntime(
    const blueprints::Blueprint &request,
    const blueprints::LegitBuildPlan &plan, const transactions::Factory &txf,
    const family_cfg::TransferConfig &transferCfg,
    const family_relg::Graph &graph, const random::RngFactory &rngFactory,
    std::span<const ::PhantomLedger::personas::Type> personas,
    std::span<const double> amountMultipliers) {
  return family_rt::Runtime{
      .graph = &graph,
      .personas = personas,
      .amountMultipliers = amountMultipliers,
      .accounts = request.network.accounts,
      .ownership = request.network.ownership,
      .merchants = request.network.merchants,
      .window = windowFromPlan(plan),
      .monthStarts =
          std::span<const ::PhantomLedger::time::TimePoint>{plan.monthStarts},
      .rngFactory = &rngFactory,
      .txf = &txf,
      .routing = transferCfg.routing,
  };
}

// =============================================================================
// (7) Dispatch helper — append one routine's output to the
// accumulator with one move.
//
// Each routine returns a `vector<Transaction>`; we concatenate
// them into `out`. Using `insert(..., make_move_iterator(...))`
// avoids per-element copies.
// =============================================================================

void appendRoutineOutput(std::vector<transactions::Transaction> &&from,
                         std::vector<transactions::Transaction> &out) {
  if (from.empty()) {
    return;
  }
  if (out.empty()) {
    out = std::move(from);
    return;
  }
  out.reserve(out.size() + from.size());
  out.insert(out.end(), std::make_move_iterator(from.begin()),
             std::make_move_iterator(from.end()));
}

} // namespace

// =============================================================================
// Public entry point.
//
// Six steps, each a named helper above:
//   (1) Persona view.
//   (2) Population size.
//   (3) Window.
//   (4) Graph construction.
//   (5) Multiplier extraction.
//   (6) Runtime assembly.
//   (7) Dispatch + concatenate.
//
// To add a new routine: include its header above, emit one
// `appendRoutineOutput(...)` line in the dispatch block. No
// other change.
// =============================================================================

std::vector<transactions::Transaction>
generateFamilyTxns(const blueprints::Blueprint &request,
                   const family_cfg::GraphConfig &graphCfg,
                   const family_cfg::TransferConfig &transferCfg,
                   const blueprints::LegitBuildPlan &plan,
                   const transactions::Factory &txf) {
  std::vector<transactions::Transaction> out;

  // Cheap fast-paths before we touch the graph.
  if (request.network.accounts == nullptr ||
      request.network.ownership == nullptr) {
    return out;
  }

  const auto personas = personasView(plan);
  const auto personCount = totalPersonCount(plan);
  if (personCount == 0 || personas.empty()) {
    return out;
  }

  // Build the graph once. All routines below read from it.
  const auto graph = buildGraph(graphCfg, personas, personCount, plan.seed);

  // Pre-extract per-person amount multipliers so routines can
  // span over them without dereferencing the full Persona table.
  const auto amountMultipliers = extractAmountMultipliers(plan);

  // Per-routine entropy comes from this factory.
  const random::RngFactory rngFactory{plan.seed};

  const auto runtime =
      assembleRuntime(request, plan, txf, transferCfg, graph, rngFactory,
                      personas, std::span<const double>{amountMultipliers});

  // ---- Dispatch ----------------------------------------------------------
  // All 8 family transfer routines, ordered by frequency:
  //   - allowances     — recurring, weekly/biweekly student stream
  //   - support        — monthly retiree income augmentation
  //   - tuition        — installment bursts per student
  //   - parent_gifts   — monthly downward transfers to adult children
  //   - siblings       — irregular intra-household pair flows
  //   - spouse         — high-volume intra-couple monthly bursts
  //   - grandparent    — monthly retiree → grandchild gifts
  //   - inheritance    — rare lump-sum, retiree → heirs
  // Order is informational only — each routine is independent and
  // uses its own derived RNG.
  // ------------------------------------------------------------------------

  appendRoutineOutput(
      family_rt::allowances::generate(runtime, transferCfg.allowances), out);
  appendRoutineOutput(
      family_rt::support::generate(runtime, transferCfg.retireeSupport), out);
  appendRoutineOutput(
      family_rt::tuition::generate(runtime, transferCfg.tuition), out);
  appendRoutineOutput(
      family_rt::parent_gifts::generate(runtime, transferCfg.parentGifts), out);
  appendRoutineOutput(
      family_rt::siblings::generate(runtime, transferCfg.siblingTransfers),
      out);
  appendRoutineOutput(family_rt::spouse::generate(runtime, transferCfg.spouses),
                      out);
  appendRoutineOutput(family_rt::grandparent_gifts::generate(
                          runtime, transferCfg.grandparentGifts),
                      out);
  appendRoutineOutput(
      family_rt::inheritance::generate(runtime, transferCfg.inheritance), out);

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::relatives
