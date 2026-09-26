#include "phantomledger/exporter/mule_temporal/streaming.hpp"

#include "phantomledger/encoding/render.hpp"
#include "phantomledger/exporter/common/render.hpp"
#include "phantomledger/exporter/mule_temporal/schema.hpp"
#include "phantomledger/exporter/mule_temporal/zelle_policy.hpp"
#include "phantomledger/primitives/crypto/blake2b.hpp"
#include "phantomledger/taxonomies/channels/predicates.hpp"
#include "phantomledger/taxonomies/locale/names.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace PhantomLedger::exporter::mule_temporal {
namespace {

using Ref = std::size_t;
enum class Kind { party, account, token, device, ip, address };
constexpr std::array<std::string_view, 6> kVertexNames{
    "Party", "Account", "Token", "Device", "IP", "Address"};

// Synthetic pseudonyms, domain-separated and independent of std::hash. Never
// expose generator role prefixes or raw PII/IPs. This is not a PII vault.
std::string opaque(std::string_view domain, std::string_view raw) {
  const auto input = std::string{domain} + ":" + std::string{raw};
  std::array<unsigned char, 16> bytes{};
  if (!crypto::blake2b::digest(input.data(), input.size(), bytes.data(),
                               bytes.size()))
    throw std::runtime_error("mule-temporal: identifier digest failed");
  constexpr char hex[] = "0123456789abcdef";
  std::string out{domain};
  out += '_';
  for (const auto byte : bytes) {
    out += hex[byte >> 4];
    out += hex[byte & 15];
  }
  return out;
}

std::uint64_t millis(std::int64_t epoch) {
  if (epoch < 0 || static_cast<std::uint64_t>(epoch) >
                       std::numeric_limits<std::uint64_t>::max() / 1000)
    throw std::invalid_argument(
        "mule-temporal: timestamp does not fit UINT milliseconds");
  return static_cast<std::uint64_t>(epoch) * 1000;
}

bool depositAccount(entity::Key key) {
  return key.role == entity::Role::account ||
         key.role == entity::Role::business ||
         key.role == entity::Role::family || key.role == entity::Role::landlord;
}

struct Rail {
  bool zelle = false;
  std::string_view name = "unknown";
  std::string_view channel = "digital";
};

Rail baseRail(const transactions::Transaction &tx) {
  using namespace channels;
  const auto c = tx.session.channel;
  if (isCurrency(c))
    return {false, "cash", "branch_or_atm"};
  if (is(c, Deposit::checkDeposit) || is(c, Rent::check))
    return {false, "check", "unknown"};
  if (is(c, Legit::merchant) || is(c, Legit::cardPurchase) ||
      is(c, Credit::refund) || is(c, Credit::chargeback))
    return {false, "card", "digital"};
  if (isLiquidity(c) || is(c, Credit::interest) || is(c, Credit::lateFee) ||
      is(c, Legit::selfTransfer))
    return {false, "internal", "bank"};
  if (is(c, Legit::salary) || is(c, Camouflage::salary) ||
      is(c, Legit::clientAchCredit) || is(c, Rent::ach))
    return {false, "ach", "digital"};
  return {};
}

bool p2pPurpose(channels::Tag c) {
  using namespace channels;
  // Semantic payment families provide the same electronic P2P opportunity.
  // The fraud verdict, ring and typology metadata never select the rail.
  return is(c, Legit::p2p) || is(c, Camouflage::p2p) || is(c, Rent::p2p) ||
         (isFamily(c) && !is(c, Family::inheritance)) || isFraud(c);
}

struct Node {
  Kind kind;
  std::string id;
  std::string type = "unknown";
  bool external = false;
  std::string country;
  bool emitted = false;
  int isMule = -1; // Supervision only; never read by graph/rail generation.
};

struct Association {
  std::string table;
  Ref from;
  Ref to;
  std::int64_t start;
  std::int64_t end;
  std::uint64_t fromSeq = 0;
  bool emitted = false;
};

struct Change {
  std::int64_t epoch;
  bool opening; // closures before openings at the same second
  Ref association;
};

} // namespace

