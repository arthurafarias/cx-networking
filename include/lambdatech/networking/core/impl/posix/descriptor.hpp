// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// POSIX backend for core::descriptor (SRS-008 §3.3). Included only through
// core/descriptor.hpp, which defines descriptor::io_result before this
// header. A `state` is an owning `int fd`.

#include <cerrno>
#include <cstddef>
#include <span>
#include <system_error>
#include <utility>

#include <unistd.h>

#include <lambdatech/networking/core/io_result.hpp>

namespace lambdatech::networking::core::descriptor::impl::posix {

using native_handle = int;
inline constexpr native_handle invalid_handle = -1;

class state {
public:
  state() = default;
  explicit state(native_handle fd) : fd_(fd) {}

  state(const state &) = delete;
  state &operator=(const state &) = delete;

  state(state &&other) noexcept : fd_(std::exchange(other.fd_, invalid_handle)) {}
  state &operator=(state &&other) noexcept {
    if (this != &other) {
      reset();
      fd_ = std::exchange(other.fd_, invalid_handle);
    }
    return *this;
  }

  ~state() { reset(); }

  native_handle fd() const { return fd_; }
  native_handle release() { return std::exchange(fd_, invalid_handle); }

  void reset() {
    if (fd_ >= 0) {
      ::close(fd_);
      fd_ = invalid_handle;
    }
  }

private:
  native_handle fd_ = invalid_handle;
};

inline state adopt(native_handle fd) { return state(fd); }

inline io_result read(state &d, std::span<std::byte> buf) {
  ssize_t n = ::read(d.fd(), buf.data(), buf.size());
  if (n < 0) {
    return {-1, std::errc(errno)};
  }
  return {static_cast<std::ptrdiff_t>(n), std::errc{}};
}

inline io_result write(state &d, std::span<const std::byte> buf) {
  ssize_t n = ::write(d.fd(), buf.data(), buf.size());
  if (n < 0) {
    return {-1, std::errc(errno)};
  }
  return {static_cast<std::ptrdiff_t>(n), std::errc{}};
}

inline void close(state &d) { d.reset(); }
inline bool valid(const state &d) { return d.fd() >= 0; }
inline native_handle native(const state &d) { return d.fd(); }
inline native_handle release(state &d) { return d.release(); }

} // namespace lambdatech::networking::core::descriptor::impl::posix
