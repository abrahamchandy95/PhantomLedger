#include "phantomledger/diagnostics/logger.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <format>
#include <string>
#include <string_view>

namespace PhantomLedger::diagnostics {

namespace {

[[nodiscard]] Level parseLevel(std::string_view s) noexcept {
  std::string lower(s);
  for (auto &c : lower) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  if (lower == "trace")
    return Level::trace;
  if (lower == "debug")
    return Level::debug;
  if (lower == "info")
    return Level::info;
  if (lower == "warn" || lower == "warning")
    return Level::warn;
  if (lower == "error")
    return Level::error;
  if (lower == "off" || lower == "none" || lower == "silent")
    return Level::off;
  return Level::warn;
}

[[nodiscard]] bool parseTopicAndSet(std::string_view tok,
                                    Logger &logger) noexcept {
  std::string lower(tok);
  for (auto &c : lower) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  while (!lower.empty() &&
         std::isspace(static_cast<unsigned char>(lower.front()))) {
    lower.erase(lower.begin());
  }
  while (!lower.empty() &&
         std::isspace(static_cast<unsigned char>(lower.back()))) {
    lower.pop_back();
  }
  if (lower.empty()) {
    return false;
  }
  if (lower == "all" || lower == "*") {
    for (std::uint8_t i = 0; i < static_cast<std::uint8_t>(Topic::kCount);
         ++i) {
      logger.enableTopic(static_cast<Topic>(i), true);
    }
    return true;
  }
  for (std::uint8_t i = 0; i < static_cast<std::uint8_t>(Topic::kCount); ++i) {
    if (lower == Logger::topicName(static_cast<Topic>(i))) {
      logger.enableTopic(static_cast<Topic>(i), true);
      return true;
    }
  }
  return false;
}

/* Wall-clock time of day. std::chrono's time zone database is unavailable on
 * this toolchain, so the local offset comes from libc. */
[[nodiscard]] std::string localTimeOfDay() noexcept {
  const std::time_t now = std::time(nullptr);
  std::tm tm{};
#ifdef _WIN32
  (void)localtime_s(&tm, &now);
#else
  (void)localtime_r(&now, &tm);
#endif
  return std::format("{:02}:{:02}:{:02}", tm.tm_hour, tm.tm_min, tm.tm_sec);
}

[[nodiscard]] const char *baseName(const char *path) noexcept {
  const char *base = path;
  for (const char *p = path; *p != '\0'; ++p) {
    if (*p == '/' || *p == '\\')
      base = p + 1;
  }
  return base;
}

} // namespace

Logger &Logger::instance() noexcept {
  static Logger inst;
  return inst;
}

Logger::Logger() : stream_(stderr) { configureFromEnv(); }

void Logger::configureFromEnv() noexcept {
  if (const char *lvl = std::getenv("PL_LOG_LEVEL")) {
    level_.store(parseLevel(lvl), std::memory_order_relaxed);
  }
  if (const char *topics = std::getenv("PL_LOG_TOPICS")) {
    topicMask_.store(0, std::memory_order_relaxed);

    std::string_view view(topics);
    std::size_t start = 0;
    while (start <= view.size()) {
      std::size_t end = view.find(',', start);
      if (end == std::string_view::npos)
        end = view.size();
      (void)parseTopicAndSet(view.substr(start, end - start), *this);
      if (end == view.size())
        break;
      start = end + 1;
    }
  }
}

void Logger::setLevel(Level level) noexcept {
  level_.store(level, std::memory_order_relaxed);
}

void Logger::enableTopic(Topic topic, bool on) noexcept {
  const auto bit = 1U << static_cast<std::uint8_t>(topic);
  auto cur = topicMask_.load(std::memory_order_relaxed);
  while (true) {
    const auto next = on ? (cur | bit) : (cur & ~bit);
    if (topicMask_.compare_exchange_weak(cur, next,
                                         std::memory_order_relaxed)) {
      return;
    }
  }
}

void Logger::setStream(std::FILE *stream) noexcept {
  std::lock_guard lock(streamMutex_);
  stream_ = stream;
}

void Logger::writeLog(Metadata meta, std::string_view fmt,
                      std::format_args args) noexcept {
  /* Formatted outside the lock so only the write itself serializes. A
   * malformed format string throws rather than corrupting the stream. */
  std::string line;
  try {
    line = std::format("[{}] [{:<5}] [{:<9}] {}:{}  {}\n", localTimeOfDay(),
                       levelName(meta.level), topicName(meta.topic),
                       baseName(meta.file), meta.line, std::vformat(fmt, args));
  } catch (...) {
    return;
  }

  std::lock_guard lock(streamMutex_);
  if (stream_ == nullptr) {
    return;
  }
  (void)std::fwrite(line.data(), 1, line.size(), stream_);
  (void)std::fflush(stream_);
}

std::atomic<std::uint64_t> &Logger::everyNCounter(std::uintptr_t key) noexcept {
  const auto idx = (key * 2654435761U) % kEveryNStripes;
  return everyN_[idx];
}

const char *Logger::levelName(Level level) noexcept {
  switch (level) {
  case Level::trace:
    return "TRACE";
  case Level::debug:
    return "DEBUG";
  case Level::info:
    return "INFO";
  case Level::warn:
    return "WARN";
  case Level::error:
    return "ERROR";
  case Level::off:
    return "OFF";
  }
  return "?";
}

const char *Logger::topicName(Topic topic) noexcept {
  switch (topic) {
  case Topic::sim:
    return "sim";
  case Topic::spending:
    return "spending";
  case Topic::routing:
    return "routing";
  case Topic::clearing:
    return "clearing";
  case Topic::liquidity:
    return "liquidity";
  case Topic::entities:
    return "entities";
  case Topic::mem:
    return "mem";
  case Topic::kCount:
    return "?";
  }
  return "?";
}

} // namespace PhantomLedger::diagnostics
