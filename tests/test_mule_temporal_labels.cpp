//
// tests/test_mule_temporal_labels.cpp
//
// mule-label-contract-2026-10: THE ACCOUNT TABLE CARRIES MULEPATTERNLEARNER'S
// LABEL CONTRACT.
//
// The mule-temporal Account table had six columns, so a graph loaded from it
// lacked nine of the fifteen fields of MulePatternLearner's (MPL's) Account
// label contract, and every mule's mule_ring_id stayed at its default -1
// although the simulator puts every mule in a ring. The table now carries
// all fifteen, in MPL's load order (load_accounts, ACCOUNT_LOAD_COLUMNS).
// The durable record is the `# AMENDMENT: mule-label-contract-2026-10`
// section of docs/fraud_model_audit.md. test_mule_temporal checks each rule
// on a hand-built fixture; this gate checks them on a real world, through
// the windowed engine (pop 600, 2019, seed 20261004, the scaled fraud
// profile, 10 rings).
//
//   A  NOTHING ELSE MOVES. Pinned on the pre-round build: the bytes of the
//      26 other tables, and the Account table cut to its first six columns,
//      which is byte-identical to the pre-round Account table.
//   B  THE HEADER is the contract's, column for column, and the Account
//      vertex of schemas/mule_temporal.gsql stores those columns at
//      $0..$4, $6..$14, $5, the positions a loading job must map.
//   C  THE TRUTH AND THE RING. is_mule is the account's mule flag; a mule's
//      mule_ring_id is its home ring (the ring whose members hold its owner),
//      every mule here has one, and every other account has -1. Against the
//      ring ids the laundering rows carry: every such row touching a mule
//      names a ring that lists the mule among its mules, and a mule in one
//      ring has that ring's id on all of them.
//   D  THE CLOCKS. An internal account's clocks are its first observation,
//      except a mule's, which are its first laundering payment when the
//      export holds one (located independently, from the rows and the event
//      tables); availability equals effectiveness. An external account has
//      zero clocks and no source.
//   E  MPL'S CONTRACT CHECK AND REVEAL GUARD, replayed on the rows. As
//      loaded, no internal label is known and none is revealed, so MPL's
//      one-time reveal runs, and the only violation validate_label_contract
//      counts is invalid_unknown, once per mule (a mule ships unknown with
//      its truth and ring, as the reveal expects). After the reveal writes
//      what label_reveal.gsql writes, every count is zero.
//

#include "phantomledger/encoding/render.hpp"
#include "phantomledger/exporter/mule_temporal/schema.hpp"
#include "phantomledger/exporter/mule_temporal/streaming.hpp"
#include "phantomledger/primitives/crypto/blake2b.hpp"
#include "phantomledger/synth/personas/join.hpp"
#include "phantomledger/taxonomies/channels/predicates.hpp"

#include "window_leg_support.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <format>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace pl = ::PhantomLedger;
namespace mt = pl::exporter::mule_temporal;
using Cells = std::vector<std::string_view>;

constexpr std::uint64_t kSeed = 20261004;
constexpr std::int32_t kPopulation = 600;

