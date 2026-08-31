// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <functional>
#include <string>
#include <utility>

#include "test_context.hpp"

namespace lambdatech::networking::testing {

class test_case {
public:
  using body_type = std::function<void(test_context &)>;

  test_case(std::string name, body_type body) : name_(std::move(name)), body_(std::move(body)) {}

  // Runs in a freshly exec'd child process, for cases that depend on
  // process-lifetime global state being untouched by any earlier case in
  // the same binary.
  test_case &isolated() {
    isolated_ = true;
    return *this;
  }

  // Runs in a freshly exec'd child process and PASSES only if that process
  // terminates abnormally rather than returning normally.
  test_case &expect_crash() {
    isolated_ = true;
    expect_crash_ = true;
    return *this;
  }

  const std::string &name() const { return name_; }
  bool needs_subprocess() const { return isolated_; }
  bool expects_crash() const { return expect_crash_; }
  void run(test_context &ctx) const { body_(ctx); }

private:
  std::string name_;
  body_type body_;
  bool isolated_ = false;
  bool expect_crash_ = false;
};

} // namespace lambdatech::networking::testing
