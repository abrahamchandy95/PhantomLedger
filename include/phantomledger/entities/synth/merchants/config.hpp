#pragma once

namespace PhantomLedger::entities::synth::merchants {

struct Config {
  struct Scale {
    double corePerTenK = 120.0;
    int coreFloor = 250;
    double sizeSigma = 1.2;
  } core;

  struct Tail {
    double perTenK = 400.0;
    double share = 0.18;
    double sizeSigma = 1.8;
  } tail;

  struct Banking {
    double internalP = 0.02;
  } banking;
};

} // namespace PhantomLedger::entities::synth::merchants