struct StreamingMuleTemporalExport::Impl {
  Config config;
  std::int64_t start;
  std::int64_t end;
  std::int64_t lastEpoch;
  std::uint64_t seq = 0;
  std::uint64_t rows = 0;
  bool finished = false;
  std::map<std::string, common::Table> tables;
  std::vector<Node> nodes;
  std::map<std::string, Ref> nodeById;
  std::map<entity::Key, Ref> accounts;
  std::map<entity::Key, Ref> tokens;
  std::map<entity::Key, entity::PersonId> owners;
  std::map<entity::PersonId, std::vector<entity::Key>> accountsByOwner;
  std::map<devices::Identity, std::string> deviceTypes;
  std::vector<Association> associations;
  std::vector<Change> changes;
  std::vector<Association> tokenAssociations;
  std::multimap<std::int64_t, Ref> tokenClosures;
  std::map<std::string, zelle::SendingWindow> sendWindows;
  Ref nextChange = 0;

  explicit Impl(Config c)
      : config(std::move(c)), start(time::toEpochSeconds(config.window.start)),
        end(time::toEpochSeconds(config.window.endExcl())), lastEpoch(start) {
    if (!config.registry || !config.pii || !config.devices || !config.ips ||
        config.window.days <= 0 || end <= start)
      throw std::invalid_argument(
          "mule-temporal: registry, PII, infrastructure and a positive window "
          "are required");
    if (time::toCalendarDate(config.window.start).year <
            zelle::kFirstSupportedYear)
      throw std::invalid_argument(
          "mule-temporal: Zelle generation requires a start year of 2019 or later");
    (void)millis(start);
    (void)millis(end);
    for (const auto &r : config.devices->records)
      deviceTypes.emplace(r.identity, synth::infra::name(r.kind));
    plan();
    const common::TableTarget target{config.pgMirror, config.capture};
    for (const auto &table : schema::kTables) {
      const std::string stem{
          table.filename.substr(0, table.filename.size() - 4)};
      tables.emplace(stem, common::openTable(target, table));
    }
  }

  csv::Writer &writer(std::string_view table) {
    return tables.at(std::string{table}).writer();
  }

  std::uint64_t tick() {
    if (seq == std::numeric_limits<std::uint64_t>::max())
      throw std::overflow_error("mule-temporal: sequence exhausted");
    return ++seq;
  }

  Ref node(Kind kind, std::string id, std::string type = "unknown",
           bool external = false, std::string country = {}) {
    if (const auto it = nodeById.find(id); it != nodeById.end())
      return it->second;
    const auto ref = nodes.size();
    nodeById.emplace(id, ref);
    nodes.push_back({kind, std::move(id), std::move(type), external,
                     std::move(country), false});
    return ref;
  }

  Ref party(entity::PersonId person) {
    return node(Kind::party, opaque("p", std::to_string(person)),
                person <= config.pii->size() ? "person" : "unknown");
  }

  Ref device(devices::Identity id) {
    const auto it = deviceTypes.find(id);
    return node(Kind::device, opaque("d", common::renderDeviceId(id).view()),
                it == deviceTypes.end() ? "unknown" : it->second);
  }

  Ref ip(network::Ipv4 id) {
    return node(Kind::ip, opaque("i", std::to_string(id.value)));
  }

  void observe(Ref ref, std::uint64_t atSeq, std::int64_t epoch) {
    auto &n = nodes[ref];
    if (n.emitted)
      return;
    auto &w = writer(kVertexNames[static_cast<std::size_t>(n.kind)]);
    switch (n.kind) {
    case Kind::party:
      w.writeRow(n.id, n.type, atSeq, millis(epoch));
      break;
    case Kind::account:
      w.writeRow(n.id, n.type, n.external, atSeq, millis(epoch), n.isMule);
      break;
    case Kind::token:
      w.writeRow(n.id, atSeq, millis(epoch), "synthetic_handle", "zelle");
      break;
    case Kind::device:
      w.writeRow(n.id, n.type, atSeq, millis(epoch));
      break;
    case Kind::ip:
      w.writeRow(n.id, atSeq, millis(epoch));
      break;
    case Kind::address:
      w.writeRow(n.id, atSeq, millis(epoch), n.country);
      break;
    }
    n.emitted = true;
  }

  std::int64_t join(entity::PersonId p) const {
    return std::max(start, time::toEpochSeconds(config.membership.joinTs(p)));
  }

  void tenure(std::string table, Ref from, Ref to, std::int64_t first,
              std::int64_t last) {
    first = std::max(first, start);
    if (last <= first || first >= end)
      return;
    associations.push_back({std::move(table), from, to, first, last});
  }

