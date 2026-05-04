#include "phantomledger/transfers/legit/routines/relatives.hpp"

#include "phantomledger/relationships/family/builder.hpp"
#include "phantomledger/relationships/family/network.hpp"

#include <iterator>
#include <stdexcept>
#include <utility>

namespace PhantomLedger::transfers::legit::routines::relatives {

namespace family_relg = ::PhantomLedger::relationships::family;
namespace family_rt = ::PhantomLedger::transfers::legit::routines::family;

namespace {

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

bool canRun(const FamilyRunRequest &request) noexcept {
  return request.accounts != nullptr && request.ownership != nullptr;
}

std::span<const ::PhantomLedger::personas::Type>
personasView(const blueprints::LegitBuildPlan &plan) noexcept {
  if (plan.personas.pack == nullptr) {
    return {};
  }
  const auto &assignment = plan.personas.pack->assignment;
  return std::span<const ::PhantomLedger::personas::Type>{assignment.byPerson};
}

std::uint32_t personCount(const blueprints::LegitBuildPlan &plan) noexcept {
  if (plan.personas.pack == nullptr) {
    return 0;
  }
  return static_cast<std::uint32_t>(
      plan.personas.pack->assignment.byPerson.size());
}

::PhantomLedger::time::Window
windowFromPlan(const blueprints::LegitBuildPlan &plan) noexcept {
  return ::PhantomLedger::time::Window{
      .start = plan.startDate,
      .days = plan.days,
  };
}

family_relg::Graph
buildFamilyGraph(const blueprints::LegitBuildPlan &plan,
                 const family_relg::Households &households,
                 const family_relg::Dependents &dependents,
                 const family_relg::RetireeSupport &retireeSupport) {
  const family_relg::BuildInputs inputs{
      .personas = personasView(plan),
      .personCount = personCount(plan),
      .baseSeed = plan.seed,
  };
  return family_relg::build(inputs, households, dependents, retireeSupport);
}

std::vector<double> amountMultipliers(const blueprints::LegitBuildPlan &plan) {
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

std::vector<transactions::Transaction>
generateFamilyTxns(const family_rt::Runtime &runtime,
                   const FamilyTransferModel &transferModel) {
  std::vector<transactions::Transaction> out;

  // Preserve the old cheap fast-path before any routine touches the graph.
  if (runtime.accounts == nullptr || runtime.ownership == nullptr ||
      runtime.graph == nullptr || runtime.personas.empty()) {
    return out;
  }

  if (runtime.rngFactory == nullptr || runtime.txf == nullptr) {
    throw std::invalid_argument(
        "family transfers require rngFactory and transaction factory");
  }

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
