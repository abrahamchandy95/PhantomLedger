#include "phantomledger/relationships/family/builder.hpp"

#include "phantomledger/primitives/random/factory.hpp"
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

[[nodiscard]] Partition runPartition(const Households &households,
                                     const random::RngFactory &factory,
                                     std::uint32_t personCount) {
  auto rng = factory.rng({"family", "households"});
  return partition(households, rng, personCount);
}

[[nodiscard]] Links runLinks(const random::RngFactory &factory,
                             const LinkInputs &inputs) {
  auto rng = factory.rng({"family", "links"});
  return buildLinks(inputs, rng);
}

[[nodiscard]] SupportTies runSupport(const random::RngFactory &factory,
                                     const SupportInputs &inputs) {
  auto rng = factory.rng({"family", "support"});
  return buildSupportTies(inputs, rng);
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

Graph build(const BuildInputs &inputs, const Households &households,
            const Dependents &dependents,
            const RetireeSupport &retireeSupport) {
  if (inputs.personCount == 0) {
    return emptyGraph();
  }

  const random::RngFactory factory{inputs.baseSeed};

  auto partition = runPartition(households, factory, inputs.personCount);
  auto links = runLinks(factory, LinkInputs{
                                     .households = &households,
                                     .dependents = &dependents,
                                     .partition = &partition,
                                     .personas = inputs.personas,
                                     .personCount = inputs.personCount,
                                 });
  auto support = runSupport(factory, SupportInputs{
                                         .support = &retireeSupport,
                                         .partition = &partition,
                                         .links = &links,
                                         .personas = inputs.personas,
                                         .personCount = inputs.personCount,
                                     });

  return fold(std::move(partition), std::move(links), std::move(support));
}

} // namespace PhantomLedger::relationships::family
