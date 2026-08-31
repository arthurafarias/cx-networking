// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// The byte container passed to every 'data' / 'message' listener - the
// analogue of Node.js's Buffer. A thin owning std::vector<std::byte> with
// string interop helpers, since DNS and most line protocols are handled as
// bytes, not text.

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lambdatech::networking::core {

using buffer = std::vector<std::byte>;

inline buffer make_buffer(std::string_view text) {
  buffer out(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    out[i] = static_cast<std::byte>(static_cast<unsigned char>(text[i]));
  }
  return out;
}

inline buffer make_buffer(std::span<const std::byte> bytes) { return buffer(bytes.begin(), bytes.end()); }

inline std::string to_string(std::span<const std::byte> bytes) {
  std::string out(bytes.size(), '\0');
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    out[i] = static_cast<char>(bytes[i]);
  }
  return out;
}

inline void append(buffer &dst, std::span<const std::byte> bytes) { dst.insert(dst.end(), bytes.begin(), bytes.end()); }

} // namespace lambdatech::networking::core