  void plan() {
    for (const auto &record : config.registry->records) {
      const auto key = record.id;
      if (!entity::valid(key) || accounts.contains(key))
        throw std::invalid_argument(
            "mule-temporal: invalid or duplicate account registry key");
      std::string type = "unknown";
      if (depositAccount(key))
        type = "deposit";
      if (key.role == entity::Role::card)
        type = "credit";
      if (key.role == entity::Role::brokerage)
        type = "brokerage";
      // A bank-owned income GL (bank-gl-2026-09): internal, ownerless, never
      // a deposit, so it is neither external nor a Zelle endpoint.
      if (key.role == entity::Role::ledger)
        type = "gl";
      const auto a =
          node(Kind::account, opaque("a", encoding::format(key).view()), type,
               key.bank == entity::Bank::external ||
                   entity::account::hasFlag(record.flags,
                                            entity::account::Flag::external));
      nodes[a].isMule =
          entity::account::hasFlag(record.flags, entity::account::Flag::mule)
              ? 1
              : 0;
      accounts.emplace(key, a);
      if (!entity::valid(record.owner))
        continue;
      const auto p = record.owner;
      owners.emplace(key, p);
      accountsByOwner[p].push_back(key);
      const auto person = party(p);
      const auto first = join(p);
      const auto last = config.membership.closeEpoch(p);
      tenure("Party_Owns_Account", person, a, first, last);
    }

    for (const auto &[p, keys] : accountsByOwner) {
      if (p > config.pii->size())
        continue;
      const auto &pii = config.pii->at(p);
      const auto country = std::string{locale::code(pii.country)};
      auto home = [&](entity::geography::GeoAreaId area, std::int64_t first,
                      std::int64_t last) {
        // Same normalized street/area shares a token. Relocations create new
        // address entities; they never rewrite a prior Address's metadata.
        const auto raw = std::format(
            "{}:{}:{}:{}", country, pii.address.streetIdx, area,
            area == entity::geography::invalidGeoArea ? pii.address.zipTableIdx
                                                      : 0);
        const auto a =
            node(Kind::address, opaque("h", raw), "unknown", false, country);
        tenure("Party_Has_Address", party(p), a, std::max(first, join(p)),
               std::min(last, config.membership.closeEpoch(p)));
      };
      const auto history =
          config.relocation
              ? config.relocation->tenures(p)
              : std::span<const entity::parties::relocation::Tenure>{};
      if (history.empty())
        home(pii.address.geoArea, start,
             std::numeric_limits<std::int64_t>::max());
      for (std::size_t i = 0; i < history.size(); ++i)
        home(history[i].area, history[i].fromEpoch,
             i + 1 < history.size() ? history[i + 1].fromEpoch
                                    : std::numeric_limits<std::int64_t>::max());
    }

    for (const auto &u : config.devices->usages) {
      if (!u.enrolled || !u.deviceId.assigned() ||
          !accountsByOwner.contains(u.personId))
        continue;
      const auto first =
          std::max(time::toEpochSeconds(u.firstSeen), join(u.personId));
      const auto last =
          std::min(time::toEpochSeconds(u.lastSeen + time::Days{1}),
                   config.membership.closeEpoch(u.personId));
      const auto d = device(u.deviceId);
      tenure("Party_Uses_Device", party(u.personId), d, first, last);
      for (const auto key : accountsByOwner.at(u.personId))
        tenure("Account_Uses_Device", accounts.at(key), d, first, last);
    }
    for (const auto &u : config.ips->usages) {
      if (!u.enrolled || u.ipAddress.value == 0 ||
          !accountsByOwner.contains(u.personId))
        continue;
      tenure("Party_Uses_IP", party(u.personId), ip(u.ipAddress),
             std::max(time::toEpochSeconds(u.firstSeen), join(u.personId)),
             std::min(time::toEpochSeconds(u.lastSeen + time::Days{1}),
                      config.membership.closeEpoch(u.personId)));
    }

    auto key = [&](const Association &a) {
      return std::tie(a.table, nodes[a.from].id, nodes[a.to].id, a.start,
                      a.end);
    };
    std::ranges::sort(associations, [&](const auto &a, const auto &b) {
      return key(a) < key(b);
    });
    // Coalesce duplicate/overlapping evidence for one continuous tenure; a
    // real gap is retained as a separate discriminated edge.
    std::vector<Association> merged;
    for (auto &a : associations) {
      if (!merged.empty()) {
        auto &back = merged.back();
        if (back.table == a.table && back.from == a.from && back.to == a.to &&
            a.start <= back.end) {
          back.end = std::max(back.end, a.end);
          continue;
        }
      }
      merged.push_back(std::move(a));
    }
    associations = std::move(merged);
    for (Ref i = 0; i < associations.size(); ++i) {
      changes.push_back({associations[i].start, true, i});
      // End-of-window truncation is censoring, not evidence of detachment.
      if (associations[i].end < end)
        changes.push_back({associations[i].end, false, i});
    }
    std::ranges::sort(changes, [](const auto &a, const auto &b) {
      return std::tie(a.epoch, a.opening, a.association) <
             std::tie(b.epoch, b.opening, b.association);
    });
  }

