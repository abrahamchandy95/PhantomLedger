#include "phantomledger/devices/identity.hpp"
#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/hashing/combine.hpp"
#include "phantomledger/hashing/fnv.hpp"
#include "phantomledger/network/ipv4.hpp"
#include "phantomledger/tokens/sized.hpp"

#include "test_support.hpp"

#include <cstdio>
#include <stdexcept>
#include <unordered_set>

using namespace PhantomLedger;

namespace {

void testEntityBuilders() {
  const auto customer = entities::people::customer(7);
  PL_CHECK_EQ(customer.type, entities::Type::customer);
  PL_CHECK_EQ(customer.bank, entities::BankAccount::internal);
  PL_CHECK_EQ(customer.number, 7ULL);

  const auto account = entities::accounts::account(11);
  PL_CHECK_EQ(account.type, entities::Type::account);
  PL_CHECK_EQ(account.bank, entities::BankAccount::internal);
  PL_CHECK_EQ(account.number, 11ULL);

  const auto merchantInternal = entities::accounts::merchant(12);
  PL_CHECK_EQ(merchantInternal.type, entities::Type::merchant);
  PL_CHECK_EQ(merchantInternal.bank, entities::BankAccount::internal);

  const auto merchantExternal =
      entities::accounts::merchant(13, entities::BankAccount::external);
  PL_CHECK_EQ(merchantExternal.bank, entities::BankAccount::external);

  PL_CHECK_EQ(entities::accounts::employer(1).type, entities::Type::employer);
  PL_CHECK_EQ(entities::accounts::landlord(2).type, entities::Type::landlord);
  PL_CHECK_EQ(entities::accounts::client(3).type, entities::Type::client);
  PL_CHECK_EQ(entities::accounts::platform(4).type, entities::Type::platform);
  PL_CHECK_EQ(entities::accounts::processor(5).type, entities::Type::processor);
  PL_CHECK_EQ(entities::accounts::business(6).type, entities::Type::business);
  PL_CHECK_EQ(entities::accounts::brokerage(7).type, entities::Type::brokerage);

  PL_CHECK(entities::accounts::employer(1).bank ==
           entities::BankAccount::external);
  std::printf("  PASS: entity identity builders\n");
}

void testEntityHashingAndEquality() {
  const auto a = entities::accounts::account(1);
  const auto b = entities::accounts::account(1);
  const auto c = entities::accounts::account(2);

  PL_CHECK(a == b);
  PL_CHECK(a != c);
  PL_CHECK_EQ(entities::hashValue(a), entities::hashValue(b));

  std::unordered_set<entities::Identity> ids;
  ids.insert(a);
  ids.insert(b);
  ids.insert(c);
  PL_CHECK_EQ(ids.size(), 2U);
  std::printf("  PASS: entity hashing/equality\n");
}

void testDeviceIdentity() {
  const auto person = devices::Identity::person(42, 3);
  PL_CHECK_EQ(person.ownerType, devices::OwnerType::person);
  PL_CHECK_EQ(person.ownerId, 42ULL);
  PL_CHECK_EQ(person.slot, 3U);

  const auto ring = devices::Identity::ring(99);
  PL_CHECK_EQ(ring.ownerType, devices::OwnerType::ring);
  PL_CHECK_EQ(ring.ownerId, 99ULL);
  PL_CHECK_EQ(ring.slot, 0U);

  std::unordered_set<devices::Identity> ids;
  ids.insert(person);
  ids.insert(devices::Identity::person(42, 3));
  ids.insert(ring);
  PL_CHECK_EQ(ids.size(), 2U);
  std::printf("  PASS: device identities and hashing\n");
}

void testIpv4Packing() {
  const auto ip = network::Ipv4::pack(192, 168, 1, 25);
  PL_CHECK_EQ(ip.value, 0xC0A80119U);
  PL_CHECK_EQ(ip.octet1(), 192);
  PL_CHECK_EQ(ip.octet2(), 168);
  PL_CHECK_EQ(ip.octet3(), 1);
  PL_CHECK_EQ(ip.octet4(), 25);

  std::unordered_set<network::Ipv4> ips;
  ips.insert(ip);
  ips.insert(network::Ipv4::pack(192, 168, 1, 25));
  ips.insert(network::Ipv4::pack(10, 0, 0, 1));
  PL_CHECK_EQ(ips.size(), 2U);
  std::printf("  PASS: IPv4 pack/unpack and hashing\n");
}

void testBoundedTokenProperties() {
  using Token = tokens::Bounded<16>;

  Token a{"ABC"};
  Token b{"ABC"};
  Token c{"DEF"};
  Token empty;

  PL_CHECK(a == b);
  PL_CHECK(a != c);
  PL_CHECK(a < c);
  PL_CHECK(empty.empty());
  PL_CHECK(!a.empty());
  PL_CHECK_EQ(a.size(), 3U);
  PL_CHECK(a.view() == "ABC");
  PL_CHECK(a.str() == "ABC");
  PL_CHECK(a.startsWith('A'));
  PL_CHECK(a.startsWith("AB"));
  PL_CHECK(!a.startsWith("AC"));

  a.assign("XYZ");
  PL_CHECK(a.view() == "XYZ");
  std::printf("  PASS: Bounded token properties\n");
}

void testBoundedTokenErrors() {
  using Token = tokens::Bounded<4>;
  bool caught = false;
  try {
    Token token;
    token.assign("ABCDE");
  } catch (const std::length_error &) {
    caught = true;
  }
  PL_CHECK(caught);
  std::printf("  PASS: Bounded token capacity checks\n");
}

void testHashingHelpers() {
  constexpr auto fnvAbc = hashing::fnv1a64("abc");
  PL_CHECK_EQ(fnvAbc, 16654208175385433931ULL);

  const auto a = hashing::make(1U, 2U, 3U);
  const auto b = hashing::make(1U, 2U, 3U);
  const auto c = hashing::make(1U, 2U, 4U);
  PL_CHECK_EQ(a, b);
  PL_CHECK(a != c);
  std::printf("  PASS: hashing helpers\n");
}

} // namespace

int main() {
  std::printf(
      "=== Value Type Tests (entities / devices / network / tokens) ===\n");
  testEntityBuilders();
  testEntityHashingAndEquality();
  testDeviceIdentity();
  testIpv4Packing();
  testBoundedTokenProperties();
  testBoundedTokenErrors();
  testHashingHelpers();
  std::printf("All value type tests passed.\n\n");
  return 0;
}
