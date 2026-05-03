#include "phantomledger/transfers/legit/routines/relatives.hpp"

#include "phantomledger/relationships/family/builder.hpp"
#include "phantomledger/relationships/family/network.hpp"

#include <utility>

namespace PhantomLedger::transfers::legit::routines::relatives {

namespace family_relg = ::PhantomLedger::relationships::family;
namespace family_rt = ::PhantomLedger::transfers::legit::routines::family;

namespace {

[[nodiscard]] std::span<const ::PhantomLedger::personas::Type>
personasView(const blueprints::LegitBuildPlan &plan) {
  if (plan.personas.pack == nullptr) {
    return {};
  }
  const auto &assignment = plan.personas.pack->assignment;
  return std::span<const ::PhantomLedger::personas::Type>{assignment.byPerson};
}

[[nodiscard]] std::uint32_t
totalPersonCount(const blueprints::LegitBuildPlan &plan) {
  if (plan.personas.pack == nullptr) {
    return 0;
  }
  return static_cast<std::uint32_t>(
      plan.personas.pack->assignment.byPerson.size());
}

[[nodiscard]] ::PhantomLedger::time::Window
windowFromPlan(const blueprints::LegitBuildPlan &plan) {
  return ::PhantomLedger::time::Window{
      .start = plan.startDate,
      .days = plan.days,
  };
}

[[nodiscard]] family_relg::Graph
buildGraph(const family_relg::Households &households,
           const family_relg::Dependents &dependents,
           const family_relg::RetireeSupport &retireeSupport,
           std::span<const ::PhantomLedger::personas::Type> personas,
           std::uint32_t personCount, std::uint64_t baseSeed) {
  const family_relg::BuildInputs inputs{
      .personas = personas,
      .personCount = personCount,
      .baseSeed = baseSeed,
  };
  return family_relg::build(inputs, households, dependents, retireeSupport);
}

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

[[nodiscard]] family_rt::Runtime assembleRuntime(
    const FamilyRunRequest &request, const blueprints::LegitBuildPlan &plan,
    const transactions::Factory &txf, family_rt::CounterpartyRouting routing,
    const family_relg::Graph &graph, const random::RngFactory &rngFactory,
    std::span<const ::PhantomLedger::personas::Type> personas,
    std::span<const double> amountMultipliers) {
  return family_rt::Runtime{
      .graph = &graph,
      .personas = personas,
      .amountMultipliers = amountMultipliers,
      .accounts = request.accounts,
      .ownership = request.ownership,
      .merchants = request.merchants,
      .window = windowFromPlan(plan),
      .monthStarts =
          std::span<const ::PhantomLedger::time::TimePoint>{plan.monthStarts},
      .rngFactory = &rngFactory,
      .txf = &txf,
      .routing = routing,
  };
}

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

std::vector<transactions::Transaction> generateFamilyTxns(
    const FamilyRunRequest &request, const family_relg::Households &households,
    const family_relg::Dependents &dependents,
    const family_relg::RetireeSupport &retireeSupport,
    const FamilyTransferModel &transferModel,
    const blueprints::LegitBuildPlan &plan, const transactions::Factory &txf) {
  std::vector<transactions::Transaction> out;

  // Cheap fast-paths before we touch the graph.
  if (request.accounts == nullptr || request.ownership == nullptr) {
    return out;
  }

  const auto personas = personasView(plan);
  const auto personCount = totalPersonCount(plan);
  if (personCount == 0 || personas.empty()) {
    return out;
  }

  // Build the graph once. All routines below read from it.
  const auto graph = buildGraph(households, dependents, retireeSupport,
                                personas, personCount, plan.seed);

  // Pre-extract per-person amount multipliers so routines can
  // span over them without dereferencing the full Persona table.
  const auto amountMultipliers = extractAmountMultipliers(plan);

  // Per-routine entropy comes from this factory.
  const random::RngFactory rngFactory{plan.seed};

  const auto runtime = assembleRuntime(
      request, plan, txf, transferModel.routing, graph, rngFactory, personas,
      std::span<const double>{amountMultipliers});

  // ---- Dispatch ----------------------------------------------------------
  appendRoutineOutput(
      family_rt::allowances::generate(runtime, transferModel.allowances), out);

  appendRoutineOutput(
      family_rt::support::generate(runtime, transferModel.support), out);

  appendRoutineOutput(
      family_rt::tuition::generate(runtime, transferModel.tuition), out);

  appendRoutineOutput(
      family_rt::parent_gifts::generate(runtime, transferModel.parentGifts),
      out);

  appendRoutineOutput(
      family_rt::siblings::generate(runtime, transferModel.siblingTransfers),
      out);

  appendRoutineOutput(
      family_rt::spouse::generate(runtime, transferModel.spouses), out);

  appendRoutineOutput(family_rt::grandparent_gifts::generate(
                          runtime, transferModel.grandparentGifts),
                      out);

  appendRoutineOutput(
      family_rt::inheritance::generate(runtime, transferModel.inheritance),
      out);

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::relatives