  void emitAssociation(Association &a, std::uint64_t to) {
    writer(a.table).writeRow(
        nodes[a.from].id, nodes[a.to].id, a.fromSeq, to, 1.0,
        a.table == "Account_Uses_Device" ? "synthetic_owner_device"
                                         : "synthetic_registry");
    a.emitted = true;
  }

  void advance(std::int64_t epoch) {
    while (true) {
      const auto planned = nextChange < changes.size()
                               ? changes[nextChange].epoch
                               : std::numeric_limits<std::int64_t>::max();
      const auto tokenEnd = tokenClosures.empty()
                                ? std::numeric_limits<std::int64_t>::max()
                                : tokenClosures.begin()->first;
      if (std::min(planned, tokenEnd) > epoch)
        break;
      if (tokenEnd <= planned) {
        auto it = tokenClosures.begin();
        emitAssociation(tokenAssociations[it->second], tick());
        tokenClosures.erase(it);
        continue;
      }
      const auto change = changes[nextChange++];
      auto &a = associations[change.association];
      const auto at = tick();
      if (change.opening) {
        a.fromSeq = at;
        observe(a.from, at, change.epoch);
        observe(a.to, at, change.epoch);
      } else {
        if (a.fromSeq == 0)
          throw std::logic_error("mule-temporal: close before open");
        emitAssociation(a, at);
      }
    }
  }

  bool eligible(entity::Key key, std::int64_t epoch) const {
    if (!depositAccount(key))
      return false;
    const auto owner = owners.find(key);
    if (owner == owners.end())
      // This scenario treats ownerless family/business/landlord counterparties
      // as domestic deposit accounts. External means another bank, not abroad.
      return true;
    const auto p = owner->second;
    return config.membership.activeAt(p, epoch) &&
           (p > config.pii->size() ||
            config.pii->at(p).country == locale::Country::us);
  }

  std::string profile(entity::Key key) const {
    const auto owner = owners.find(key);
    const bool business = key.role == entity::Role::business ||
                          key.role == entity::Role::landlord;
    // Consumer accounts of the same owner share adoption and a sending budget;
    // a small-business profile is separate. Ownerless external accounts each
    // stand for one unobserved customer profile; do not invent Party ownership.
    return std::string{business ? "business:" : "consumer:"} +
           (owner == owners.end() ? std::string{encoding::format(key).view()}
                                  : std::to_string(owner->second));
  }

  Rail railFor(const transactions::Transaction &tx) {
    const auto base = baseRail(tx);
    if (base.name != "unknown" || !p2pPurpose(tx.session.channel) ||
        tx.source == tx.target ||
        !eligible(tx.source, tx.timestamp) ||
        !eligible(tx.target, tx.timestamp))
      return base;
    const auto sender = profile(tx.source);
    if (sender == profile(tx.target) ||
        zelle::draw(config.seed, "zelle-sender", sender) >=
            zelle::kSenderProbability)
      return base;
    const auto payment =
        std::format("{}:{}:{}:{}", encoding::format(tx.source).view(),
                    encoding::format(tx.target).view(), tx.timestamp, rows + 1);
    if (zelle::draw(config.seed, "zelle-payment", payment) >=
        zelle::kPaymentProbability)
      return base;
    const bool business = tx.source.role == entity::Role::business ||
                          tx.source.role == entity::Role::landlord;
    if (!sendWindows[sender].accept(tx.timestamp, tx.amount, business))
      return base;
    return {true, "zelle", "digital"};
  }

  void enrollToken(entity::Key key, std::int64_t epoch) {
    if (tokens.contains(key))
      return;
    const auto token =
        node(Kind::token, opaque("t", encoding::format(key).view()));
    tokens.emplace(key, token);
    const auto owner = owners.find(key);
    const auto last = owner == owners.end()
                          ? std::numeric_limits<std::int64_t>::max()
                          : config.membership.closeEpoch(owner->second);
    auto open = [&](std::string table, Ref from, Ref to) {
      const auto at = tick();
      observe(from, at, epoch);
      observe(to, at, epoch);
      const auto ref = tokenAssociations.size();
      tokenAssociations.push_back(
          {std::move(table), from, to, epoch, last, at});
      if (last < end)
        tokenClosures.emplace(last, ref);
    };
    open("Token_Bound_To_Account", token, accounts.at(key));
    if (owner != owners.end())
      open("Party_Uses_Token", party(owner->second), token);
  }

