#pragma once

#include "phantomledger/primitives/random/distributions/cdf.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/taxonomies/fraud/types.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace PhantomLedger::transfers::fraud {

using ::PhantomLedger::fraud::Typology;

struct PlaybookPhase {
  Typology typology;
  double budgetFraction;
};

struct Playbook {
  std::span<const PlaybookPhase> phases;
  std::string_view name;
};

namespace playbooks {

namespace detail {

inline constexpr std::array<PlaybookPhase, 1> kClassicPhases{{
    {Typology::classic, 1.0},
}};
inline constexpr std::array<PlaybookPhase, 1> kPureLayeringPhases{{
    {Typology::layering, 1.0},
}};
inline constexpr std::array<PlaybookPhase, 1> kPureFunnelPhases{{
    {Typology::funnel, 1.0},
}};
inline constexpr std::array<PlaybookPhase, 1> kPureStructuringPhases{{
    {Typology::structuring, 1.0},
}};
inline constexpr std::array<PlaybookPhase, 1> kPureInvoicePhases{{
    {Typology::invoice, 1.0},
}};
inline constexpr std::array<PlaybookPhase, 1> kPureMulePhases{{
    {Typology::mule, 1.0},
}};
inline constexpr std::array<PlaybookPhase, 1> kPureCyclePhases{{
    {Typology::cycle, 1.0},
}};
inline constexpr std::array<PlaybookPhase, 1> kPureScatterGatherPhases{{
    {Typology::scatterGather, 1.0},
}};
inline constexpr std::array<PlaybookPhase, 1> kPureBipartitePhases{{
    {Typology::bipartite, 1.0},
}};

inline constexpr std::array<PlaybookPhase, 3> kPlacementToIntegrationPhases{{
    {Typology::structuring, 0.25},
    {Typology::layering, 0.55},
    {Typology::invoice, 0.20},
}};
inline constexpr std::array<PlaybookPhase, 2> kSmurfThenLayerPhases{{
    {Typology::structuring, 0.40},
    {Typology::layering, 0.60},
}};
inline constexpr std::array<PlaybookPhase, 3> kRapidFunnelMulePhases{{
    {Typology::structuring, 0.20},
    {Typology::mule, 0.65},
    {Typology::funnel, 0.15},
}};
inline constexpr std::array<PlaybookPhase, 2> kShellLaunderingPhases{{
    {Typology::layering, 0.65},
    {Typology::invoice, 0.35},
}};
inline constexpr std::array<PlaybookPhase, 2> kClassicWithLayeringPhases{{
    {Typology::classic, 0.45},
    {Typology::layering, 0.55},
}};
inline constexpr std::array<PlaybookPhase, 2> kScatterGatherWithLayeringPhases{
    {{Typology::scatterGather, 0.45}, {Typology::layering, 0.55}}};
inline constexpr std::array<PlaybookPhase, 3> kBipartiteWebPhases{{
    {Typology::bipartite, 0.50},
    {Typology::scatterGather, 0.30},
    {Typology::invoice, 0.20},
}};
inline constexpr std::array<PlaybookPhase, 3> kMixingServicePhases{{
    {Typology::structuring, 0.20},
    {Typology::cycle, 0.40},
    {Typology::scatterGather, 0.40},
}};

} // namespace detail

inline constexpr Playbook kClassic{detail::kClassicPhases, "classic"};
inline constexpr Playbook kPureLayering{detail::kPureLayeringPhases,
                                        "pure_layering"};
inline constexpr Playbook kPureFunnel{detail::kPureFunnelPhases, "pure_funnel"};
inline constexpr Playbook kPureStructuring{detail::kPureStructuringPhases,
                                           "pure_structuring"};
inline constexpr Playbook kPureInvoice{detail::kPureInvoicePhases,
                                       "pure_invoice"};
inline constexpr Playbook kPureMule{detail::kPureMulePhases, "pure_mule"};
inline constexpr Playbook kPureCycle{detail::kPureCyclePhases, "pure_cycle"};
inline constexpr Playbook kPureScatterGather{detail::kPureScatterGatherPhases,
                                             "pure_scatter_gather"};
inline constexpr Playbook kPureBipartite{detail::kPureBipartitePhases,
                                         "pure_bipartite"};

inline constexpr Playbook kPlacementToIntegration{
    detail::kPlacementToIntegrationPhases, "placement_to_integration"};
inline constexpr Playbook kSmurfThenLayer{detail::kSmurfThenLayerPhases,
                                          "smurf_then_layer"};
inline constexpr Playbook kRapidFunnelMule{detail::kRapidFunnelMulePhases,
                                           "rapid_funnel_mule"};
inline constexpr Playbook kShellLaundering{detail::kShellLaunderingPhases,
                                           "shell_laundering"};
inline constexpr Playbook kClassicWithLayering{
    detail::kClassicWithLayeringPhases, "classic_with_layering"};
inline constexpr Playbook kScatterGatherWithLayering{
    detail::kScatterGatherWithLayeringPhases, "scatter_gather_with_layering"};
inline constexpr Playbook kBipartiteWeb{detail::kBipartiteWebPhases,
                                        "bipartite_web"};
inline constexpr Playbook kMixingService{detail::kMixingServicePhases,
                                         "mixing_service"};

} // namespace playbooks

