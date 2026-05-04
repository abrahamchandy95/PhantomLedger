#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/merchants.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/relationships/family/links.hpp"
#include "phantomledger/relationships/family/network.hpp"
#include "phantomledger/relationships/family/partition.hpp"
#include "phantomledger/relationships/family/support.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"
#include "phantomledger/transfers/legit/routines/family/allowances.hpp"
#include "phantomledger/transfers/legit/routines/family/grandparent_gifts.hpp"
#include "phantomledger/transfers/legit/routines/family/inheritance.hpp"
#include "phantomledger/transfers/legit/routines/family/parent_gifts.hpp"
#include "phantomledger/transfers/legit/routines/family/runtime.hpp"
#include "phantomledger/transfers/legit/routines/family/siblings.hpp"
#include "phantomledger/transfers/legit/routines/family/spouse.hpp"
#include "phantomledger/transfers/legit/routines/family/support.hpp"
#include "phantomledger/transfers/legit/routines/family/tuition.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::transfers::legit::routines::relatives {

namespace family_rt = ::PhantomLedger::transfers::legit::routines::family;
namespace family_rel = ::PhantomLedger::relationships::family;

struct FamilyRunRequest {
  const entity::account::Registry *accounts = nullptr;
  const entity::account::Ownership *ownership = nullptr;
  const entity::merchant::Catalog *merchants = nullptr;
};

struct FamilyTransferModel {
  family_rt::allowances::AllowanceSchedule allowances{};
  family_rt::support::SupportFlow support{};
  family_rt::tuition::TuitionSchedule tuition{};
  family_rt::parent_gifts::ParentGiftFlow parentGifts{};
  family_rt::siblings::SiblingFlow siblingTransfers{};
  family_rt::spouse::CoupleFlow spouses{};
  family_rt::grandparent_gifts::GrandparentGiftFlow grandparentGifts{};
  family_rt::inheritance::InheritanceEvent inheritance{};
  family_rt::CounterpartyRouting routing{};

  void validate(primitives::validate::Report &r) const {
    allowances.validate(r);
    support.validate(r);
    tuition.validate(r);
    parentGifts.validate(r);
    siblingTransfers.validate(r);
    spouses.validate(r);
    grandparentGifts.validate(r);
    inheritance.validate(r);
    routing.validate(r);
  }
};

inline constexpr FamilyTransferModel kDefaultFamilyTransferModel{};

[[nodiscard]] bool canRun(const FamilyRunRequest &request) noexcept;

[[nodiscard]] std::span<const ::PhantomLedger::personas::Type>
personasView(const blueprints::LegitBuildPlan &plan) noexcept;

[[nodiscard]] std::uint32_t
personCount(const blueprints::LegitBuildPlan &plan) noexcept;

[[nodiscard]] ::PhantomLedger::time::Window
windowFromPlan(const blueprints::LegitBuildPlan &plan) noexcept;

[[nodiscard]] family_rel::Graph
buildFamilyGraph(const blueprints::LegitBuildPlan &plan,
                 const family_rel::Households &households,
                 const family_rel::Dependents &dependents,
                 const family_rel::RetireeSupport &retireeSupport);

[[nodiscard]] std::vector<double>
amountMultipliers(const blueprints::LegitBuildPlan &plan);

[[nodiscard]] std::vector<transactions::Transaction>
generateFamilyTxns(const family_rt::Runtime &runtime,
                   const FamilyTransferModel &transferModel);

} // namespace PhantomLedger::transfers::legit::routines::relatives
