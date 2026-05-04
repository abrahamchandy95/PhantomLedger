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

bool canRun(const FamilyLedgerSources &sources) noexcept {
  return sources.accounts != nullptr && sources.ownership != nullptr;
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

family_rt::TransferRun makeTransferRun(
    const blueprints::LegitBuildPlan &plan, const family_relg::Graph &graph,
    std::span<const double> multipliers, const FamilyLedgerSources &sources,
    const random::RngFactory &rngFactory, const transactions::Factory &txf,
    family_rt::CounterpartyRouting routing) noexcept {
  family_rt::EducationPayees education{};

  if (sources.educationMerchants != nullptr) {
    education = family_rt::EducationPayees{*sources.educationMerchants};
  }

  return family_rt::TransferRun{
      family_rt::KinshipView{graph, personasView(plan), multipliers},
      family_rt::FamilyAccountDirectory{*sources.accounts, *sources.ownership,
                                        routing},
      education,
      family_rt::PostingWindow{
          windowFromPlan(plan),
          std::span<const ::PhantomLedger::time::TimePoint>{plan.monthStarts}},
      family_rt::TransferEmission{rngFactory, txf},
  };
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
