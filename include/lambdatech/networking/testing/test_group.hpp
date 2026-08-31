// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <initializer_list>
#include <string>
#include <vector>

#include "registry.hpp"
#include "test_case.hpp"

namespace lambdatech::networking::testing {

// One test_group per module, declared as an `inline static` in that
// module's testing/*_test.hpp. Construction self-registers the group into
// the global registry, so the generic runner in tests/main.cpp needs no
// knowledge of what tests exist.
class test_group {
public:
  test_group(std::string name, std::initializer_list<test_case> cases) : name_(std::move(name)), cases_(cases) {
    registry::instance().add(this);
  }

  const std::string &name() const { return name_; }
  const std::vector<test_case> &cases() const { return cases_; }

private:
  std::string name_;
  std::vector<test_case> cases_;
};

} // namespace lambdatech::networking::testing