// A. Measured on the pre-round build (HEAD dac2d64, the same world and rows).
struct Pin {
  std::string_view table;
  std::uint64_t digest;
  std::size_t bytes;
};
constexpr Pin kPins[]{
    {"Account_Uses_Device", 0x2e4ba4fb99c73bf0ULL, 185178},
    {"Address", 0xbdca3e0fc0bdaf27ULL, 37261},
    {"Device", 0xad871cf8470b7701ULL, 70678},
    {"IP", 0x7f50309afca2de34ULL, 218737},
    {"Party", 0x571f64040ade1687ULL, 37251},
    {"Party_Has_Address", 0x7a59f915843e1b8dULL, 65433},
    {"Party_Owns_Account", 0x9c8b0953e52d6099ULL, 95467},
    {"Party_Uses_Device", 0xf560aab0628ad304ULL, 113690},
    {"Party_Uses_IP", 0x813522cc913d7015ULL, 343513},
    {"Party_Uses_Token", 0x9e0a73e7634f98f2ULL, 50886},
    {"Payment_Transaction", 0x5c1b5f21c983b711ULL, 25535486},
    {"Token", 0xaef1c498c39f7a48ULL, 39735},
    {"Token_Bound_To_Account", 0xc36a4327d34337baULL, 51297},
    {"Transaction_From_Account", 0x61e712c5abcbe982ULL, 21263954},
    {"Transaction_From_Token", 0xfbf278336786edeeULL, 37},
    {"Transaction_To_Account", 0x98659c86ccbbaf2bULL, 21263954},
    {"Transaction_To_Token", 0xfbf278336786edeeULL, 37},
    {"Transaction_Used_Device", 0xb4c4f6f0bcc1eb39ULL, 19081275},
    {"Transaction_Used_IP", 0x26fc6368c9d527d7ULL, 19081064},
    {"Transfer_From_Account", 0x13d2fe985df739e4ULL, 241521},
    {"Transfer_From_Token", 0xd5e6e35f8b90c2deULL, 241521},
    {"Transfer_To_Account", 0x0fbc174f2634c7d9ULL, 241521},
    {"Transfer_To_Token", 0x39ebc2c9c260839cULL, 241521},
    {"Transfer_Used_Device", 0xb3200fdad68c9dc6ULL, 241450},
    {"Transfer_Used_IP", 0x88a2c6c4f5398f0eULL, 241521},
    {"Zelle_Transfer", 0x371d89c7e4e38ad5ULL, 364105},
};
// The whole pre-round Account table (six columns).
constexpr std::uint64_t kPreRoundAccount = 0x954256dfa302dbf7ULL;
constexpr std::size_t kPreRoundAccountBytes = 277088;

// Contract positions (MPL's ACCOUNT_LOAD_COLUMNS).
constexpr std::size_t kExternal = 2, kFirstSeq = 3, kFirstMs = 4, kIsMule = 5,
                      kKnown = 6, kMasked = 7, kPu = 8, kEffSeq = 9,
                      kEffMs = 10, kAvailSeq = 11, kAvailMs = 12, kRing = 13,
                      kSource = 14;

struct Capture : pl::exporter::common::TableCapture {
  std::map<std::string, std::string> tables;
  void put(std::string_view stem, const char *data, std::size_t n) override {
    tables[std::string{stem}].append(data, n);
  }
};

void check(bool ok, const char *what) {
  if (!ok) {
    std::fprintf(stderr, "\n[FAIL] %s\n", what);
    std::abort();
  }
}

std::uint64_t fnv1a(std::string_view bytes) {
  std::uint64_t h = 0xcbf29ce484222325ULL;
  for (const unsigned char c : bytes) {
    h ^= c;
    h *= 0x100000001b3ULL;
  }
  return h;
}

// Calls `row` with the cells of every data line (the header is skipped).
void eachRow(std::string_view text,
             const std::function<void(const Cells &)> &row) {
  bool header = true;
  Cells cells;
  while (!text.empty()) {
    const auto end = text.find("\r\n");
    auto line = text.substr(0, end);
    text = end == text.npos ? std::string_view{} : text.substr(end + 2);
    if (std::exchange(header, false))
      continue;
    cells.clear();
    while (true) {
      const auto comma = line.find(',');
      cells.push_back(line.substr(0, comma));
      if (comma == line.npos)
        break;
      line.remove_prefix(comma + 1);
    }
    row(cells);
  }
}

// Every line, header included, cut to its first `columns` cells.
std::string projection(std::string_view text, std::size_t columns) {
  std::string out;
  while (!text.empty()) {
    const auto end = text.find("\r\n");
    const auto line = text.substr(0, end);
    text = end == text.npos ? std::string_view{} : text.substr(end + 2);
    std::size_t cut = line.npos;
    for (std::size_t i = 0, from = 0; i < columns; ++i) {
      cut = line.find(',', from);
      if (cut == line.npos)
        break;
      from = cut + 1;
    }
    out.append(line.substr(0, cut));
    out += "\r\n";
  }
  return out;
}

std::uint64_t number(std::string_view cell) {
  return std::stoull(std::string{cell});
}

