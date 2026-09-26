#pragma once
//
// phantomledger/synth/products/providers.hpp
//
// WHO a loan or policy pays (institutional-providers-2026-09). Before this
// round every mortgage, auto loan, student loan and auto, home and life
// policy in the population paid one of six fixed keys, and every mortgage paid
// the STUDENT servicer. Each contract now draws its provider once, at
// issuance, from a national market-share table.
//
// THE DRAW IS ON ITS OWN LANE. ProviderPicker::pick builds
// RngFactory{seed}.rng({"product-provider", <market>, <person>}) and takes
// exactly one uniform. It never reads the per-person portfolio stream the
// emitters share, so every adoption coin, payment, term, age and timestamp is
// byte-identical to a build without providers; only the counterparty key on a
// contract changes. A person holds at most one contract per market, so the
// lane is unique per contract. The gates are test_product_providers A1 (no
// draw depends on the table) and A7 (the key-free products equal the
// pre-round build's).
//
// The tables are value-weighted shares (servicing UPB, written premium,
// outstanding balances) assigned per contract; see the institutional-providers
// amendment in docs/fraud_model_audit.md for the authority rows and the
// registered limitations.
//

#include "phantomledger/entities/counterparties/providers.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/taxonomies/counterparties/types.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::synth::products {

// A cumulative share the research reports at a rank past the named block
// (for example "top 25 hold 72.83%"). The ranks between two anchors are
// filled with a 1/rank shape scaled to the mass between them. The LAST
// anchor's rank is the pool size and its cumulative is the market mass the
// pool represents; the table is renormalized by it.
struct ProviderTailAnchor {
  std::uint32_t rank = 0;
  double cumulative = 0.0;
};

struct ProviderMarketSpec {
  ::PhantomLedger::counterparties::Market market{};
  std::uint32_t poolSize = 0;
  // Published shares for ranks 1..K, largest first.
  std::span<const double> named;
  std::span<const ProviderTailAnchor> anchors;
};

class ProviderMarkets {
public:
  // Validates every spec and throws std::invalid_argument when a table is
  // malformed: a pool outside 1..99,999, named shares that increase or are
  // not positive, anchors out of order or not ending at the pool size, a
  // segment with no mass, a cumulative above 1, or a tail whose first share
  // exceeds the share before it. Every market must appear exactly once.
  explicit ProviderMarkets(std::span<const ProviderMarketSpec> specs);

  // The cited national tables (synth/products/providers.cpp). Built once.
  [[nodiscard]] static const ProviderMarkets &production();

  // One provider per market: the pre-round shape, and the disarm the gate
  // uses. A pool of one needs no draw, so pick() makes none here; that is
  // what arms the gate's invariance leg against a picker that reads the
  // portfolio stream.
  [[nodiscard]] static const ProviderMarkets &singleton();

  [[nodiscard]] std::uint32_t
  poolSize(::PhantomLedger::counterparties::Market market) const noexcept;

  // The ordinal (1-based rank) a uniform in [0, 1) selects.
  [[nodiscard]] std::uint32_t
  ordinalFor(::PhantomLedger::counterparties::Market market,
             double u) const noexcept;

  // Normalized share of one ordinal, and of the top `rank` ordinals.
  [[nodiscard]] double share(::PhantomLedger::counterparties::Market market,
                             std::uint32_t ordinal) const;
  [[nodiscard]] double
  cumulativeShare(::PhantomLedger::counterparties::Market market,
                  std::uint32_t rank) const;

private:
  [[nodiscard]] const std::vector<double> &
  cdf(::PhantomLedger::counterparties::Market market) const noexcept;

  std::array<std::vector<double>, ::PhantomLedger::counterparties::kMarketCount>
      cdf_{};
};

// Which (market, ordinal) pairs some contract actually uses. The products
// stage registers exactly these, so a small population exports no provider
// that nothing pays.
class UsedProviders {
public:
  void mark(::PhantomLedger::counterparties::Market market,
            std::uint32_t ordinal);

  // Market order, then ordinal ascending: a pure function of the set, so
  // the registration order cannot depend on emission order.
  [[nodiscard]] std::vector<::PhantomLedger::entity::Key> keys() const;

private:
  std::array<std::vector<bool>, ::PhantomLedger::counterparties::kMarketCount>
      byMarket_{};
};

class ProviderPicker {
public:
  ProviderPicker(const ProviderMarkets &markets, std::uint64_t seed,
                 ::PhantomLedger::entity::PersonId person,
                 UsedProviders *used = nullptr) noexcept;

  [[nodiscard]] ::PhantomLedger::entity::Key
  pick(::PhantomLedger::counterparties::Market market) const;

private:
  const ProviderMarkets *markets_;
  std::uint64_t seed_;
  ::PhantomLedger::entity::PersonId person_;
  UsedProviders *used_;
};

} // namespace PhantomLedger::synth::products
