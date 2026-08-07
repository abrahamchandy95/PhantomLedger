#include "phantomledger/app/reporting.hpp"
#include "phantomledger/diagnostics/memory.hpp"

#include <print>

namespace PhantomLedger::app::reporting {

PhaseMonitor::PhaseMonitor() : start_(Clock::now()), last_(start_) {}

void PhaseMonitor::mark(std::string_view label) {
  const auto now = Clock::now();
  const auto deltaMs =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - last_)
          .count();
  const auto totalMs =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - start_)
          .count();
  std::println(
      stderr, "[phase] {:<24}  +{:9.1f}s  (total {:9.1f}s)  peakRSS={:9.1f} MB",
      label, static_cast<double>(deltaMs) / 1000.0,
      static_cast<double>(totalMs) / 1000.0, diagnostics::memory::peakRssMB());
  last_ = now;
}

void windowedSummary(
    std::uint32_t peopleCount, std::size_t accountCount,
    const pipeline::stages::transfers::WindowedRunResult &transfers,
    std::uint64_t streamRows) {
  const auto &summary = transfers.summary;

  const double ratio =
      (streamRows == 0)
          ? 0.0
          : static_cast<double>(summary.phaseB.fraudRows) / streamRows;

  std::println("People: {}  Accounts: {}", peopleCount, accountCount);
  std::println("Transactions: {}  Fraud rows: {} ({:.4f}%)  candidates "
               "L={}  card events={}",
               streamRows, summary.phaseB.fraudRows, ratio * 100.0,
               summary.phaseA.candidateRows, summary.phaseA.cardEvents);
  std::println("Candidate spool: {} rows, {:.1f} MiB on disk",
               transfers.spoolRows,
               static_cast<double>(transfers.spoolBytes) / (1024.0 * 1024.0));
  std::println("Posted book hash: 0x{:x}", transfers.postedBookHash);
}

void amlSummary(const exporter::aml::Summary &summary) {
  const double ratio = (summary.totalTxnCount == 0)
                           ? 0.0
                           : static_cast<double>(summary.illicitTxnCount) /
                                 summary.totalTxnCount;

  std::println("AML export complete");
  std::println("  Customers:        {}", summary.customerCount);
  std::println("  Accounts:         {}", summary.internalAccountCount);
  std::println("  Counterparties:   {}", summary.counterpartyCount);
  std::println("  Transactions:     {}  (illicit: {}, {:.4f}%)",
               summary.totalTxnCount, summary.illicitTxnCount, ratio * 100.0);
  std::println("  Fraud rings:      {}", summary.fraudRingCount);
  std::println("  Solo fraudsters:  {}", summary.soloFraudCount);
  std::println("  SARs filed:       {}", summary.sarsFiledCount);
}

void amlTxnEdgesSummary(const exporter::aml_txn_edges::Summary &summary) {
  const double ratio = (summary.totalTxnCount == 0)
                           ? 0.0
                           : static_cast<double>(summary.illicitTxnCount) /
                                 summary.totalTxnCount;

  std::println("AML (txn-edges) export complete");
  std::println("  Customers:        {}", summary.customerCount);
  std::println("  Accounts:         {}", summary.internalAccountCount);
  std::println("  Counterparties:   {}", summary.counterpartyCount);
  std::println("  Transactions:     {}  (illicit: {}, {:.4f}%)",
               summary.totalTxnCount, summary.illicitTxnCount, ratio * 100.0);
  std::println("  Fraud rings:      {}", summary.fraudRingCount);
  std::println("  Solo fraudsters:  {}", summary.soloFraudCount);
  std::println("  SARs filed:       {}", summary.sarsFiledCount);
  std::println("  Alerts:           {}  (CTRs: {})", summary.alertCount,
               summary.ctrCount);
  std::println("  Cases:            {}  (businesses: {})", summary.caseCount,
               summary.businessCount);
  std::println("  Flow-agg edges:   {}  (link-comm: {})",
               summary.flowAggEdgeCount, summary.linkCommEdgeCount);
}

void cardFraudSummary(const exporter::card_fraud::Summary &summary) {
  const double ratio = (summary.viewRows == 0)
                           ? 0.0
                           : static_cast<double>(summary.fraudViewRows) /
                                 static_cast<double>(summary.viewRows);

  std::println("card-fraud export complete (TF_GNN_v3 loaded attributes)");
  std::println("  Payment txns:    {} of {} corpus rows  (fraud: {}, {:.4f}%)",
               summary.viewRows, summary.totalRows, summary.fraudViewRows,
               ratio * 100.0);
  /* The payment table is settled rows PLUS these, so print the addend rather
   * than leave the reader to subtract. A zero here on a card-fraud run means
   * no authorization was ever refused, which is a defect, not a quiet corpus. */
  std::println("  Declined auths:  {} additional rows (labels withheld)",
               summary.declinedRows);
  std::println("  Card-testing:    {} probe rows (labels withheld)",
               summary.enumerationRows);
  std::println("  Cards:           {}", summary.cardCount);
  std::println("  Merchants:       {}", summary.merchantCount);
  std::println("  Parties:         {}", summary.partyCount);
  std::println("  Geo:             {} cities, {} states, {} zipcodes",
               summary.cityCount, summary.stateCount, summary.zipcodeCount);
}

} // namespace PhantomLedger::app::reporting
