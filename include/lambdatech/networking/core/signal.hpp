// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// Vendored from the cxflow library's threading::signal
// (github.com/arthurafarias/cxflow), re-homed into
// lambdatech::networking::core. `core::event` (event.hpp) wraps this to
// give the Node.js EventEmitter-style on()/once()/emit() surface every
// socket type exposes.
//
// emit() is synchronous and ordered: it snapshots the connected slots under
// the lock, releases the lock, then invokes them in connection order on the
// calling thread - so a slot may safely disconnect itself or connect/emit
// again from within its own invocation. emit_async() is an explicit opt-in
// for fire-and-forget dispatch through a thread_pool, disabled at
// instantiation for reference-typed args.

#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>

#include <lambdatech/networking/core/thread_pool.hpp>

namespace lambdatech::networking::core {

template <typename... args_types> class signal {
private:
  struct control_block {
    std::mutex mutex;
    std::list<std::function<void(args_types...)>> sinks;
  };

public:
  using slot_type = std::function<void(args_types...)>;

  signal() : control_(std::make_shared<control_block>()) {}

  signal(const signal &) = delete;
  signal &operator=(const signal &) = delete;
  signal(signal &&) = delete;
  signal &operator=(signal &&) = delete;

  ~signal() { disconnect_all(); }

  // A live handle to one connected slot. Safe to disconnect() even after
  // the owning signal has been destroyed.
  class connection {
  public:
    connection() = default;

    void disconnect() {
      if (auto control = control_.lock(); control && valid_) {
        std::unique_lock lock(control->mutex);
        control->sinks.erase(it_);
      }
      valid_ = false;
    }

    bool connected() const { return valid_ && !control_.expired(); }

  private:
    friend class signal;

    connection(std::weak_ptr<control_block> control, typename std::list<slot_type>::iterator it)
        : control_(std::move(control)), it_(it), valid_(true) {}

    std::weak_ptr<control_block> control_;
    typename std::list<slot_type>::iterator it_{};
    bool valid_ = false;
  };

  // Move-only RAII wrapper: disconnects automatically when it goes out of
  // scope.
  class scoped_connection {
  public:
    scoped_connection() = default;
    explicit scoped_connection(connection c) : connection_(std::move(c)) {}

    scoped_connection(const scoped_connection &) = delete;
    scoped_connection &operator=(const scoped_connection &) = delete;

    scoped_connection(scoped_connection &&other) noexcept : connection_(std::move(other.connection_)) {
      other.connection_ = connection{};
    }

    scoped_connection &operator=(scoped_connection &&other) noexcept {
      if (this != &other) {
        connection_.disconnect();
        connection_ = std::move(other.connection_);
        other.connection_ = connection{};
      }
      return *this;
    }

    ~scoped_connection() { connection_.disconnect(); }

    void disconnect() { connection_.disconnect(); }
    bool connected() const { return connection_.connected(); }

  private:
    connection connection_;
  };

  connection connect(slot_type slot) {
    std::unique_lock lock(control_->mutex);
    auto it = control_->sinks.insert(control_->sinks.end(), std::move(slot));
    return connection(control_, it);
  }

  void disconnect_all() {
    std::unique_lock lock(control_->mutex);
    control_->sinks.clear();
  }

  void operator()(args_types... args) const { emit(args...); }

  void emit(args_types... args) const {
    for (const auto &slot : snapshot_sinks()) {
      slot(args...);
    }
  }

  void emit_async(thread_pool &pool, args_types... args) const {
    emit_async(pool, task_priority::normal, args...);
  }

  void emit_async(thread_pool &pool, task_priority priority, args_types... args) const {
    static_assert((!std::is_reference_v<args_types> && ...),
                  "signal::emit_async cannot safely capture reference-typed arguments "
                  "(the referent may not outlive the deferred call) - use emit() instead");
    for (const auto &slot : snapshot_sinks()) {
      pool.submit([slot, args...]() { slot(args...); }, priority);
    }
  }

  signal &operator+=(slot_type slot) {
    connect(std::move(slot));
    return *this;
  }

  std::size_t slot_count() const {
    std::unique_lock lock(control_->mutex);
    return control_->sinks.size();
  }

private:
  std::vector<slot_type> snapshot_sinks() const {
    std::unique_lock lock(control_->mutex);
    return std::vector<slot_type>(control_->sinks.begin(), control_->sinks.end());
  }

  std::shared_ptr<control_block> control_;
};

} // namespace lambdatech::networking::core
