#include "phantomledger/diagnostics/logger.hpp"

#include <array>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
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
  return Level::info;
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
    // PL_LOG_TOPICS set => start with everything off and enable only
    // the explicitly listed topics. Default (env not set) leaves all
    // topics on.
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
  std::lock_guard lock(streamMu_);
  stream_ = stream;
}

void Logger::log(Level level, Topic topic, const char *file, int line,
                 const char *fmt, ...) noexcept {
  std::array<char, 1024> buf{};
  std::array<char, 32> tbuf{};

  const std::time_t now = std::time(nullptr);
  std::tm tm{};
#ifdef _WIN32
  // localtime_s populates the tm by side effect; the error code is
  // not material for log formatting.
  (void)localtime_s(&tm, &now);
#else
  // localtime_r returns the pointer it was passed; we already hold
  // it. Casting to void silences the nodiscard attribute that
  // newer libc / clang headers attach to the POSIX wrapper.
  (void)localtime_r(&now, &tm);
#endif
  (void)std::strftime(tbuf.data(), tbuf.size(), "%H:%M:%S", &tm);

  // Strip file path down to basename.
  const char *base = file;
  for (const char *p = file; *p; ++p) {
    if (*p == '/' || *p == '\\')
      base = p + 1;
  }

  va_list ap;
  va_start(ap, fmt);
  (void)std::vsnprintf(buf.data(), buf.size(), fmt, ap);
  va_end(ap);

  std::lock_guard lock(streamMu_);
  if (stream_ == nullptr) {
    return;
  }
  std::fprintf(stream_, "[%s] [%-5s] [%-9s] %s:%d  %s\n", tbuf.data(),
               levelName(level), topicName(topic), base, line, buf.data());
  std::fflush(stream_);
}

std::atomic<std::uint64_t> &Logger::everyNCounter(std::uintptr_t key) noexcept {
  // Simple multiplicative hash; collisions are tolerable.
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
  case Topic::kCount:
    return "?";
  }
  return "?";
}

} // namespace PhantomLedger::diagnostics