struct PlaybookWeights {
  double classic = 0.12;
  double pureLayering = 0.04;
  double pureFunnel = 0.04;
  double pureStructuring = 0.04;
  double pureInvoice = 0.02;
  double pureMule = 0.12;
  double pureCycle = 0.04;
  double pureScatterGather = 0.05;
  double pureBipartite = 0.04;
  double placementToIntegration = 0.12;
  double smurfThenLayer = 0.08;
  double rapidFunnelMule = 0.10;
  double shellLaundering = 0.06;
  double classicWithLayering = 0.04;
  double scatterGatherWithLayering = 0.04;
  double bipartiteWeb = 0.03;
  double mixingService = 0.02;

  void validate(primitives::validate::Report &r) const;
  [[nodiscard]] const Playbook &choose(random::Rng &rng) const;
};

namespace detail {

struct PlaybookEntry {
  std::string_view name;
  double PlaybookWeights::*weight;
  const Playbook *playbook;
};

inline constexpr auto kPlaybookEntries = std::to_array<PlaybookEntry>({
    {"classic", &PlaybookWeights::classic, &playbooks::kClassic},
    {"pureLayering", &PlaybookWeights::pureLayering, &playbooks::kPureLayering},
    {"pureFunnel", &PlaybookWeights::pureFunnel, &playbooks::kPureFunnel},
    {"pureStructuring", &PlaybookWeights::pureStructuring,
     &playbooks::kPureStructuring},
    {"pureInvoice", &PlaybookWeights::pureInvoice, &playbooks::kPureInvoice},
    {"pureMule", &PlaybookWeights::pureMule, &playbooks::kPureMule},
    {"pureCycle", &PlaybookWeights::pureCycle, &playbooks::kPureCycle},
    {"pureScatterGather", &PlaybookWeights::pureScatterGather,
     &playbooks::kPureScatterGather},
    {"pureBipartite", &PlaybookWeights::pureBipartite,
     &playbooks::kPureBipartite},
    {"placementToIntegration", &PlaybookWeights::placementToIntegration,
     &playbooks::kPlacementToIntegration},
    {"smurfThenLayer", &PlaybookWeights::smurfThenLayer,
     &playbooks::kSmurfThenLayer},
    {"rapidFunnelMule", &PlaybookWeights::rapidFunnelMule,
     &playbooks::kRapidFunnelMule},
    {"shellLaundering", &PlaybookWeights::shellLaundering,
     &playbooks::kShellLaundering},
    {"classicWithLayering", &PlaybookWeights::classicWithLayering,
     &playbooks::kClassicWithLayering},
    {"scatterGatherWithLayering", &PlaybookWeights::scatterGatherWithLayering,
     &playbooks::kScatterGatherWithLayering},
    {"bipartiteWeb", &PlaybookWeights::bipartiteWeb, &playbooks::kBipartiteWeb},
    {"mixingService", &PlaybookWeights::mixingService,
     &playbooks::kMixingService},
});

[[nodiscard]] inline std::array<double, kPlaybookEntries.size()>
collectWeights(const PlaybookWeights &w) noexcept {
  std::array<double, kPlaybookEntries.size()> weights{};
  for (std::size_t i = 0; i < kPlaybookEntries.size(); ++i) {
    weights[i] = w.*kPlaybookEntries[i].weight;
  }
  return weights;
}

[[nodiscard]] inline double sum(std::span<const double> values) noexcept {
  double total = 0.0;
  for (const auto v : values) {
    total += v;
  }
  return total;
}

} // namespace detail

inline void PlaybookWeights::validate(primitives::validate::Report &r) const {
  namespace v = primitives::validate;
  for (const auto &entry : detail::kPlaybookEntries) {
    r.check([&] { v::nonNegative(entry.name, this->*entry.weight); });
  }
}

inline const Playbook &PlaybookWeights::choose(random::Rng &rng) const {
  const auto weights = detail::collectWeights(*this);
  const auto total = detail::sum(weights);
  if (total <= 0.0) {
    return *detail::kPlaybookEntries[0].playbook;
  }
  const auto cdf = probability::distributions::buildCdf(
      std::span<const double>(weights.data(), weights.size()));
  const auto idx =
      probability::distributions::sampleIndex(cdf, rng.nextDouble());
  return *detail::kPlaybookEntries[idx].playbook;
}

} // namespace PhantomLedger::transfers::fraud
