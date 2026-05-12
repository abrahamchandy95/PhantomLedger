#pragma once

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <mutex>

// PhantomLedger logger.
//
// Lightweight, thread-safe, header-only-ish logging facility modelled
// after LLVM's `LLVM_DEBUG(...)` and Folly's `XLOG(CATEGORY, ...)`.
// Zero external dependencies.
//
// Three primary levers:
//
//  1. Compile-time minimum level. Anything below kCompileMinLevel is
//     eliminated by the optimizer (the gate is a constexpr-comparable
//     enum). Default kCompileMinLevel = Level::trace lets all levels
//     through at compile time; tighten to Level::info before tagging
//     a release if you want to remove DEBUG/TRACE call sites entirely.
//
//  2. Runtime level. Set via env var PL_LOG_LEVEL = trace | debug |
//     info | warn | error | off (default: info). Lower-priority lines
//     are silently dropped at runtime.
//
//  3. Topic mask. Set via env var PL_LOG_TOPICS = comma-separated
//     topic names (default: all topics enabled). Example:
//
//       PL_LOG_TOPICS=spending,routing,clearing  PL_LOG_LEVEL=debug \
//         ./phantomledger ...
//
// Macros:
//
//   PL_LOG_TRACE(topic, "fmt %d", a)
//   PL_LOG_DEBUG(topic, "fmt %d", a)
//   PL_LOG_INFO (topic, "fmt %d", a)
//   PL_LOG_WARN (topic, "fmt %d", a)
//   PL_LOG_ERROR(topic, "fmt %d", a)
//
//   PL_LOG_EVERY_N(level, topic, n, "fmt %d", a)  // rate-limited
//
// Macro form is preferred over direct calls because the argument
// evaluation is short-circuited when the level/topic is disabled --
// no allocation, no formatting, no function-call overhead.

namespace PhantomLedger::diagnostics {

enum class Level : std::uint8_t {
  trace = 0,
  debug = 1,
  info = 2,
  warn = 3,
  error = 4,
  off = 5,
};

enum class Topic : std::uint8_t {
  sim = 0,
  spending = 1,
  routing = 2,
  clearing = 3,
  liquidity = 4,
  entities = 5,
  // Add new topics before kCount; update Logger::topicName accordingly.
  kCount = 6,
};

inline constexpr Level kCompileMinLevel = Level::trace;

class Logger {
public:
  static Logger &instance() noexcept;

  // Configure level / topic mask. Called automatically on first
  // instance() access from environment variables; callers may also
  // override at runtime.
  void setLevel(Level level) noexcept;
  void enableTopic(Topic topic, bool on) noexcept;
  void setStream(std::FILE *stream) noexcept;

  [[nodiscard]] Level level() const noexcept {
    return level_.load(std::memory_order_relaxed);
  }

  [[nodiscard]] bool enabled(Level level, Topic topic) const noexcept {
    if (level < kCompileMinLevel) {
      return false;
    }
    if (level < this->level()) {
      return false;
    }
    return (topicMask_.load(std::memory_order_relaxed) &
            (1U << static_cast<std::uint8_t>(topic))) != 0;
  }

  // Thread-safe formatted log. Prefer the macros below over direct
  // calls -- they bypass the entire formatting cost when the line is
  // filtered out.
  void log(Level level, Topic topic, const char *file, int line,
           const char *fmt, ...) noexcept __attribute__((format(printf, 6, 7)));

  // Slot for rate-limited logging.
  [[nodiscard]] std::atomic<std::uint64_t> &
  everyNCounter(std::uintptr_t key) noexcept;

  [[nodiscard]] static const char *levelName(Level level) noexcept;
  [[nodiscard]] static const char *topicName(Topic topic) noexcept;

private:
  Logger();

  void configureFromEnv() noexcept;

  std::atomic<Level> level_{Level::info};
  std::atomic<std::uint32_t> topicMask_{
      (1U << static_cast<std::uint8_t>(Topic::kCount)) - 1U};
  std::FILE *stream_{nullptr};
  std::mutex streamMu_;

  // 16-slot striped counter table for PL_LOG_EVERY_N. We hash the
  // caller's site pointer into this table; collisions are tolerable
  // (a collision means two sites share a rate-limit counter, which
  // skews the rate slightly but is otherwise harmless).
  static constexpr std::size_t kEveryNStripes = 16;
  std::atomic<std::uint64_t> everyN_[kEveryNStripes]{};
};

} // namespace PhantomLedger::diagnostics

// ---------------------------------------------------------------------------
// Macros
// ---------------------------------------------------------------------------

#define PL_LOG_(level_, topic_, ...)                                           \
  do {                                                                         \
    if (::PhantomLedger::diagnostics::kCompileMinLevel <= (level_) &&          \
        ::PhantomLedger::diagnostics::Logger::instance().enabled((level_),     \
                                                                 (topic_))) {  \
      ::PhantomLedger::diagnostics::Logger::instance().log(                    \
          (level_), (topic_), __FILE__, __LINE__, __VA_ARGS__);                \
    }                                                                          \
  } while (0)

#define PL_LOG_TRACE(topic_, ...)                                              \
  PL_LOG_(::PhantomLedger::diagnostics::Level::trace,                          \
          ::PhantomLedger::diagnostics::Topic::topic_, __VA_ARGS__)

#define PL_LOG_DEBUG(topic_, ...)                                              \
  PL_LOG_(::PhantomLedger::diagnostics::Level::debug,                          \
          ::PhantomLedger::diagnostics::Topic::topic_, __VA_ARGS__)

#define PL_LOG_INFO(topic_, ...)                                               \
  PL_LOG_(::PhantomLedger::diagnostics::Level::info,                           \
          ::PhantomLedger::diagnostics::Topic::topic_, __VA_ARGS__)

#define PL_LOG_WARN(topic_, ...)                                               \
  PL_LOG_(::PhantomLedger::diagnostics::Level::warn,                           \
          ::PhantomLedger::diagnostics::Topic::topic_, __VA_ARGS__)

#define PL_LOG_ERROR(topic_, ...)                                              \
  PL_LOG_(::PhantomLedger::diagnostics::Level::error,                          \
          ::PhantomLedger::diagnostics::Topic::topic_, __VA_ARGS__)

#define PL_LOG_EVERY_N(level_, topic_, n_, ...)                                \
  do {                                                                         \
    if (::PhantomLedger::diagnostics::kCompileMinLevel <= (level_) &&          \
        ::PhantomLedger::diagnostics::Logger::instance().enabled((level_),     \
                                                                 (topic_))) {  \
      static const char kPlSiteKey_ = 0;                                       \
      auto &counter_ =                                                         \
          ::PhantomLedger::diagnostics::Logger::instance().everyNCounter(      \
              reinterpret_cast<std::uintptr_t>(&kPlSiteKey_));                 \
      const auto cnt_ = counter_.fetch_add(1, std::memory_order_relaxed) + 1U; \
      if ((cnt_ % static_cast<std::uint64_t>(n_)) == 0U) {                     \
        ::PhantomLedger::diagnostics::Logger::instance().log(                  \
            (level_), (topic_), __FILE__, __LINE__, __VA_ARGS__);              \
      }                                                                        \
    }                                                                          \
  } while (0)