// The Account vertex's attributes in storage order, read from the DDL.
std::vector<std::string> accountStorageOrder() {
  std::ifstream in{PL_MULE_TEMPORAL_GSQL};
  check(in.good(), "B: cannot read schemas/mule_temporal.gsql");
  std::vector<std::string> names;
  bool inside = false;
  for (std::string line; std::getline(in, line);) {
    if (!inside) {
      inside = line.find("ADD VERTEX Account (") != line.npos;
      continue;
    }
    if (line.find(") WITH") != line.npos)
      break;
    std::istringstream words{line};
    std::string name;
    words >> name;
    if (name == "PRIMARY_ID")
      words >> name;
    if (!name.empty())
      names.push_back(name);
  }
  return names;
}

// The exporter's account pseudonym, restated to find each row's registry
// record; checked against the participation tables before it is used.
std::string accountId(pl::entity::Key key) {
  const auto input =
      std::string{"a:"} + std::string{pl::encoding::format(key).view()};
  std::array<unsigned char, 16> bytes{};
  check(pl::crypto::blake2b::digest(input.data(), input.size(), bytes.data(),
                                    bytes.size()),
        "digest");
  constexpr char hex[] = "0123456789abcdef";
  std::string out = "a_";
  for (const auto byte : bytes) {
    out += hex[byte >> 4];
    out += hex[byte & 15];
  }
  return out;
}

struct Event {
  std::uint64_t seq;
  std::uint64_t ms;
};

// MPL's validate_label_contract over the contract's fields.
struct Violations {
  std::size_t mule = 0, pu = 0, unknown = 0, clocks = 0, ring = 0;
  [[nodiscard]] std::size_t total() const {
    return mule + pu + unknown + clocks + ring;
  }
};

struct Label {
  bool external;
  std::int64_t isMule;
  bool known;
  bool masked;
  std::int64_t pu;
  std::uint64_t effSeq, effMs, availSeq, availMs;
  std::int64_t ring;
  std::uint64_t firstSeq, firstMs;
};

Violations validate(const std::vector<Label> &labels) {
  Violations v;
  for (const auto &a : labels) {
    v.mule += a.isMule != 0 && a.isMule != 1;
    v.pu += (a.pu != 0 && a.pu != 1) ||
            (a.pu == 1 && (a.isMule != 1 || !a.known || a.masked)) ||
            (a.pu == 0 && a.known && a.isMule == 1 && !a.masked);
    v.unknown += !a.known && (a.isMule != 0 || !a.masked || a.ring != -1);
    v.clocks += a.known && (a.effSeq == 0 || a.effMs == 0 ||
                            a.availSeq < a.effSeq || a.availMs < a.effMs);
    v.ring += a.ring < -1 || (a.isMule != 1 && a.ring != -1);
  }
  return v;
}

} // namespace

