#include "phantomledger/app/options.hpp"
#include "phantomledger/encoding/render.hpp"
#include "phantomledger/exporter/mule_temporal/schema.hpp"
#include "phantomledger/exporter/mule_temporal/streaming.hpp"
#include "phantomledger/exporter/mule_temporal/zelle_policy.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace pl = PhantomLedger;
namespace mt = pl::exporter::mule_temporal;
using Row = std::vector<std::string>;

struct Capture : pl::exporter::common::TableCapture {
  std::map<std::string, std::string> tables;
  void put(std::string_view stem, const char *data, std::size_t n) override {
    tables[std::string{stem}].append(data, n);
  }
  std::vector<Row> read(const std::string &table) const {
    std::vector<Row> out;
    std::string_view text = tables.at(table);
    while (!text.empty()) {
      auto end = text.find('\n');
      auto line = text.substr(0, end);
      text = end == text.npos ? std::string_view{} : text.substr(end + 1);
      if (!line.empty() && line.back() == '\r')
        line.remove_suffix(1);
      if (line.empty())
        continue;
      Row row;
      while (true) {
        const auto comma = line.find(',');
        row.emplace_back(line.substr(0, comma));
        if (comma == line.npos)
          break;
        line.remove_prefix(comma + 1);
      }
      out.push_back(std::move(row));
    }
    PL_CHECK(!out.empty());
    out.erase(out.begin());
    return out;
  }
};

struct Fixture {
  pl::time::Window window{pl::time::makeTime({2019, 1, 1}), 6};
  std::int64_t start = pl::time::toEpochSeconds(window.start);
  pl::entity::Key a = pl::entity::makeKey(pl::entity::Role::account,
                                          pl::entity::Bank::internal, 1);
  pl::entity::Key b = pl::entity::makeKey(pl::entity::Role::account,
                                          pl::entity::Bank::internal, 2);
  pl::entity::Key external = pl::entity::makeKey(pl::entity::Role::family,
                                                 pl::entity::Bank::external, 3);
  pl::devices::Identity shared = pl::devices::Identity::ring(77);
  pl::network::Ipv4 address = pl::network::Ipv4::pack(10, 2, 3, 4);
  pl::entity::account::Registry registry;
  pl::entity::pii::Roster pii;
  pl::synth::infra::devices::Output devices;
  pl::synth::infra::ips::Output ips;
  pl::entity::parties::relocation::Schedule relocation;
  pl::synth::pii::Membership membership;
  std::vector<pl::transactions::Transaction> txns;

  Fixture() {
    registry.records = {
        {a, 1, pl::entity::account::bit(pl::entity::account::Flag::mule)},
        {b, 2, 0},
        {external, 0,
         pl::entity::account::bit(pl::entity::account::Flag::external)}};
    pii.records.resize(2);
    membership = {
        window,
        {0, 2},
        {start + 5 * 86400, std::numeric_limits<std::int64_t>::max()}};
    relocation = {{0, 2, 3}, {{start, 1}, {start + 3 * 86400, 2}, {start, 1}}};
    devices.records.push_back(
        {shared, pl::synth::infra::DeviceKind::android, true});
    devices.usages = {
        {1, shared, window.start, window.start, true},
        {1, shared, window.start + pl::time::Days{3},
         window.start + pl::time::Days{3}, true},
        {2, shared, window.start + pl::time::Days{2},
         window.start + pl::time::Days{4}, true},
        // Session-only infrastructure must not acquire a registry association.
        {1, pl::devices::Identity::person(1, 5), window.start, window.endExcl(),
         false}};
    ips.usages = {
        {1, address, window.start, window.start + pl::time::Days{1}, true}};
    for (int i = 0; i < 20; ++i) {
      pl::transactions::Transaction tx;
      tx.source = a;
      tx.target = external;
      tx.timestamp = start + i;
      tx.amount = i == 0 ? 0.0 : 100.0;
      tx.fraud.flag = i % 2;
      tx.session = {shared, address,
                    pl::channels::tag(pl::channels::Legit::p2p)};
      txns.push_back(tx);
    }
    auto tx = txns.back();
    tx.timestamp = start + 86400; // exactly at device closure
    tx.session.channel = pl::channels::tag(pl::channels::Legit::salary);
    tx.amount = std::numeric_limits<double>::quiet_NaN();
    txns.push_back(tx);
    tx.timestamp = start + 2 * 86400; // exactly at new party/device enrollment
    tx.source = b;
    tx.target = a;
    tx.amount = 200;
    txns.push_back(tx);
    tx.timestamp = start + 3 * 86400; // repeat tenure starts
    tx.source = a;
    tx.target = external;
    txns.push_back(tx);
    tx.timestamp = start + 4 * 86400; // repeat tenure ends
    txns.push_back(tx);
  }

