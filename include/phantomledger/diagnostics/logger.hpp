#pragma once

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <format>
#include <mutex>
#include <string_view>

/*
  Logger
  Lightweight, thread-safe, zero-dependency logging.

  Configuration:
  1. Compile-time: kCompileMinLevel (eliminates dead branches).
  2. Runtime Level: PL_LOG_LEVEL (trace|debug|info|warn|error|off). Default:
  warn.
  3. Runtime Topic: PL_LOG_TOPICS (comma-separated topics). Default: all.

  Usage. The topic is named bare; the format string is std::format, not
  printf. Arguments are only evaluated when the level and topic are both
  enabled.
  PL_LOG_INFO(sim, "day {} emitted {} rows", day, rows);
  PL_LOG_EVERY_N(Level::warn, Topic::mem, 100, "peak {:.1f} MB", mb);
 */

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
  mem = 6,
  /* Add new topics before kCount; update Logger::topicName accordingly. */
  kCount = 7,
};

inline constexpr Level kCompileMinLevel = Level::trace;

/* Where a log line came from, and under what severity. */
struct Metadata {
  Level level;
  Topic topic;
  const char *file;
  int line;
};

class Logger {
public:
  static Logger &instance() noexcept;

  /* Configure level/topic mask. Defaults to env vars on first access. */
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

  template <typename... Args>
  void log(Metadata meta, std::format_string<Args...> fmt,
           Args &&...args) noexcept {
    writeLog(meta, fmt.get(), std::make_format_args(args...));
  }

  /* Rate-limited logging state */
  [[nodiscard]] std::atomic<std::uint64_t> &
  everyNCounter(std::uintptr_t key) noexcept;

  [[nodiscard]] static const char *levelName(Level level) noexcept;
  [[nodiscard]] static const char *topicName(Topic topic) noexcept;

private:
  Logger();

  void configureFromEnv() noexcept;

  void writeLog(Metadata meta, std::string_view fmt,
                std::format_args args) noexcept;

  std::atomic<Level> level_{Level::warn};
  std::atomic<std::uint32_t> topicMask_{
      (1U << static_cast<std::uint8_t>(Topic::kCount)) - 1U};
  std::FILE *stream_{nullptr};
  std::mutex streamMutex_;

  /* 16-slot striped counter table for PL_LOG_EVERY_N. Collisions skew rates
   * slightly but are harmless. */
  static constexpr std::size_t kEveryNStripes = 16;
  std::atomic<std::uint64_t> everyN_[kEveryNStripes]{};
};

} // namespace PhantomLedger::diagnostics

/*
 * ---------------------------------------------------------------------------
 * Macros
 * ---------------------------------------------------------------------------
 */

#define PL_LOG_(level_, topic_, ...)                                           \
  do {                                                                         \
    if (::PhantomLedger::diagnostics::kCompileMinLevel <= (level_) &&          \
        ::PhantomLedger::diagnostics::Logger::instance().enabled((level_),     \
                                                                 (topic_))) {  \
      const ::PhantomLedger::diagnostics::Metadata meta_{(level_), (topic_),   \
                                                         __FILE__, __LINE__};  \
      ::PhantomLedger::diagnostics::Logger::instance().log(meta_,              \
                                                           __VA_ARGS__);       \
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
        const ::PhantomLedger::diagnostics::Metadata meta_{                    \
            (level_), (topic_), __FILE__, __LINE__};                           \
        ::PhantomLedger::diagnostics::Logger::instance().log(meta_,            \
                                                             __VA_ARGS__);     \
      }                                                                        \
    }                                                                          \
  } while (0)
