"""
MinHash signatures for entity resolution — byte-for-byte compatible
with TigerGraph's reference ``TokenBank.cpp``.

Reference:
    https://github.com/TigerGraph-DevLabs/minhash-based-fuzzy-match-on-graph/blob/main/TokenBank.cpp

Contract
--------
Given the same (text, shingle_k, b, r) inputs, :func:`_minhash_signature`
returns the same 32-bit unsigned integer signatures as TigerGraph's
``minHash`` UDF. Specifically:

* Shingles are *byte* (not character) windows over the UTF-8 encoding
  of the input, matching the C reference that indexes ``char*``.
* Each shingle is hashed with MurmurHash2 (Austin Appleby), 32-bit,
  seed=0 — the exact function in ``TokenBank.cpp``. 4-byte chunks are
  read little-endian (matches x86/x64 ``*(uint32_t*)data``).
* The universal hash family is ``(c1[idx] * x + c2[idx]) mod p`` with
  ``p = 4_294_967_311`` and the exact 101-element c1/c2 tables from
  ``TokenBank.cpp``.
* ``idx`` increments once per ``(band, row)`` pair — matching the C
  control flow (bug-fixed version).

Public API preserves the previous function signatures so the rest of
the exporter keeps working without changes.

Normalization
-------------
The public helpers lower-case and strip the input so that
``"John Smith"`` and ``"JOHN SMITH"`` cluster. To exactly match a
TigerGraph deployment that ingests raw (non-normalized) strings,
call :func:`_minhash_signature` directly or pre-normalize input going
into TigerGraph the same way.

Vertex IDs
----------
Bucket IDs are emitted as ``"{PREFIX}_{band_index}_{hash}"``. The band
index keeps LSH bands independent (two strings that happen to collide
in band 0 and band 3 don't merge those bands' vertices). TigerGraph's
native vertex IDs omit the band index — if you need exact ID match,
change :func:`_bucket_ids` to ``str(sig)`` and tolerate the slightly
higher cross-band collision rate.

Reference C source
------------------
The standalone C program used to generate the byte-for-byte reference
values this module is tested against lives at the bottom of this file
as ``_TG_REFERENCE_C_SOURCE``. It contains a verbatim extract of
TigerGraph's ``MurmurHash2`` and minhash core plus a ``main()`` that
prints test vectors to stdout. When you migrate to C++, lift it into
your test harness and compute reference values at runtime instead of
hardcoding them.
"""

from __future__ import annotations

# ─── MurmurHash2 32-bit (Austin Appleby) ─────────────────────────────
_MH2_M: int = 0x5BD1E995
_MH2_R: int = 24
_MASK_32: int = 0xFFFFFFFF

# ─── TigerGraph universal-hash-family parameters ─────────────────────
# Prime strictly greater than 2**32 − 1 so (c1*x + c2) % p covers the
# full uint32 output range.
_UH_P: int = 4_294_967_311

# Exact c1[0..100] / c2[0..100] from TokenBank.cpp.
# b*r must remain ≤ 100.
_UH_C1: tuple[int, ...] = (
    1,
    482067061,
    1133999723,
    2977257653,
    2666089543,
    3098200677,
    3985189319,
    1857983931,
    3801236429,
    522456919,
    4057595205,
    4176190031,
    1652234975,
    2294716503,
    1644020081,
    3407377875,
    3749518465,
    4244672803,
    3053397733,
    3273255487,
    598272097,
    3989043777,
    1414109747,
    697129027,
    67285677,
    98002333,
    158583451,
    1424122447,
    2159224677,
    3478101309,
    277468927,
    1902928727,
    2459288935,
    3941065327,
    1244061689,
    1898521317,
    4205778491,
    1987240989,
    3446018461,
    2407533397,
    3151958893,
    1553147067,
    208156801,
    2362352445,
    2458343227,
    4134443,
    36216853,
    932983869,
    2800766507,
    252990279,
    2994662963,
    2760285623,
    4510445,
    1458512423,
    3500568231,
    689831701,
    887836659,
    315834449,
    2394075311,
    1360826347,
    439713319,
    633358329,
    749540625,
    444867375,
    531150885,
    2871421439,
    2347294453,
    3975247983,
    3255073387,
    3561466319,
    2616895667,
    742825395,
    3300710079,
    1231551531,
    3576325163,
    3229203393,
    2662941725,
    3495109109,
    2202779339,
    2997513035,
    1952088617,
    2177967115,
    1685362661,
    2160536397,
    2628206479,
    1678152567,
    775989269,
    2114809421,
    3882162141,
    3267509575,
    3869378355,
    283353181,
    306744579,
    2793152333,
    1454134621,
    3021652657,
    1664069155,
    1711688171,
    1264486497,
    359065375,
    1616608617,
)
assert len(_UH_C1) == 101, "c1 table must have exactly 101 coefficients"

