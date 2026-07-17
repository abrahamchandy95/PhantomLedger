#pragma once
//
// phantomledger/pipeline/chunk/async_sink.hpp
//
// Async<Inner>: a chunk::Sink decorator that moves span writes onto a
// single worker thread behind a bounded queue, so PostgreSQL COPY for
// span k overlaps generation of window k+1.
//
// WHY THIS IS SAFE (and what is deliberately NOT overlapped):
//   * Finalized output never feeds back into generation (the no-read-back
//     invariant), so deferring the write changes nothing upstream.
//   * Each queued item OWNS its rows: the buffer stays alive until the
//     inner sink finishes with it, per the overlap-safety rule.
//   * One worker + FIFO queue => spans reach the inner sink in exactly
//     the order they were produced. Ordering-sensitive sinks (Golden)
//     would still be correct behind Async, but keep Golden on the
//     synchronous side of a Tee anyway: the digest then never depends on
//     this decorator existing:  Tee(golden, Async(postgres)).
//   * Settlement/generation overlap is NOT provided here and must not be
//     attempted until settlement has its own content-keyed RNG lane; both
//     replay accumulators draw blind-retry randomness from the shared
//     pipeline RNG today.
//
// FAILURE HANDLING: worker exceptions are captured and rethrown on the
// producer thread at the next sink call (fail fast) and at finish().
// The bounded queue applies backpressure: producers block when maxQueued
// spans are in flight, which caps memory at ~maxQueued spans of rows.
//

#include "phantomledger/pipeline/chunk/schedule.hpp"
#include "phantomledger/pipeline/chunk/sink.hpp"
#include "phantomledger/transactions/record.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <exception>
#include <mutex>
#include <span>
#include <thread>
#include <utility>
#include <vector>

namespace PhantomLedger::pipeline::chunk {

template <Sink Inner> class Async {
public:
  explicit Async(Inner &inner, std::size_t maxQueued = 2)
      : inner_(&inner), maxQueued_(maxQueued == 0 ? 1 : maxQueued),
        worker_([this] { workerLoop(); }) {}

  Async(const Async &) = delete;
  Async &operator=(const Async &) = delete;

  ~Async() {
    // finish() is the orderly path; this is the abandon-ship path.
    {
      std::lock_guard lock(mutex_);
      stopping_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable()) {
      worker_.join();
    }
  }

  void beginSpan(const Span &span) {
    rethrowIfFailed();
    current_.span = span;
    current_.rows.clear();
  }

  void append(std::span<const transactions::Transaction> txns) {
    // Copy into the owned buffer: the caller's span may die immediately,
    // ours must outlive the COPY.
    current_.rows.insert(current_.rows.end(), txns.begin(), txns.end());
  }

  void endSpan(const Span &span) {
    rethrowIfFailed();
    current_.span = span;
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [this] {
      return queue_.size() < maxQueued_ || failed_ || stopping_;
    });
    if (failed_) {
      lock.unlock();
      rethrowIfFailed();
    }
    queue_.push_back(std::move(current_));
    current_ = Item{};
    lock.unlock();
    cv_.notify_all();
  }

  void finish() {
    {
      std::lock_guard lock(mutex_);
      finishing_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable()) {
      worker_.join();
    }
    rethrowIfFailed();
  }

  [[nodiscard]] std::uint64_t rowsWritten() const noexcept {
    return rowsWritten_.load(std::memory_order_relaxed);
  }

private:
  struct Item {
    Span span{};
    std::vector<transactions::Transaction> rows;
  };

  void workerLoop() {
    for (;;) {
      Item item;
      {
        std::unique_lock lock(mutex_);
        cv_.wait(lock,
                 [this] { return !queue_.empty() || finishing_ || stopping_; });
        if (stopping_ && queue_.empty()) {
          return;
        }
        if (queue_.empty()) {
          // finishing_ with an empty queue: run inner finish, then exit.
          lock.unlock();
          runGuarded([this] { inner_->finish(); });
          return;
        }
        item = std::move(queue_.front());
        queue_.pop_front();
      }
      cv_.notify_all();

      const auto count = item.rows.size();
      const bool ok = runGuarded([this, &item] {
        inner_->beginSpan(item.span);
        inner_->append(std::span<const transactions::Transaction>(
            item.rows.data(), item.rows.size()));
        inner_->endSpan(item.span);
      });
      if (!ok) {
        return;
      }
      rowsWritten_.fetch_add(count, std::memory_order_relaxed);
    }
  }

  template <class Fn> bool runGuarded(Fn &&fn) noexcept {
    try {
      fn();
      return true;
    } catch (...) {
      std::lock_guard lock(mutex_);
      error_ = std::current_exception();
      failed_ = true;
      cv_.notify_all();
      return false;
    }
  }

  void rethrowIfFailed() {
    std::exception_ptr error;
    {
      std::lock_guard lock(mutex_);
      error = error_;
    }
    if (error) {
      std::rethrow_exception(error);
    }
  }

  Inner *inner_;
  std::size_t maxQueued_;

  Item current_{};

  std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<Item> queue_;
  bool finishing_ = false;
  bool stopping_ = false;
  bool failed_ = false;
  std::exception_ptr error_;

  std::atomic<std::uint64_t> rowsWritten_{0};

  std::thread worker_;
};

static_assert(Sink<Async<NullSink>>);

} // namespace PhantomLedger::pipeline::chunk
