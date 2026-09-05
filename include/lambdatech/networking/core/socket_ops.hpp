// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// core::socket_ops - the façade over the BSD socket calls (SRS-008 §5.1).
// Replaces core/native.hpp and every inline ::socket / ::connect / ::recv /
// ::sendto in the protocol layer. Trades in core::socket_address and
// std::errc, never sockaddr or errno.

#include <concepts>
#include <span>
#include <string>

#include <lambdatech/networking/core/config.hpp>
#include <lambdatech/networking/core/descriptor.hpp>
#include <lambdatech/networking/core/socket_ops_types.hpp>

#if LNW_NET_BACKEND_POSIX
#  include <lambdatech/networking/core/impl/posix/socket_ops.hpp>
#elif LNW_NET_BACKEND_STANDALONE
#  include <lambdatech/networking/core/impl/standalone/socket_ops.hpp>
#endif

namespace lambdatech::networking::core::socket_ops {

#if LNW_NET_BACKEND_POSIX
namespace backend = impl::posix;
#elif LNW_NET_BACKEND_STANDALONE
namespace backend = impl::standalone;
#endif

inline opened open(domain d, transport t) { return backend::open(d, t); }
inline std::errc set_nonblocking(descriptor::state &s) { return backend::set_nonblocking(s); }
inline std::errc set_reuse_addr(descriptor::state &s) { return backend::set_reuse_addr(s); }
inline std::errc connect(descriptor::state &s, const socket_address &a) { return backend::connect(s, a); }
inline std::errc bind(descriptor::state &s, const socket_address &a) { return backend::bind(s, a); }
inline std::errc listen(descriptor::state &s, int backlog) { return backend::listen(s, backlog); }
inline accepted accept(descriptor::state &s) { return backend::accept(s); }
inline transfer send(descriptor::state &s, std::span<const std::byte> b) { return backend::send(s, b); }
inline transfer recv(descriptor::state &s, std::span<std::byte> b) { return backend::recv(s, b); }
inline transfer send_to(descriptor::state &s, std::span<const std::byte> b, const socket_address &a) {
  return backend::send_to(s, b, a);
}
inline received recv_from(descriptor::state &s, std::span<std::byte> b) { return backend::recv_from(s, b); }
inline std::errc shutdown(descriptor::state &s, shut how) { return backend::shutdown(s, how); }
inline socket_address local_endpoint(descriptor::state &s) { return backend::local_endpoint(s); }
inline std::errc pending_error(descriptor::state &s) { return backend::pending_error(s); }
inline std::string describe(std::errc e, const char *prefix) { return backend::describe(e, prefix); }

// --- backend contract (SRS-008 §2.3) -----------------------------------

static_assert(
    requires(descriptor::state s, const socket_address a, std::span<std::byte> w, std::span<const std::byte> r,
             std::errc e) {
      { backend::open(domain::inet, transport::stream) } -> std::same_as<opened>;
      { backend::set_nonblocking(s) } -> std::same_as<std::errc>;
      { backend::connect(s, a) } -> std::same_as<std::errc>;
      { backend::bind(s, a) } -> std::same_as<std::errc>;
      { backend::listen(s, 0) } -> std::same_as<std::errc>;
      { backend::accept(s) } -> std::same_as<accepted>;
      { backend::send(s, r) } -> std::same_as<transfer>;
      { backend::recv(s, w) } -> std::same_as<transfer>;
      { backend::send_to(s, r, a) } -> std::same_as<transfer>;
      { backend::recv_from(s, w) } -> std::same_as<received>;
      { backend::shutdown(s, shut::both) } -> std::same_as<std::errc>;
      { backend::local_endpoint(s) } -> std::same_as<socket_address>;
      { backend::pending_error(s) } -> std::same_as<std::errc>;
      { backend::describe(e, "") } -> std::convertible_to<std::string>;
    },
    "selected core::socket_ops backend is incomplete (SRS-008 §2.3)");

} // namespace lambdatech::networking::core::socket_ops
