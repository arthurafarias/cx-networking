// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// The single place core::socket_address <-> sockaddr_storage conversion
// lives (SRS-008 §5.3). Included only by the posix backends of socket_ops
// and resolver.

#include <cstring>
#include <string>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <lambdatech/networking/core/address.hpp>

namespace lambdatech::networking::core::impl::posix {

inline sockaddr_storage to_sockaddr(const socket_address &addr, socklen_t &out_len) {
  sockaddr_storage storage{};
  if (addr.family == AF_INET6) {
    auto *in6 = reinterpret_cast<sockaddr_in6 *>(&storage);
    in6->sin6_family = AF_INET6;
    in6->sin6_port = htons(addr.port);
    ::inet_pton(AF_INET6, addr.address.c_str(), &in6->sin6_addr);
    out_len = sizeof(sockaddr_in6);
  } else {
    auto *in4 = reinterpret_cast<sockaddr_in *>(&storage);
    in4->sin_family = AF_INET;
    in4->sin_port = htons(addr.port);
    ::inet_pton(AF_INET, addr.address.c_str(), &in4->sin_addr);
    out_len = sizeof(sockaddr_in);
  }
  return storage;
}

inline socket_address from_sockaddr(const sockaddr *addr) {
  socket_address out;
  char text[INET6_ADDRSTRLEN] = {};
  if (addr->sa_family == AF_INET6) {
    const auto *in6 = reinterpret_cast<const sockaddr_in6 *>(addr);
    ::inet_ntop(AF_INET6, &in6->sin6_addr, text, sizeof(text));
    out.port = ntohs(in6->sin6_port);
    out.family = AF_INET6;
  } else {
    const auto *in4 = reinterpret_cast<const sockaddr_in *>(addr);
    ::inet_ntop(AF_INET, &in4->sin_addr, text, sizeof(text));
    out.port = ntohs(in4->sin_port);
    out.family = AF_INET;
  }
  out.address = text;
  return out;
}

} // namespace lambdatech::networking::core::impl::posix
