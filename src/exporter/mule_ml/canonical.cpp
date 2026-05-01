#include "phantomledger/exporter/mule_ml/canonical.hpp"

#include "phantomledger/entities/encoding/render.hpp"
#include "phantomledger/exporter/standard/infra.hpp"
#include "phantomledger/primitives/hashing/combine.hpp"
#include "phantomledger/transactions/network/format.hpp"

#include <cstdint>
#include <utility>

namespace PhantomLedger::exporter::mule_ml {

namespace {

template <typename ValueT> struct ObservedCount {
  ValueT value{};
  std::uint32_t count = 0;
};

struct AccountHistograms {
  std::unordered_map<std::string, std::uint32_t> deviceCounts;
  std::unordered_map<std::string, std::uint32_t> ipCounts;
};

[[nodiscard]] std::string
pickMostFrequent(const std::unordered_map<std::string, std::uint32_t> &counts) {
  if (counts.empty()) {
    return {};
  }

  const std::string *best = nullptr;
  std::uint32_t bestCount = 0;
  for (const auto &[key, count] : counts) {
    if (best == nullptr) {
      best = &key;
      bestCount = count;
      continue;
    }
    if (count > bestCount || (count == bestCount && key < *best)) {
      best = &key;
      bestCount = count;
    }
  }
  return *best;
}

[[nodiscard]] std::uint64_t
stableU64ForKey(const ::PhantomLedger::entity::Key &key,
                std::uint64_t salt) noexcept {
  return ::PhantomLedger::hashing::make(static_cast<std::uint16_t>(key.role),
                                        static_cast<std::uint8_t>(key.bank),
                                        key.number, salt);
}

[[nodiscard]] std::string
fallbackIp(const ::PhantomLedger::entity::Key &accountKey) {
  constexpr std::uint64_t kSalt = 0x49500A6E1B0CDEFEULL;
  const auto x = stableU64ForKey(accountKey, kSalt);
  const auto o1 = 11U + static_cast<std::uint32_t>(x % 212U);
  const auto o2 = static_cast<std::uint32_t>((x >> 8U) % 256U);
  const auto o3 = static_cast<std::uint32_t>((x >> 16U) % 256U);
  const auto o4 = 1U + static_cast<std::uint32_t>((x >> 24U) % 254U);
  return std::to_string(o1) + "." + std::to_string(o2) + "." +
         std::to_string(o3) + "." + std::to_string(o4);
}

[[nodiscard]] std::string
fallbackDevice(const ::PhantomLedger::entity::Key &accountKey) {
  return "DEV_" + ::PhantomLedger::encoding::format(accountKey);
}

} // namespace

CanonicalMap
buildCanonicalMaps(std::span<const ::PhantomLedger::entity::Key> partyIds,
                   const CanonicalInputs &inputs) {
  CanonicalMap out;
  out.reserve(partyIds.size());

  std::unordered_map<::PhantomLedger::entity::Key, AccountHistograms>
      perAccount;
  perAccount.reserve(partyIds.size());

  for (const auto &tx : inputs.finalTxns) {
    auto &hist = perAccount[tx.source];

    const auto deviceStr =
        ::PhantomLedger::exporter::standard::detail::renderDeviceId(
            tx.session.deviceId);
    if (!deviceStr.empty()) {
      ++hist.deviceCounts[deviceStr];
    }

    // IP: same shape.
    const auto ipStr = ::PhantomLedger::network::format(tx.session.ipAddress);
    if (!ipStr.empty()) {
      ++hist.ipCounts[ipStr];
    }
  }

  for (const auto &accountKey : partyIds) {
    CanonicalPair pair{};

    if (const auto it = perAccount.find(accountKey); it != perAccount.end()) {
      pair.deviceId = pickMostFrequent(it->second.deviceCounts);
      pair.ipAddress = pickMostFrequent(it->second.ipCounts);
    }

    if ((pair.deviceId.empty() || pair.ipAddress.empty()) &&
        inputs.accountToOwner != nullptr) {
      const auto ownerIt = inputs.accountToOwner->find(accountKey);
      if (ownerIt != inputs.accountToOwner->end()) {
        const auto person = ownerIt->second;

        if (pair.deviceId.empty() && inputs.devicesByPerson != nullptr) {
          if (const auto devIt = inputs.devicesByPerson->find(person);
              devIt != inputs.devicesByPerson->end() &&
              !devIt->second.empty()) {
            pair.deviceId =
                ::PhantomLedger::exporter::standard::detail::renderDeviceId(
                    devIt->second.front());
          }
        }

        if (pair.ipAddress.empty() && inputs.ipsByPerson != nullptr) {
          if (const auto ipIt = inputs.ipsByPerson->find(person);
              ipIt != inputs.ipsByPerson->end() && !ipIt->second.empty()) {
            pair.ipAddress =
                ::PhantomLedger::network::format(ipIt->second.front());
          }
        }
      }
    }

    if (pair.deviceId.empty()) {
      pair.deviceId = fallbackDevice(accountKey);
    }
    if (pair.ipAddress.empty()) {
      pair.ipAddress = fallbackIp(accountKey);
    }

    out.emplace(accountKey, std::move(pair));
  }

  return out;
}

} // namespace PhantomLedger::exporter::mule_ml
