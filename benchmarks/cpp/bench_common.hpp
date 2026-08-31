// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// Shared harness for the C++ echo benchmarks (LambdaTech Networking and
// Boost.Asio). Every stack - C++, Node.js, Python - speaks the same CLI and
// emits the same single-line JSON result so benchmarks/run.py can diff them.
//
// Wire format of one request/response: `payload` bytes, the first 8 of which
// are a little-endian uint64 monotonic-clock timestamp (nanoseconds) written
// by the client. The echo server returns the bytes verbatim; the client
// reads the timestamp back out and books now() - ts as the round-trip.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace bench {

inline std::uint64_t now_ns() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

struct options {
  std::string proto = "tcp"; // "tcp" | "udp"
  std::string host = "127.0.0.1";
  std::uint16_t port = 0;
  int connections = 1;   // TCP connections / UDP flows
  double duration_s = 10.0;
  double warmup_s = 3.0;
  int payload = 64;      // bytes per message (>= 8)
  int pipeline = 1;      // in-flight requests per connection
  std::string stack = "unknown";
};

inline void die_usage(const char *arg) {
  std::fprintf(stderr,
               "bad or missing argument near '%s'\n"
               "  --proto tcp|udp  --host H  --port P  --connections N\n"
               "  --duration SECS  --warmup SECS  --payload BYTES  --pipeline K\n",
               arg);
  std::exit(2);
}

inline options parse_options(int argc, char **argv, std::string_view stack) {
  options o;
  o.stack = std::string(stack);
  for (int i = 1; i < argc; ++i) {
    std::string tok = argv[i];
    std::string key = tok, val;
    if (auto eq = tok.find('='); eq != std::string::npos) {
      key = tok.substr(0, eq);
      val = tok.substr(eq + 1);
    } else if (i + 1 < argc) {
      val = argv[i + 1];
    }
    auto take = [&](int &idx) {
      if (val.empty()) {
        die_usage(argv[idx]);
      }
      if (tok.find('=') == std::string::npos) {
        ++idx;
      }
      return val;
    };
    if (key == "--proto") {
      o.proto = take(i);
    } else if (key == "--host") {
      o.host = take(i);
    } else if (key == "--port") {
      o.port = static_cast<std::uint16_t>(std::stoi(take(i)));
    } else if (key == "--connections" || key == "-c") {
      o.connections = std::stoi(take(i));
    } else if (key == "--duration" || key == "-d") {
      o.duration_s = std::stod(take(i));
    } else if (key == "--warmup") {
      o.warmup_s = std::stod(take(i));
    } else if (key == "--payload") {
      o.payload = std::stoi(take(i));
    } else if (key == "--pipeline") {
      o.pipeline = std::stoi(take(i));
    } else {
      die_usage(argv[i]);
    }
  }
  o.payload = std::max(o.payload, 8);
  o.pipeline = std::max(o.pipeline, 1);
  o.connections = std::max(o.connections, 1);
  return o;
}

// A fixed 1-microsecond-resolution histogram from 0 to 2 s. 16 MB of counts;
// cheap to record into and to walk once at the end.
class histogram {
public:
  static constexpr std::uint64_t us_ceiling = 2'000'000;

  histogram() : counts_(us_ceiling, 0) {}

  void record(std::uint64_t ns) {
    std::uint64_t us = ns / 1000;
    if (us >= us_ceiling) {
      ++overflow_;
      us = us_ceiling - 1;
    }
    ++counts_[us];
    ++count_;
    sum_ns_ += ns;
    min_ns_ = std::min(min_ns_, ns);
    max_ns_ = std::max(max_ns_, ns);
  }

  void reset() {
    std::fill(counts_.begin(), counts_.end(), 0);
    count_ = sum_ns_ = overflow_ = max_ns_ = 0;
    min_ns_ = UINT64_MAX;
  }

  std::uint64_t count() const { return count_; }
  double min_us() const { return count_ ? min_ns_ / 1000.0 : 0.0; }
  double max_us() const { return count_ ? max_ns_ / 1000.0 : 0.0; }
  double mean_us() const { return count_ ? static_cast<double>(sum_ns_) / count_ / 1000.0 : 0.0; }

  double percentile_us(double p) const {
    if (!count_) {
      return 0.0;
    }
    std::uint64_t target = static_cast<std::uint64_t>(std::ceil(p * static_cast<double>(count_)));
    target = std::max<std::uint64_t>(target, 1);
    std::uint64_t seen = 0;
    for (std::uint64_t i = 0; i < us_ceiling; ++i) {
      seen += counts_[i];
      if (seen >= target) {
        return static_cast<double>(i);
      }
    }
    return static_cast<double>(us_ceiling - 1);
  }

private:
  std::vector<std::uint64_t> counts_;
  std::uint64_t count_ = 0;
  std::uint64_t sum_ns_ = 0;
  std::uint64_t overflow_ = 0;
  std::uint64_t min_ns_ = UINT64_MAX;
  std::uint64_t max_ns_ = 0;
};

// A reply this late isn't a latency sample - on loopback it means its
// request (or the reply) was dropped, typically a UDP socket-buffer
// overflow under a bursty pipeline. Counted as an error, kept out of the
// histogram so it can't smear the tail percentiles.
inline constexpr std::uint64_t late_ns = 250'000'000;

inline void put_ts(void *p, std::uint64_t ns) { std::memcpy(p, &ns, sizeof(ns)); }
inline std::uint64_t get_ts(const void *p) {
  std::uint64_t ns;
  std::memcpy(&ns, p, sizeof(ns));
  return ns;
}

inline std::vector<std::byte> make_frame(int payload) {
  std::vector<std::byte> f(static_cast<std::size_t>(payload), std::byte{0});
  put_ts(f.data(), now_ns());
  return f;
}

// The one line benchmarks/run.py parses. throughput counts the payload
// crossing the wire in both directions.
inline void emit_result(const options &o, const histogram &h, std::uint64_t requests, std::uint64_t errors,
                        double elapsed_s) {
  double rps = elapsed_s > 0 ? static_cast<double>(requests) / elapsed_s : 0.0;
  double mbps =
      elapsed_s > 0 ? static_cast<double>(requests) * o.payload * 2.0 / elapsed_s / 1e6 : 0.0;
  std::printf("{\"stack\":\"%s\",\"proto\":\"%s\",\"connections\":%d,\"pipeline\":%d,\"payload\":%d,"
              "\"duration_s\":%.3f,\"requests\":%llu,\"errors\":%llu,\"rps\":%.1f,"
              "\"throughput_mbps\":%.2f,\"lat_us\":{\"min\":%.1f,\"mean\":%.1f,\"p50\":%.1f,"
              "\"p90\":%.1f,\"p99\":%.1f,\"p999\":%.1f,\"max\":%.1f}}\n",
              o.stack.c_str(), o.proto.c_str(), o.connections, o.pipeline, o.payload, elapsed_s,
              static_cast<unsigned long long>(requests), static_cast<unsigned long long>(errors), rps,
              mbps, h.min_us(), h.mean_us(), h.percentile_us(0.50), h.percentile_us(0.90),
              h.percentile_us(0.99), h.percentile_us(0.999), h.max_us());
  std::fflush(stdout);
}

} // namespace bench