  void append(std::span<const transactions::Transaction> batch) {
    if (finished)
      throw std::logic_error("mule-temporal: append after finish");
    for (const auto &tx : batch) {
      if (tx.timestamp < lastEpoch || tx.timestamp < start ||
          tx.timestamp >= end)
        throw std::invalid_argument("mule-temporal: payments must be "
                                    "chronological and inside the window");
      if (!accounts.contains(tx.source) || !accounts.contains(tx.target))
        throw std::invalid_argument(
            "mule-temporal: payment references an unregistered account");
      if (std::isinf(tx.amount) || (std::isfinite(tx.amount) && tx.amount < 0))
        throw std::invalid_argument("mule-temporal: invalid payment amount");
      advance(tx.timestamp);
      lastEpoch = tx.timestamp;
      const auto rail = railFor(tx);
      if (rail.zelle) {
        // A settled Zelle payment has registered endpoints. Registration is
        // first observed immediately before first use, including recipients
        // who had not previously sent money. Never look ahead to choose it.
        enrollToken(tx.source, tx.timestamp);
        enrollToken(tx.target, tx.timestamp);
      }
      const auto at = tick();
      const auto ms = millis(tx.timestamp);
      const auto eventId = std::format("T{:012}", ++rows);
      const bool present = std::isfinite(tx.amount);
      const double amount = present ? tx.amount : 0.0;
      if (rail.zelle) {
        // Simulator supervision arrives immediately after the event. Keep
        // the same sequence slot regardless of the fraud verdict.
        const auto arrival = tick();
        writer("Zelle_Transfer")
            .writeRow(
                eventId,
                time::formatTimestamp(time::fromEpochSeconds(tx.timestamp)), ms,
                at, amount, present, "USD", rail.channel,
                tx.fraud.flag != 0 ? 1 : 0, true, arrival, ms);
      } else {
        writer("Payment_Transaction")
            .writeRow(
                eventId, amount, "USD", rail.name, rail.channel,
                time::formatTimestamp(time::fromEpochSeconds(tx.timestamp)), ms,
                at, present);
      }
      const std::string prefix = rail.zelle ? "Transfer_" : "Transaction_";
      auto participation = [&](std::string_view role, Ref endpoint) {
        observe(endpoint, at, tx.timestamp);
        writer(prefix + std::string{role})
            .writeRow(eventId, nodes[endpoint].id, ms, at);
      };
      participation("From_Account", accounts.at(tx.source));
      participation("To_Account", accounts.at(tx.target));
      if (rail.zelle) {
        for (const auto &[key, role] : {std::pair{tx.source, "From_Token"},
                                        std::pair{tx.target, "To_Token"}}) {
          participation(role, tokens.at(key));
        }
      }
      if (tx.session.deviceId.assigned())
        participation("Used_Device", device(tx.session.deviceId));
      if (tx.session.ipAddress.value != 0)
        participation("Used_IP", ip(tx.session.ipAddress));
    }
  }

  void finish() {
    if (finished)
      return;
    advance(end - 1);
    for (auto &a : associations)
      if (a.fromSeq != 0 && !a.emitted)
        emitAssociation(a, 0);
    for (auto &a : tokenAssociations)
      if (!a.emitted)
        emitAssociation(a, 0);
    for (auto &[name, table] : tables)
      table.close(); // propagate COPY failures
    finished = true;
  }
};

StreamingMuleTemporalExport::StreamingMuleTemporalExport(Config config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}
StreamingMuleTemporalExport::~StreamingMuleTemporalExport() = default;
StreamingMuleTemporalExport::StreamingMuleTemporalExport(
    StreamingMuleTemporalExport &&) noexcept = default;
StreamingMuleTemporalExport &StreamingMuleTemporalExport::operator=(
    StreamingMuleTemporalExport &&) noexcept = default;
void StreamingMuleTemporalExport::append(
    std::span<const transactions::Transaction> batch) {
  impl_->append(batch);
}
void StreamingMuleTemporalExport::finish() { impl_->finish(); }
std::uint64_t StreamingMuleTemporalExport::rowsWritten() const noexcept {
  return impl_->rows;
}

} // namespace PhantomLedger::exporter::mule_temporal
