// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// Standalone backend for core::resolver - SRS-008 §5.2 / M3. Resolves the
// loopback and unspecified literals so the fabric's own bind()/connect()
// paths work; a process-global host table for other names is M3 work.

#include <cstdint>
#include <string>
#include <vector>

#include <lambdatech/networking/core/address.hpp>

namespace lambdatech::networking::core::resolver::impl::standalone {

inline std::vector<socket_address> resolve(const std::string &host, std::uint16_t port, bool /*datagram*/) {
  const bool v6 = host.find(':') != std::string::npos;
  std::string text = host;
  if (host == "localhost") {
    text = "127.0.0.1";
  } else if (host.empty() || host == "0.0.0.0") {
    text = "0.0.0.0";
  }
  return {socket_address{text, port, v6 ? static_cast<int>(address_family::inet6)
                                        : static_cast<int>(address_family::inet)}};
}

} // namespace lambdatech::networking::core::resolver::impl::standalone
