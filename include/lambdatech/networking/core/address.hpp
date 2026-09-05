// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// core::socket_address - a resolved transport endpoint, the analogue of
// Node's { address, port, family } rinfo object. A backend-neutral value
// (SRS-008 §5.3): text address, port, and a family tag. The sockaddr
// conversion lives in core/impl/posix/endpoint.hpp; name resolution is
// core::resolver (core/resolver.hpp).

#include <cstdint>
#include <string>

namespace lambdatech::networking::core {

// AF_INET / AF_INET6 numerically, but an opaque tag to every consumer - it
// is never dereferenced above core/impl/. 2 == AF_INET, 10 == AF_INET6 on
// Linux; the posix endpoint backend is the single source of truth.
enum class address_family : int { unspecified = 0, inet = 2, inet6 = 10 };

struct socket_address {
  std::string address; // presentation form, e.g. "127.0.0.1" or "::1"
  std::uint16_t port = 0;
  int family = 0; // AF_INET / AF_INET6 (kept as int for zero-cost interop)

  bool is_inet6() const { return family == static_cast<int>(address_family::inet6); }

  bool operator==(const socket_address &) const = default;
};

} // namespace lambdatech::networking::core
