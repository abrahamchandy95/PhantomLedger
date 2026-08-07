#pragma once

#include "phantomledger/entities/infra/derived_endpoints.hpp"
#include "phantomledger/entities/infra/router.hpp"
#include "phantomledger/entities/infra/shared.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/taxonomies/channels/predicates.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transactions/record.hpp"

namespace PhantomLedger::transactions {

class Factory {
public:
  Factory(random::Rng &rng, const infra::Router *router = nullptr,
          const infra::SharedInfra *ringInfra = nullptr)
      : rng_(&rng), router_(router), ringInfra_(ringInfra) {}

  [[nodiscard]] Factory rebound(random::Rng &newRng) const noexcept {
    return Factory(newRng, router_, ringInfra_);
  }

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

    if (draft.chainId >= 0) {
      txn.fraud.chainId = static_cast<std::uint32_t>(draft.chainId);
    }

    bool deviceResolved = false;
    bool ipResolved = false;

    if (draft.ringId >= 0 && ringInfra_ != nullptr) {
      const auto sharedDevice = ringInfra_->deviceForRing(draft.ringId);
      if (sharedDevice.has_value() &&
          (*rng_).coin(ringInfra_->useSharedDeviceP)) {
        txn.session.deviceId = *sharedDevice;
        deviceResolved = true;
      }

      const auto sharedIp = ringInfra_->ipForRing(draft.ringId);
      if (sharedIp.has_value() && (*rng_).coin(ringInfra_->useSharedIpP)) {
        txn.session.ipAddress = *sharedIp;
        ipResolved = true;
      }
    }

    const bool isCustomerSession =
        !channels::isExternallyInitiated(draft.channel);

    if (router_ != nullptr && isCustomerSession &&
        (!deviceResolved || !ipResolved)) {
      const auto owner = router_->ownerOf(draft.source);
      if (owner.has_value()) {
        /* MUST RESOLVE AS OF THE ROW'S OWN TIMESTAMP. Asking the router
         * without a time argument answers from the person's whole-window
         * pool, which put 53.51% of sessions outside the endpoint's own
         * recorded tenure. */
        if (!deviceResolved) {
          const auto d =
              router_->routeDeviceFor(*rng_, *owner, draft.timestamp);
          if (d.has_value()) {
            txn.session.deviceId = *d;
          }
        }
        if (!ipResolved) {
          const auto ip = router_->routeIpFor(*rng_, *owner, draft.timestamp);
          if (ip.has_value()) {
            txn.session.ipAddress = *ip;
          }
        }

        /* PASS-THROUGH ENDPOINT. A declared share of rows happens on a
         * PUBLIC TERMINAL — an endpoint the person uses but does not own —
         * which is the only construct in the model that gives a LEGITIMATE
         * device a fan-out a single consumer cannot reach. Without a
         * legitimate population up at those degrees, "many distinct cards on
         * one device" is a synonym for attacker infrastructure and a degree
         * threshold classifies at ceiling.
         *
         * THE ROUTER CALLS ABOVE STILL RUN, AND THEIR RESULT IS OVERRIDDEN
         * RATHER THAN SKIPPED. Two things depend on that and they pull the
         * same way:
         *
         *   * DRAW COUNT. `routeDeviceFor` / `routeIpFor` may spend a switch
         *     coin and a choice index. Skipping them on terminal rows would
         *     make the routing lane's draw count depend on this predicate and
         *     re-roll every amount and timestamp after it. Overriding costs
         *     nothing.
         *   * STICKY STATE. The person's `currentDeviceIdx_` therefore
         *     evolves EXACTLY as it does without this branch, so their later
         *     rows cannot depend on whether they passed through a terminal —
         *     which is what keeps the batch and windowed engines in lockstep.
         *     "Do not advance the sticky index" is delivered by leaving the
         *     walk untouched, not by stepping around it.
         *
         * THE ADDRESS TRAVELS WITH THE TERMINAL. A terminal keeps one exit
         * address for its service life, so the IP layer gets the same
         * legitimate high-fan-out population the device layer does. Moving
         * the device alone would leave the IP-side degree shortcut — measured
         * at essentially the same strength — untouched, and the round would
         * relocate the leak rather than close it.
         *
         * The row salt is the row's own TIMESTAMP: world-derived and
         * row-stable, never an emission ordinal, so a short run stays a
         * byte-identical prefix of a long one.
         *
         * BOTH LEGS OR NEITHER, and a ring's shared infrastructure outranks
         * it. Overriding one leg alone would put a terminal address beside a
         * ring device on the same row — a pairing no construct in the model
         * produces, which is the same failure the fraud planner's
         * both-legs-or-neither rule exists to prevent. */
        bool onTerminal = false;
        if (!deviceResolved && !ipResolved) {
          if (const auto terminal = router_->publicDeviceFor(
                  *owner, draft.timestamp,
                  static_cast<std::uint64_t>(draft.timestamp))) {
            txn.session.deviceId = *terminal;
            txn.session.ipAddress = infra::derived::endpointIp(*terminal);
            onTerminal = true;
          }
        }

        /* CARRIER-GRADE NAT — the same override one layer down, and the
         * stronger of the two: a terminal carries ~2% of rows, a carrier
         * address ~30%. It is what puts a LEGITIMATE population at IP degrees
         * a single subscriber cannot reach, which the device axis alone does
         * not do; closing device fan-out while leaving this would move the
         * degree shortcut from one exported column to the next one over.
         *
         * IP LEG ONLY. The subscriber keeps their own handset — see
         * `Router::publicIpFor`.
         *
         * RANKED BELOW BOTH OTHER CLAIMS ON THE ADDRESS. A terminal already
         * carries its own exit address, and a ring's shared infrastructure
         * outranks everything; overriding either would pair an address with a
         * device no construct in the model puts beside it. Draw-free, so the
         * ranking costs nothing on the lane. */
        if (!ipResolved && !onTerminal) {
          if (const auto exit = router_->publicIpFor(
                  *owner, draft.timestamp,
                  static_cast<std::uint64_t>(draft.timestamp))) {
            txn.session.ipAddress = *exit;
          }
        }
      }
    }

    return txn;
  }

private:
  random::Rng *rng_;
  const infra::Router *router_;
  const infra::SharedInfra *ringInfra_;
};

} // namespace PhantomLedger::transactions