int main() {
  std::printf("=== mule-temporal Account label contract on a real world ===\n");
  const pl::time::Window window{.start = pl::time::makeTime({2019, 1, 1}),
                                .days = 365};
  const auto poolSet = pltest::buildPoolSet(kSeed);

  pltest::LegOptions options;
  options.seed = kSeed;
  options.window = window;
  options.population = kPopulation;
  const auto leg = pltest::runLeg(poolSet, options);

  // The leg's own world, rebuilt from the same spec (byte-identical).
  pltest::WorldSpec spec;
  spec.seed = kSeed;
  spec.window = window;
  spec.population = kPopulation;
  spec.fraudProfile = pltest::scaledFraudProfile();
  const pltest::GateWorld world(poolSet, spec);
  const auto &registry = world.holdings.accounts.registry;
  const auto &topology = world.people.roster.topology;

  Capture capture;
  {
    mt::StreamingMuleTemporalExport sink{
        mt::StreamingMuleTemporalExport::Config{
            .registry = &registry,
            .pii = &world.people.pii,
            .relocation = &world.people.relocation,
            .devices = &world.infra.devices,
            .ips = &world.infra.ips,
            .membership = pl::synth::personas::join_cohort::membershipOf(
                world.people.personas, window),
            .window = window,
            .seed = kSeed,
            .capture = &capture,
            .topology = &topology}};
    sink.append(leg.rows);
    sink.finish();
  }
  std::printf("  %zu payments, %zu rings\n", leg.rows.size(),
              topology.rings.size());

  // ---------------------------------------------------------------- A
  check(capture.tables.size() == std::size(kPins) + 1, "A: 27 tables");
  for (const auto &pin : kPins) {
    const auto &bytes = capture.tables.at(std::string{pin.table});
    std::printf("  A %-25s %016llx (pinned %016llx)\n",
                std::string{pin.table}.c_str(),
                static_cast<unsigned long long>(fnv1a(bytes)),
                static_cast<unsigned long long>(pin.digest));
    check(fnv1a(bytes) == pin.digest && bytes.size() == pin.bytes,
          "A: a table other than Account moved off the pre-round build");
  }
  const auto &accountBytes = capture.tables.at("Account");
  const auto six = projection(accountBytes, 6);
  std::printf("  A Account, first six columns %016llx (pinned %016llx)\n",
              static_cast<unsigned long long>(fnv1a(six)),
              static_cast<unsigned long long>(kPreRoundAccount));
  check(fnv1a(six) == kPreRoundAccount && six.size() == kPreRoundAccountBytes,
        "A: the Account table's first six columns moved off the pre-round "
        "table");
  check(accountBytes.size() > six.size(),
        "A: the Account table carries no label columns");

  // ---------------------------------------------------------------- B
  std::string header;
  for (const auto column : mt::schema::kAccount.header)
    header += (header.empty() ? "" : ",") + std::string{column};
  check(header == "id,account_type,is_external,first_seen_seq,first_seen_ts_ms,"
                  "is_mule,mule_label_known,is_mule_masked,pu_label,"
                  "mule_label_effective_seq,mule_label_effective_ts_ms,"
                  "mule_label_available_seq,mule_label_available_ts_ms,"
                  "mule_ring_id,mule_label_source",
        "B: the header is not MPL's ACCOUNT_LOAD_COLUMNS");
  check(accountBytes.starts_with(header + "\r\n"), "B: the table's header");
  // The DDL stores is_mule last, so a loading job reads the table's columns
  // by position as $0..$4, $6..$14, $5: MPL's load_accounts mapping, and the
  // one the tf_gnn_loader_v2 Account job needs (docs/mule_temporal.md,
  // "Loading the corpus into TigerGraph").
  {
    const auto columns = mt::schema::kAccount.header;
    std::vector<std::size_t> positions;
    for (const auto &name : accountStorageOrder()) {
      const auto it = std::ranges::find(columns, std::string_view{name});
      check(it != columns.end(), "B: a DDL Account attribute is no column");
      positions.push_back(static_cast<std::size_t>(it - columns.begin()));
    }
    const std::vector<std::size_t> loadOrder{0, 1,  2,  3,  4,  6,  7, 8,
                                             9, 10, 11, 12, 13, 14, 5};
    check(positions == loadOrder,
          "B: the DDL's Account storage order is not the columns at $0..$4, "
          "$6..$14, $5");
    std::printf("  B header is MPL's; the DDL stores the columns at $0..$4, "
                "$6..$14, $5\n");
  }

  // The registry record behind each exported account id, checked against
  // the exporter's own participation rows first.
  std::map<std::string, std::size_t> recordOf;
  for (std::size_t i = 0; i < registry.records.size(); ++i)
    recordOf.emplace(accountId(registry.records[i].id), i);
  std::map<std::string, Event> events;
  eachRow(capture.tables.at("Payment_Transaction"), [&](const Cells &c) {
    events.emplace(std::string{c[0]}, Event{number(c[7]), number(c[6])});
  });
  eachRow(capture.tables.at("Zelle_Transfer"), [&](const Cells &c) {
    events.emplace(std::string{c[0]}, Event{number(c[3]), number(c[2])});
  });
  check(events.size() == leg.rows.size(), "one event per payment");
  std::size_t mapped = 0;
  for (const std::string_view table :
       {"Transaction_From_Account", "Transfer_From_Account",
        "Transaction_To_Account", "Transfer_To_Account"})
    eachRow(capture.tables.at(std::string{table}), [&](const Cells &c) {
      const auto &tx = leg.rows.at(number(c[0].substr(1)) - 1);
      const bool from = table.find("From") != std::string_view::npos;
      check(c[1] == accountId(from ? tx.source : tx.target),
            "the restated account id is not the exporter's");
      ++mapped;
    });
  check(mapped == 2 * leg.rows.size(), "every payment has two accounts");

  // Each mule's first laundering payment, from the rows (the export's event
  // order is the rows' order) and the event tables.
  std::map<pl::entity::Key, Event> firstLaundering;
  // The ring ids the laundering rows touching each mule carry.
  std::map<pl::entity::Key, std::set<std::uint32_t>> launderingRings;
  for (std::size_t i = 0; i < leg.rows.size(); ++i) {
    const auto &tx = leg.rows[i];
    if (!pl::channels::isFraud(tx.session.channel))
      continue;
    for (const auto key : {tx.source, tx.target}) {
      const auto &record = registry.records[recordOf.at(accountId(key))];
      if (!pl::entity::account::hasFlag(record.flags,
                                        pl::entity::account::Flag::mule))
        continue;
      firstLaundering.try_emplace(key,
                                  events.at(std::format("T{:012}", i + 1)));
      if (tx.fraud.ringId.has_value())
        launderingRings[key].insert(*tx.fraud.ringId);
    }
  }

  // The rings, read straight off the topology: the ring holding each person
  // as a member, and every ring listing them among its mules.
  std::map<pl::entity::PersonId, std::uint32_t> memberOf;
  std::map<pl::entity::PersonId, std::set<std::uint32_t>> muleOf;
  for (const auto &ring : topology.rings) {
    for (std::uint32_t i = 0; i < ring.members.size; ++i)
      memberOf[topology.memberStore[ring.members.offset + i]] = ring.id;
    for (std::uint32_t i = 0; i < ring.mules.size; ++i)
      muleOf[topology.muleStore[ring.mules.offset + i]].insert(ring.id);
  }

  std::vector<Label> labels;
  std::size_t mules = 0, active = 0, idle = 0, multiRing = 0, external = 0;
  std::set<std::int64_t> ringsSeen;
  eachRow(accountBytes, [&](const Cells &c) {
    check(c.size() == 15, "B: fifteen cells");
    const auto &record = registry.records[recordOf.at(std::string{c[0]})];
    const bool isMule = pl::entity::account::hasFlag(
        record.flags, pl::entity::account::Flag::mule);
    const bool ext = c[kExternal] == "True";
    Label label{
        .external = ext,
        .isMule =
            static_cast<std::int64_t>(std::stoll(std::string{c[kIsMule]})),
        .known = c[kKnown] == "True",
        .masked = c[kMasked] == "True",
        .pu = static_cast<std::int64_t>(std::stoll(std::string{c[kPu]})),
        .effSeq = number(c[kEffSeq]),
        .effMs = number(c[kEffMs]),
        .availSeq = number(c[kAvailSeq]),
        .availMs = number(c[kAvailMs]),
        .ring = static_cast<std::int64_t>(std::stoll(std::string{c[kRing]})),
        .firstSeq = number(c[kFirstSeq]),
        .firstMs = number(c[kFirstMs]),
    };
    labels.push_back(label);

    // ------------------------------------------------------------ C
    check(c[kKnown] == "False" && c[kMasked] == "True" && c[kPu] == "0",
          "C: a label is known, unmasked or revealed in the export");
    check(label.isMule == (isMule ? 1 : 0), "C: is_mule is not the role");
    if (isMule) {
      ++mules;
      check(memberOf.contains(record.owner),
            "C: a mule's owner is in no ring (the generator makes none)");
      check(label.ring == memberOf.at(record.owner),
            "C: mule_ring_id is not the mule's home ring");
      check(muleOf.at(record.owner).contains(memberOf.at(record.owner)),
            "C: the home ring does not list the mule among its mules");
      ringsSeen.insert(label.ring);
      const auto &mine = muleOf.at(record.owner);
      multiRing += mine.size() > 1;
      if (const auto it = launderingRings.find(record.id);
          it != launderingRings.end())
        for (const auto ring : it->second) {
          check(mine.contains(ring),
                "C: a laundering row names a ring the mule is not in");
          if (mine.size() == 1)
            check(static_cast<std::int64_t>(ring) == label.ring,
                  "C: a one-ring mule's rows carry another ring id");
        }
    } else {
      check(label.ring == -1, "C: a non-mule has a ring");
    }

    // ------------------------------------------------------------ D
    check(label.availSeq == label.effSeq && label.availMs == label.effMs,
          "D: availability is not effectiveness");
    if (ext) {
      ++external;
      check(label.effSeq == 0 && label.effMs == 0 && c[kSource].empty(),
            "D: an external account's label is not unknown");
      return;
    }
    check(c[kSource] == "phantomledger_role", "D: the source");
    check(label.firstSeq > 0 && label.firstMs > 0, "D: first observation");
    if (const auto it = firstLaundering.find(record.id);
        isMule && it != firstLaundering.end()) {
      ++active;
      check(label.effSeq == it->second.seq && label.effMs == it->second.ms,
            "D: a mule's clock is not its first laundering payment");
      check(label.effSeq >= label.firstSeq && label.effMs >= label.firstMs,
            "D: a mule's clock precedes its first observation");
    } else {
      idle += isMule;
      check(label.effSeq == label.firstSeq && label.effMs == label.firstMs,
            "D: the clock is not the first observation");
    }
  });
  std::printf("  C %zu mules, %zu in several rings, %zu distinct home rings\n",
              mules, multiRing, ringsSeen.size());
  std::printf("  D %zu mules with a laundering payment, %zu without; %zu "
              "external accounts\n",
              active, idle, external);
  // Non-vacuity: the world must exercise each branch the checks read.
  check(mules >= 10 && ringsSeen.size() >= 5,
        "C: too few mules or rings to measure");
  check(multiRing >= 1, "C: no mule in several rings, so the home-ring rule "
                        "is not exercised");
  check(active >= 10, "D: too few mules with a laundering payment");
  check(external >= 1, "D: no external account");

  // ---------------------------------------------------------------- E
  std::size_t internalKnown = 0, internalRevealed = 0;
  for (const auto &a : labels)
    if (!a.external) {
      internalKnown += a.known;
      internalRevealed += a.pu == 1;
    }
  check(internalKnown == 0 && internalRevealed == 0,
        "E: the reveal guard would report already_revealed");
  const auto loaded = validate(labels);
  std::printf("  E as loaded: invalid_mule %zu, invalid_pu %zu, "
              "invalid_unknown %zu, invalid_clocks %zu, invalid_ring %zu\n",
              loaded.mule, loaded.pu, loaded.unknown, loaded.clocks,
              loaded.ring);
  check(loaded.unknown == mules && loaded.total() == mules,
        "E: as loaded, a violation other than one invalid_unknown per mule");
  // What label_reveal.gsql writes with apply = TRUE: every internal account
  // known, effective at its first observation; a mule available at its
  // discovery (never before its first observation; the end of the data
  // stands in) and masked unless revealed; the rest masked, available at
  // their first observation. The ring is not written. Reveal every other
  // mule, so both mule cases are checked.
  auto revealed = labels;
  std::size_t nth = 0;
  for (auto &a : revealed) {
    if (a.external)
      continue;
    a.known = true;
    a.effSeq = a.firstSeq;
    a.effMs = a.firstMs;
    if (a.isMule == 1) {
      const bool shown = nth++ % 2 == 0;
      a.masked = !shown;
      a.pu = shown ? 1 : 0;
      a.availSeq = std::numeric_limits<std::uint32_t>::max();
      a.availMs = pl::time::toEpochSeconds(window.endExcl()) * 1000ULL;
    } else {
      a.masked = true;
      a.pu = 0;
      a.availSeq = a.firstSeq;
      a.availMs = a.firstMs;
    }
  }
  const auto after = validate(revealed);
  check(after.total() == 0, "E: a contract violation after the reveal");
  std::printf("  E after the reveal: every violation count is zero\n");

  std::printf("mule-temporal Account label contract holds\n");
  return 0;
}