_UH_C2: tuple[int, ...] = (
    0,
    3727790985,
    1655242260,
    422784933,
    2834380338,
    4079603720,
    1017777578,
    1055049545,
    825468350,
    3746952992,
    2417510437,
    3900896500,
    3136156509,
    1967993956,
    884863111,
    4005736455,
    1938983485,
    2483034815,
    1473738861,
    1601812014,
    1032880017,
    678118779,
    1812018788,
    3051015163,
    2813145762,
    682451094,
    951775451,
    3820751955,
    2228245394,
    1056831682,
    427537107,
    2657761231,
    3814309543,
    3334270873,
    3235290147,
    966385569,
    1334131699,
    2416080521,
    2435664499,
    1659112141,
    2691180285,
    2923984717,
    221396509,
    1668769566,
    1550424660,
    380560680,
    842750068,
    1766885112,
    4154190178,
    2485286538,
    3541066000,
    1618584604,
    2380482404,
    2292025459,
    114224687,
    2440503753,
    2185819824,
    3056187596,
    1938153078,
    1168725776,
    816653688,
    3394169238,
    2371002911,
    1307887949,
    593463004,
    2931928778,
    3974621746,
    2809084272,
    2034840031,
    771132519,
    8056062,
    1459555392,
    313600432,
    822723327,
    102584381,
    3018185789,
    396652004,
    1414061560,
    3226032953,
    2027177418,
    3746841614,
    3506805383,
    184340437,
    169978587,
    294242210,
    1958086314,
    3662203479,
    251991695,
    2970678332,
    3854518895,
    3111516179,
    1642607091,
    1669640538,
    3180287192,
    1557513074,
    3712923940,
    3226089967,
    396996256,
    3520232177,
    1934744235,
    3239017990,
)
assert len(_UH_C2) == 101, "c2 table must have exactly 101 coefficients"

# ─── MinHash parameters (TigerGraph blog defaults) ───────────────────
_SHINGLE_K: int = 3
_BAND_B: int = 10  # 10 bands → 10 signatures per field (one per band) when r=1
_BAND_R: int = 1
assert _BAND_B * _BAND_R <= 100


# ─── core hash functions ─────────────────────────────────────────────


def _murmurhash2(data: bytes, seed: int = 0) -> int:
    """
    Austin Appleby's 32-bit MurmurHash2 — pure-Python port of the
    implementation in TigerGraph's ``TokenBank.cpp``.

    4-byte chunks are read little-endian, matching x86/x64
    ``*(uint32_t*)data``. The tail switch falls through exactly as in
    the C version.
    """
    m = _MH2_M
    r = _MH2_R
    length = len(data)
    h = (seed ^ length) & _MASK_32

    # Main loop: 4-byte chunks.
    i = 0
    while length - i >= 4:
        k = int.from_bytes(data[i : i + 4], "little", signed=False)
        k = (k * m) & _MASK_32
        k ^= k >> r
        k = (k * m) & _MASK_32
        h = (h * m) & _MASK_32
        h ^= k
        i += 4

    # Tail: mirror the fall-through switch(len) in the C reference.
    remaining = length - i
    if remaining == 3:
        h ^= data[i + 2] << 16
        h ^= data[i + 1] << 8
        h ^= data[i]
        h = (h * m) & _MASK_32
    elif remaining == 2:
        h ^= data[i + 1] << 8
        h ^= data[i]
        h = (h * m) & _MASK_32
    elif remaining == 1:
        h ^= data[i]
        h = (h * m) & _MASK_32

    # Final mix.
    h ^= h >> 13
    h = (h * m) & _MASK_32
    h ^= h >> 15
    return h & _MASK_32


