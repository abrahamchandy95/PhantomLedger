#pragma once

#include "phantomledger/entities/identifier/person.hpp"
#include "phantomledger/random/rng.hpp"
#include "phantomledger/taxonomies/personas/archetypes.hpp"

#include <algorithm>
#include <array>
#include <vector>

namespace PhantomLedger::entities::synth::personas {

struct Mix {
  std::array<double, ::PhantomLedger::personas::kKindCount> fractions =
      ::PhantomLedger::personas::kDefaultFractions;
};

[[nodiscard]] inline std::vector<::PhantomLedger::personas::Type>
assign(random::Rng &rng, std::uint32_t people, const Mix &mix = {}) {
  std::vector<::PhantomLedger::personas::Type> out(
      static_cast<std::size_t>(people),
      ::PhantomLedger::personas::Type::salaried);

  if (people == 0) {
    return out;
  }

  struct Slice {
    ::PhantomLedger::personas::Type type;
    int count;
  };

  std::vector<Slice> slices;
  for (std::size_t i = 0; i < ::PhantomLedger::personas::kKindCount; ++i) {
    const auto kind = static_cast<::PhantomLedger::personas::Type>(i);
    if (kind == ::PhantomLedger::personas::Type::salaried) {
      continue;
    }

    const int count =
        static_cast<int>(mix.fractions[i] * static_cast<double>(people));
    if (count > 0) {
      slices.push_back(Slice{kind, count});
    }
  }

  std::vector<identifier::PersonId> remaining;
  remaining.reserve(people);
  for (identifier::PersonId person = 1; person <= people; ++person) {
    remaining.push_back(person);
  }

  for (const auto &slice : slices) {
    if (remaining.empty()) {
      break;
    }

    const int count = std::min(slice.count, static_cast<int>(remaining.size()));
    if (count <= 0) {
      continue;
    }

    auto chosen = rng.choiceIndices(remaining.size(),
                                    static_cast<std::size_t>(count), false);
    std::sort(chosen.begin(), chosen.end(), std::greater<>());

    for (auto idx : chosen) {
      const auto person = remaining[idx];
      out[person - 1] = slice.type;
      remaining[idx] = remaining.back();
      remaining.pop_back();
    }
  }

  return out;
}

} // namespace PhantomLedger::entities::synth::personas
