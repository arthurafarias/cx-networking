// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// A resolved transport endpoint - the analogue of Node's { address, port,
// family } rinfo object - plus a blocking getaddrinfo() wrapper. Callers
// that must not block the event loop run resolve() on core::thread_pool and
// deliver the result back via event_loop::post().

#include <arpa/inet.h>
#include <cstdint>
#include <cstring>
#include <netdb.h>
#include <optional>
#include <string>
#include <sys/socket.h>
#include <vector>

namespace lambdatech::networking::core {

struct socket_address {
  std::string address; // presentation form, e.g. "127.0.0.1" or "::1"
  std::uint16_t port = 0;
  int family = AF_UNSPEC; // AF_INET / AF_INET6

  sockaddr_storage to_sockaddr(socklen_t &out_len) const {
    sockaddr_storage storage{};
    if (family == AF_INET6) {
      auto *in6 = reinterpret_cast<sockaddr_in6 *>(&storage);
      in6->sin6_family = AF_INET6;
      in6->sin6_port = htons(port);
      inet_pton(AF_INET6, address.c_str(), &in6->sin6_addr);
      out_len = sizeof(sockaddr_in6);
    } else {
      auto *in4 = reinterpret_cast<sockaddr_in *>(&storage);
      in4->sin_family = AF_INET;
      in4->sin_port = htons(port);
      inet_pton(AF_INET, address.c_str(), &in4->sin_addr);
      out_len = sizeof(sockaddr_in);
    }
    return storage;
  }

  static socket_address from_sockaddr(const sockaddr *addr) {
    socket_address out;
    char text[INET6_ADDRSTRLEN] = {};
    if (addr->sa_family == AF_INET6) {
      auto *in6 = reinterpret_cast<const sockaddr_in6 *>(addr);
      inet_ntop(AF_INET6, &in6->sin6_addr, text, sizeof(text));
      out.port = ntohs(in6->sin6_port);
      out.family = AF_INET6;
    } else {
      auto *in4 = reinterpret_cast<const sockaddr_in *>(addr);
      inet_ntop(AF_INET, &in4->sin_addr, text, sizeof(text));
      out.port = ntohs(in4->sin_port);
      out.family = AF_INET;
    }
    out.address = text;
    return out;
  }

  bool operator==(const socket_address &) const = default;
};

// Blocking. Returns every A/AAAA result getaddrinfo() yields for host:port.
// `prefer_ipv6 == false` (the default) sorts IPv4 results first, matching
// most stub-resolver behaviour.
inline std::vector<socket_address> resolve(const std::string &host, std::uint16_t port, bool datagram = false) {
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
    out.push_back(socket_address::from_sockaddr(it->ai_addr));
  }
  ::freeaddrinfo(result);
  return out;
}

inline std::optional<socket_address> resolve_one(const std::string &host, std::uint16_t port, bool datagram = false) {
  auto all = resolve(host, port, datagram);
  if (all.empty()) {
    return std::nullopt;
  }
  return all.front();
}

} // namespace lambdatech::networking::core
