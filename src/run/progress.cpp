#include "phantomledger/run/progress.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <utility>

namespace PhantomLedger::run::progress {

namespace {

std::atomic<bool> g_enabled{false};

constexpr auto kRenderInterval = std::chrono::milliseconds{100};

constexpr std::size_t kBarWidth = 32;

[[nodiscard]] std::string formatElapsed(std::chrono::steady_clock::duration d) {
  using namespace std::chrono;
  const auto totalSec = duration_cast<duration<double>>(d).count();
  char buf[32];
  if (totalSec < 60.0) {
    std::snprintf(buf, sizeof(buf), "%.1fs", totalSec);
  } else {
    const auto mins = static_cast<int>(totalSec) / 60;
    const auto secs = static_cast<int>(totalSec) % 60;
    std::snprintf(buf, sizeof(buf), "%dm%02ds", mins, secs);
  }
  return buf;
}

} // namespace

void setEnabled(bool e) noexcept {
  g_enabled.store(e, std::memory_order_relaxed);
}

bool enabled() noexcept { return g_enabled.load(std::memory_order_relaxed); }

void status(std::string_view message) {
  if (!enabled()) {
    return;
  }
  std::cerr << message << '\n';
}

// ────────────────────────────── Stage ──────────────────────────────

Stage::Stage(std::string label, std::size_t total)
    : label_(std::move(label)), total_(total),
      startTime_(std::chrono::steady_clock::now()), lastRender_(startTime_),
      active_(enabled() && total > 0) {
  if (active_) {
    render(/*force=*/true);
  }
}

Stage::~Stage() { done(); }

void Stage::tick(std::size_t n) noexcept {
  if (!active_) {
    return;
  }
  current_ += n;
  if (current_ > total_) {
    current_ = total_;
  }
  render(/*force=*/false);
}

void Stage::setProgress(std::size_t current) noexcept {
  if (!active_) {
    return;
  }
  current_ = (current > total_) ? total_ : current;
  render(/*force=*/false);
}

void Stage::done() noexcept {
  if (!active_) {
    return;
  }
  current_ = total_;
  render(/*force=*/true);
  std::cerr << '\n';
  active_ = false;
}

void Stage::render(bool force) noexcept {
  const auto now = std::chrono::steady_clock::now();
  if (!force && (now - lastRender_) < kRenderInterval) {
    return;
  }
  lastRender_ = now;

  const auto pct = (total_ == 0) ? 0.0
                                 : (100.0 * static_cast<double>(current_) /
                                    static_cast<double>(total_));
  const auto filled =
      (total_ == 0) ? std::size_t{0} : (kBarWidth * current_ / total_);

  std::string bar;
  bar.reserve(kBarWidth + 2);
  bar.push_back('[');
  for (std::size_t i = 0; i < kBarWidth; ++i) {
    bar.push_back(i < filled ? '=' : (i == filled ? '>' : ' '));
  }
  bar.push_back(']');

  const auto elapsed = formatElapsed(now - startTime_);

  std::cerr << '\r' << label_ << ' ' << bar << ' ' << current_ << '/' << total_
            << " (" << static_cast<int>(pct) << "%)"
            << " [" << elapsed << "]    ";
  std::cerr.flush();
}

} // namespace PhantomLedger::run::progress
