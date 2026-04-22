#pragma once
/*
 * Rent payment channel routing.
 *
 * Given a landlord type, picks a payment channel using the per-type
 * distribution from the landlord config. Individual landlords get
 * Zelle/check/ACH mixes; corporate property managers get near-exclusive
 * portal ACH.
 */

#include "phantomledger/distributions/cdf.hpp"
#include "phantomledger/entities/landlords/class.hpp"
#include "phantomledger/random/rng.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"

#include <array>
#include <optional>
#include <span>

namespace PhantomLedger::recurring {

namespace detail {

template <std::size_t N> struct ChannelTable {
  std::array<channels::Tag, N> channels;
  std::array<double, N> cdf;

  [[nodiscard]] channels::Tag pick(random::Rng &rng) const {
    const auto idx = distributions::sampleIndex(std::span<const double>{cdf},
                                                rng.nextDouble());
    return channels[idx];
  }
};

inline constexpr channels::Tag kFallbackChannel =
    channels::tag(channels::Rent::generic);

inline constexpr ChannelTable<4> kIndividualChannels{
    .channels =
        {
            channels::tag(channels::Rent::p2p),
            channels::tag(channels::Rent::check),
            channels::tag(channels::Rent::ach),
            channels::tag(channels::Rent::generic),
        },
    .cdf = {0.40, 0.65, 0.90, 1.00},
};

inline constexpr ChannelTable<3> kLlcSmallChannels{
    .channels =
        {
            channels::tag(channels::Rent::ach),
            channels::tag(channels::Rent::check),
            channels::tag(channels::Rent::p2p),
        },
    .cdf = {0.55, 0.85, 1.00},
};

inline constexpr ChannelTable<2> kCorporateChannels{
    .channels =
        {
            channels::tag(channels::Rent::portal),
            channels::tag(channels::Rent::ach),
        },
    .cdf = {0.95, 1.00},
};

} // namespace detail

class RentRouter {
public:
  /// Pick a channel for one rent event.
  ///
  /// Returns the generic rent channel if the landlord class is unknown.
  [[nodiscard]] channels::Tag
  pick(random::Rng &rng,
       std::optional<entities::landlords::Class> landlordClass) const {
    using Class = entities::landlords::Class;

    if (!landlordClass.has_value()) {
      return detail::kFallbackChannel;
    }

    switch (*landlordClass) {
    case Class::individual:
      return detail::kIndividualChannels.pick(rng);
    case Class::llcSmall:
      return detail::kLlcSmallChannels.pick(rng);
    case Class::corporate:
      return detail::kCorporateChannels.pick(rng);
    }

    return detail::kFallbackChannel;
  }
};

} // namespace PhantomLedger::recurring
