#pragma once
/*
 * Transaction factory.
 *
 * Constructs final Transaction records from Drafts by attaching
 * infrastructure routing (device_id, ip_address). During fraud
 * transactions, the factory uses shared ring infra with high
 * probability, falling back to personal infra.
 */

#include "phantomledger/infra/router.hpp"
#include "phantomledger/infra/shared.hpp"
#include "phantomledger/random/rng.hpp"
#include "phantomledger/transactions/draft.hpp"
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

    // --- Device attribution ---
    bool deviceResolved = false;
    bool ipResolved = false;

    // Try shared ring infra first for fraud transactions.
    if (draft.ringId >= 0 && ringInfra_ != nullptr) {
      auto sharedDevice = ringInfra_->deviceForRing(draft.ringId);
      if (sharedDevice.has_value() && rng_.coin(ringInfra_->useSharedDeviceP)) {
        txn.session.deviceId = *sharedDevice;
        deviceResolved = true;
      }

      auto sharedIp = ringInfra_->ipForRing(draft.ringId);
      if (sharedIp.has_value() && rng_.coin(ringInfra_->useSharedIpP)) {
        txn.session.ipAddress = *sharedIp;
        ipResolved = true;
      }
    }

    // Fall back to personal infra for unresolved fields.
    if (router_ != nullptr && (!deviceResolved || !ipResolved)) {
      auto personal = router_->routeSource(rng_, draft.source);

      if (!deviceResolved && personal.device.has_value()) {
        txn.session.deviceId = *personal.device;
      }
      if (!ipResolved && personal.ip.has_value()) {
        txn.session.ipAddress = *personal.ip;
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
