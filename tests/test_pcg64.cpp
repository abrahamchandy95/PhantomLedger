#include "phantomledger/primitives/random/pcg64.hpp"

#include "test_support.hpp"

#include <array>
#include <cstdint>
#include <cstdio>

using namespace PhantomLedger;

namespace {

static constexpr std::uint64_t stateHi = 11316117344166709393ULL;
static constexpr std::uint64_t stateLo = 11160554683178229816ULL;
static constexpr std::uint64_t incrementHi = 14156248039698386623ULL;
static constexpr std::uint64_t incrementLo = 7388295216453704025ULL;

static constexpr std::array<std::uint64_t, 10> expectedRaw = {
    11530976094092348043ULL, 16550673365885938325ULL, 14308875409591826786ULL,
    4154339397315733314ULL,  5537090637313560901ULL,  16114216841932056372ULL,
    97127725791292528ULL,    15148990459964163805ULL, 14703335761166866782ULL,
    8631876318251464993ULL,
};

static constexpr std::array<double, 20> expectedDoubles = {
    0.625095466604667,    0.8972138009695755,  0.7756856902451935,
    0.22520718999059186,  0.30016628491122543, 0.8735534453962619,
    0.005265304565574724, 0.8212284183827663,  0.7970694287520462,
    0.4679349528437208,   0.3030324268193135,  0.2784256121007733,
    0.2548695876541246,   0.4450763058826466,  0.5045482589579533,
    0.5534973520744925,   0.9955002834343927,  0.7926619192137531,
    0.6221792294411627,   0.9889601476818849,
};

void testConstructionAccessors() {
  const random::Pcg64 rng(stateHi, stateLo, incrementHi, incrementLo);
  PL_CHECK_EQ(rng.state_hi(), stateHi);
  PL_CHECK_EQ(rng.state_lo(), stateLo);
  PL_CHECK_EQ(rng.increment_hi(), incrementHi);
  PL_CHECK_EQ(rng.increment_lo(), incrementLo);
  std::printf("  PASS: constructor accessors preserve state\n");
}

void testRawUint64() {
  random::Pcg64 rng(stateHi, stateLo, incrementHi, incrementLo);
  for (std::size_t i = 0; i < expectedRaw.size(); ++i) {
    PL_CHECK_EQ(rng.next_u64(), expectedRaw[i]);
  }
  std::printf("  PASS: raw uint64 matches numpy PCG64\n");
}

void testDoubles() {
  random::Pcg64 rng(stateHi, stateLo, incrementHi, incrementLo);
  for (std::size_t i = 0; i < expectedDoubles.size(); ++i) {
    PL_CHECK_EQ(rng.next_double(), expectedDoubles[i]);
  }
  std::printf("  PASS: doubles match numpy PCG64 bit-exactly\n");
}

void testDeterministic() {
  random::Pcg64 a(stateHi, stateLo, incrementHi, incrementLo);
  random::Pcg64 b(stateHi, stateLo, incrementHi, incrementLo);
  for (int i = 0; i < 10000; ++i) {
    PL_CHECK_EQ(a.next_u64(), b.next_u64());
  }
  std::printf("  PASS: identical state is deterministic\n");
}

void testDifferentStateDiffers() {
  random::Pcg64 a(stateHi, stateLo, incrementHi, incrementLo);
  random::Pcg64 b(stateHi + 1, stateLo, incrementHi, incrementLo);

  int same = 0;
  for (int i = 0; i < 100; ++i) {
    if (a.next_u64() == b.next_u64()) {
      ++same;
    }
  }
  PL_CHECK(same < 5);
  std::printf("  PASS: nearby states diverge quickly\n");
}

} // namespace

int main() {
  std::printf("=== PCG64 Tests (bit-exact vs numpy) ===\n");
  testConstructionAccessors();
  testRawUint64();
  testDoubles();
  testDeterministic();
  testDifferentStateDiffers();
  std::printf("All PCG64 tests passed.\n\n");
  return 0;
}
