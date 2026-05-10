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

FamilyTransferScenario &FamilyTransferScenario::households(
    const family_rel::Households &value) noexcept {
  households_ = &value;
  return *this;
}

FamilyTransferScenario &FamilyTransferScenario::dependents(
    const family_rel::Dependents &value) noexcept {
  dependents_ = &value;
  return *this;
}

FamilyTransferScenario &FamilyTransferScenario::retireeSupport(
    const family_rel::RetireeSupport &value) noexcept {
  retireeSupport_ = &value;
  return *this;
}

FamilyTransferScenario &
FamilyTransferScenario::transfers(const FamilyTransferModel &value) noexcept {
  transfers_ = &value;
  return *this;
}

const family_rel::Households &
FamilyTransferScenario::households() const noexcept {
  return households_ != nullptr ? *households_ : family_rel::kDefaultHouseholds;
}

const family_rel::Dependents &
FamilyTransferScenario::dependents() const noexcept {
  return dependents_ != nullptr ? *dependents_ : family_rel::kDefaultDependents;
}

const family_rel::RetireeSupport &
FamilyTransferScenario::retireeSupport() const noexcept {
  return retireeSupport_ != nullptr ? *retireeSupport_
                                    : family_rel::kDefaultRetireeSupport;
}

bool canRun(const FamilyLedgerSources &sources) noexcept {
  return sources.accounts != nullptr && sources.ownership != nullptr;
}

std::span<const ::PhantomLedger::personas::Type>
personasView(const blueprints::LegitBlueprint &plan) noexcept {
  if (plan.personas().pack == nullptr) {
    return {};
  }

  const auto &assignment = plan.personas().pack->assignment;
  return std::span<const ::PhantomLedger::personas::Type>{assignment.byPerson};
}

std::uint32_t personCount(const blueprints::LegitBlueprint &plan) noexcept {
  if (plan.personas().pack == nullptr) {
    return 0;
  }

  return static_cast<std::uint32_t>(
      plan.personas().pack->assignment.byPerson.size());
}

::PhantomLedger::time::Window
windowFromPlan(const blueprints::LegitBlueprint &plan) noexcept {
  return ::PhantomLedger::time::Window{
      .start = plan.startDate(),
      .days = plan.days(),
  };
}

family_relg::Graph
buildFamilyGraph(const blueprints::LegitBlueprint &plan,
                 const family_relg::Households &households,
                 const family_relg::Dependents &dependents,
                 const family_relg::RetireeSupport &retireeSupport) {
  const family_relg::BuildInputs inputs{
      .personas = personasView(plan),
      .personCount = personCount(plan),
      .baseSeed = plan.seed(),
  };

  return family_relg::build(inputs, households, dependents, retireeSupport);
}

std::vector<double> amountMultipliers(const blueprints::LegitBlueprint &plan) {
  std::vector<double> out;

  if (plan.personas().pack == nullptr) {
    return out;
  }

  const auto &table = plan.personas().pack->table.byPerson;
  out.reserve(table.size());

  for (const auto &persona : table) {
    out.push_back(persona.cash.amountMultiplier);
  }

  return out;
}

family_rt::KinshipView
makeKinship(const blueprints::LegitBlueprint &plan,
            const family_relg::Graph &graph,
            std::span<const double> multipliers) noexcept {
  return family_rt::KinshipView{graph, personasView(plan), multipliers};
}

family_rt::FamilyAccountDirectory
makeAccounts(const FamilyLedgerSources &sources,
             family_rt::CounterpartyRouting routing) noexcept {
  return family_rt::FamilyAccountDirectory{*sources.accounts,
                                           *sources.ownership, routing};
}

family_rt::EducationPayees
makeEducation(const FamilyLedgerSources &sources) noexcept {
  if (sources.educationMerchants == nullptr) {
    return family_rt::EducationPayees{};
  }

  return family_rt::EducationPayees{*sources.educationMerchants};
}

family_rt::PostingWindow
makePosting(const blueprints::LegitBlueprint &plan) noexcept {
  return family_rt::PostingWindow{
      windowFromPlan(plan),
      std::span<const ::PhantomLedger::time::TimePoint>{plan.monthStarts()}};
}

family_rt::TransferEmission
makeEmission(const random::RngFactory &rngFactory,
             const transactions::Factory &txf) noexcept {
  return family_rt::TransferEmission{rngFactory, txf};
}

family_rt::TransferRun
makeTransferRun(family_rt::KinshipView kinship,
                family_rt::FamilyAccountDirectory accounts,
                family_rt::PostingWindow posting,
                family_rt::TransferEmission emission) noexcept {
  return family_rt::TransferRun{kinship, accounts, posting, emission};
}

std::vector<transactions::Transaction>
generateFamilyTxns(const blueprints::LegitBlueprint &plan,
                   const FamilyLedgerSources &sources,
                   const family_rt::TransferEmission &emission,
                   const FamilyTransferScenario &scenario) {
  const auto *transferModel = scenario.transfers();
  if (transferModel == nullptr) {
    return {};
  }

  if (!canRun(sources)) {
    return {};
  }

  const auto personas = personasView(plan);
  if (personas.empty() || personCount(plan) == 0) {
    return {};
  }

  const auto graph =
      buildFamilyGraph(plan, scenario.households(), scenario.dependents(),
                       scenario.retireeSupport());
  const auto multipliers = amountMultipliers(plan);

  auto run = makeTransferRun(
      makeKinship(plan, graph, std::span<const double>{multipliers}),
      makeAccounts(sources, transferModel->routing), makePosting(plan),
      emission);
  run.education(makeEducation(sources));

  return generateFamilyTxns(run, *transferModel);
}

std::vector<transactions::Transaction>
generateFamilyTxns(const family_rt::TransferRun &run,
                   const FamilyTransferModel &transferModel) {
  std::vector<transactions::Transaction> out;

  if (!run.kinship().ready() || !run.accounts().ready()) {
    return out;
  }

  if (!run.emission().ready()) {
    throw std::invalid_argument(
        "family transfers require rngFactory and transaction factory");
  }

  appendRoutineOutput(
      family_rt::allowances::generate(run, transferModel.allowances), out);

  appendRoutineOutput(family_rt::support::generate(run, transferModel.support),
                      out);

  appendRoutineOutput(family_rt::tuition::generate(run, transferModel.tuition),
                      out);

  appendRoutineOutput(
      family_rt::parent_gifts::generate(run, transferModel.parentGifts), out);

  appendRoutineOutput(
      family_rt::siblings::generate(run, transferModel.siblingTransfers), out);

  appendRoutineOutput(family_rt::spouse::generate(run, transferModel.spouses),
                      out);

  appendRoutineOutput(family_rt::grandparent_gifts::generate(
                          run, transferModel.grandparentGifts),
                      out);

  appendRoutineOutput(
      family_rt::inheritance::generate(run, transferModel.inheritance), out);

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::relatives
