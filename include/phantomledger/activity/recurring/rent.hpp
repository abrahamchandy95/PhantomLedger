#pragma once

#include "phantomledger/entities/landlords.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/probability/distributions/cdf.hpp"
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
  [[nodiscard]] channels::Tag
  pick(random::Rng &rng,
       std::optional<entity::landlord::Type> landlordType) const {
    using Type = entity::landlord::Type;

    if (!landlordType.has_value()) {
      return detail::kFallbackChannel;
    }

    switch (*landlordType) {
    case Type::individual:
      return detail::kIndividualChannels.pick(rng);
    case Type::llcSmall:
      return detail::kLlcSmallChannels.pick(rng);
    case Type::corporate:
      return detail::kCorporateChannels.pick(rng);
    }

    return detail::kFallbackChannel;
  }
};

} // namespace PhantomLedger::recurring
