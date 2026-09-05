// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// Backend-independent vocabulary for core::poller (SRS-008 §4.1). Its own
// header so a poller backend under core/impl/ uses it without circularly
// including the façade.

#include <lambdatech/networking/core/descriptor.hpp>

namespace lambdatech::networking::core::poller {

enum class interest : unsigned { none = 0, read = 1, write = 2 };

constexpr interest operator|(interest a, interest b) {
  return static_cast<interest>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}
constexpr bool has(interest set, interest bit) {
  return (static_cast<unsigned>(set) & static_cast<unsigned>(bit)) != 0;
}

struct ready {
  bool readable = false;
  bool writable = false;
  bool error = false;
  bool hangup = false;
};

struct event {
  descriptor::native_handle handle{};
  ready what;
};

} // namespace lambdatech::networking::core::poller
