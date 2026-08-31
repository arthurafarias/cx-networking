// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

// LambdaTech Networking closed-loop load generator for the cross-stack
// benchmark. Opens `--connections` TCP connections (or UDP flows) on one
// event loop - the single-reactor model the library is built around - keeps
// `--pipeline` requests in flight on each, and books the round-trip of every
// echoed frame into a histogram. Emits one JSON result line (see
// bench_common.hpp).
//
//   lnw-bench-echo-client --proto tcp --host 127.0.0.1 --port 7001 \
//       --connections 64 --duration 10 --warmup 3 --payload 64 --pipeline 1

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include <lambdatech/networking/core/buffer.hpp>
#include <lambdatech/networking/core/event_loop.hpp>
#include <lambdatech/networking/protocol/tcp/client.hpp>
#include <lambdatech/networking/protocol/udp/peer.hpp>

#include "bench_common.hpp"

namespace core = lambdatech::networking::core;
namespace tcp = lambdatech::networking::protocol::tcp;
namespace udp = lambdatech::networking::protocol::udp;

namespace {

// Shared state - every field is touched only on the loop thread (from
// listeners and deferred callbacks); main() reads them after loop.stop()
// has joined that thread.
struct run_state {
  bench::options o;
  bench::histogram hist;
  bool measuring = false;
  bool running = true;
  std::uint64_t requests = 0;
  std::uint64_t errors = 0;
  std::uint64_t t_start = 0;
  std::uint64_t t_end = 0;
};

void sleep_s(double s) { std::this_thread::sleep_for(std::chrono::duration<double>(s)); }

int run_tcp(run_state &st) {
  core::event_loop loop;
  const int P = st.o.payload;

  struct conn {
    std::shared_ptr<tcp::client> sock;
    std::vector<std::byte> in;
  };
  auto conns = std::make_shared<std::vector<conn>>(st.o.connections);

  auto send_one = [&st, P](conn &c) {
    auto frame = bench::make_frame(P);
    c.sock->write(frame);
  };

  for (int i = 0; i < st.o.connections; ++i) {
    conn &c = (*conns)[i];
    c.sock = tcp::client::create(loop);
    conn *cp = &c;
    c.sock->on_connect() += [&st, cp, send_one] {
      for (int k = 0; k < st.o.pipeline; ++k) {
        send_one(*cp);
      }
    };
    c.sock->on_data() += [&st, cp, P, send_one](const core::buffer &chunk) {
      cp->in.insert(cp->in.end(), chunk.begin(), chunk.end());
      while (static_cast<int>(cp->in.size()) >= P) {
        std::uint64_t ts = bench::get_ts(cp->in.data());
        cp->in.erase(cp->in.begin(), cp->in.begin() + P);
        if (st.measuring) {
          st.hist.record(bench::now_ns() - ts);
          ++st.requests;
        }
        if (st.running) {
          send_one(*cp);
        }
      }
    };
    c.sock->on_error() += [&st](const std::string &) { ++st.errors; };
    c.sock->connect(st.o.port, st.o.host);
  }

  loop.start();
  sleep_s(st.o.warmup_s);
  loop.defer([&st] {
    st.measuring = true;
    st.t_start = bench::now_ns();
  });
  sleep_s(st.o.duration_s);
  loop.defer([&st] {
    st.measuring = false;
    st.running = false;
    st.t_end = bench::now_ns();
  });
  sleep_s(0.1);
  loop.stop();
  return 0;
}

int run_udp(run_state &st) {
  core::event_loop loop;
  const int P = st.o.payload;

  struct flow {
    std::shared_ptr<udp::peer> sock;
    int inflight = 0;
    std::uint64_t last_rx = 0;
  };
  auto flows = std::make_shared<std::vector<flow>>(st.o.connections);

  auto fire = [&st, P](flow &f) {
    auto frame = bench::make_frame(P);
    f.sock->send(frame, st.o.port, st.o.host);
    ++f.inflight;
  };
  auto refill = [&st, fire](flow &f) {
    while (f.inflight < st.o.pipeline && st.running) {
      fire(f);
    }
  };

  for (int i = 0; i < st.o.connections; ++i) {
    flow &f = (*flows)[i];
    f.sock = udp::peer::create("udp4", loop);
    flow *fp = &f;
    f.sock->on_message() += [&st, fp, refill](const core::buffer &msg, const core::socket_address &) {
      if (msg.size() >= 8) {
        std::uint64_t rtt = bench::now_ns() - bench::get_ts(msg.data());
        if (rtt > bench::late_ns) {
          ++st.errors;
        } else if (st.measuring) {
          st.hist.record(rtt);
          ++st.requests;
        }
      }
      if (fp->inflight > 0) {
        --fp->inflight;
      }
      fp->last_rx = bench::now_ns();
      refill(*fp);
    };
    f.sock->on_error() += [&st](const std::string &) { ++st.errors; };
  }

  // Liveness sweep: a datagram lost on loopback would otherwise wedge its
  // flow forever. Every 100 ms, write off anything outstanding longer than
  // 200 ms as a drop and top the flow back up.
  auto sweep = std::make_shared<std::function<void()>>();
  *sweep = [&st, flows, sweep, &loop, refill] {
    std::uint64_t now = bench::now_ns();
    for (flow &f : *flows) {
      if (f.inflight > 0 && f.last_rx != 0 && now - f.last_rx > 200'000'000ULL) {
        st.errors += static_cast<std::uint64_t>(f.inflight);
        f.inflight = 0;
      }
      f.last_rx = now;
      refill(f);
    }
    if (st.running) {
      loop.set_timeout(std::chrono::milliseconds(100), *sweep);
    }
  };

  loop.start();
  loop.defer([flows, &loop, refill] {
    for (flow &f : *flows) {
      f.last_rx = bench::now_ns();
      refill(f);
    }
  });
  loop.set_timeout(std::chrono::milliseconds(100), *sweep);

  sleep_s(st.o.warmup_s);
  loop.defer([&st] {
    st.measuring = true;
    st.t_start = bench::now_ns();
  });
  sleep_s(st.o.duration_s);
  loop.defer([&st] {
    st.measuring = false;
    st.running = false;
    st.t_end = bench::now_ns();
  });
  sleep_s(0.1);
  loop.stop();
  *sweep = nullptr; // break the self-reference the re-arm closure holds
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  run_state st;
  st.o = bench::parse_options(argc, argv, "lnw");

  if (st.o.proto == "udp") {
    run_udp(st);
  } else {
    run_tcp(st);
  }

  double elapsed = st.t_end > st.t_start ? (st.t_end - st.t_start) / 1e9 : st.o.duration_s;
  bench::emit_result(st.o, st.hist, st.requests, st.errors, elapsed);
  return 0;
}