  mt::StreamingMuleTemporalExport::Config config(Capture &capture,
                                                 int days = 6) const {
    auto w = window;
    w.days = days;
    return {.registry = &registry,
            .pii = &pii,
            .relocation = &relocation,
            .devices = &devices,
            .ips = &ips,
            .membership = membership,
            .window = w,
            .seed = 1,
            .capture = &capture};
  }
};

struct Event {
  std::uint64_t seq;
  std::uint64_t ms;
  bool zelle;
};

std::map<std::string, Event> events(const Capture &capture) {
  std::map<std::string, Event> result;
  for (const auto &row : capture.read("Payment_Transaction"))
    PL_CHECK(result
                 .emplace(row[0], Event{std::stoull(row[7]),
                                        std::stoull(row[6]), false})
                 .second);
  for (const auto &row : capture.read("Zelle_Transfer"))
    PL_CHECK(result
                 .emplace(row[0],
                          Event{std::stoull(row[3]), std::stoull(row[2]), true})
                 .second);
  return result;
}

void checkZellePolicy() {
  // Statistical gates test the measured denominators, not an all-A2A share.
  int senders = 0, choices = 0;
  for (int i = 0; i < 10000; ++i) {
    senders +=
        mt::zelle::draw(42, "zelle-sender", "consumer:" + std::to_string(i)) <
        mt::zelle::kSenderProbability;
    choices += mt::zelle::draw(42, "zelle-payment", std::to_string(i)) <
               mt::zelle::kPaymentProbability;
  }
  PL_CHECK(senders > 3000 && senders < 3300);
  PL_CHECK(choices > 5400 && choices < 5750);
  mt::zelle::SendingWindow cap;
  PL_CHECK(!cap.accept(0, 0, false));
  PL_CHECK(!cap.accept(0, 3500.01, false));
  PL_CHECK(!cap.accept(0, std::numeric_limits<double>::quiet_NaN(), false));
  PL_CHECK(cap.accept(0, 3500, false));
  PL_CHECK(!cap.accept(86399, 1, false));
  PL_CHECK(cap.accept(86400, 3500, false));
  for (int day = 2; day < 5; ++day)
    PL_CHECK(cap.accept(day * 86400, 3500, false));
  PL_CHECK(cap.accept(5 * 86400, 2500, false));
  PL_CHECK(!cap.accept(6 * 86400, 0.01, false));
  PL_CHECK(cap.accept(30 * 86400, 3500, false));
  mt::zelle::SendingWindow business;
  PL_CHECK(business.accept(0, 15000, true));
  PL_CHECK(!business.accept(1, .01, true));
  for (int day = 1; day < 4; ++day)
    PL_CHECK(business.accept(day * 86400, 15000, true));
  PL_CHECK(!business.accept(4 * 86400, 1, true));

  Fixture f;
  Capture crossBank;
  auto cfg = f.config(crossBank);
  const auto externalProfile =
      "consumer:" + std::string{pl::encoding::format(f.external).view()};
  while (mt::zelle::draw(cfg.seed, "zelle-sender", "consumer:1") >=
             mt::zelle::kSenderProbability ||
         mt::zelle::draw(cfg.seed, "zelle-sender", externalProfile) >=
             mt::zelle::kSenderProbability)
    ++cfg.seed;
  std::vector<pl::transactions::Transaction> txns;
  for (int i = 0; i < 90; ++i) {
    auto tx = f.txns[1];
    tx.timestamp = f.start + i * 3600;
    if (i % 2 == 0)
      std::swap(tx.source, tx.target);
    txns.push_back(tx);
  }
  mt::StreamingMuleTemporalExport sink{cfg};
  sink.append(txns);
  sink.finish();
  PL_CHECK(!crossBank.read("Payment_Transaction").empty());
  PL_CHECK(!crossBank.read("Zelle_Transfer").empty());
  std::string extId;
  for (const auto &r : crossBank.read("Account"))
    if (r[2] == "True")
      extId = r[0];
  PL_CHECK(!extId.empty());
  for (const auto *role : {"Transfer_From_Account", "Transfer_To_Account"}) {
    bool found = false;
    for (const auto &r : crossBank.read(role))
      found |= r[1] == extId;
    PL_CHECK(found);
  }
  PL_CHECK_EQ(crossBank.read("Transfer_From_Token").size(),
              crossBank.read("Zelle_Transfer").size());
  PL_CHECK_EQ(crossBank.read("Transfer_To_Token").size(),
              crossBank.read("Zelle_Transfer").size());
  for (const auto &r : crossBank.read("Party_Owns_Account"))
    PL_CHECK(r[1] != extId);
  // Every token binding is visible at participation; closed customer tokens
  // expire in the same common sequence domain as scheduled associations.
  for (const auto *role : {"Transfer_From_Token", "Transfer_To_Token"})
    for (const auto &event : crossBank.read(role)) {
      bool found = false;
      for (const auto &binding : crossBank.read("Token_Bound_To_Account"))
        found |= binding[0] == event[1] &&
                 mt::visible(std::stoull(binding[2]), std::stoull(binding[3]),
                             std::stoull(event[3]));
      PL_CHECK(found);
    }
  Capture foreign;
  f.pii.records[0].country = pl::locale::Country::gb;
  cfg.capture = &foreign;
  mt::StreamingMuleTemporalExport foreignSink{cfg};
  foreignSink.append(txns);
  foreignSink.finish();
  PL_CHECK(foreign.read("Zelle_Transfer").empty());

  f.pii.records[0].country = pl::locale::Country::us;
  Capture excluded;
  cfg.capture = &excluded;
  const pl::channels::Tag tags[] = {
      pl::channels::tag(pl::channels::Legit::salary),
      pl::channels::tag(pl::channels::Rent::ach),
      pl::channels::tag(pl::channels::Legit::selfTransfer),
      pl::channels::tag(pl::channels::Legit::cardPurchase),
      pl::channels::tag(pl::channels::Family::inheritance),
      pl::channels::tag(pl::channels::Deposit::checkDeposit)};
  for (std::size_t i = 0; i < txns.size(); ++i)
    txns[i].session.channel = tags[i % std::size(tags)];
  mt::StreamingMuleTemporalExport excludedSink{cfg};
  excludedSink.append(txns);
  excludedSink.finish();
  PL_CHECK(excluded.read("Zelle_Transfer").empty());
}

