#pragma once

/*
  A draw-free weighted pool over a class-contiguous counterparty roster
  (counterparty-sizes-2026-09).

  Employers and landlords are built as size classes laid end to end: every
  member of a class holds one slot, and a class's members are equally likely
  except in the one rank-size TAIL class, whose members carry their own
  weights. The pool stores only that structure (class offsets, a class CDF and
  the tail CDF), never a per-member weight, so it is O(classes + tail) however
  large the roster grows.

  pick(u) turns ONE uniform into a member by successive rescaling: the class
  from the class CDF, then the residual of u inside that class picks the
  member. pickExcluding(u, x) renormalises over the complement of x exactly,
  the same construction as ResidenceSampler::sampleExcluding. Neither draws;
  the caller owns the uniform, which is what lets the picks reuse the first
  u64 of the existing per-person lanes.
*/

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/distributions/cdf.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace PhantomLedger::entity::counterparty {

class SizedPool {
public:
  static constexpr std::size_t npos = std::numeric_limits<std::size_t>::max();

  struct Class {
    std::uint32_t members = 0;
    double mass = 0.0;
  };

  SizedPool() = default;

  /// One member, all mass on it (the standalone fallbacks).
  [[nodiscard]] static SizedPool single() {
    const Class only{.members = 1, .mass = 1.0};
    return build(std::span<const Class>{&only, 1}, npos, {});
  }

  /// `classes` in roster order. `tailClass` (or npos) names the one class
  /// whose members take `tailWeights` (relative, in roster order) instead of
  /// an equal share of the class mass.
  [[nodiscard]] static SizedPool build(std::span<const Class> classes,
                                       std::size_t tailClass,
                                       std::span<const double> tailWeights) {
    if (classes.empty()) {
      throw std::invalid_argument("SizedPool requires at least one class");
    }

    SizedPool out;
    out.classStart_.reserve(classes.size() + 1);
    out.classStart_.push_back(0);

    std::vector<double> masses;
    masses.reserve(classes.size());

    std::uint64_t total = 0;
    for (const auto &cls : classes) {
      if (cls.members == 0 || !std::isfinite(cls.mass) || cls.mass <= 0.0) {
        throw std::invalid_argument(
            "SizedPool classes need members and a positive mass");
      }
      total += cls.members;
      if (total > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("SizedPool roster exceeds 2^32 members");
      }
      out.classStart_.push_back(static_cast<std::uint32_t>(total));
      masses.push_back(cls.mass);
    }
    out.classCdf_ = probability::distributions::buildCdf(masses);

    if (tailClass != npos) {
      if (tailClass >= classes.size() ||
          tailWeights.size() != classes[tailClass].members) {
        throw std::invalid_argument(
            "SizedPool tail weights must match the tail class size");
      }
      out.tailClass_ = tailClass;
      out.tailCdf_ = probability::distributions::buildCdf(tailWeights);
    }

    return out;
  }

  [[nodiscard]] std::size_t size() const noexcept {
    return classStart_.empty() ? 0 : classStart_.back();
  }

  [[nodiscard]] std::size_t classCount() const noexcept {
    return classCdf_.size();
  }

  /// One uniform in [0, 1) to one member index.
  [[nodiscard]] std::size_t pick(double u) const {
    if (size() == 0) {
      throw std::logic_error("SizedPool::pick on an empty pool");
    }
    const auto k = probability::distributions::sampleIndex(classCdf_, u);
    const double lo = k == 0 ? 0.0 : classCdf_[k - 1];
    const double mass = classCdf_[k] - lo;
    double r = mass > 0.0 ? (u - lo) / mass : 0.0;
    r = std::clamp(r, 0.0, kBelowOne);

    const std::size_t n = classStart_[k + 1] - classStart_[k];
    std::size_t j = 0;
    if (k == tailClass_) {
      j = probability::distributions::sampleIndex(tailCdf_, r);
    } else {
      j = std::min(n - 1, static_cast<std::size_t>(r * static_cast<double>(n)));
    }
    return classStart_[k] + j;
  }

  /// One uniform to one member other than `excluded`, drawn from the weights
  /// renormalised over the complement. An `excluded` outside the pool is no
  /// exclusion. Needs at least two members.
  [[nodiscard]] std::size_t pickExcluding(double u,
                                          std::size_t excluded) const {
    if (size() < 2) {
      throw std::logic_error("SizedPool::pickExcluding needs two members");
    }
    if (excluded >= size()) {
      return pick(u);
    }

    const auto span = spanOf(excluded);
    double v = u * (1.0 - span.weight);
    const bool above = v >= span.start;
    if (above) {
      v += span.weight;
    }

    const auto got = pick(v);
    if (got != excluded) {
      return got;
    }
    // Only a rounding tie at the excluded member's edge lands here. Step to
    // the neighbour on the side the uniform was on, without a draw.
    if (above) {
      return excluded + 1 < size() ? excluded + 1 : excluded - 1;
    }
    return excluded > 0 ? excluded - 1 : excluded + 1;
  }

  /// A member's share of the whole pool's mass.
  [[nodiscard]] double weight(std::size_t member) const {
    return spanOf(member).weight;
  }

  [[nodiscard]] std::size_t heapBytes() const noexcept {
    return classStart_.capacity() * sizeof(std::uint32_t) +
           (classCdf_.capacity() + tailCdf_.capacity()) * sizeof(double);
  }

private:
  static constexpr double kBelowOne = 1.0 - 0x1.0p-53;

  struct MemberSpan {
    double start = 0.0;
    double weight = 0.0;
  };

  [[nodiscard]] MemberSpan spanOf(std::size_t member) const {
    if (member >= size()) {
      throw std::out_of_range("SizedPool member out of range");
    }
    const auto it =
        std::upper_bound(classStart_.begin() + 1, classStart_.end(),
                         static_cast<std::uint32_t>(member));
    const auto k = static_cast<std::size_t>(it - (classStart_.begin() + 1));
    const double lo = k == 0 ? 0.0 : classCdf_[k - 1];
    const double mass = classCdf_[k] - lo;
    const std::size_t j = member - classStart_[k];

    if (k == tailClass_) {
      const double below = j == 0 ? 0.0 : tailCdf_[j - 1];
      return {.start = lo + mass * below,
              .weight = mass * (tailCdf_[j] - below)};
    }
    const double n = static_cast<double>(classStart_[k + 1] - classStart_[k]);
    return {.start = lo + mass * static_cast<double>(j) / n,
            .weight = mass / n};
  }

  std::vector<std::uint32_t> classStart_;
  std::vector<double> classCdf_;
  std::size_t tailClass_ = npos;
  std::vector<double> tailCdf_;
};

/// A roster's keys beside its size law. Roster serials are ordinal + 1, so a
/// key finds its slot in O(1); anything else is npos.
struct SizedKeys {
  std::vector<Key> keys;
  SizedPool law;

  [[nodiscard]] static SizedKeys single(Key key) {
    return SizedKeys{.keys = {key}, .law = SizedPool::single()};
  }

  [[nodiscard]] std::size_t size() const noexcept { return keys.size(); }
  [[nodiscard]] bool empty() const noexcept { return keys.empty(); }

  [[nodiscard]] std::size_t indexOf(const Key &key) const noexcept {
    if (key.number >= 1 && key.number <= keys.size() &&
        keys[key.number - 1] == key) {
      return static_cast<std::size_t>(key.number - 1);
    }
    // A one-key pool (the fallbacks) need not hold serial 1.
    if (keys.size() == 1 && keys.front() == key) {
      return 0;
    }
    return SizedPool::npos;
  }

  [[nodiscard]] std::size_t heapBytes() const noexcept {
    return keys.capacity() * sizeof(Key) + law.heapBytes();
  }
};

} // namespace PhantomLedger::entity::counterparty
