// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// POSIX backend for core::resolver (SRS-008 §5.2): a blocking getaddrinfo()
// wrapper, moved verbatim from the old core/address.hpp. Included only
// through core/resolver.hpp.

#include <cstdint>
#include <string>
#include <vector>

#include <netdb.h>
#include <sys/socket.h>

#include <lambdatech/networking/core/address.hpp>
#include <lambdatech/networking/core/impl/posix/endpoint.hpp>

namespace lambdatech::networking::core::resolver::impl::posix {

inline std::vector<socket_address> resolve(const std::string &host, std::uint16_t port, bool datagram) {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = datagram ? SOCK_DGRAM : SOCK_STREAM;

  addrinfo *result = nullptr;
  std::string service = std::to_string(port);
  if (::getaddrinfo(host.c_str(), service.c_str(), &hints, &result) != 0) {
    return {};
  }

  std::vector<socket_address> out;
  for (addrinfo *it = result; it != nullptr; it = it->ai_next) {
    out.push_back(core::impl::posix::from_sockaddr(it->ai_addr));
  }
  ::freeaddrinfo(result);
  return out;
}

} // namespace lambdatech::networking::core::resolver::impl::posix