int main() {
  checkZellePolicy();
  PL_CHECK(pl::app::parseUseCase("mule-temporal") ==
           pl::app::UseCase::muleTemporal);
  PL_CHECK(pl::app::name(pl::app::UseCase::muleTemporal) == "mule-temporal");
  PL_CHECK(!mt::visible(0, 0, 1));
  PL_CHECK(mt::visible(10, 20, 10));
  PL_CHECK(!mt::visible(10, 20, 20));
  PL_CHECK(mt::visible(10, 0, 100));
  Fixture fixture;
  Capture full, chunks, prefix;
  {
    mt::StreamingMuleTemporalExport sink{fixture.config(full)};
    sink.append(fixture.txns);
    sink.finish();
    sink.finish();
    PL_CHECK_EQ(sink.rowsWritten(), fixture.txns.size());
    PL_CHECK_THROWS(sink.append({}));
  }
  {
    mt::StreamingMuleTemporalExport sink{fixture.config(chunks)};
    for (const auto &tx : fixture.txns)
      sink.append({&tx, 1});
    sink.finish();
  }
  PL_CHECK(full.tables == chunks.tables);
  PL_CHECK_EQ(full.tables.size(), 27U);
  for (const auto &table : mt::schema::kTables) {
    const std::string stem{table.filename.substr(0, table.filename.size() - 4)};
    for (const auto &row : full.read(stem))
      PL_CHECK_EQ(row.size(), table.header.size());
  }
  const auto allEvents = events(full);
  PL_CHECK_EQ(allEvents.size(), fixture.txns.size());
  PL_CHECK(!full.read("Zelle_Transfer").empty());
  PL_CHECK(!full.read("Payment_Transaction").empty());
  std::uint64_t previous = 0;
  for (const auto &[id, e] : allEvents) {
    PL_CHECK(e.seq > previous);
    previous = e.seq;
  }

  // Referential integrity, immutable entity clocks, every participation clock
  // identical to its parent, and exactly one sender/receiver per payment.
  std::map<std::string, std::uint64_t> firstSeen;
  std::map<std::string, std::string> entityTable;
  const std::map<std::string, std::size_t> entitySeq{
      {"Party", 2}, {"Account", 3}, {"Device", 2},
      {"IP", 1},    {"Token", 1},   {"Address", 1}};
  for (const auto &[table, index] : entitySeq) {
    for (const auto &row : full.read(table)) {
      PL_CHECK(firstSeen.emplace(row[0], std::stoull(row[index])).second);
      entityTable[row[0]] = table;
      PL_CHECK(row[0].find("77") != 0); // no role-bearing ring prefix
    }
  }
  std::map<std::pair<std::string, std::string>, int> participationCount;
  std::set<std::uint64_t> associationSeqs;
  for (const auto &table : mt::schema::kTables) {
    if (table.header.size() != 4 || table.header[0] != "from_id")
      continue;
    const std::string stem{table.filename.substr(0, table.filename.size() - 4)};
    for (const auto &row : full.read(stem)) {
      const auto &e = allEvents.at(row[0]);
      PL_CHECK_EQ(std::stoull(row[2]), e.ms);
      PL_CHECK_EQ(std::stoull(row[3]), e.seq);
      PL_CHECK(firstSeen.at(row[1]) <= e.seq);
      PL_CHECK((stem.starts_with("Transfer_")) == e.zelle);
      const auto endpoint = stem.substr(stem.find_last_of('_') + 1);
      PL_CHECK_EQ(entityTable.at(row[1]), endpoint);
      ++participationCount[{row[0], stem}];
    }
  }
  for (const auto &[id, e] : allEvents) {
    const std::string stem = e.zelle ? "Transfer_" : "Transaction_";
    PL_CHECK_EQ(participationCount[std::make_pair(id, stem + "From_Account")],
                1);
    PL_CHECK_EQ(participationCount[std::make_pair(id, stem + "To_Account")], 1);
  }
  for (const auto &table : mt::schema::kTables) {
    if (table.header.size() != 6 || table.header[0] != "from_id")
      continue;
    const std::string stem{table.filename.substr(0, table.filename.size() - 4)};
    for (const auto &row : full.read(stem)) {
      const auto from = std::stoull(row[2]), to = std::stoull(row[3]);
      PL_CHECK(from > 0 && (to == 0 || from < to));
      PL_CHECK(firstSeen.at(row[0]) <= from && firstSeen.at(row[1]) <= from);
      PL_CHECK(associationSeqs.insert(from).second);
      if (to != 0)
        PL_CHECK(associationSeqs.insert(to).second);
    }
  }
  for (const auto &[id, e] : allEvents)
    PL_CHECK(!associationSeqs.contains(e.seq));

  // Repeated tenure: one pair, a gap, a return; the event at the first end
  // still references the device but must not see an ownership association.
  const auto sender = full.read("Transaction_From_Account").front()[1];
  const auto device = full.read("Transaction_Used_Device").front()[1];
  std::vector<Row> repeated;
  for (const auto &r : full.read("Account_Uses_Device"))
    if (r[0] == sender && r[1] == device)
      repeated.push_back(r);
  PL_CHECK_EQ(repeated.size(), 2U);
  std::ranges::sort(repeated, {},
                    [](const Row &r) { return std::stoull(r[2]); });
  const auto boundary = allEvents.at("T000000000021").seq;
  PL_CHECK(!mt::visible(std::stoull(repeated[0][2]),
                        std::stoull(repeated[0][3]), boundary));
  PL_CHECK(!mt::visible(std::stoull(repeated[1][2]),
                        std::stoull(repeated[1][3]), boundary));
  PL_CHECK(mt::visible(std::stoull(repeated[1][2]), std::stoull(repeated[1][3]),
                       allEvents.at("T000000000023").seq));

  // Zero is a present amount; NaN explicitly renders as missing, never a
  // plausible zero. Labels are supervision after the event in the same clock.
  bool foundMissing = false;
  for (const auto &r : full.read("Payment_Transaction")) {
    if (r[0] == "T000000000021") {
      PL_CHECK_EQ(r[8], "False");
      foundMissing = true;
    }
  }
  PL_CHECK(foundMissing);
  for (const auto &r : full.read("Zelle_Transfer")) {
    PL_CHECK_EQ(r[9], "True");
    PL_CHECK(std::stoull(r[10]) > std::stoull(r[3]));
    PL_CHECK_EQ(r[11], r[2]);
  }

  // Prefix experiment: future endpoints, enrollment, closure and relocation
  // must not change any earlier entity metadata, event or visible tenure.
  {
    mt::StreamingMuleTemporalExport sink{fixture.config(prefix, 1)};
    sink.append(std::span{fixture.txns}.first(20));
    sink.finish();
  }
  PL_CHECK_EQ(prefix.read("Party").size(), 1U); // joiner is still in the future
  PL_CHECK_EQ(full.read("Party").size(), 2U);
  for (const auto &[table, index] : entitySeq) {
    const auto complete = full.read(table);
    for (const auto &r : prefix.read(table))
      PL_CHECK(std::ranges::find(complete, r) != complete.end());
  }
  for (const auto &[name, bytes] : prefix.tables) {
    const auto pr = prefix.read(name), fr = full.read(name);
    if (name == "Payment_Transaction" || name == "Zelle_Transfer" ||
        name.starts_with("Transfer_") || name.starts_with("Transaction_"))
      for (const auto &r : pr)
        PL_CHECK(std::ranges::find(fr, r) != fr.end());
  }
  for (const auto &[id, e] : events(prefix)) {
    PL_CHECK_EQ(allEvents.at(id).seq, e.seq);
    for (const auto &table : mt::schema::kTables) {
      if (table.header.size() != 6 || table.header[0] != "from_id")
        continue;
      const std::string name{
          table.filename.substr(0, table.filename.size() - 4)};
      auto visiblePairs = [&](const Capture &cap) {
        std::set<Row> rows;
        for (auto r : cap.read(name)) {
          if (mt::visible(std::stoull(r[2]), std::stoull(r[3]), e.seq)) {
            r[3] = ""; // future closure is a predicate, never a feature
            rows.insert(r);
          }
        }
        return rows;
      };
      PL_CHECK(visiblePairs(full) == visiblePairs(prefix));
    }
  }

  // Account labels come from the assigned account role, not payment truth.
  const auto labeledAccounts = full.read("Account");
  std::size_t positives = 0;
  for (const auto &r : labeledAccounts) {
    PL_CHECK(r[5] == "0" || r[5] == "1");
    positives += r[5] == "1";
    if (r[2] == "True")
      PL_CHECK_EQ(r[5], "0");
  }
  PL_CHECK_EQ(positives, 1U);
  Capture changedRole;
  fixture.registry.records[0].flags =
      pl::entity::account::bit(pl::entity::account::Flag::victim) |
      pl::entity::account::bit(pl::entity::account::Flag::fraud);
  mt::StreamingMuleTemporalExport roleChanged{fixture.config(changedRole)};
  roleChanged.append(fixture.txns);
  roleChanged.finish();
  for (const auto &[name, bytes] : full.tables)
    if (name != "Account")
      PL_CHECK_EQ(bytes, changedRole.tables.at(name));
  const auto roleAccounts = changedRole.read("Account");
  PL_CHECK_EQ(roleAccounts.size(), labeledAccounts.size());
  for (std::size_t i = 0; i < roleAccounts.size(); ++i) {
    PL_CHECK(std::equal(labeledAccounts[i].begin(),
                        labeledAccounts[i].begin() + 5,
                        roleAccounts[i].begin()));
    PL_CHECK_EQ(roleAccounts[i][5], "0");
  }
  fixture.registry.records[0].flags =
      pl::entity::account::bit(pl::entity::account::Flag::mule);

  const auto truth = full.read("Zelle_Transfer");

  // Changing hidden fraud truth may change only supervision, never node
  // attributes, graph structure, rail assignment or event/association clocks.
  Capture changedTruth;
  auto changedTxns = fixture.txns;
  for (auto &tx : changedTxns) {
    tx.fraud.flag = tx.fraud.flag == 0 ? 1 : 0;
    tx.fraud.ringId = 999;
  }
  mt::StreamingMuleTemporalExport relabeled{fixture.config(changedTruth)};
  relabeled.append(changedTxns);
  relabeled.finish();
  for (const auto &[name, bytes] : full.tables)
    if (name != "Zelle_Transfer")
      PL_CHECK_EQ(bytes, changedTruth.tables.at(name));
  const auto changed = changedTruth.read("Zelle_Transfer");
  for (std::size_t i = 0; i < truth.size(); ++i) {
    PL_CHECK(
        std::equal(truth[i].begin(), truth[i].begin() + 8, changed[i].begin()));
    PL_CHECK(truth[i][8] != changed[i][8]);
    PL_CHECK(std::equal(truth[i].begin() + 9, truth[i].end(),
                        changed[i].begin() + 9));
  }

  Capture invalid;
  mt::StreamingMuleTemporalExport guarded{fixture.config(invalid)};
  auto bad = fixture.txns.front();
  bad.source.number = 99999;
  PL_CHECK_THROWS(guarded.append({&bad, 1}));
  bad = fixture.txns.front();
  bad.timestamp = fixture.start - 1;
  PL_CHECK_THROWS(guarded.append({&bad, 1}));
  bad = fixture.txns.front();
  bad.amount = std::numeric_limits<double>::infinity();
  PL_CHECK_THROWS(guarded.append({&bad, 1}));
  bad.amount = -1;
  PL_CHECK_THROWS(guarded.append({&bad, 1}));
  bad = fixture.txns.front();
  bad.timestamp = pl::time::toEpochSeconds(fixture.window.endExcl());
  PL_CHECK_THROWS(guarded.append({&bad, 1}));
  guarded.append(std::span{fixture.txns}.subspan(10, 1));
  PL_CHECK_THROWS(guarded.append(std::span{fixture.txns}.first(1)));
  return 0;
}
