// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// Appends DNS wire-format fields (RFC 1035 §4) to a growable byte buffer.
// Big-endian for every multi-byte integer. This writer emits uncompressed
// names only - name compression is an optimization a later pass can layer
// on by tracking already-written suffixes.

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace lambdatech::networking::protocol::dns::wire {

class writer {
public:
  const std::vector<std::byte> &bytes() const { return buffer_; }
  std::size_t size() const { return buffer_.size(); }

  void write_u8(std::uint8_t value) { buffer_.push_back(static_cast<std::byte>(value)); }

  void write_u16(std::uint16_t value) {
    write_u8(static_cast<std::uint8_t>(value >> 8));
    write_u8(static_cast<std::uint8_t>(value & 0xFF));
  }

  void write_u32(std::uint32_t value) {
    write_u8(static_cast<std::uint8_t>(value >> 24));
    write_u8(static_cast<std::uint8_t>(value >> 16));
    write_u8(static_cast<std::uint8_t>(value >> 8));
    write_u8(static_cast<std::uint8_t>(value & 0xFF));
  }

  void write_bytes(std::span<const std::byte> data) { buffer_.insert(buffer_.end(), data.begin(), data.end()); }

  // Encodes a dotted domain name as a sequence of length-prefixed labels
  // terminated by a zero byte (RFC 1035 §3.1). An empty name is the root.
  // Returns false if any label is empty or exceeds 63 bytes.
  bool write_name(std::string_view name) {
    std::size_t start = 0;
    while (start < name.size()) {
      std::size_t dot = name.find('.', start);
      std::size_t end = dot == std::string_view::npos ? name.size() : dot;
      std::size_t length = end - start;
      if (length == 0 || length > 63) {
        return false;
      }
      write_u8(static_cast<std::uint8_t>(length));
      for (std::size_t i = start; i < end; ++i) {
        write_u8(static_cast<std::uint8_t>(name[i]));
      }
      if (dot == std::string_view::npos) {
        break;
      }
      start = dot + 1;
    }
    write_u8(0);
    return true;
  }

private:
  std::vector<std::byte> buffer_;
};

} // namespace lambdatech::networking::protocol::dns::wire
