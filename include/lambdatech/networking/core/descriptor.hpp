// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// core::descriptor - the façade over one owned OS handle and its raw I/O
// (SRS-008 §3). Selects a backend from core/config.hpp; every consumer names
// only this header. `descriptor::state` is a move-only RAII owner; raw use is
// always spelled descriptor::native(s).

#include <concepts>
#include <cstddef>
#include <span>
#include <system_error>

#include <lambdatech/networking/core/config.hpp>
#include <lambdatech/networking/core/io_result.hpp>

#if LNW_NET_BACKEND_POSIX
#  include <lambdatech/networking/core/impl/posix/descriptor.hpp>
#elif LNW_NET_BACKEND_STANDALONE
#  include <lambdatech/networking/core/impl/standalone/descriptor.hpp>
#endif

namespace lambdatech::networking::core::descriptor {

#if LNW_NET_BACKEND_POSIX
namespace backend = impl::posix;
#elif LNW_NET_BACKEND_STANDALONE
namespace backend = impl::standalone;
#endif

using native_handle = backend::native_handle;
using state = backend::state;
inline constexpr native_handle invalid_handle = backend::invalid_handle;
using core::would_block;

inline state adopt(native_handle h) { return backend::adopt(h); }
inline io_result read(state &d, std::span<std::byte> b) { return backend::read(d, b); }
inline io_result write(state &d, std::span<const std::byte> b) { return backend::write(d, b); }
inline void close(state &d) { backend::close(d); }
inline bool valid(const state &d) { return backend::valid(d); }
inline native_handle native(const state &d) { return backend::native(d); }
inline native_handle release(state &d) { return backend::release(d); }

// --- backend contract (SRS-008 §2.3) --------------------------------------

static_assert(
    requires(state s, const state cs, native_handle h, std::span<std::byte> w, std::span<const std::byte> r) {
      { backend::adopt(h) } -> std::same_as<state>;
      { backend::read(s, w) } -> std::same_as<io_result>;
      { backend::write(s, r) } -> std::same_as<io_result>;
      { backend::close(s) };
      { backend::valid(cs) } -> std::same_as<bool>;
      { backend::native(cs) } -> std::same_as<native_handle>;
    },
    "selected core::descriptor backend is incomplete (SRS-008 §2.3)");

} // namespace lambdatech::networking::core::descriptor
