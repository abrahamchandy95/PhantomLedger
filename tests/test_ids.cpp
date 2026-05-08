#include "phantomledger/entities/encoding/device.hpp"
#include "phantomledger/entities/encoding/external.hpp"
#include "phantomledger/entities/encoding/landlord.hpp"
#include "phantomledger/entities/encoding/named.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/transactions/network/random.hpp"

#include "test_support.hpp"

#include <cstdio>
#include <string>

using namespace PhantomLedger;

namespace {

void testConvenienceFactories() {
  PL_CHECK_EQ(encoding::customerId(1), std::string("C00000000001"));
  PL_CHECK_EQ(encoding::customerId(4'294'967'295ULL),
              std::string("C04294967295"));

  PL_CHECK_EQ(encoding::accountId(1), std::string("A0000000001"));
  PL_CHECK_EQ(encoding::merchantId(1), std::string("M00000001"));
  PL_CHECK_EQ(encoding::merchantExternalId(1), std::string("XM00000001"));

  PL_CHECK_EQ(encoding::employerId(1), std::string("E00000001"));
  PL_CHECK_EQ(encoding::employerExternalId(1), std::string("XE00000001"));

  PL_CHECK_EQ(encoding::landlordExternalId(1), std::string("XL00000001"));
  PL_CHECK_EQ(encoding::landlordIndividualId(1), std::string("XLI0000001"));
  PL_CHECK_EQ(encoding::landlordIndividualInternalId(1),
              std::string("LI0000001"));
  PL_CHECK_EQ(encoding::landlordSmallLlcId(1), std::string("XLS0000001"));
  PL_CHECK_EQ(encoding::landlordSmallLlcInternalId(1),
              std::string("LS0000001"));
  PL_CHECK_EQ(encoding::landlordCorporateId(1), std::string("XLC0000001"));
  PL_CHECK_EQ(encoding::landlordCorporateInternalId(1),
              std::string("LC0000001"));

  PL_CHECK_EQ(encoding::clientId(1), std::string("IC00000001"));
  PL_CHECK_EQ(encoding::clientExternalId(1), std::string("XC00000001"));
  PL_CHECK_EQ(encoding::platformExternalId(1), std::string("XP00000001"));
  PL_CHECK_EQ(encoding::processorExternalId(1), std::string("XS00000001"));
  PL_CHECK_EQ(encoding::businessExternalId(1), std::string("XO00000001"));
  PL_CHECK_EQ(encoding::brokerageExternalId(1), std::string("XB00000001"));

  std::printf("  PASS: convenience factories\n");
}

void testFactoryZeroThrows() {
  PL_CHECK_THROWS(encoding::customerId(0));
  PL_CHECK_THROWS(encoding::accountId(0));
  PL_CHECK_THROWS(encoding::merchantId(0));
  std::printf("  PASS: factory zero throws\n");
}

void testDeviceId() {
  PL_CHECK_EQ(encoding::deviceId("C00000000123", 1),
              std::string("D00000000123_1"));
  PL_CHECK_EQ(encoding::deviceId("C00000000001", 3),
              std::string("D00000000001_3"));

  PL_CHECK_THROWS(encoding::deviceId("C00000000001", 0));
  PL_CHECK_THROWS(encoding::deviceId("A0000000001", 1));
  PL_CHECK_THROWS(encoding::deviceId("", 1));

  std::printf("  PASS: deviceId\n");
}

void testFraudDeviceId() {
  PL_CHECK_EQ(encoding::fraudDeviceId(0), std::string("FD0000"));
  PL_CHECK_EQ(encoding::fraudDeviceId(1), std::string("FD0001"));
  PL_CHECK_EQ(encoding::fraudDeviceId(9999), std::string("FD9999"));
  std::printf("  PASS: fraudDeviceId\n");
}

void testRandomIp() {
  auto rng = random::Rng::fromSeed(42);

  for (int i = 0; i < 1000; ++i) {
    std::string ip = network::randomIp(rng);

    int o1, o2, o3, o4;
    int matched = std::sscanf(ip.c_str(), "%d.%d.%d.%d", &o1, &o2, &o3, &o4);
    PL_CHECK_EQ(matched, 4);
    PL_CHECK(o1 >= 11 && o1 < 223);
    PL_CHECK(o2 >= 0 && o2 < 256);
    PL_CHECK(o3 >= 0 && o3 < 256);
    PL_CHECK(o4 >= 1 && o4 < 255);
  }

  std::printf("  PASS: randomIp produces valid IPs\n");
}

void testRandomIpDeterministic() {
  auto a = random::Rng::fromSeed(99);
  auto b = random::Rng::fromSeed(99);

  for (int i = 0; i < 10; ++i) {
    PL_CHECK_EQ(network::randomIp(a), network::randomIp(b));
  }

  std::printf("  PASS: randomIp is deterministic\n");
}

void testIsExternal() {
  PL_CHECK(encoding::isExternal("XE00000001"));
  PL_CHECK(encoding::isExternal("XM00000001"));
  PL_CHECK(encoding::isExternal("XGOV00000001"));
  PL_CHECK(encoding::isExternal("XF12345678901234567890"));

  PL_CHECK(!encoding::isExternal("A0000000001"));
  PL_CHECK(!encoding::isExternal("C00000000001"));
  PL_CHECK(!encoding::isExternal("M00000001"));
  PL_CHECK(!encoding::isExternal(""));

  std::printf("  PASS: isExternal\n");
}

} // namespace

int main() {
  std::printf("=== ID Tests ===\n");
  testConvenienceFactories();
  testFactoryZeroThrows();
  testDeviceId();
  testFraudDeviceId();
  testRandomIp();
  testRandomIpDeterministic();
  testIsExternal();
  std::printf("All ID tests passed.\n\n");
  return 0;
}
