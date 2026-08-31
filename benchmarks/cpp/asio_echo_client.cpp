// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

// Boost.Asio closed-loop load generator - the C++ reference point for the
// benchmark. One io_context on one thread; `--connections` sockets each
// keeping `--pipeline` frames in flight. Emits one JSON result line.

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <thread>
#include <vector>

#include <boost/asio.hpp>

#include "bench_common.hpp"

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using udp = asio::ip::udp;

namespace {

struct run_state {
  bench::options o;
  bench::histogram hist;
  bool measuring = false;
  bool running = true;
  std::uint64_t requests = 0;
  std::uint64_t errors = 0;
};

void sleep_s(double s) { std::this_thread::sleep_for(std::chrono::duration<double>(s)); }

// --- TCP -----------------------------------------------------------------
class tcp_conn : public std::enable_shared_from_this<tcp_conn> {
public:
  tcp_conn(asio::io_context &io, run_state &st) : sock_(io), st_(st), rbuf_(65536) {}

  void connect(const tcp::endpoint &ep) {
    auto self = shared_from_this();
    sock_.async_connect(ep, [this, self](boost::system::error_code ec) {
      if (ec) {
        ++st_.errors;
        return;
      }
      sock_.set_option(tcp::no_delay(true));
      for (int k = 0; k < st_.o.pipeline; ++k) {
        enqueue();
      }
      read();
    });
  }

private:
  void enqueue() {
    auto frame = std::make_shared<std::vector<std::byte>>(bench::make_frame(st_.o.payload));
    wq_.push_back(frame);
    ++inflight_;
    if (!writing_) {
      flush();
    }
  }

  void flush() {
    if (wq_.empty()) {
      writing_ = false;
      return;
    }
    writing_ = true;
    // Coalesce everything queued into one scatter-gather write, so a
    // pipelined client isn't paying one send() syscall per frame.
    auto self = shared_from_this();
    auto batch = std::make_shared<std::deque<std::shared_ptr<std::vector<std::byte>>>>(std::move(wq_));
    wq_.clear();
    std::vector<asio::const_buffer> iov;
    iov.reserve(batch->size());
    for (auto &f : *batch) {
      iov.push_back(asio::buffer(*f));
    }
    asio::async_write(sock_, iov, [this, self, batch](boost::system::error_code ec, std::size_t) {
      if (ec) {
        ++st_.errors;
        return;
      }
      flush(); // anything enqueued while that write was in flight
    });
  }

  void read() {
    auto self = shared_from_this();
    sock_.async_read_some(asio::buffer(rbuf_), [this, self](boost::system::error_code ec, std::size_t n) {
      if (ec) {
        ++st_.errors;
        return;
      }
      acc_.insert(acc_.end(), rbuf_.begin(), rbuf_.begin() + static_cast<std::ptrdiff_t>(n));
      const int P = st_.o.payload;
      while (static_cast<int>(acc_.size()) >= P) {
        std::uint64_t ts = bench::get_ts(acc_.data());
        acc_.erase(acc_.begin(), acc_.begin() + P);
        if (st_.measuring) {
          st_.hist.record(bench::now_ns() - ts);
          ++st_.requests;
        }
        --inflight_;
      }
      while (st_.running && inflight_ < st_.o.pipeline) {
        enqueue();
      }
      if (st_.running) {
        read();
      }
    });
  }

  tcp::socket sock_;
  run_state &st_;
  std::vector<std::byte> rbuf_;
  std::vector<std::byte> acc_;
  std::deque<std::shared_ptr<std::vector<std::byte>>> wq_;
  bool writing_ = false;
  int inflight_ = 0;
};

// --- UDP ---------------------------------------------------------------
class udp_flow : public std::enable_shared_from_this<udp_flow> {
public:
  udp_flow(asio::io_context &io, run_state &st, const udp::endpoint &peer)
      : sock_(io, udp::endpoint(peer.protocol(), 0)), peer_(peer), st_(st), rbuf_(st.o.payload) {}

  void start() {
    receive();
    top_up();
  }

  void top_up() {
    while (st_.running && inflight_ < st_.o.pipeline) {
      auto frame = bench::make_frame(st_.o.payload);
      boost::system::error_code ec;
      sock_.send_to(asio::buffer(frame), peer_, 0, ec);
      if (ec) {
        ++st_.errors;
        break;
      }
      ++inflight_;
    }
    last_activity_ = bench::now_ns();
  }

