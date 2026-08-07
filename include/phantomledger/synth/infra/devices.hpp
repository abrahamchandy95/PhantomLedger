#pragma once

#include "phantomledger/entities/parties/people.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/synth/infra/devices_output.hpp"
#include "phantomledger/synth/infra/types.hpp"

#include <cstdint>
#include <unordered_map>

namespace PhantomLedger::synth::infra::devices {

struct AssignmentRules {
  /* Probability that a person who is still unattached anchors a HOUSEHOLD
   * device — a line several people co-own and route through, not an episode.
   *
   * This was `legitDeviceNoiseP = 0.05` and the name was the defect: at that
   * rate 14% of people shared anything at all, and the device they shared was
   * given a tenure bounded at seven days, so the whole population carried
   * 0.0065%-0.0144% of card-view rows. Sharing was modelled as noise.
   *
   * DIRECTION IS CITED, LEVEL IS A DECLARED CHOICE. Gomez-Boix, Laperdrix and
   * Baudry, "Hiding in the Crowd: an Analysis of the Effectiveness of Browser
   * Fingerprinting at Large Scale", WWW 2018 (n = 2,067,942 fingerprints from
   * a top-15 French site, accessed 2026-08-07): only 33.6% of desktop and
   * 18.5% of mobile fingerprints were UNIQUE. Sharing is the majority case,
   * not a 5% garnish. That paper counts USERS per fingerprint, never CARDS per
   * device, so it fixes the direction and the fact that the majority shares —
   * it does not license any statement about fan-out.
   *
   * At 0.25 with the group size below, roughly two thirds of the legitimate
   * roster ends up in a household group of mean size four. */
  double sharedDeviceP = 0.25;

  /* Peers drawn in ADDITION to the anchor, uniform on [1, this]. Group size is
   * therefore 2 to `sharedGroupMaxExtra + 1`. */
  std::uint32_t sharedGroupMaxExtra = 5;

  double secondDeviceP = 0.20;

  /* SHARED AND PUBLIC TERMINALS. Mean people served by one terminal line, the
   * ceiling on the largest line's expected user count, and the share of a
   * person's rows that happen there instead of on an endpoint they hold.
   *
   * ALL THREE ARE DECLARED CHOICES, and the mean was previously 64 — which is
   * Richter et al.'s SUBSCRIBERS PER PUBLIC IPv4 spent on a device. That is an
   * address-multiplexing fact with no device analogue, so the coincidence read
   * as support for a number it could not support. The device-side anchors are
   * these, and they bound rather than fix:
   *
   *   * ALA Library Fact Sheet 26 (accessed 2026-08-07): 271,146 public-access
   *     internet computers in US public libraries against 340.5 million use
   *     sessions a year — 1,256 sessions per terminal per year, so distinct
   *     PEOPLE per terminal per year is order 10^2 before discounting the
   *     sessions that involve no card at all.
   *   * Casado and Freedman's proxy regime and Richter et al.'s 128
   *     subscribers at a 512-port chunk put the top of the band near 10^2 from
   *     the other direction.
   *
   * The mean sits at 24 so ONE pool spans both shared regimes — the office,
   * dorm and cafe boxes serving low tens, and the public terminal serving
   * around a hundred — rather than leaving an empty stretch between the
   * household device and the terminal. That also lands the share of device
   * identifiers above ten users near the 1.75% implied by Gomez-Boix's desktop
   * uniqueness, which the household population alone does not reach.
   *
   * The ceiling is stated in USERS because that is the quantity a source
   * bounds; cards per terminal follows from the cards-per-person law and is
   * PRINTED, not pinned.
   *
   * The pool is sized off the ROSTER, never off the window. */
  double peoplePerTerminal = 24.0;
  double maxUsersPerTerminal = 120.0;
  double terminalRowShare = 0.02;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::between("sharedDeviceP", sharedDeviceP, 0.0, 1.0); });
    r.check([&] {
      v::between("sharedGroupMaxExtra",
                 static_cast<double>(sharedGroupMaxExtra), 1.0, 64.0);
    });
    r.check([&] { v::between("secondDeviceP", secondDeviceP, 0.0, 1.0); });
    r.check([&] {
      v::between("peoplePerTerminal", peoplePerTerminal, 1.0, 100000.0);
    });
    r.check([&] {
      v::between("maxUsersPerTerminal", maxUsersPerTerminal, 1.0, 100000.0);
    });
    r.check(
        [&] { v::between("terminalRowShare", terminalRowShare, 0.0, 1.0); });
  }

  /* `runSeed` keys the two ISOLATED LANES this pass runs its data-dependent
   * halves on — the household groups and the terminal pool. It is the run
   * seed, not a draw off `rng`, so those two populations stay immune to any
   * upstream change in how many uniforms the entity stream has already spent.
   * Same construction, same reason, as the attacker-infrastructure lane. */
  [[nodiscard]] Output
  build(random::Rng &rng, time::Window window,
        const entity::person::Roster &people,
        const std::unordered_map<std::uint32_t, RingPlan> &ringPlans,
        std::uint64_t runSeed) const;
};

} // namespace PhantomLedger::synth::infra::devices
