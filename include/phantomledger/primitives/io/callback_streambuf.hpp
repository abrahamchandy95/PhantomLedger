#pragma once

#include <cstddef>
#include <functional>
#include <streambuf>
#include <vector>

namespace PhantomLedger::io {

class CallbackStreambuf final : public std::streambuf {
public:
  using Callback = std::function<void(const char *, std::size_t)>;

  explicit CallbackStreambuf(Callback cb, std::size_t bufferBytes = 64 * 1024)
      : cb_(std::move(cb)), buf_(bufferBytes) {
    setp(buf_.data(), buf_.data() + buf_.size());
  }

protected:
  int overflow(int ch) override {
    flushBuffer();
    if (ch != traits_type::eof()) {
      *pptr() = static_cast<char>(ch);
      pbump(1);
    }
    return ch;
  }

  int sync() override {
    flushBuffer();
    return 0;
  }

private:
  void flushBuffer() {
    const auto n = static_cast<std::size_t>(pptr() - pbase());
    if (n > 0) {
      cb_(pbase(), n);
      setp(buf_.data(), buf_.data() + buf_.size());
    }
  }

  Callback cb_;
  std::vector<char> buf_;
};

} // namespace PhantomLedger::io
