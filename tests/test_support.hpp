#pragma once

#include <cstdio>
#include <cstdlib>

#define PL_CHECK(cond)                                                         \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "\n[FAIL] %s:%d: CHECK(%s)\n", __FILE__, __LINE__,  \
                   #cond);                                                     \
      std::abort();                                                            \
    }                                                                          \
  } while (0)

#define PL_CHECK_EQ(lhs, rhs)                                                  \
  do {                                                                         \
    const auto plLhs_ = (lhs);                                                 \
    const auto plRhs_ = (rhs);                                                 \
    if (!(plLhs_ == plRhs_)) {                                                 \
      std::fprintf(stderr, "\n[FAIL] %s:%d: CHECK_EQ(%s, %s)\n", __FILE__,     \
                   __LINE__, #lhs, #rhs);                                      \
      std::abort();                                                            \
    }                                                                          \
  } while (0)

#define PL_CHECK_THROWS(expr)                                                  \
  do {                                                                         \
    bool plThrew_ = false;                                                     \
    try {                                                                      \
      (void)(expr);                                                            \
    } catch (...) {                                                            \
      plThrew_ = true;                                                         \
    }                                                                          \
    if (!plThrew_) {                                                           \
      std::fprintf(stderr, "\n[FAIL] %s:%d: CHECK_THROWS(%s)\n", __FILE__,     \
                   __LINE__, #expr);                                           \
      std::abort();                                                            \
    }                                                                          \
  } while (0)
