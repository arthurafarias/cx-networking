// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// core::resolver - the façade over name resolution (SRS-008 §5.2). Blocking,
// exactly as before: callers run resolve() on core::thread_pool and marshal
// the result back with event_loop::defer().

#include <concepts>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <lambdatech/networking/core/address.hpp>
#include <lambdatech/networking/core/config.hpp>

#if LNW_NET_BACKEND_POSIX
#  include <lambdatech/networking/core/impl/posix/resolver.hpp>
#elif LNW_NET_BACKEND_STANDALONE
#  include <lambdatech/networking/core/impl/standalone/resolver.hpp>
#endif

namespace lambdatech::networking::core::resolver {

#if LNW_NET_BACKEND_POSIX
namespace backend = impl::posix;
#elif LNW_NET_BACKEND_STANDALONE
namespace backend = impl::standalone;
#endif

// Every A/AAAA result for host:port. IPv4 first, matching most stub
// resolvers. Empty on failure.
inline std::vector<socket_address> resolve(const std::string &host, std::uint16_t port, bool datagram = false) {
  return backend::resolve(host, port, datagram);
}

inline std::optional<socket_address> resolve_one(const std::string &host, std::uint16_t port,
                                                 bool datagram = false) {
  auto all = resolve(host, port, datagram);
  if (all.empty()) {
    return std::nullopt;
  }
  return all.front();
}

static_assert(requires(std::string h, std::uint16_t p) {
  { backend::resolve(h, p, false) } -> std::same_as<std::vector<socket_address>>;
}, "selected core::resolver backend is incomplete (SRS-008 §2.3)");

} // namespace lambdatech::networking::core::resolver