def _shingle_hashes(data: bytes, k: int) -> list[int]:
    """
    Extract length-``k`` byte shingles over ``data`` and hash each with
    MurmurHash2. Matches the loop in ``TokenBank.cpp``:

        k = min(iTokenLen[0], shingleLen);
        for (int i = 0; i < iTokenLen[0] - k + 1; ++i)
            signatures.push_back(MurmurHash2(&iToken[0][i], k, 0));

    The multiset is preserved for parity with the C reference; duplicate
    shingles hash identically under the universal family anyway, so the
    minimum is unaffected.
    """
    length = len(data)
    if length == 0:
        # Same edge case as the C reference: one "shingle" of length 0,
        # which hashes to MurmurHash2(ptr, 0, 0) = 0.
        return [0]
    k = min(length, k)
    return [_murmurhash2(data[i : i + k], 0) for i in range(length - k + 1)]


def _minhash_signature(
    text: str,
    *,
    k: int = _SHINGLE_K,
    b: int = _BAND_B,
    r: int = _BAND_R,
) -> tuple[int, ...]:
    """
    Byte-for-byte TigerGraph-compatible MinHash signature.

    Returns a tuple of ``b * r`` uint32 signatures. Does NOT normalize
    the input — caller is responsible for any case-folding / stripping
    so that the Python side and the TigerGraph ingest side see the
    identical byte sequence.
    """
    if b * r > 100:
        raise ValueError(f"b*r must be <= 100, got {b * r}")

    data = text.encode("utf-8")
    shingle_hashes = _shingle_hashes(data, k)

    out: list[int] = []
    idx = 0
    for _band in range(b):
        for _row in range(r):
            c1 = _UH_C1[idx]
            c2 = _UH_C2[idx]
            min_hash = _MASK_32  # equivalent to C's 0xFFFFFFFF
            for s in shingle_hashes:
                x = (c1 * s + c2) % _UH_P
                if x < min_hash:
                    min_hash = x
            idx += 1
            out.append(min_hash)

    return tuple(out)


def _bucket_ids(prefix: str, signature: tuple[int, ...]) -> list[str]:
    """Emit one ``{prefix}_{band_index}_{hash}`` ID per signature slot."""
    return [f"{prefix}_{i}_{h}" for i, h in enumerate(signature)]


# ─── public API (signatures unchanged from the previous version) ─────


def _normalize(text: str) -> str:
    return text.lower().strip()


def name_minhash_ids(first_name: str, last_name: str) -> list[str]:
    """Bucket IDs for ``Name_MinHash`` vertices."""
    full = _normalize(f"{first_name} {last_name}".strip())
    sig = _minhash_signature(full)
    return _bucket_ids("NMH", sig)


def address_minhash_ids(full_address: str) -> list[str]:
    """Bucket IDs for ``Address_MinHash`` vertices."""
    sig = _minhash_signature(_normalize(full_address))
    return _bucket_ids("AMH", sig)


def street_minhash_ids(street_line1: str) -> list[str]:
    """Bucket IDs for ``Street_Line1_MinHash`` vertices."""
    sig = _minhash_signature(_normalize(street_line1))
    return _bucket_ids("SMH", sig)


