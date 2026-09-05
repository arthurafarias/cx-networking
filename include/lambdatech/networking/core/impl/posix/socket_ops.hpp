// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// POSIX backend for core::socket_ops (SRS-008 §5.1). Included only through
// core/socket_ops.hpp. Thin wrappers over the BSD socket calls that the
// protocol layer needs, dealing in core::socket_address and std::errc.

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <span>
#include <string>
#include <system_error>

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <lambdatech/networking/core/descriptor.hpp>
#include <lambdatech/networking/core/impl/posix/endpoint.hpp>
#include <lambdatech/networking/core/socket_ops_types.hpp>

namespace lambdatech::networking::core::socket_ops::impl::posix {

namespace ep = core::impl::posix;

inline std::errc last_errc() { return std::errc(errno); }

inline int af_of(domain d) { return d == domain::inet6 ? AF_INET6 : AF_INET; }
inline int type_of(transport t) { return t == transport::datagram ? SOCK_DGRAM : SOCK_STREAM; }

inline opened open(domain d, transport t) {
  int fd = ::socket(af_of(d), type_of(t), 0);
  if (fd < 0) {
    return opened{descriptor::state{}, last_errc()};
  }
  return opened{descriptor::adopt(fd), std::errc{}};
}

inline std::errc set_nonblocking(descriptor::state &s) {
  int fd = descriptor::native(s);
  int flags = ::fcntl(fd, F_GETFL, 0);
  if (flags == -1 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    return last_errc();
  }
  return std::errc{};
}

inline std::errc set_reuse_addr(descriptor::state &s) {
  int yes = 1;
  if (::setsockopt(descriptor::native(s), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) != 0) {
    return last_errc();
  }
  return std::errc{};
}

inline std::errc connect(descriptor::state &s, const socket_address &a) {
  socklen_t len = 0;
  sockaddr_storage ss = ep::to_sockaddr(a, len);
  int rc = ::connect(descriptor::native(s), reinterpret_cast<sockaddr *>(&ss), len);
  if (rc == 0) {
    return std::errc{};
  }
  if (errno == EINPROGRESS) {
    return std::errc::operation_in_progress;
  }
  return last_errc();
}

inline std::errc bind(descriptor::state &s, const socket_address &a) {
  socklen_t len = 0;
  sockaddr_storage ss = ep::to_sockaddr(a, len);
  if (::bind(descriptor::native(s), reinterpret_cast<sockaddr *>(&ss), len) != 0) {
    return last_errc();
  }
  return std::errc{};
}

inline std::errc listen(descriptor::state &s, int backlog) {
  if (::listen(descriptor::native(s), backlog) != 0) {
    return last_errc();
  }
  return std::errc{};
}

inline accepted accept(descriptor::state &s) {
  sockaddr_storage peer{};
  socklen_t peer_len = sizeof(peer);
  int cfd = ::accept(descriptor::native(s), reinterpret_cast<sockaddr *>(&peer), &peer_len);
  if (cfd < 0) {
    return accepted{descriptor::state{}, socket_address{}, last_errc()};
  }
  return accepted{descriptor::adopt(cfd), ep::from_sockaddr(reinterpret_cast<sockaddr *>(&peer)), std::errc{}};
}

inline transfer send(descriptor::state &s, std::span<const std::byte> b) {
  ssize_t n = ::send(descriptor::native(s), b.data(), b.size(), MSG_NOSIGNAL);
  if (n < 0) {
    return transfer{-1, last_errc()};
  }
  return transfer{static_cast<std::ptrdiff_t>(n), std::errc{}};
}

inline transfer recv(descriptor::state &s, std::span<std::byte> b) {
  ssize_t n = ::recv(descriptor::native(s), b.data(), b.size(), 0);
  if (n < 0) {
    return transfer{-1, last_errc()};
  }
  return transfer{static_cast<std::ptrdiff_t>(n), std::errc{}};
}

inline transfer send_to(descriptor::state &s, std::span<const std::byte> b, const socket_address &a) {
  socklen_t len = 0;
  sockaddr_storage ss = ep::to_sockaddr(a, len);
  ssize_t n = ::sendto(descriptor::native(s), b.data(), b.size(), MSG_NOSIGNAL,
                       reinterpret_cast<sockaddr *>(&ss), len);
  if (n < 0) {
    return transfer{-1, last_errc()};
  }
  return transfer{static_cast<std::ptrdiff_t>(n), std::errc{}};
}

inline received recv_from(descriptor::state &s, std::span<std::byte> b) {
  sockaddr_storage from{};
  socklen_t from_len = sizeof(from);
  ssize_t n = ::recvfrom(descriptor::native(s), b.data(), b.size(), 0,
                         reinterpret_cast<sockaddr *>(&from), &from_len);
  if (n < 0) {
    return received{-1, socket_address{}, last_errc()};
  }
  return received{static_cast<std::ptrdiff_t>(n), ep::from_sockaddr(reinterpret_cast<sockaddr *>(&from)),
                  std::errc{}};
}

inline std::errc shutdown(descriptor::state &s, shut how) {
  int h = how == shut::read ? SHUT_RD : how == shut::write ? SHUT_WR : SHUT_RDWR;
  if (::shutdown(descriptor::native(s), h) != 0) {
    return last_errc();
  }
  return std::errc{};
}

inline socket_address local_endpoint(descriptor::state &s) {
  sockaddr_storage ss{};
  socklen_t len = sizeof(ss);
  if (::getsockname(descriptor::native(s), reinterpret_cast<sockaddr *>(&ss), &len) != 0) {
    return socket_address{};
  }
  return ep::from_sockaddr(reinterpret_cast<sockaddr *>(&ss));
}

inline std::errc pending_error(descriptor::state &s) {
  int err = 0;
  socklen_t len = sizeof(err);
  if (::getsockopt(descriptor::native(s), SOL_SOCKET, SO_ERROR, &err, &len) != 0) {
    return last_errc();
  }
  return std::errc(err);
}

inline std::string describe(std::errc e, const char *prefix) {
  return std::string(prefix) + ": " + std::generic_category().message(static_cast<int>(e));
}

} // namespace lambdatech::networking::core::socket_ops::impl::posix
