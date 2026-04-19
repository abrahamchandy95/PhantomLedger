#pragma once

#include <algorithm>
#include <cmath>

namespace PhantomLedger::entities::synth::people {

struct Fraud {
  struct Rings {
    double perTenKMean = 6.0;
    double perTenKSigma = 0.4;
  } rings;

  struct Size {
    double mu = 2.0;
    double sigma = 0.7;
    int min = 3;
    int max = 150;
  } size;

  struct Mules {
    double alpha = 2.0;
    double beta = 4.0;
    double minFrac = 0.10;
    double maxFrac = 0.70;
    double multiRingP = 0.06;
  } mules;

  struct Victims {
    double mu = 3.0;
    double sigma = 0.8;
    int min = 3;
    int max = 500;
    double repeatP = 0.10;
  } victims;

  struct Solos {
    double perTenK = 4.0;
  } solos;

  struct Limits {
    double maxParticipationP = 0.06;
    double targetIllicitP = 0.005;
  } limits;

  [[nodiscard]] int maxParticipants(int population) const {
    return static_cast<int>(limits.maxParticipationP * population);
  }

  [[nodiscard]] int expectedRingCount(int population) const {
    return std::max(0, static_cast<int>(std::round(
                           rings.perTenKMean *
                           (static_cast<double>(population) / 10000.0))));
  }

  [[nodiscard]] double expectedRingSizeMean() const {
    return std::exp(size.mu + (size.sigma * size.sigma) / 2.0);
  }

  [[nodiscard]] int expectedSoloCount(int population) const {
    return std::max(
        0, static_cast<int>(std::round(
               solos.perTenK * (static_cast<double>(population) / 10000.0))));
  }
};

struct RingPlan {
  int size = 0;
  int mules = 0;
  int victims = 0;
};

} // namespace PhantomLedger::entities::synth::people
