#pragma once

#include "phantomledger/entities/parties/people.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/synth/infra/ips_output.hpp"
#include "phantomledger/synth/infra/types.hpp"

#include <cstdint>
#include <unordered_map>

namespace PhantomLedger::synth::infra::ips {

struct AssignmentRules {
  /* Probability that a person who is still unattached anchors a HOUSEHOLD
   * address — the address a group shares because they share a router, not a
   * seven-day coincidence. The device-side twin of this field carries the full
   * argument and the Gomez-Boix direction; see `synth/infra/devices.hpp`.
   *
   * LEVEL IS A DECLARED CHOICE. It is kept equal to the device side because a
   * household that shares a tablet is the same household that shares a router,
   * and giving the two different rates would assert a difference between them
   * that nothing here measures. */
  double sharedIpP = 0.25;

  /* Peers drawn in ADDITION to the anchor, uniform on [1, this]. */
  std::uint32_t sharedGroupMaxExtra = 5;

  double extraIpP1 = 0.35;
  double extraIpP2 = 0.10;

  /* CARRIER-GRADE NAT. Mean subscribers behind one public address, and the
   * share of a person's rows that exit through it rather than through an
   * address of their own.
   *
   * THE MEAN IS ANCHORED IN ORDER OF MAGNITUDE, THE SHARE IS NOT. Richter et
   * al., IMC 2016 (accessed 2026-08-07) measure 64 subscribers per public IPv4
   * at a 1K port chunk and up to ~128 at 512 ports, so 96 is the midpoint of a
   * reported range rather than an invention. The ROW SHARE has no anchor at
   * all: that paper reports how many ISPs deploy CGNAT, never what fraction of
   * a consumer's card transactions leave through one. It is a DECLARED CHOICE
   * and reads as "roughly a third of sessions are mobile or CGNAT-bound",
   * which is consistent with each person already holding one to three
   * addresses meant as home, mobile and work.
   *
   * The pool is sized off the ROSTER, never off the window. */
  double peoplePerCarrierNat = 96.0;

  /* CITED, and the one level in either public pool that is. Richter et al.'s
   * upper reading — 128 subscribers behind one public IPv4 at a 512-port
   * chunk — is a ceiling on USERS PER ADDRESS measured directly, so it can be
   * spent here as itself rather than as a direction. The device-side twin has
   * no equivalent and says so.
   *
   * Without it the head of this pool is bounded only by the drawn weight
   * ratio, which at a mean of 96 is a hidden ceiling of about 1,512
   * subscribers — an order of magnitude past what the source reports. */
  double maxUsersPerCarrierNat = 128.0;

  double carrierNatRowShare = 0.30;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::between("sharedIpP", sharedIpP, 0.0, 1.0); });
    r.check([&] {
      v::between("sharedGroupMaxExtra",
                 static_cast<double>(sharedGroupMaxExtra), 1.0, 64.0);
    });
    r.check([&] { v::between("extraIpP1", extraIpP1, 0.0, 1.0); });
    r.check([&] { v::between("extraIpP2", extraIpP2, 0.0, 1.0); });
    r.check([&] {
      v::between("peoplePerCarrierNat", peoplePerCarrierNat, 1.0, 100000.0);
    });
    r.check([&] {
      v::between("maxUsersPerCarrierNat", maxUsersPerCarrierNat, 1.0, 100000.0);
    });
    r.check([&] {
      v::between("carrierNatRowShare", carrierNatRowShare, 0.0, 1.0);
    });
  }

  /* `runSeed` keys the two ISOLATED LANES this pass runs its data-dependent
   * halves on; see the device-side twin for the argument. */
  [[nodiscard]] Output
  build(random::Rng &rng, time::Window window,
        const entity::person::Roster &people,
        const std::unordered_map<std::uint32_t, RingPlan> &ringPlans,
        std::uint64_t runSeed) const;
};

} // namespace PhantomLedger::synth::infra::ips
