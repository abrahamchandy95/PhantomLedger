#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace PhantomLedger::activity::spending::simulator {

struct PartitionRange {
  std::size_t begin = 0;
  std::size_t end = 0;
  [[nodiscard]] std::size_t size() const noexcept { return end - begin; }
};

[[nodiscard]] inline PartitionRange
partitionRange(std::size_t total, std::uint32_t threadCount,
               std::uint32_t threadIdx) noexcept {
  if (threadCount == 0) {
    return {0, 0};
  }
  const std::size_t base = total / threadCount;
  const std::size_t rem = total % threadCount;
  const std::size_t begin =
      threadIdx * base + (threadIdx < rem ? threadIdx : rem);
  const std::size_t extra = (threadIdx < rem) ? 1 : 0;
  const std::size_t end = begin + base + extra;
  return {begin, end};
}

[[nodiscard]] inline std::uint32_t resolveThreadCount() noexcept {
  if (const char *env = std::getenv("PL_THREADS")) {
    char *end = nullptr;
    const auto parsed = std::strtoul(env, &end, 10);
    if (end != env && parsed > 0 && parsed < 1024) {
      return static_cast<std::uint32_t>(parsed);
    }
  }
  const auto hw = std::thread::hardware_concurrency();
  return hw == 0 ? std::uint32_t{1} : hw;
}

class WorkerPool {
public:
  WorkerPool() = default;

  explicit WorkerPool(std::uint32_t threadCount) { start(threadCount); }

  WorkerPool(const WorkerPool &) = delete;
  WorkerPool &operator=(const WorkerPool &) = delete;

  ~WorkerPool() { stop(); }

  void start(std::uint32_t threadCount) {
    stop();
    threadCount_ = threadCount == 0 ? 1 : threadCount;
    if (threadCount_ <= 1) {
      return;
    }
    stop_ = false;
    workers_.reserve(threadCount_ - 1);
    for (std::uint32_t t = 1; t < threadCount_; ++t) {
      workers_.emplace_back([this, t]() { workerLoop(t); });
    }
  }

  void stop() {
    if (!workers_.empty()) {
      {
        const std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
        ++generation_;
      }
      wakeCv_.notify_all();
      for (auto &worker : workers_) {
        worker.join();
      }
      workers_.clear();
    }
    stop_ = false;
  }

  [[nodiscard]] std::uint32_t threadCount() const noexcept {
    return threadCount_;
  }

  void run(const std::function<void(std::uint32_t)> &body) {
    if (threadCount_ <= 1 || workers_.empty()) {
      body(0);
      return;
    }
    {
      const std::lock_guard<std::mutex> lock(mutex_);
      body_ = &body;
      remaining_ = threadCount_ - 1;
      ++generation_;
    }
    wakeCv_.notify_all();

    body(0);

    std::unique_lock<std::mutex> lock(mutex_);
    doneCv_.wait(lock, [this]() { return remaining_ == 0; });
    body_ = nullptr;
  }

private:
  void workerLoop(std::uint32_t threadIdx) {
    std::uint64_t seenGeneration = 0;
    for (;;) {
      const std::function<void(std::uint32_t)> *job = nullptr;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        wakeCv_.wait(lock,
                     [&]() { return stop_ || generation_ != seenGeneration; });
        seenGeneration = generation_;
        if (stop_) {
          return;
        }
        job = body_;
      }
      (*job)(threadIdx);
      {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (--remaining_ == 0) {
          doneCv_.notify_one();
        }
      }
    }
  }

  std::vector<std::thread> workers_;
  std::mutex mutex_;
  std::condition_variable wakeCv_;
  std::condition_variable doneCv_;
  const std::function<void(std::uint32_t)> *body_ = nullptr;
  std::uint32_t threadCount_ = 1;
  std::uint32_t remaining_ = 0;
  std::uint64_t generation_ = 0;
  bool stop_ = false;
};

template <typename Body>
inline void runParallel(std::uint32_t threadCount, Body &&body) {
  if (threadCount <= 1) {
    body(static_cast<std::uint32_t>(0));
    return;
  }
  std::vector<std::thread> workers;
  workers.reserve(threadCount);
  for (std::uint32_t t = 0; t < threadCount; ++t) {
    workers.emplace_back([t, &body]() { body(t); });
  }
  for (auto &w : workers) {
    w.join();
  }
}

} // namespace PhantomLedger::activity::spending::simulator
