// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// Shared helper for the networking test groups: block the test thread on a
// std::future produced by a listener running on the event loop, with a hard
// timeout so a broken socket path fails the case instead of hanging the run.

#include <chrono>
#include <future>
#include <optional>

namespace lambdatech::networking::testing {

template <typename T>
std::optional<T> await(std::future<T> &future,
                       std::chrono::milliseconds timeout = std::chrono::milliseconds(2000)) {
  if (future.wait_for(timeout) != std::future_status::ready) {
    return std::nullopt;
  }
  return future.get();
}

} // namespace lambdatech::networking::testing
