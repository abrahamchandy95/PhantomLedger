#include "phantomledger/exporter/aml/minhash.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace PhantomLedger::exporter::aml::minhash {

const std::array<std::uint32_t, 101> kUhC1{{
    1U,          482067061U,  1133999723U, 2977257653U, 2666089543U,
    3098200677U, 3985189319U, 1857983931U, 3801236429U, 522456919U,
    4057595205U, 4176190031U, 1652234975U, 2294716503U, 1644020081U,
    3407377875U, 3749518465U, 4244672803U, 3053397733U, 3273255487U,
    598272097U,  3989043777U, 1414109747U, 697129027U,  67285677U,
    98002333U,   158583451U,  1424122447U, 2159224677U, 3478101309U,
    277468927U,  1902928727U, 2459288935U, 3941065327U, 1244061689U,
    1898521317U, 4205778491U, 1987240989U, 3446018461U, 2407533397U,
    3151958893U, 1553147067U, 208156801U,  2362352445U, 2458343227U,
    4134443U,    36216853U,   932983869U,  2800766507U, 252990279U,
    2994662963U, 2760285623U, 4510445U,    1458512423U, 3500568231U,
    689831701U,  887836659U,  315834449U,  2394075311U, 1360826347U,
    439713319U,  633358329U,  749540625U,  444867375U,  531150885U,
    2871421439U, 2347294453U, 3975247983U, 3255073387U, 3561466319U,
    2616895667U, 742825395U,  3300710079U, 1231551531U, 3576325163U,
    3229203393U, 2662941725U, 3495109109U, 2202779339U, 2997513035U,
    1952088617U, 2177967115U, 1685362661U, 2160536397U, 2628206479U,
    1678152567U, 775989269U,  2114809421U, 3882162141U, 3267509575U,
    3869378355U, 283353181U,  306744579U,  2793152333U, 1454134621U,
    3021652657U, 1664069155U, 1711688171U, 1264486497U, 359065375U,
    1616608617U,
}};

const std::array<std::uint32_t, 101> kUhC2{{
    0U,          3727790985U, 1655242260U, 422784933U,  2834380338U,
    4079603720U, 1017777578U, 1055049545U, 825468350U,  3746952992U,
    2417510437U, 3900896500U, 3136156509U, 1967993956U, 884863111U,
    4005736455U, 1938983485U, 2483034815U, 1473738861U, 1601812014U,
    1032880017U, 678118779U,  1812018788U, 3051015163U, 2813145762U,
    682451094U,  951775451U,  3820751955U, 2228245394U, 1056831682U,
    427537107U,  2657761231U, 3814309543U, 3334270873U, 3235290147U,
    966385569U,  1334131699U, 2416080521U, 2435664499U, 1659112141U,
    2691180285U, 2923984717U, 221396509U,  1668769566U, 1550424660U,
    380560680U,  842750068U,  1766885112U, 4154190178U, 2485286538U,
    3541066000U, 1618584604U, 2380482404U, 2292025459U, 114224687U,
    2440503753U, 2185819824U, 3056187596U, 1938153078U, 1168725776U,
    816653688U,  3394169238U, 2371002911U, 1307887949U, 593463004U,
    2931928778U, 3974621746U, 2809084272U, 2034840031U, 771132519U,
    8056062U,    1459555392U, 313600432U,  822723327U,  102584381U,
    3018185789U, 396652004U,  1414061560U, 3226032953U, 2027177418U,
    3746841614U, 3506805383U, 184340437U,  169978587U,  294242210U,
    1958086314U, 3662203479U, 251991695U,  2970678332U, 3854518895U,
    3111516179U, 1642607091U, 1669640538U, 3180287192U, 1557513074U,
    3712923940U, 3226089967U, 396996256U,  3520232177U, 1934744235U,
    3239017990U,
}};

std::uint32_t murmurhash2(std::span<const std::uint8_t> data,
                          std::uint32_t seed) noexcept {
  const std::uint32_t m = kMh2M;
  const std::uint32_t r = kMh2R;
  const auto length = static_cast<std::uint32_t>(data.size());
  std::uint32_t h = seed ^ length;

  // Main loop: 4-byte chunks, little-endian.
  std::size_t i = 0;
  while (data.size() - i >= 4) {
    std::uint32_t k = static_cast<std::uint32_t>(data[i]) |
                      (static_cast<std::uint32_t>(data[i + 1]) << 8) |
                      (static_cast<std::uint32_t>(data[i + 2]) << 16) |
                      (static_cast<std::uint32_t>(data[i + 3]) << 24);
    k *= m;
    k ^= k >> r;
    k *= m;
    h *= m;
    h ^= k;
    i += 4;
  }

  // Tail: mirror the fall-through switch(len) in the C reference.
  const auto remaining = data.size() - i;
  if (remaining == 3) {
    h ^= static_cast<std::uint32_t>(data[i + 2]) << 16;
    h ^= static_cast<std::uint32_t>(data[i + 1]) << 8;
    h ^= static_cast<std::uint32_t>(data[i]);
    h *= m;
  } else if (remaining == 2) {
    h ^= static_cast<std::uint32_t>(data[i + 1]) << 8;
    h ^= static_cast<std::uint32_t>(data[i]);
    h *= m;
  } else if (remaining == 1) {
    h ^= static_cast<std::uint32_t>(data[i]);
    h *= m;
  }

  // Final mix.
  h ^= h >> 13;
  h *= m;
  h ^= h >> 15;
  return h;
}

