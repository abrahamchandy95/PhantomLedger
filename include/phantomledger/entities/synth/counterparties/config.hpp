#pragma once

namespace PhantomLedger::entities::synth::counterparties {

struct Config {
  struct Density {
    double employers = 25.0;
    double clientPayers = 250.0;
    double ownerBusinesses = 200.0;
    double brokerages = 40.0;
    double platforms = 2.0;
    double processors = 1.0;
  } perTenK;

  struct Floor {
    int employers = 5;
    int clientPayers = 25;
    int ownerBusinesses = 25;
    int brokerages = 5;
    int platforms = 2;
    int processors = 2;
  } floor;
};

} // namespace PhantomLedger::entities::synth::counterparties
