// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// Thin wrappers over the POSIX socket calls the protocol layer needs, so
// tcp/udp code deals in RAII and error strings instead of raw fds and
// errno. Linux/BSD only - this library targets the same platforms cxflow
// does.

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace lambdatech::networking::core::native {

inline bool set_nonblocking(int fd) {
  int flags = ::fcntl(fd, F_GETFL, 0);
  return flags != -1 && ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

inline void close_fd(int &fd) {
  if (fd >= 0) {
    ::close(fd);
    fd = -1;
  }
}

inline std::string last_error(const char *prefix) {
  return std::string(prefix) + ": " + std::strerror(errno);
}

inline int socket_error(int fd) {
  int err = 0;
  socklen_t len = sizeof(err);
  if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) != 0) {
    return errno;
  }
  return err;
}

} // namespace lambdatech::networking::core::native