  void reap(std::uint64_t now) {
    if (inflight_ > 0 && now - last_activity_ > 200'000'000ULL) {
      st_.errors += static_cast<std::uint64_t>(inflight_);
      inflight_ = 0;
    }
    top_up();
  }

private:
  void receive() {
    auto self = shared_from_this();
    sock_.async_receive_from(asio::buffer(rbuf_), from_, [this, self](boost::system::error_code ec, std::size_t n) {
      if (!ec && n >= 8) {
        std::uint64_t rtt = bench::now_ns() - bench::get_ts(rbuf_.data());
        if (rtt > bench::late_ns) {
          ++st_.errors;
        } else if (st_.measuring) {
          st_.hist.record(rtt);
          ++st_.requests;
        }
        if (inflight_ > 0) {
          --inflight_;
        }
        top_up();
      }
      if (st_.running) {
        receive();
      }
    });
  }

  udp::socket sock_;
  udp::endpoint peer_;
  udp::endpoint from_;
  run_state &st_;
  std::vector<std::byte> rbuf_;
  int inflight_ = 0;
  std::uint64_t last_activity_ = 0;
};

int run_tcp(run_state &st) {
  asio::io_context io;
  auto guard = asio::make_work_guard(io);
  std::thread loop([&io] { io.run(); });

  tcp::endpoint ep(asio::ip::make_address(st.o.host), st.o.port);
  std::vector<std::shared_ptr<tcp_conn>> conns;
  for (int i = 0; i < st.o.connections; ++i) {
    auto c = std::make_shared<tcp_conn>(io, st);
    conns.push_back(c);
    asio::post(io, [c, ep] { c->connect(ep); });
  }

  sleep_s(st.o.warmup_s);
  std::uint64_t t0 = 0, t1 = 0;
  asio::post(io, [&st, &t0] {
    st.measuring = true;
    t0 = bench::now_ns();
  });
  sleep_s(st.o.duration_s);
  asio::post(io, [&st, &t1] {
    st.measuring = false;
    st.running = false;
    t1 = bench::now_ns();
  });
  sleep_s(0.1);
  guard.reset();
  io.stop();
  loop.join();

  double elapsed = t1 > t0 ? (t1 - t0) / 1e9 : st.o.duration_s;
  bench::emit_result(st.o, st.hist, st.requests, st.errors, elapsed);
  return 0;
}

int run_udp(run_state &st) {
  asio::io_context io;
  auto guard = asio::make_work_guard(io);
  std::thread loop([&io] { io.run(); });

  udp::endpoint peer(asio::ip::make_address(st.o.host), st.o.port);
  auto flows = std::make_shared<std::vector<std::shared_ptr<udp_flow>>>();
  for (int i = 0; i < st.o.connections; ++i) {
    flows->push_back(std::make_shared<udp_flow>(io, st, peer));
  }
  asio::post(io, [flows] {
    for (auto &f : *flows) {
      f->start();
    }
  });

  auto sweep = std::make_shared<asio::steady_timer>(io);
  std::function<void(const boost::system::error_code &)> tick =
      [&st, flows, sweep, &tick](const boost::system::error_code &ec) {
        if (ec || !st.running) {
          return;
        }
        std::uint64_t now = bench::now_ns();
        for (auto &f : *flows) {
          f->reap(now);
        }
        sweep->expires_after(std::chrono::milliseconds(100));
        sweep->async_wait(tick);
      };
  asio::post(io, [sweep, &tick] {
    sweep->expires_after(std::chrono::milliseconds(100));
    sweep->async_wait(tick);
  });

  sleep_s(st.o.warmup_s);
  std::uint64_t t0 = 0, t1 = 0;
  asio::post(io, [&st, &t0] {
    st.measuring = true;
    t0 = bench::now_ns();
  });
  sleep_s(st.o.duration_s);
  asio::post(io, [&st, &t1] {
    st.measuring = false;
    st.running = false;
    t1 = bench::now_ns();
  });
  sleep_s(0.1);
  guard.reset();
  io.stop();
  loop.join();

  double elapsed = t1 > t0 ? (t1 - t0) / 1e9 : st.o.duration_s;
  bench::emit_result(st.o, st.hist, st.requests, st.errors, elapsed);
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  run_state st;
  st.o = bench::parse_options(argc, argv, "asio");
  if (st.o.proto == "udp") {
    run_udp(st);
  } else {
    run_tcp(st);
  }
  return 0;
}