def city_minhash_id(city: str) -> str:
    """Single bucket ID for ``City_MinHash`` (exact normalized match)."""
    return f"CMH_{city.lower().strip().replace(' ', '_')}"


def state_minhash_id(state: str) -> str:
    """Single bucket ID for ``State_MinHash`` (exact normalized match)."""
    return f"STMH_{state.upper().strip()}"


_TG_REFERENCE_C_SOURCE: str = r"""
/*
 * Standalone extract of TigerGraph's MurmurHash2 and minhash core,
 * verbatim from:
 *   https://github.com/TigerGraph-DevLabs/minhash-based-fuzzy-match-on-graph/blob/main/TokenBank.cpp
 *
 * Strips TigerGraph loader plumbing but preserves the hash math
 * exactly. Emits known-good test vectors.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* --- verbatim from TokenBank.cpp --- */
uint32_t MurmurHash2 (const void * key, int len, uint32_t seed)
{
    const uint32_t m = 0x5bd1e995;
    const int r = 24;
    uint32_t h = seed ^ len;
    const unsigned char * data = (const unsigned char *)key;
    while(len >= 4)
    {
        uint32_t k = *(uint32_t*)data;
        k *= m;
        k ^= k >> r;
        k *= m;
        h *= m;
        h ^= k;
        data += 4;
        len -= 4;
    }
    switch(len)
    {
        case 3: h ^= data[2] << 16;
        case 2: h ^= data[1] << 8;
        case 1: h ^= data[0];
                h *= m;
    };
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;
    return h;
}

/* --- verbatim coefficients from TokenBank.cpp --- */
static uint32_t c1[101] = {1, 482067061, 1133999723, 2977257653, 2666089543, 3098200677, 3985189319, 1857983931, 3801236429, 522456919, 4057595205, 4176190031, 1652234975, 2294716503, 1644020081, 3407377875, 3749518465, 4244672803, 3053397733, 3273255487, 598272097, 3989043777, 1414109747, 697129027, 67285677, 98002333, 158583451, 1424122447, 2159224677, 3478101309, 277468927, 1902928727, 2459288935, 3941065327, 1244061689, 1898521317, 4205778491, 1987240989, 3446018461, 2407533397, 3151958893, 1553147067, 208156801, 2362352445, 2458343227, 4134443, 36216853, 932983869, 2800766507, 252990279, 2994662963, 2760285623, 4510445, 1458512423, 3500568231, 689831701, 887836659, 315834449, 2394075311, 1360826347, 439713319, 633358329, 749540625, 444867375, 531150885, 2871421439, 2347294453, 3975247983, 3255073387, 3561466319, 2616895667, 742825395, 3300710079, 1231551531, 3576325163, 3229203393, 2662941725, 3495109109, 2202779339, 2997513035, 1952088617, 2177967115, 1685362661, 2160536397, 2628206479, 1678152567, 775989269, 2114809421, 3882162141, 3267509575, 3869378355, 283353181, 306744579, 2793152333, 1454134621, 3021652657, 1664069155, 1711688171, 1264486497, 359065375, 1616608617};
static uint32_t c2[101] = {0, 3727790985, 1655242260, 422784933, 2834380338, 4079603720, 1017777578, 1055049545, 825468350, 3746952992, 2417510437, 3900896500, 3136156509, 1967993956, 884863111, 4005736455, 1938983485, 2483034815, 1473738861, 1601812014, 1032880017, 678118779, 1812018788, 3051015163, 2813145762, 682451094, 951775451, 3820751955, 2228245394, 1056831682, 427537107, 2657761231, 3814309543, 3334270873, 3235290147, 966385569, 1334131699, 2416080521, 2435664499, 1659112141, 2691180285, 2923984717, 221396509, 1668769566, 1550424660, 380560680, 842750068, 1766885112, 4154190178, 2485286538, 3541066000, 1618584604, 2380482404, 2292025459, 114224687, 2440503753, 2185819824, 3056187596, 1938153078, 1168725776, 816653688, 3394169238, 2371002911, 1307887949, 593463004, 2931928778, 3974621746, 2809084272, 2034840031, 771132519, 8056062, 1459555392, 313600432, 822723327, 102584381, 3018185789, 396652004, 1414061560, 3226032953, 2027177418, 3746841614, 3506805383, 184340437, 169978587, 294242210, 1958086314, 3662203479, 251991695, 2970678332, 3854518895, 3111516179, 1642607091, 1669640538, 3180287192, 1557513074, 3712923940, 3226089967, 396996256, 3520232177, 1934744235, 3239017990};

/* --- minhash core (hash-producing portion of TokenBank.cpp::minHash) --- */
static void minhash(const char* str, uint32_t str_len, int shingleLen, int b, int r,
                    uint32_t* out /* length b*r */)
{
    assert(b * r <= 100);
    uint32_t k = (str_len < (uint32_t)shingleLen) ? str_len : (uint32_t)shingleLen;

    uint32_t signatures[4096];
    uint32_t sig_count = 0;
    if (str_len == 0) {
        /* matches C reference: len=0, k=0 -> loop runs once, MurmurHash2(ptr,0,0)=0 */
        signatures[sig_count++] = MurmurHash2(str, 0, 0);
    } else {
        for (uint32_t i = 0; i + k <= str_len; ++i) {
            signatures[sig_count++] = MurmurHash2(&str[i], (int)k, 0);
        }
    }

    uint64_t p = 4294967311ULL;
    int idx = 0;
    int oi = 0;
    for (int i = 0; i < b; ++i) {
        for (int j = 0; j < r; ++j) {
            uint32_t minHash = 0xFFFFFFFF;
            for (uint32_t s_i = 0; s_i < sig_count; ++s_i) {
                uint32_t s = signatures[s_i];
                uint32_t x = (uint32_t)(((uint64_t)c1[idx] * s + c2[idx]) % p);
                if (x < minHash) minHash = x;
            }
            ++idx;
            out[oi++] = minHash;
        }
    }
}

int main(void) {
    /* 1. MurmurHash2 test vectors across byte lengths */
    const char* mh2_tests[] = {
        "",
        "a",
        "ab",
        "abc",
        "abcd",
        "abcde",
        "Hello World",
        "TigerGraph",
        "123 Main St New York NY 10001",
        "john smith"
    };
    printf("# MurmurHash2 test vectors (seed=0)\n");
    for (int i = 0; i < (int)(sizeof(mh2_tests)/sizeof(mh2_tests[0])); i++) {
        uint32_t h = MurmurHash2(mh2_tests[i], (int)strlen(mh2_tests[i]), 0);
        printf("MH2 | %-40s | %u\n", mh2_tests[i], h);
    }

    /* 2. Full minhash test vectors (k=3, b=10, r=1) */
    const char* mh_tests[] = {
        "TigerGraph",
        "TiiigerGraph",
        "Tiger Grph",
        "Sheila M. Swinton",
        "Sheila Swinton",
        "Beverly Farmer",
        "Bev Farmer",
        "Crystal Pablo",
        "Chrissy Pablo",
        "john smith",
        "JOHN SMITH",
        "123 main st new york ny 10001",
        "456 oak ave los angeles ca 90012"
    };
    printf("\n# MinHash signatures (k=3, b=10, r=1)\n");
    for (int i = 0; i < (int)(sizeof(mh_tests)/sizeof(mh_tests[0])); i++) {
        uint32_t sig[10];
        minhash(mh_tests[i], (uint32_t)strlen(mh_tests[i]), 3, 10, 1, sig);
        printf("MH  | %-36s |", mh_tests[i]);
        for (int j = 0; j < 10; j++) printf(" %u", sig[j]);
        printf("\n");
    }
    return 0;
}
"""
