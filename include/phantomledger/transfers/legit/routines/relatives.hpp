#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/merchants.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/family/graph_config.hpp"
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

#include <vector>

namespace PhantomLedger::transfers::legit::routines::relatives {

namespace family_rt = ::PhantomLedger::transfers::legit::routines::family;

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

[[nodiscard]] std::vector<transactions::Transaction> generateFamilyTxns(
    const FamilyRunRequest &request,
    const ::PhantomLedger::transfers::family::GraphConfig &graphModel,
    const FamilyTransferModel &transferModel,
    const blueprints::LegitBuildPlan &plan, const transactions::Factory &txf);

} // namespace PhantomLedger::transfers::legit::routines::relatives
