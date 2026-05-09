#pragma once

#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>

namespace PhantomLedger::app::progress {

void status(std::string_view message);

class Stage {
public:
  Stage(std::string label, std::size_t total);
  ~Stage();

  Stage(const Stage &) = delete;
  Stage &operator=(const Stage &) = delete;
  Stage(Stage &&) = delete;
  Stage &operator=(Stage &&) = delete;

  void tick(std::size_t n = 1) noexcept;
  void setProgress(std::size_t current) noexcept;
  void done() noexcept;

private:
  std::string label_;
  std::size_t total_ = 0;
  std::size_t current_ = 0;
  std::chrono::steady_clock::time_point startTime_;
  std::chrono::steady_clock::time_point lastRender_;
  bool active_ = false;

  void render(bool force) noexcept;
};

} // namespace PhantomLedger::app::progress
