// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// Vendored from the cxflow library's threading::task
// (github.com/arthurafarias/cxflow), re-homed into
// lambdatech::networking::core. A dedicated-thread repeating-loop runner;
// the network event_loop is exactly one of these driving a poll() cycle.

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace lambdatech::networking::core {

class task {
public:
  using loop_function = std::function<void()>;

  explicit task(loop_function loop);

  task(const task &) = delete;
  task &operator=(const task &) = delete;
  task(task &&) = delete;
  task &operator=(task &&) = delete;

  ~task();

  // Spawns the dedicated thread if not already running. No-op if running.
  void start();

  // Blocks the loop before its next iteration until resume() is called.
  // Does not interrupt an iteration already in progress.
  void pause();
  void resume();

  // Signals the loop to exit and joins the thread. No-op if not running.
  void stop();

  bool is_running() const;

private:
  void run(std::stop_token stop_token);

  loop_function loop_;
  std::jthread thread_;

  mutable std::mutex mutex_;
  std::condition_variable_any cond_;
  bool paused_ = false;
  bool running_ = false;
};

inline task::task(loop_function loop) : loop_(std::move(loop)) {}

inline task::~task() { stop(); }

inline void task::start() {
  std::unique_lock lock(mutex_);
  if (running_) {
    return;
  }
  running_ = true;
  paused_ = false;
  lock.unlock();

  thread_ = std::jthread([this](std::stop_token stop_token) { run(stop_token); });
}

inline void task::pause() {
  std::unique_lock lock(mutex_);
  paused_ = true;
}

inline void task::resume() {
  {
    std::unique_lock lock(mutex_);
    paused_ = false;
  }
  cond_.notify_all();
}

inline void task::stop() {
  {
    std::unique_lock lock(mutex_);
    if (!running_) {
      return;
    }
    running_ = false;
    paused_ = false;
  }
  cond_.notify_all();

  if (thread_.joinable()) {
    thread_.request_stop();
    thread_.join();
  }
}

inline bool task::is_running() const {
  std::unique_lock lock(mutex_);
  return running_;
}

inline void task::run(std::stop_token stop_token) {
  while (!stop_token.stop_requested()) {
    {
      std::unique_lock lock(mutex_);
      cond_.wait(lock, stop_token, [this] { return !paused_; });
    }

    if (stop_token.stop_requested()) {
      break;
    }

    if (loop_) {
      loop_();
    }
  }
}

} // namespace lambdatech::networking::core
