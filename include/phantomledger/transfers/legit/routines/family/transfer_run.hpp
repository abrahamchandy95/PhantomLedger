#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/merchants.hpp"
#include "phantomledger/entities/synth/family/pick.hpp"
#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/relationships/family/network.hpp"
#include "phantomledger/taxonomies/merchants/types.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace PhantomLedger::transfers::legit::routines::family {

struct CounterpartyRouting {
  double externalP = 0.18;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::between("externalP", externalP, 0.0, 1.0); });
  }
};

inline constexpr CounterpartyRouting kDefaultCounterpartyRouting{};

class KinshipView {
public:
  KinshipView() = default;

  KinshipView(const ::PhantomLedger::relationships::family::Graph &graph,
              std::span<const ::PhantomLedger::personas::Type> personas,
              std::span<const double> amountMultipliers) noexcept
      : graph_(&graph), personas_(personas),
        amountMultipliers_(amountMultipliers) {}

  [[nodiscard]] bool ready() const noexcept {
    return graph_ != nullptr && !personas_.empty();
  }

  [[nodiscard]] std::uint32_t personCount() const noexcept {
    return graph_ != nullptr ? graph_->personCount() : 0U;
  }

  [[nodiscard]] std::uint32_t householdCount() const noexcept {
    return graph_ != nullptr ? graph_->householdCount() : 0U;
  }

  [[nodiscard]] std::span<const entity::PersonId>
  householdMembersOf(std::uint32_t householdIdx) const noexcept {
    return graph_->householdMembersOf(householdIdx);
  }

  [[nodiscard]] entity::PersonId
  spouseOf(entity::PersonId person) const noexcept {
    return graph_->spouseFor(person);
  }

  [[nodiscard]] std::array<entity::PersonId, 2>
  parentsOf(entity::PersonId person) const noexcept {
    return graph_->parentsFor(person);
  }

  [[nodiscard]] std::span<const entity::PersonId>
  childrenOf(entity::PersonId person) const noexcept {
    if (!entity::valid(person) || person > graph_->childrenOf.size()) {
      return {};
    }

    const auto &children = graph_->childrenOf[person - 1];
    return std::span<const entity::PersonId>{children};
  }

  [[nodiscard]] std::span<const entity::PersonId>
  supportingChildrenOf(entity::PersonId person) const noexcept {
    if (!entity::valid(person) ||
        person > graph_->supportingChildrenOf.size()) {
      return {};
    }

    const auto &children = graph_->supportingChildrenOf[person - 1];
    return std::span<const entity::PersonId>{children};
  }

  [[nodiscard]] ::PhantomLedger::personas::Type
  persona(entity::PersonId person) const noexcept {
    return personas_[person - 1];
  }

  [[nodiscard]] std::span<const ::PhantomLedger::personas::Type>
  personas() const noexcept {
    return personas_;
  }

  [[nodiscard]] double
  amountMultiplier(entity::PersonId person) const noexcept {
    if (amountMultipliers_.empty() || !entity::valid(person) ||
        person > amountMultipliers_.size()) {
      return 1.0;
    }

    return amountMultipliers_[person - 1];
  }

  [[nodiscard]] std::span<const double> amountMultipliers() const noexcept {
    return amountMultipliers_;
  }

private:
  const ::PhantomLedger::relationships::family::Graph *graph_ = nullptr;
  std::span<const ::PhantomLedger::personas::Type> personas_;
  std::span<const double> amountMultipliers_;
};

class FamilyAccountDirectory {
public:
  FamilyAccountDirectory() = default;

  FamilyAccountDirectory(const entity::account::Registry &accounts,
                         const entity::account::Ownership &ownership,
                         CounterpartyRouting routing) noexcept
      : accounts_(&accounts), ownership_(&ownership), routing_(routing) {}

  [[nodiscard]] bool ready() const noexcept {
    return accounts_ != nullptr && ownership_ != nullptr;
  }

  [[nodiscard]] std::optional<entity::Key>
  localMemberAccount(entity::PersonId person) const {
    return memberAccount(person, 0.0);
  }

  [[nodiscard]] std::optional<entity::Key>
  routedMemberAccount(entity::PersonId person) const {
    return memberAccount(person, routing_.externalP);
  }

  [[nodiscard]] std::optional<entity::Key>
  memberAccount(entity::PersonId person, double externalP) const {
    return entities::synth::family::pickMemberId(person, *accounts_,
                                                 *ownership_, externalP);
  }

