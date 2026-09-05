// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// Shared, backend-independent result vocabulary for the core::* facility
// façades (SRS-008 §2.5). Its own header so a backend under core/impl/ can
// use it without pulling in - or circularly including - a façade.

#include <cstddef>
#include <system_error>

namespace lambdatech::networking::core {

// std::errc{} (value-initialised) means success everywhere in core::*.

namespace descriptor {

struct io_result {
  std::ptrdiff_t count = 0;      // bytes transferred; 0 == EOF for a stream read
  std::errc error = std::errc{};
};

} // namespace descriptor

inline bool would_block(std::errc e) {
  return e == std::errc::operation_would_block || e == std::errc::resource_unavailable_try_again;
}

} // namespace lambdatech::networking::core
