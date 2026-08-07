// TEMPORARY AUDIT PROBE — measures whether backdated enumeration-probe and
// non-funding-decline rows fall before the export window start, and before
// their owning Party's created_at.  Delete after reading.

#include "phantomledger/entities/infra/enumeration.hpp"
#include "phantomledger/exporter/card_fraud/export.hpp"
#include "phantomledger/exporter/common/table.hpp"
#include "phantomledger/pipeline/simulate.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/pii/pools.hpp"
#include "phantomledger/synth/personas/join_cohort.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"
#include "phantomledger/transactions/clearing/balance_book.hpp"
#include "phantomledger/transfers/channels/credit_cards/lifecycle.hpp"

#include <cstdio>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace pl = PhantomLedger;

namespace {

class Capture final : public pl::exporter::common::TableCapture {
public:
  void put(std::string_view stem, const char *data, std::size_t size) override {
    tables_[std::string(stem)].append(data, size);
  }
  [[nodiscard]] std::vector<std::string> lines(const std::string &stem) const {
    std::vector<std::string> out;
    auto it = tables_.find(stem);
    if (it == tables_.end()) {
      return out;
    }
    std::string cur;
    for (char c : it->second) {
      if (c == '\n') {
        out.push_back(cur);
        cur.clear();
      } else if (c != '\r') {
        cur.push_back(c);
      }
    }
    if (!cur.empty()) {
      out.push_back(cur);
    }
    return out;
  }

private:
  std::map<std::string, std::string> tables_;
};

std::vector<std::string> split(const std::string &line) {
  std::vector<std::string> out;
  std::string cur;
  bool quoted = false;
  for (std::size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];
    if (quoted) {
      if (c == '"') {
        if (i + 1 < line.size() && line[i + 1] == '"') {
          cur.push_back('"');
          ++i;
        } else {
          quoted = false;
        }
      } else {
        cur.push_back(c);
      }
    } else if (c == '"') {
      quoted = true;
    } else if (c == ',') {
      out.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  out.push_back(cur);
  return out;
}

pl::synth::pii::PoolSet buildPoolSet(std::uint64_t seed) {
  pl::synth::pii::PoolSet poolSet;
  const pl::synth::pii::PoolSizes sizes;
  poolSet.byCountry[pl::taxonomies::enums::toIndex(pl::locale::Country::us)] =
      pl::synth::pii::buildLocalePool(pl::locale::Country::us, sizes,
                                      static_cast<std::uint32_t>(seed));
  return poolSet;
}

void runLeg(std::uint64_t seed, int startYear, int days, int population) {
  const pl::time::Window window{
      .start = pl::time::makeTime({startYear, 1, 1}),
      .days = days,
  };

  const auto poolSet = buildPoolSet(seed);

  pl::synth::people::Fraud fraudProfile{};
  fraudProfile.rings.perTenKMean = 20.0;
  fraudProfile.rings.perTenKSigma = 0.0;
  fraudProfile.limits.targetTxnFraudP = 0.008;

  const pl::pipeline::stages::entities::EntitySynthesis entities{
      .population = population,
      .identity =
          pl::synth::pii::IdentityContext{
              .pools = &poolSet,
              .simStart = window.start,
              .localeMix = pl::synth::pii::LocaleMix::usOnly(),
          },
      .fraud = fraudProfile,
  };

  pl::clearing::BalanceRules balanceRules{};
  pl::transfers::credit_cards::LifecycleRules lifecycleRules{};

  auto rng = pl::random::Rng::fromSeed(seed);
  pl::pipeline::SimulationPipeline pipeline{rng, window, entities, seed};
  pipeline.transferStage()
      .legit()
      .window(window)
      .seed(seed)
      .openingBalanceRules(&balanceRules)
      .creditLifecycle(&lifecycleRules);
  pipeline.transferStage().fraud().profile(&fraudProfile);
  const auto result = pipeline.run();

  Capture capture;
  const auto &declined = result.transfers.ledger.posted.declined;
  pl::exporter::card_fraud::StreamingCardFraudExport sink({
      .registry = &result.holdings.accounts.registry,
      .lookup = &result.holdings.accounts.lookup,
      .membership = pl::synth::personas::join_cohort::membershipOf(
          result.people.personas, window),
      .cards = &result.holdings.creditCards,
      .merchants = &result.counterparties.merchants,
      .pgMirror = nullptr,
      .capture = &capture,
      .declined = &declined,
      .attackers = &result.infra.attackers,
  });
  sink.append(result.transfers.ledger.posted.transactions);
  sink.finish();

  pl::exporter::card_fraud::Options opts{};
  opts.piiPools = &poolSet;
  opts.window = window;
  opts.capture = &capture;
  const auto summary = pl::exporter::card_fraud::exportFromArtifacts(
      result, opts, sink.takeArtifacts());

  const auto startEpoch = pl::time::toEpochSeconds(window.start);
  const auto endEpoch = pl::time::toEpochSeconds(window.endExcl());

  // card_number -> party_id
  std::unordered_map<std::string, std::string> cardOwner;
  {
    const auto rows = capture.lines("Party_Has_Card");
    for (std::size_t i = 1; i < rows.size(); ++i) {
      const auto f = split(rows[i]);
      if (f.size() >= 2) {
        cardOwner[f[1]] = f[0];
      }
    }
  }
  // party_id -> created_at
  std::unordered_map<std::string, std::string> partyCreated;
  {
    const auto rows = capture.lines("Party");
    for (std::size_t i = 1; i < rows.size(); ++i) {
      const auto f = split(rows[i]);
      if (f.size() >= 7) {
        partyCreated[f[0]] = f[6];
      }
    }
  }
  // txn_id -> card_number
  std::unordered_map<std::string, std::string> txnCard;
  {
    const auto rows = capture.lines("Card_Send_Transaction");
    for (std::size_t i = 1; i < rows.size(); ++i) {
      const auto f = split(rows[i]);
      if (f.size() >= 2) {
        txnCard[f[0]] = f[1];
      }
    }
  }

  std::size_t payRows = 0;
  std::size_t beforeStart = 0;
  std::size_t beforeStartProbe = 0;
  std::size_t beforeStartDecline = 0;
  std::size_t beforeCreated = 0;
  std::size_t beforeCreatedProbe = 0;
  std::size_t beforeCreatedDecline = 0;
  std::size_t atOrAfterEnd = 0;
  std::int64_t minTs = 0;
  bool first = true;

  for (const auto &line : capture.lines("Payment_Transaction")) {
    const auto f = split(line);
    if (f.size() < 8 || f[0] == "id") {
      continue;
    }
    ++payRows;
    const auto ts = static_cast<std::int64_t>(std::stoll(f[4]));
    if (first || ts < minTs) {
      minTs = ts;
      first = false;
    }
    const bool isProbe = !f[0].empty() && f[0].back() == 'E';
    const bool isDecline = !f[0].empty() && f[0].back() == 'D';
    if (ts < startEpoch) {
      ++beforeStart;
      beforeStartProbe += isProbe ? 1 : 0;
      beforeStartDecline += isDecline ? 1 : 0;
    }
    if (ts >= endEpoch) {
      ++atOrAfterEnd;
    }
    auto tc = txnCard.find(f[0]);
    if (tc == txnCard.end()) {
      continue;
    }
    auto co = cardOwner.find(tc->second);
    if (co == cardOwner.end()) {
      continue;
    }
    auto pc = partyCreated.find(co->second);
    if (pc == partyCreated.end()) {
      continue;
    }
    if (f[1] < pc->second) {
      ++beforeCreated;
      beforeCreatedProbe += isProbe ? 1 : 0;
      beforeCreatedDecline += isDecline ? 1 : 0;
    }
  }

  std::printf(
      "seed=%llu start=%d days=%d pop=%d | payRows=%zu declines=%llu "
      "probes=%llu | minTs-startEpoch=%lld | beforeStart=%zu (probe %zu, "
      "decline %zu) | atOrAfterEnd=%zu | beforeCreatedAt=%zu (probe %zu, "
      "decline %zu)\n",
      static_cast<unsigned long long>(seed), startYear, days, population,
      payRows, static_cast<unsigned long long>(summary.declinedRows),
      static_cast<unsigned long long>(summary.enumerationRows),
      static_cast<long long>(minTs - startEpoch), beforeStart,
      beforeStartProbe, beforeStartDecline, atOrAfterEnd, beforeCreated,
      beforeCreatedProbe, beforeCreatedDecline);
}

} // namespace

int main() {
  runLeg(0xA11CEULL, 2000, 90, 2000);
  runLeg(0xB0B0ULL, 2005, 60, 8000);
  runLeg(0xC0FFEEULL, 2010, 30, 20000);
  return 0;
}
