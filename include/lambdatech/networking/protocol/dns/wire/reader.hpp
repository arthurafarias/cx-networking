// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// A forward-only cursor over a DNS message on the wire (RFC 1035 §4). All
// multi-byte integers in the DNS wire format are big-endian; every read
// here is bounds-checked and returns std::nullopt on truncation rather than
// throwing, so a parser can bail cleanly on a malformed datagram.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace lambdatech::networking::protocol::dns::wire {

class reader {
public:
  explicit reader(std::span<const std::byte> buffer) : buffer_(buffer) {}

  std::size_t position() const { return position_; }
  std::size_t remaining() const { return buffer_.size() - position_; }
  bool at_end() const { return position_ >= buffer_.size(); }

  void seek(std::size_t position) { position_ = position; }

  std::optional<std::uint8_t> read_u8() {
    if (remaining() < 1) {
      return std::nullopt;
    }
    return static_cast<std::uint8_t>(buffer_[position_++]);
  }

  std::optional<std::uint16_t> read_u16() {
    if (remaining() < 2) {
      return std::nullopt;
    }
    std::uint16_t value = static_cast<std::uint16_t>(static_cast<std::uint8_t>(buffer_[position_])) << 8 |
                          static_cast<std::uint16_t>(static_cast<std::uint8_t>(buffer_[position_ + 1]));
    position_ += 2;
    return value;
  }

  std::optional<std::uint32_t> read_u32() {
    if (remaining() < 4) {
      return std::nullopt;
    }
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
      value = value << 8 | static_cast<std::uint8_t>(buffer_[position_ + i]);
    }
    position_ += 4;
    return value;
  }

  std::optional<std::span<const std::byte>> read_bytes(std::size_t count) {
    if (remaining() < count) {
      return std::nullopt;
    }
    auto slice = buffer_.subspan(position_, count);
    position_ += count;
    return slice;
  }

  // Reads a domain name (RFC 1035 §4.1.4), following compression pointers.
  // The cursor is left just past the name in the *original* stream, not at
  // the pointer target. Returns std::nullopt on a malformed name or a
  // pointer loop.
  std::optional<std::string> read_name() {
    std::string name;
    std::size_t cursor = position_;
    bool jumped = false;
    std::size_t safety = buffer_.size(); // hard cap against pointer loops

    while (safety-- > 0) {
      if (cursor >= buffer_.size()) {
        return std::nullopt;
      }
      std::uint8_t length = static_cast<std::uint8_t>(buffer_[cursor]);

      if ((length & 0xC0) == 0xC0) {
        if (cursor + 1 >= buffer_.size()) {
          return std::nullopt;
        }
        std::size_t target =
            (static_cast<std::size_t>(length & 0x3F) << 8) | static_cast<std::uint8_t>(buffer_[cursor + 1]);
        if (!jumped) {
          position_ = cursor + 2;
          jumped = true;
        }
        cursor = target;
        continue;
      }

      if ((length & 0xC0) != 0) {
        return std::nullopt; // reserved label type
      }

      cursor += 1;
      if (length == 0) {
        if (!jumped) {
          position_ = cursor;
        }
        return name;
      }

      if (cursor + length > buffer_.size()) {
        return std::nullopt;
      }
      if (!name.empty()) {
        name.push_back('.');
      }
      for (std::size_t i = 0; i < length; ++i) {
        name.push_back(static_cast<char>(buffer_[cursor + i]));
      }
      cursor += length;
    }

    return std::nullopt;
  }

private:
  std::span<const std::byte> buffer_;
  std::size_t position_ = 0;
};

} // namespace lambdatech::networking::protocol::dns::wire
