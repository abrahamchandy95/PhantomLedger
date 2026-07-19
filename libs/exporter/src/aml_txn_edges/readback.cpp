#include "phantomledger/exporter/aml_txn_edges/readback.hpp"

#include "phantomledger/exporter/sinks/txn_readback.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstddef>
#include <utility>

namespace PhantomLedger::exporter::aml_txn_edges::readback {

namespace pg = ::PhantomLedger::postgres;

derived::Bundle buildBundle(const pipeline::People &people,
                            const pipeline::Holdings &holdings,
                            std::span<const aml::sar::SarRecord> sars,
                            pg::Connection &conn, const std::string &table) {
  derived::Bundle b;

  // The corpus path's min/max scan, done server-side; same empty-corpus
  // fallback (derived::kFallbackEpoch).
  const auto bounds = pg::queryStreamBounds(conn, table);
  if (bounds.rows == 0) {
    derived::applySimWindow(b, derived::kFallbackEpoch,
                            derived::kFallbackEpoch);
  } else {
    derived::applySimWindow(b, bounds.minTs, bounds.maxTs);
  }

  // Same expectedRows as the corpus path (its span size), so reserves —
  // and the burst maps' bucket trajectories — match exactly.
  derived::TxnSweep sweep(b.simEndEpoch,
                          static_cast<std::size_t>(bounds.rows));

  // One ordered scan; observe() reads only the losslessly decoded
  // fields (keys, amount, ts, fraud flag, channel), so the
  // reconstructed row is equivalent to the corpus row for every
  // derived purpose.
  pg::TransactionScan scan{conn, table};
  pg::StreamTxnRow row;
  transactions::Transaction tx{};
  while (scan.next(row)) {
    tx.source = row.source;
    tx.target = row.target;
    tx.amount = row.amount;
    tx.timestamp = row.timestamp;
    tx.fraud.flag = row.fraudFlag;
    tx.session.channel = row.channel;
    sweep.observe(tx, static_cast<std::size_t>(row.rowSeq));
  }

  return derived::finishBundle(std::move(b), std::move(sweep), people,
                               holdings, sars);
}

} // namespace PhantomLedger::exporter::aml_txn_edges::readback
