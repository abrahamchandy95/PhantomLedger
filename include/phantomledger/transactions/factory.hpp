#pragma once
/*
 * Transaction factory.
 *
 * Constructs final Transaction records from Drafts by attaching
 * infrastructure routing (device_id, ip_address). During fraud
 * transactions, the factory uses shared ring infra with high
 * probability, falling back to personal infra.
 *
 * Performance notes:
 *
 *   - One owner-lookup per transaction instead of one-per-leg. The
 *     router now exposes ownerOf() separately from the per-leg route
 *     functions, so we resolve the source account's owner once and
 *     reuse it for both device and IP.
 *
 *   - RNG draws for personal infra are skipped when shared-ring infra
 *     has already resolved that leg. Under the previous revision the
 *     router always drew for both legs even if only one was needed.
 */

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transactions/infra/router.hpp"
#include "phantomledger/transactions/infra/shared.hpp"
#include "phantomledger/transactions/record.hpp"

namespace PhantomLedger::transactions {

class Factory {
public:
  Factory(random::Rng &rng, const infra::Router *router = nullptr,
          const infra::SharedInfra *ringInfra = nullptr)
      : rng_(rng), router_(router), ringInfra_(ringInfra) {}

  [[nodiscard]] Transaction make(const Draft &draft) const {
    Transaction txn;
    txn.source = draft.source;
    txn.target = draft.destination;
    txn.amount = draft.amount;
    txn.timestamp = draft.timestamp;
    txn.fraud.flag = draft.isFraud;
    txn.session.channel = draft.channel;

    if (draft.ringId >= 0) {
      txn.fraud.ringId = static_cast<std::uint32_t>(draft.ringId);
    }

    bool deviceResolved = false;
    bool ipResolved = false;

    // Try shared ring infra first for fraud transactions. Short-circuit
    // around the coin flip when the ring doesn't have shared infra for
    // that leg, so we don't consume RNG we don't use.
    if (draft.ringId >= 0 && ringInfra_ != nullptr) {
      const auto sharedDevice = ringInfra_->deviceForRing(draft.ringId);
      if (sharedDevice.has_value() && rng_.coin(ringInfra_->useSharedDeviceP)) {
        txn.session.deviceId = *sharedDevice;
        deviceResolved = true;
      }

      const auto sharedIp = ringInfra_->ipForRing(draft.ringId);
      if (sharedIp.has_value() && rng_.coin(ringInfra_->useSharedIpP)) {
        txn.session.ipAddress = *sharedIp;
        ipResolved = true;
      }
    }

    // Fall back to personal infra. Resolve the owner once; then route
    // only the legs that still need a value.
    if (router_ != nullptr && (!deviceResolved || !ipResolved)) {
      const auto owner = router_->ownerOf(draft.source);
      if (owner.has_value()) {
        if (!deviceResolved) {
          const auto d = router_->routeDeviceFor(rng_, *owner);
          if (d.has_value()) {
            txn.session.deviceId = *d;
          }
        }
        if (!ipResolved) {
          const auto ip = router_->routeIpFor(rng_, *owner);
          if (ip.has_value()) {
            txn.session.ipAddress = *ip;
          }
        }
      }
    }

    return txn;
  }

private:
  random::Rng &rng_;
  const infra::Router *router_;
  const infra::SharedInfra *ringInfra_;
};

} // namespace PhantomLedger::transactions
