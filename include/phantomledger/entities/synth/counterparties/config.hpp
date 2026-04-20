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

  /// Probability that an employer also banks at our institution.
  ///
  /// For a large retail bank (~10% deposit market share), geographic
  /// co-location with small/medium employers raises overlap above the
  /// base rate. NFIB 2023: 56% of small business owners keep personal
  /// and business accounts at the same bank. Large employers typically
  /// use commercial banking or third-party payroll processors (ADP,
  /// Paychex), so the blended in-bank rate is lower than for merchants.
  ///
  /// Estimate: ~4% of the employer pool banks at the same institution.
  double employerInBankP = 0.04;

  /// Probability that a freelance/small-business client payer also banks
  /// at our institution. Clients are geographically diverse businesses,
  /// so overlap is lower than local merchants or employers.
  ///
  /// Estimate: ~2% of the client pool banks at the same institution.
  double clientInBankP = 0.02;
};

} // namespace PhantomLedger::entities::synth::counterparties