std::vector<std::uint32_t> shingleHashes(std::span<const std::uint8_t> data,
                                         std::size_t k) {
  if (data.empty()) {
    return {0U};
  }
  const auto effectiveK = std::min(data.size(), k);
  const auto windowCount = data.size() - effectiveK + 1;

  std::vector<std::uint32_t> out;
  out.reserve(windowCount);
  for (std::size_t i = 0; i < windowCount; ++i) {
    out.push_back(murmurhash2(data.subspan(i, effectiveK), 0U));
  }
  return out;
}

std::array<std::uint32_t, kSignatureLen> signature(std::string_view text) {
  // UTF-8 byte view — std::string_view already exposes raw bytes.
  std::span<const std::uint8_t> data{
      reinterpret_cast<const std::uint8_t *>(text.data()), text.size()};

  const auto shingles = shingleHashes(data, kShingleK);

  std::array<std::uint32_t, kSignatureLen> out{};
  std::size_t idx = 0;
  for (std::size_t band = 0; band < kBandB; ++band) {
    for (std::size_t row = 0; row < kBandR; ++row) {
      const auto c1 = static_cast<std::uint64_t>(kUhC1[idx]);
      const auto c2 = static_cast<std::uint64_t>(kUhC2[idx]);
      std::uint32_t minHash = 0xFFFFFFFFU;
      for (const auto s : shingles) {
        const auto x = static_cast<std::uint32_t>((c1 * s + c2) % kUhP);
        if (x < minHash) {
          minHash = x;
        }
      }
      out[idx] = minHash;
      ++idx;
      (void)row;
    }
    (void)band;
  }
  return out;
}

std::vector<std::string>
bucketIds(std::string_view prefix,
          std::array<std::uint32_t, kSignatureLen> signature) {
  std::vector<std::string> out;
  out.reserve(kSignatureLen);
  for (std::size_t i = 0; i < kSignatureLen; ++i) {
    std::string s;
    s.reserve(prefix.size() + 16);
    s.append(prefix);
    s.push_back('_');
    s.append(std::to_string(i));
    s.push_back('_');
    s.append(std::to_string(signature[i]));
    out.push_back(std::move(s));
  }
  return out;
}

namespace {

[[nodiscard]] std::string normalize(std::string_view text) {
  // Strip leading whitespace.
  std::size_t start = 0;
  while (start < text.size() &&
         std::isspace(static_cast<unsigned char>(text[start])) != 0) {
    ++start;
  }
  // Strip trailing whitespace.
  std::size_t end = text.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
    --end;
  }
  std::string out;
  out.reserve(end - start);
  for (std::size_t i = start; i < end; ++i) {
    out.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(text[i]))));
  }
  return out;
}

} // namespace

std::vector<std::string> nameMinhashIds(std::string_view firstName,
                                        std::string_view lastName) {
  std::string combined;
  combined.reserve(firstName.size() + lastName.size() + 1);
  combined.append(firstName);
  if (!firstName.empty() && !lastName.empty()) {
    combined.push_back(' ');
  }
  combined.append(lastName);
  return bucketIds("NMH", signature(normalize(combined)));
}

std::vector<std::string> addressMinhashIds(std::string_view fullAddress) {
  return bucketIds("AMH", signature(normalize(fullAddress)));
}

std::vector<std::string> streetMinhashIds(std::string_view streetLine1) {
  return bucketIds("SMH", signature(normalize(streetLine1)));
}

std::string cityMinhashId(std::string_view city) {
  std::string norm = normalize(city);
  for (auto &c : norm) {
    if (c == ' ') {
      c = '_';
    }
  }
  return "CMH_" + norm;
}

std::string stateMinhashId(std::string_view state) {
  std::size_t start = 0;
  while (start < state.size() &&
         std::isspace(static_cast<unsigned char>(state[start])) != 0) {
    ++start;
  }
  std::size_t end = state.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(state[end - 1])) != 0) {
    --end;
  }
  std::string out;
  out.reserve(5 + (end - start));
  out.append("STMH_");
  for (std::size_t i = start; i < end; ++i) {
    out.push_back(
        static_cast<char>(std::toupper(static_cast<unsigned char>(state[i]))));
  }
  return out;
}

} // namespace PhantomLedger::exporter::aml::minhash
