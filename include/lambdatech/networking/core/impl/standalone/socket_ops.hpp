// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// Standalone backend for core::socket_ops - SRS-008 §5 / M3. Scaffold only:
// an in-process loopback fabric is not implemented yet. Every call reports
// std::errc::function_not_supported so a standalone build links and the
// failure is explicit.

#include <cstddef>
#include <span>
#include <string>
#include <system_error>

#include <lambdatech/networking/core/descriptor.hpp>
#include <lambdatech/networking/core/socket_ops_types.hpp>

namespace lambdatech::networking::core::socket_ops::impl::standalone {

inline constexpr std::errc unsupported = std::errc::function_not_supported;

inline opened open(domain, transport) { return opened{descriptor::state{}, unsupported}; }
inline std::errc set_nonblocking(descriptor::state &) { return unsupported; }
inline std::errc set_reuse_addr(descriptor::state &) { return unsupported; }
inline std::errc connect(descriptor::state &, const socket_address &) { return unsupported; }
inline std::errc bind(descriptor::state &, const socket_address &) { return unsupported; }
inline std::errc listen(descriptor::state &, int) { return unsupported; }
inline accepted accept(descriptor::state &) { return accepted{descriptor::state{}, socket_address{}, unsupported}; }
inline transfer send(descriptor::state &, std::span<const std::byte>) { return transfer{-1, unsupported}; }
inline transfer recv(descriptor::state &, std::span<std::byte>) { return transfer{-1, unsupported}; }
inline transfer send_to(descriptor::state &, std::span<const std::byte>, const socket_address &) {
  return transfer{-1, unsupported};
}
inline received recv_from(descriptor::state &, std::span<std::byte>) {
  return received{-1, socket_address{}, unsupported};
}
inline std::errc shutdown(descriptor::state &, shut) { return unsupported; }
inline socket_address local_endpoint(descriptor::state &) { return socket_address{}; }
inline std::errc pending_error(descriptor::state &) { return unsupported; }
inline std::string describe(std::errc e, const char *prefix) {
  return std::string(prefix) + ": " + std::generic_category().message(static_cast<int>(e));
}

} // namespace lambdatech::networking::core::socket_ops::impl::standalone
