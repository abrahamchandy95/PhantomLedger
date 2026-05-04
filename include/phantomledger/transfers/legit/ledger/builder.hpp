#pragma once

#include "phantomledger/transfers/legit/blueprints/plans.hpp"
#include "phantomledger/transfers/legit/ledger/limits.hpp"
#include "phantomledger/transfers/legit/ledger/passes.hpp"
#include "phantomledger/transfers/legit/ledger/result.hpp"

namespace PhantomLedger::infra {
class Router;
} // namespace PhantomLedger::infra

namespace PhantomLedger::relationships::family {
struct Households;
struct Dependents;
struct RetireeSupport;
} // namespace PhantomLedger::relationships::family

namespace PhantomLedger::transfers::legit::ledger {

class LegitTransferBuilder {
public:
  struct FamilyPrograms {
    const ::PhantomLedger::relationships::family::Households *households =
        nullptr;
    const ::PhantomLedger::relationships::family::Dependents *dependents =
        nullptr;
    const ::PhantomLedger::relationships::family::RetireeSupport
        *retireeSupport = nullptr;
    const ::PhantomLedger::transfers::legit::routines::relatives::
        FamilyTransferModel *transfers = nullptr;
  };

  LegitTransferBuilder() = default;
  LegitTransferBuilder(blueprints::PlanRequest blueprint,
                       BalanceBookRequest openingBook) noexcept;

  LegitTransferBuilder &blueprint(blueprints::PlanRequest value) noexcept;
  LegitTransferBuilder &openingBook(BalanceBookRequest value) noexcept;
  LegitTransferBuilder &income(passes::IncomePassRequest value) noexcept;
  LegitTransferBuilder &routines(passes::RoutinePassRequest value) noexcept;
  LegitTransferBuilder &family(passes::FamilyPassRequest value) noexcept;
  LegitTransferBuilder &credit(passes::CreditPassRequest value) noexcept;
  LegitTransferBuilder &familyPrograms(FamilyPrograms value) noexcept;
  LegitTransferBuilder &
  router(const ::PhantomLedger::infra::Router &value) noexcept;
  LegitTransferBuilder &
  router(const ::PhantomLedger::infra::Router *value) noexcept;

  [[nodiscard]] TransfersPayload build() const;

private:
  [[nodiscard]] const entity::account::Registry *accounts() const noexcept;

  blueprints::PlanRequest blueprint_{};
  BalanceBookRequest openingBook_{};

  passes::IncomePassRequest income_{};
  passes::RoutinePassRequest routines_{};
  passes::FamilyPassRequest family_{};
  passes::CreditPassRequest credit_{};

  FamilyPrograms familyPrograms_{};
  const ::PhantomLedger::infra::Router *router_ = nullptr;
  bool hasBlueprint_ = false;
};

} // namespace PhantomLedger::transfers::legit::ledger
