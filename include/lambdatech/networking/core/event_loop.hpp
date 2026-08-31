// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// The reactor at the heart of the networking layer - the analogue of
// libuv's loop behind Node.js. It is a single core::task driving one
// poll(2) cycle: fd readiness callbacks, deferred functions (queueMicrotask
// / process.nextTick), and timers (setTimeout) all run on that one loop
// thread, in that order, so socket listeners never race each other.
//
// Blocking work (getaddrinfo, disk) belongs on core::thread_pool; hand its
// result back to the loop with defer().

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include <lambdatech/networking/core/task.hpp>

namespace lambdatech::networking::core {

class event_loop {
public:
  using io_callback = std::function<void(short revents)>;
  using deferred = std::function<void()>;
  using timer_id = std::uint64_t;

  static event_loop &instance() {
    static event_loop loop;
    return loop;
  }

  event_loop() : task_([this] { iterate(); }) {
    wake_fd_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  }

  ~event_loop() {
    stop();
    if (wake_fd_ >= 0) {
      ::close(wake_fd_);
    }
  }

  event_loop(const event_loop &) = delete;
  event_loop &operator=(const event_loop &) = delete;

  // --- fd watches (all thread-safe; callback runs on the loop thread) ----

  void watch(int fd, short events, io_callback cb) {
    {
      std::unique_lock lock(mutex_);
      watches_[fd] = watch_entry{events, std::move(cb)};
    }
    wake();
  }

  void modify(int fd, short events) {
    {
      std::unique_lock lock(mutex_);
      auto it = watches_.find(fd);
      if (it == watches_.end()) {
        return;
      }
      it->second.events = events;
    }
    wake();
  }

  void unwatch(int fd) {
    {
      std::unique_lock lock(mutex_);
      watches_.erase(fd);
    }
    wake();
  }

  // --- deferred work & timers ------------------------------------------

  void defer(deferred fn) {
    {
      std::unique_lock lock(mutex_);
      deferred_.push_back(std::move(fn));
    }
    wake();
  }

  timer_id set_timeout(std::chrono::milliseconds delay, deferred fn) {
    timer_id id;
    {
      std::unique_lock lock(mutex_);
      id = next_timer_id_++;
      timers_.push_back(timer{id, std::chrono::steady_clock::now() + delay, std::move(fn)});
    }
    wake();
    return id;
  }

  void clear_timeout(timer_id id) {
    std::unique_lock lock(mutex_);
    std::erase_if(timers_, [id](const timer &t) { return t.id == id; });
  }

  // --- lifecycle -----------------------------------------------------

  void start() {
    if (started_.exchange(true)) {
      return;
    }
    quitting_.store(false);
    task_.start();
  }

  void stop() {
    if (!started_.exchange(false)) {
      return;
    }
    quitting_.store(true);
    wake();
    task_.stop(); // requests stop + joins the loop thread
    {
      std::unique_lock lock(run_mutex_);
      stop_requested_ = true;
    }
    run_cv_.notify_all();
  }

  // Blocks the calling thread until stop() is called from elsewhere.
  void run() {
    start();
    std::unique_lock lock(run_mutex_);
    run_cv_.wait(lock, [this] { return stop_requested_; });
    stop_requested_ = false;
  }

  bool running() const { return started_.load(); }
  int wake_descriptor() const { return wake_fd_; }

private:
  struct watch_entry {
    short events;
    io_callback cb;
  };
  struct timer {
    timer_id id;
    std::chrono::steady_clock::time_point when;
    deferred fn;
  };

  void wake() const {
    std::uint64_t one = 1;
    [[maybe_unused]] auto written = ::write(wake_fd_, &one, sizeof(one));
  }

  void drain_wake() {
    std::uint64_t sink = 0;
    while (::read(wake_fd_, &sink, sizeof(sink)) > 0) {
    }
  }

  int next_timeout_ms() {
    std::unique_lock lock(mutex_);
    if (!deferred_.empty()) {
      return 0;
    }
    if (timers_.empty()) {
      return -1;
    }
    auto soonest = timers_.front().when;
    for (const timer &t : timers_) {
      soonest = std::min(soonest, t.when);
    }
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(soonest - std::chrono::steady_clock::now()).count();
    return ms < 0 ? 0 : static_cast<int>(ms);
  }

  void fire_due_timers() {
    std::vector<deferred> due;
    {
      std::unique_lock lock(mutex_);
      auto now = std::chrono::steady_clock::now();
      for (auto it = timers_.begin(); it != timers_.end();) {
        if (it->when <= now) {
          due.push_back(std::move(it->fn));
          it = timers_.erase(it);
        } else {
          ++it;
        }
      }
    }
    for (auto &fn : due) {
      fn();
    }
  }

  void iterate() {
    if (quitting_.load()) {
      return;
    }

    std::vector<pollfd> fds;
    {
      std::unique_lock lock(mutex_);
      fds.reserve(watches_.size() + 1);
      fds.push_back(pollfd{wake_fd_, POLLIN, 0});
      for (const auto &[fd, w] : watches_) {
        fds.push_back(pollfd{fd, w.events, 0});
      }
    }

    int n = ::poll(fds.data(), fds.size(), next_timeout_ms());
    if (n < 0) {
      return; // EINTR or transient - just re-enter
    }

    if (fds[0].revents & POLLIN) {
      drain_wake();
    }

    std::vector<deferred> run_now;
    {
      std::unique_lock lock(mutex_);
      run_now.swap(deferred_);
    }
    for (auto &fn : run_now) {
      fn();
    }

    fire_due_timers();

    for (std::size_t i = 1; i < fds.size(); ++i) {
      if (fds[i].revents == 0 || quitting_.load()) {
        continue;
      }
      io_callback cb;
      {
        std::unique_lock lock(mutex_);
        auto it = watches_.find(fds[i].fd);
        if (it == watches_.end()) {
          continue; // removed by an earlier callback this cycle
        }
        cb = it->second.cb;
      }
      cb(fds[i].revents);
    }
  }

  task task_;
  int wake_fd_ = -1;

  mutable std::mutex mutex_;
  std::unordered_map<int, watch_entry> watches_;
  std::vector<deferred> deferred_;
  std::vector<timer> timers_;
  timer_id next_timer_id_ = 1;

  std::atomic<bool> started_{false};
  std::atomic<bool> quitting_{false};

  std::mutex run_mutex_;
  std::condition_variable run_cv_;
  bool stop_requested_ = false;
};

} // namespace lambdatech::networking::core
