#include "phantomledger/diagnostics/logger.hpp"

#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <print>
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
  std::lock_guard lock(streamMu_);
  stream_ = stream;
}

void Logger::writeLog(Metadata ctx, std::string_view fmt,
                      std::format_args args) noexcept {
  const char *base = ctx.file;
  for (const char *p = ctx.file; *p; ++p) {
    if (*p == '/' || *p == '\\')
      base = p + 1;
  }

  std::lock_guard lock(streamMu_);
  if (stream_ == nullptr) {
    return;
  }

  try {
    const auto now = std::chrono::system_clock::now();
    const auto time_fmt = std::chrono::current_zone()->to_local(now);

    std::print(stream_, "[{:%H:%M:%S}] [{:<5}] [{:<9}] {}:{}  ", time_fmt,
               levelName(ctx.level), topicName(ctx.topic), base, ctx.line);

    std::vprint_nonunicode(stream_, fmt, args);

    std::println(stream_);
    std::fflush(stream_);
  } catch (...) {
  }
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
