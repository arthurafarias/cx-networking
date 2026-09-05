// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// Standalone (no-kernel) backend for core::descriptor - SRS-008 §3.4 / M3.
// Scaffold only: satisfies the descriptor_backend concept so the façade
// compiles under -DLNW_NET_BACKEND_STANDALONE=1; the in-process pipe table
// is not implemented yet.

#include <cstddef>
#include <cstdint>
#include <span>
#include <system_error>
#include <utility>

#include <lambdatech/networking/core/io_result.hpp>

namespace lambdatech::networking::core::descriptor::impl::standalone {

using native_handle = std::uint64_t;
inline constexpr native_handle invalid_handle = 0;

class state {
public:
  state() = default;
  explicit state(native_handle h) : handle_(h) {}

  state(const state &) = delete;
  state &operator=(const state &) = delete;
  state(state &&other) noexcept : handle_(std::exchange(other.handle_, invalid_handle)) {}
  state &operator=(state &&other) noexcept {
    handle_ = std::exchange(other.handle_, invalid_handle);
    return *this;
  }
  ~state() = default;

  native_handle handle() const { return handle_; }
  native_handle release() { return std::exchange(handle_, invalid_handle); }
  void reset() { handle_ = invalid_handle; }

private:
  native_handle handle_ = invalid_handle;
};

inline state adopt(native_handle h) { return state(h); }
inline io_result read(state &, std::span<std::byte>) { return {-1, std::errc::function_not_supported}; }
inline io_result write(state &, std::span<const std::byte>) { return {-1, std::errc::function_not_supported}; }
inline void close(state &d) { d.reset(); }
inline bool valid(const state &d) { return d.handle() != invalid_handle; }
inline native_handle native(const state &d) { return d.handle(); }
inline native_handle release(state &d) { return d.release(); }

} // namespace lambdatech::networking::core::descriptor::impl::standalone
