// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// lambdatech::networking::protocol::core::client - a non-blocking TCP
// connection modeled on Node.js's net.Socket. Events (subscribe with
// on["name"] += listener):
//
//   connect   the connection is established
//   data      a chunk arrived                (core::buffer)
//   drain     the write buffer emptied
//   end       the peer half-closed (FIN)
//   error     a fatal error                  (std::string)
//   close     the socket is fully closed
//
// Always hold a client through std::shared_ptr (use core::client::create or
// take one from core::server's 'connection' event): the event loop keeps a
// weak_ptr and every listener runs on the loop thread.

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <utility>

#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <lambdatech/networking/core/address.hpp>
#include <lambdatech/networking/core/buffer.hpp>
#include <lambdatech/networking/core/event.hpp>
#include <lambdatech/networking/core/native.hpp>
#include <lambdatech/networking/core/thread_pool.hpp>

namespace lambdatech::networking::core::descriptor {

namespace impl::posix {
struct state {
  int fd = -1;
};
} // namespace impl::posix

namespace impl::standalone {
struct state {
  int fd = -1;
};
} // namespace impl::standalone

namespace impl {
namespace current = impl::posix;
};

struct state {
  impl::current::state impl;
};

namespace impl::posix {

inline descriptor::state create() { return descriptor::state(); }

}; // namespace impl::posix

namespace impl::posix {

inline int write(descriptor::state &desc, void *buf, size_t n) { return {}; }

}; // namespace impl::posix

namespace impl::posix {

inline int read(descriptor::state &desc, void *buf, size_t n) {
  return ::read(desc.impl.fd, buf, n);
}

}; // namespace impl::posix

namespace impl::posix {

inline int close(descriptor::state &desc) {
  return ::close(desc.impl.fd);
}

};

namespace impl::posix {

inline bool valid(descriptor::state &desc) {
  return desc.impl.fd >= 0;
}

}; // namespace impl::posix

namespace impl::standalone {

inline descriptor::state create() { return descriptor::state(); }

}; // namespace impl::standalone

namespace impl::standalone {

inline int write(descriptor::state &desc, void *buf, size_t n) { return {}; }

}; // namespace impl::standalone

namespace impl::standalone {

inline int read(descriptor::state &desc, void *buf, size_t n) { return {}; }

}; // namespace impl::standalone

namespace impl::standalone {

inline int close(descriptor::state &desc) { return {}; }

}; // namespace impl::standalone
inline descriptor::state create() { return impl::current::create(); }

inline int read(descriptor::state &desc, void *buf, size_t n) {
  return impl::current::read(desc, buf, n);
}

inline int write(descriptor::state &desc, void *buf, size_t n) {
  return impl::current::write(desc, buf, n);
}

inline int close(descriptor::state &desc) {
  return impl::current::close(desc);
}

inline int valid(descriptor::state &desc) {
  return impl::current::valid(desc);
}

} // namespace lambdatech::networking::protocol::core::descriptor
