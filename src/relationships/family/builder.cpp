#include "phantomledger/relationships/family/builder.hpp"

#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/relationships/family/links.hpp"
#include "phantomledger/relationships/family/partition.hpp"
#include "phantomledger/relationships/family/support.hpp"

#include <utility>

namespace PhantomLedger::relationships::family {

namespace {

[[nodiscard]] Graph emptyGraph() {
  Graph g{};
  g.householdOffsets.push_back(0U);
  return g;
}

[[nodiscard]] Partition runPartition(const transfers::family::GraphConfig &cfg,
                                     const random::RngFactory &factory,
                                     std::uint32_t personCount) {
  auto rng = factory.rng({"family", "households"});
  return partition(cfg.households, rng, personCount);
}

[[nodiscard]] Links runLinks(const transfers::family::GraphConfig &cfg,
                             const random::RngFactory &factory,
                             const Partition &part, const BuildInputs &inputs) {
  auto rng = factory.rng({"family", "links"});
  const LinkInputs li{
      .cfg = &cfg,
      .partition = &part,
      .personas = inputs.personas,
      .personCount = inputs.personCount,
  };
  return buildLinks(li, rng);
}

[[nodiscard]] SupportTies runSupport(const transfers::family::GraphConfig &cfg,
                                     const random::RngFactory &factory,
                                     const Partition &part, const Links &links,
                                     const BuildInputs &inputs) {
  auto rng = factory.rng({"family", "support"});
  const SupportInputs si{
      .cfg = &cfg.retireeSupport,
      .partition = &part,
      .links = &links,
      .personas = inputs.personas,
      .personCount = inputs.personCount,
  };
  return buildSupportTies(si, rng);
}

[[nodiscard]] Graph fold(Partition &&part, Links &&links,
                         SupportTies &&support) {
  Graph g;
  g.householdMembers = std::move(part.members);
  g.householdOffsets = std::move(part.offsets);
  g.householdOf = std::move(part.householdOf);

  g.spouseOf = std::move(links.spouseOf);
  g.parentsOf = std::move(links.parentsOf);
  g.childrenOf = std::move(links.childrenOf);

  g.supportingChildrenOf = std::move(support.supportingChildrenOf);
  g.supportedParentsBy = std::move(support.supportedParentsBy);

  return g;
}

} // namespace

Graph build(const transfers::family::GraphConfig &cfg,
            const BuildInputs &inputs) {
  if (inputs.personCount == 0) {
    return emptyGraph();
  }

  const random::RngFactory factory{inputs.baseSeed};

  auto partition = runPartition(cfg, factory, inputs.personCount);
  auto links = runLinks(cfg, factory, partition, inputs);
  auto support = runSupport(cfg, factory, partition, links, inputs);

  return fold(std::move(partition), std::move(links), std::move(support));
}

} // namespace PhantomLedger::relationships::family
