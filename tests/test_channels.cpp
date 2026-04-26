#include "phantomledger/taxonomies/channels/names.hpp"
#include "phantomledger/taxonomies/channels/predicates.hpp"

#include "test_support.hpp"

#include <cstdint>
#include <cstdio>
#include <string_view>

using namespace PhantomLedger;

namespace {

channels::Tag requireParsedTag(std::string_view s) {
  auto parsed = channels::parse(s);
  PL_CHECK(parsed.has_value());
  return *parsed;
}

void testParseKnownChannels() {
  PL_CHECK_EQ(requireParsedTag("salary"),
              channels::tag(channels::Legit::salary));
  PL_CHECK_EQ(requireParsedTag("rent"), channels::tag(channels::Rent::generic));
  PL_CHECK_EQ(requireParsedTag("fraud_invoice"),
              channels::tag(channels::Fraud::invoice));
  PL_CHECK_EQ(requireParsedTag("gov_disability"),
              channels::tag(channels::Government::disability));

  std::printf("  PASS: parse known channels\n");
}

void testParseRejectsUnknownChannels() {
  PL_CHECK(!channels::parse("").has_value());
  PL_CHECK(!channels::parse("Rent").has_value());
  PL_CHECK(!channels::parse("rent_").has_value());
  PL_CHECK(!channels::parse("fraud").has_value());
  PL_CHECK(!channels::parse("not_a_channel").has_value());

  std::printf("  PASS: parse rejects unknown channels\n");
}

void testNameRoundTrip() {
  auto rent = requireParsedTag("rent");
  auto fraud = requireParsedTag("fraud_mule_forward");
  auto salary = requireParsedTag("salary");

  PL_CHECK_EQ(channels::name(rent), std::string_view("rent"));
  PL_CHECK_EQ(channels::name(fraud), std::string_view("fraud_mule_forward"));
  PL_CHECK_EQ(channels::name(salary), std::string_view("salary"));

  PL_CHECK_EQ(channels::name(channels::Legit::atm),
              std::string_view("atm_withdrawal"));
  PL_CHECK_EQ(channels::name(channels::Credit::payment),
              std::string_view("cc_payment"));
  PL_CHECK_EQ(channels::name(channels::Product::mortgage),
              std::string_view("mortgage_payment"));

  std::printf("  PASS: name round-trip\n");
}

void testRentMembership() {
  PL_CHECK(channels::isRent(requireParsedTag("rent")));
  PL_CHECK(channels::isRent(requireParsedTag("rent_ach")));
  PL_CHECK(channels::isRent(requireParsedTag("rent_portal")));
  PL_CHECK(channels::isRent(requireParsedTag("rent_p2p")));
  PL_CHECK(channels::isRent(requireParsedTag("rent_check")));

  PL_CHECK(!channels::isRent(requireParsedTag("salary")));
  PL_CHECK(!channels::isRent(requireParsedTag("mortgage_payment")));
  PL_CHECK(!channels::isRent(channels::none));

  std::printf("  PASS: isRent\n");
}

void testPaydayInboundMembership() {
  PL_CHECK(channels::isPaydayInbound(requireParsedTag("salary")));
  PL_CHECK(channels::isPaydayInbound(requireParsedTag("gov_social_security")));
  PL_CHECK(channels::isPaydayInbound(requireParsedTag("gov_pension")));
  PL_CHECK(channels::isPaydayInbound(requireParsedTag("gov_disability")));
  PL_CHECK(channels::isPaydayInbound(requireParsedTag("client_ach_credit")));
  PL_CHECK(channels::isPaydayInbound(requireParsedTag("card_settlement")));
  PL_CHECK(channels::isPaydayInbound(requireParsedTag("platform_payout")));
  PL_CHECK(channels::isPaydayInbound(requireParsedTag("owner_draw")));
  PL_CHECK(channels::isPaydayInbound(requireParsedTag("investment_inflow")));

  PL_CHECK(!channels::isPaydayInbound(requireParsedTag("rent")));
  PL_CHECK(!channels::isPaydayInbound(requireParsedTag("p2p")));
  PL_CHECK(!channels::isPaydayInbound(requireParsedTag("merchant")));
  PL_CHECK(!channels::isPaydayInbound(requireParsedTag("fraud_classic")));
  PL_CHECK(!channels::isPaydayInbound(channels::none));

  std::printf("  PASS: isPaydayInbound\n");
}

void testFraudMembership() {
  PL_CHECK(channels::isFraud(requireParsedTag("fraud_classic")));
  PL_CHECK(channels::isFraud(requireParsedTag("fraud_cycle")));
  PL_CHECK(channels::isFraud(requireParsedTag("fraud_layering_in")));
  PL_CHECK(channels::isFraud(requireParsedTag("fraud_layering_hop")));
  PL_CHECK(channels::isFraud(requireParsedTag("fraud_layering_out")));
  PL_CHECK(channels::isFraud(requireParsedTag("fraud_funnel_in")));
  PL_CHECK(channels::isFraud(requireParsedTag("fraud_funnel_out")));
  PL_CHECK(channels::isFraud(requireParsedTag("fraud_structuring")));
  PL_CHECK(channels::isFraud(requireParsedTag("fraud_invoice")));
  PL_CHECK(channels::isFraud(requireParsedTag("fraud_mule_in")));
  PL_CHECK(channels::isFraud(requireParsedTag("fraud_mule_forward")));

  PL_CHECK(!channels::isFraud(requireParsedTag("camouflage_bill")));
  PL_CHECK(!channels::isFraud(requireParsedTag("camouflage_p2p")));
  PL_CHECK(!channels::isFraud(requireParsedTag("camouflage_salary")));
  PL_CHECK(!channels::isFraud(requireParsedTag("salary")));
  PL_CHECK(!channels::isFraud(requireParsedTag("rent")));
  PL_CHECK(!channels::isFraud(channels::none));

  std::printf("  PASS: isFraud\n");
}

void testKnown() {
  PL_CHECK(channels::isKnown(requireParsedTag("salary")));
  PL_CHECK(channels::isKnown(requireParsedTag("rent")));
  PL_CHECK(channels::isKnown(requireParsedTag("fraud_classic")));
  PL_CHECK(!channels::isKnown(channels::none));

  std::printf("  PASS: isKnown\n");
}

void testByteLayout() {
  PL_CHECK_EQ(channels::tag(channels::Legit::salary).value, std::uint8_t{0x01});
  PL_CHECK_EQ(channels::tag(channels::Rent::generic).value, std::uint8_t{0x10});
  PL_CHECK_EQ(channels::tag(channels::Family::allowance).value,
              std::uint8_t{0x20});
  PL_CHECK_EQ(channels::tag(channels::Fraud::classic).value,
              std::uint8_t{0x70});
  PL_CHECK_EQ(channels::tag(channels::Camouflage::salary).value,
              std::uint8_t{0x82});

  std::printf("  PASS: byte layout\n");
}

} // namespace

int main() {
  std::printf("=== Channel Tests ===\n");
  testParseKnownChannels();
  testParseRejectsUnknownChannels();
  testNameRoundTrip();
  testRentMembership();
  testPaydayInboundMembership();
  testFraudMembership();
  testKnown();
  testByteLayout();
  std::printf("All channel tests passed.\n\n");
  return 0;
}
