#pragma once

#include "phantomledger/primitives/crypto/blake2b.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <stdexcept>
#include <string>
#include <string_view>

namespace PhantomLedger::exporter::mule_temporal::zelle {

// Federal Reserve 2024 DCPC public microdata; reproduce with
// docs/research/zelle_2024.py. These describe past-year senders and their
// electronic P2P payment choices, NOT network enrollment or all A2A payments.
inline constexpr double kSenderProbability = 0.3151632893823225;
inline constexpr double kPaymentProbability = 0.556224548689485;

// A pinned 2024 behavioral scenario, not a historical adoption reconstruction.
// Temporal mule exports require a start year of 2019 or later.
inline constexpr int kFirstSupportedYear = 2019;

inline double draw(std::uint64_t seed, std::string_view domain,
                   std::string_view identity) {
  const auto input = std::to_string(seed) + ":" + std::string{domain} + ":" +
                     std::string{identity};
  std::array<unsigned char, 8> bytes{};
  if (!crypto::blake2b::digest(input.data(), input.size(), bytes.data(),
                               bytes.size()))
    throw std::runtime_error("mule-temporal: Zelle policy digest failed");
  std::uint64_t value = 0;
  for (auto byte : bytes)
    value = (value << 8) | byte;
  return static_cast<double>(value >> 11) * 0x1.0p-53;
}

// Published Wells Fargo maximum dollar limits, applied as a homogeneous
// synthetic bank-policy scenario. Banks may impose different/lower limits.
// Only accepted earlier sends affect these rolling windows; receives do not.
class SendingWindow {
public:
  bool accept(std::int64_t epoch, double amount, bool business) {
    const std::int64_t dailyCap = business ? 1'500'000 : 350'000;
    const std::int64_t monthlyCap = business ? 6'000'000 : 2'000'000;
    if (!std::isfinite(amount) || amount <= 0 || amount > dailyCap / 100.0)
      return false;
    const auto cents = std::llround(amount * 100.0);
    if (cents <= 0)
      return false;
    while (!sent_.empty() && sent_.front().epoch <= epoch - 30 * 86400)
      sent_.pop_front();
    std::int64_t daily = 0, monthly = 0;
    for (const auto &sent : sent_) {
      monthly += sent.cents;
      if (sent.epoch > epoch - 86400)
        daily += sent.cents;
    }
    if (daily + cents > dailyCap || monthly + cents > monthlyCap)
      return false;
    sent_.push_back({epoch, cents});
    return true;
  }

private:
  struct Sent {
    std::int64_t epoch, cents;
  };
  std::deque<Sent> sent_;
};

} // namespace PhantomLedger::exporter::mule_temporal::zelle