  [[nodiscard]] CounterpartyRouting routing() const noexcept {
    return routing_;
  }

private:
  const entity::account::Registry *accounts_ = nullptr;
  const entity::account::Ownership *ownership_ = nullptr;
  CounterpartyRouting routing_{};
};

class EducationPayees {
public:
  EducationPayees() = default;

  explicit EducationPayees(const entity::merchant::Catalog &catalog) noexcept
      : catalog_(&catalog) {}

  [[nodiscard]] bool ready() const noexcept { return catalog_ != nullptr; }

  [[nodiscard]] std::optional<entity::Key> pick(random::Rng &rng) const {
    using ::PhantomLedger::merchants::Category;

    std::size_t educationCount = 0;
    for (const auto &record : catalog_->records) {
      if (record.category == Category::education) {
        ++educationCount;
      }
    }

    if (educationCount == 0) {
      return std::nullopt;
    }

    const auto target = static_cast<std::size_t>(
        rng.uniformInt(0, static_cast<std::int64_t>(educationCount)));

    std::size_t seen = 0;
    for (const auto &record : catalog_->records) {
      if (record.category != Category::education) {
        continue;
      }

      if (seen == target) {
        return record.counterpartyId;
      }

      ++seen;
    }

    return std::nullopt;
  }

private:
  const entity::merchant::Catalog *catalog_ = nullptr;
};

class PostingWindow {
public:
  PostingWindow() = default;

  PostingWindow(
      ::PhantomLedger::time::Window window,
      std::span<const ::PhantomLedger::time::TimePoint> monthStarts) noexcept
      : window_(window), monthStarts_(monthStarts) {}

  [[nodiscard]] ::PhantomLedger::time::TimePoint start() const noexcept {
    return window_.start;
  }

  [[nodiscard]] int days() const noexcept { return window_.days; }

  [[nodiscard]] ::PhantomLedger::time::Window window() const noexcept {
    return window_;
  }

  [[nodiscard]] std::span<const ::PhantomLedger::time::TimePoint>
  monthStarts() const noexcept {
    return monthStarts_;
  }

  [[nodiscard]] bool hasMonths() const noexcept {
    return !monthStarts_.empty();
  }

  [[nodiscard]] ::PhantomLedger::time::TimePoint
  firstMonthStart() const noexcept {
    return monthStarts_.front();
  }

  [[nodiscard]] std::int64_t endEpochSec() const {
    return ::PhantomLedger::time::toEpochSeconds(window_.endExcl());
  }

private:
  ::PhantomLedger::time::Window window_{};
  std::span<const ::PhantomLedger::time::TimePoint> monthStarts_;
};

class TransferEmission {
public:
  TransferEmission() = default;

  TransferEmission(const random::RngFactory &rngFactory,
                   const transactions::Factory &txf) noexcept
      : rngFactory_(&rngFactory), txf_(&txf) {}

  [[nodiscard]] bool ready() const noexcept {
    return rngFactory_ != nullptr && txf_ != nullptr;
  }

  [[nodiscard]] random::Rng
  rng(std::initializer_list<std::string_view> parts) const {
    return rngFactory_->rng(parts);
  }

  [[nodiscard]] transactions::Transaction
  make(const transactions::Draft &draft) const {
    return txf_->make(draft);
  }

private:
  const random::RngFactory *rngFactory_ = nullptr;
  const transactions::Factory *txf_ = nullptr;
};

class TransferRun {
public:
  TransferRun() = default;

  TransferRun(KinshipView kinship, FamilyAccountDirectory accounts,
              EducationPayees education, PostingWindow posting,
              TransferEmission emission) noexcept
      : kinship_(kinship), accounts_(accounts), education_(education),
        posting_(posting), emission_(emission) {}

  [[nodiscard]] bool ready() const noexcept {
    return kinship_.ready() && accounts_.ready() && emission_.ready();
  }

  [[nodiscard]] const KinshipView &kinship() const noexcept { return kinship_; }

  [[nodiscard]] const FamilyAccountDirectory &accounts() const noexcept {
    return accounts_;
  }

  [[nodiscard]] const EducationPayees &education() const noexcept {
    return education_;
  }

  [[nodiscard]] const PostingWindow &posting() const noexcept {
    return posting_;
  }

  [[nodiscard]] const TransferEmission &emission() const noexcept {
    return emission_;
  }

private:
  KinshipView kinship_{};
  FamilyAccountDirectory accounts_{};
  EducationPayees education_{};
  PostingWindow posting_{};
  TransferEmission emission_{};
};

} // namespace PhantomLedger::transfers::legit::routines::family
