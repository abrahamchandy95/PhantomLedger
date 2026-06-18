#pragma once

#include "phantomledger/primitives/random/rng.hpp"

#include <cmath>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <vector>

namespace PhantomLedger::probability::distributions {

/// Walker/Vose alias table for repeated weighted sampling.
///
/// Build:  O(n)
/// Sample: O(1)
///
/// The input weights may contain zeros, but must be non-negative and must have
/// a finite positive total.
class AliasTable {
public:
  AliasTable() = default;

  explicit AliasTable(std::span<const double> weights) { build(weights); }

  void build(std::span<const double> weights) {
    if (weights.empty()) {
      throw std::invalid_argument("AliasTable requires at least one weight");
    }

    double total = 0.0;
    for (const double weight : weights) {
      if (weight < 0.0) {
        throw std::invalid_argument("AliasTable requires non-negative weights");
      }
      total += weight;
    }

    if (!std::isfinite(total) || total <= 0.0) {
      throw std::invalid_argument(
          "AliasTable requires a finite positive weight sum");
    }

    const auto n = weights.size();

    prob_.assign(n, 1.0);
    alias_.resize(n);

    std::vector<double> scaled(n);
    std::vector<std::size_t> small;
    std::vector<std::size_t> large;

    small.reserve(n);
    large.reserve(n);

    for (std::size_t i = 0; i < n; ++i) {
      alias_[i] = i;
      scaled[i] = weights[i] * static_cast<double>(n) / total;

      if (scaled[i] < 1.0) {
        small.push_back(i);
      } else {
        large.push_back(i);
      }
    }

    while (!small.empty() && !large.empty()) {
      const auto s = small.back();
      small.pop_back();

      const auto l = large.back();
      large.pop_back();

      prob_[s] = scaled[s];
      alias_[s] = l;

      scaled[l] = (scaled[l] + scaled[s]) - 1.0;

      if (scaled[l] < 1.0) {
        small.push_back(l);
      } else {
        large.push_back(l);
      }
    }

    // Numerical leftovers. These columns are effectively certain.
    for (const auto i : large) {
      prob_[i] = 1.0;
      alias_[i] = i;
    }

    for (const auto i : small) {
      prob_[i] = 1.0;
      alias_[i] = i;
    }
  }

  [[nodiscard]] bool empty() const noexcept { return prob_.empty(); }

  [[nodiscard]] std::size_t size() const noexcept { return prob_.size(); }

  [[nodiscard]] std::size_t sample(random::Rng &rng) const {
    if (prob_.empty()) {
      throw std::invalid_argument(
          "AliasTable::sample requires a non-empty table");
    }

    const auto column = rng.choiceIndex(prob_.size());
    return rng.nextDouble() < prob_[column] ? column : alias_[column];
  }

private:
  std::vector<double> prob_;
  std::vector<std::size_t> alias_;
};

} // namespace PhantomLedger::probability::distributions
