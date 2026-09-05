// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// Backend-independent vocabulary for core::socket_ops (SRS-008 §5.1). Its
// own header so a backend under core/impl/ uses it without circularly
// including the façade.

#include <cstddef>
#include <system_error>

#include <lambdatech/networking/core/address.hpp>
#include <lambdatech/networking/core/descriptor.hpp>

namespace lambdatech::networking::core::socket_ops {

enum class domain { inet, inet6 };
enum class transport { stream, datagram };
enum class shut { read, write, both };

struct opened {
  descriptor::state handle;
  std::errc error = std::errc{};
  explicit operator bool() const { return error == std::errc{}; }
};

struct accepted {
  descriptor::state handle;
  socket_address peer;
  std::errc error = std::errc{};
  explicit operator bool() const { return error == std::errc{}; }
};

struct transfer {
  std::ptrdiff_t count = 0;   // 0 == orderly shutdown for a stream recv
  std::errc error = std::errc{};
};

struct received {
  std::ptrdiff_t count = 0;
  socket_address from;
  std::errc error = std::errc{};
};

} // namespace lambdatech::networking::core::socket_ops
