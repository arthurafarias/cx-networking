// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// The reactor at the heart of the networking layer - the analogue of
// libuv's loop behind Node.js. One core::task drives one core::poller cycle:
// fd readiness callbacks, deferred functions (queueMicrotask /
// process.nextTick), and timers (setTimeout) all run on that one loop
// thread, in that order, so socket listeners never race each other.
//
// Blocking work (getaddrinfo, disk) belongs on core::thread_pool; hand its
// result back to the loop with defer().
//
// The OS is isolated behind core::poller / core::descriptor (SRS-008): this
// header names neither poll(2) nor eventfd.

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <vector>

#include <lambdatech/networking/core/descriptor.hpp>
#include <lambdatech/networking/core/poller.hpp>
#include <lambdatech/networking/core/task.hpp>

namespace lambdatech::networking::core {

class event_loop {
public:
  using handle = descriptor::native_handle;
  using io_callback = std::function<void(poller::ready)>;
  using deferred = std::function<void()>;
  using timer_id = std::uint64_t;

  static event_loop &instance() {
    static event_loop loop;
    return loop;
  }

  event_loop() : task_([this] { iterate(); }) {}

  ~event_loop() { stop(); }

  event_loop(const event_loop &) = delete;
  event_loop &operator=(const event_loop &) = delete;

  // --- fd watches (all thread-safe; callback runs on the loop thread) ----

  void watch(handle h, poller::interest events, io_callback cb) {
    {
      std::unique_lock lock(mutex_);
      watches_[h] = watch_entry{events, std::move(cb)};
    }
    poller::add(poller_, h, events);
  }
  void watch(const descriptor::state &d, poller::interest events, io_callback cb) {
    watch(descriptor::native(d), events, std::move(cb));
  }

  void modify(handle h, poller::interest events) {
    {
      std::unique_lock lock(mutex_);
      auto it = watches_.find(h);
      if (it == watches_.end()) {
        return;
      }
      it->second.events = events;
    }
    poller::update(poller_, h, events);
  }
  void modify(const descriptor::state &d, poller::interest events) { modify(descriptor::native(d), events); }

  void unwatch(handle h) {
    {
      std::unique_lock lock(mutex_);
      watches_.erase(h);
    }
    poller::drop(poller_, h);
  }
  void unwatch(const descriptor::state &d) { unwatch(descriptor::native(d)); }

  // --- deferred work & timers ------------------------------------------

  void defer(deferred fn) {
    {
      std::unique_lock lock(mutex_);
      deferred_.push_back(std::move(fn));
    }
    poller::interrupt(poller_);
  }

  timer_id set_timeout(std::chrono::milliseconds delay, deferred fn) {
    timer_id id;
    {
      std::unique_lock lock(mutex_);
      id = next_timer_id_++;
      timers_.push_back(timer{id, std::chrono::steady_clock::now() + delay, std::move(fn)});
    }
    poller::interrupt(poller_);
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
    poller::interrupt(poller_);
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

private:
  struct watch_entry {
    poller::interest events;
    io_callback cb;
  };
  struct timer {
    timer_id id;
    std::chrono::steady_clock::time_point when;
    deferred fn;
  };

  std::optional<std::chrono::milliseconds> next_timeout() {
    std::unique_lock lock(mutex_);
    if (!deferred_.empty()) {
      return std::chrono::milliseconds(0);
    }
    if (timers_.empty()) {
      return std::nullopt;
    }
    auto soonest = timers_.front().when;
    for (const timer &t : timers_) {
      soonest = std::min(soonest, t.when);
    }
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(soonest - std::chrono::steady_clock::now());
    return ms.count() < 0 ? std::chrono::milliseconds(0) : ms;
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

    int n = poller::wait(poller_, ready_, next_timeout());
    if (n < 0) {
      return; // transient - just re-enter
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

    for (const poller::event &ev : ready_) {
      if (quitting_.load()) {
        break;
      }
      io_callback cb;
      {
        std::unique_lock lock(mutex_);
        auto it = watches_.find(ev.handle);
        if (it == watches_.end()) {
          continue; // removed by an earlier callback this cycle
        }
        cb = it->second.cb;
      }
      cb(ev.what);
    }
  }

  task task_;
  poller::state poller_;
  std::vector<poller::event> ready_;

  mutable std::mutex mutex_;
  std::map<handle, watch_entry> watches_;
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
