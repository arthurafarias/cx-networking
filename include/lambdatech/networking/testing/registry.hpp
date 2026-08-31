// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <vector>

namespace lambdatech::networking::testing {

class test_group;

// Meyer's singleton: guarantees the registry exists before any `inline
// static test_group` in any generated all_tests.cpp registers itself,
// regardless of static-initialization order across translation units.
class registry {
public:
  static registry &instance() {
    static registry r;
    return r;
  }

  void add(test_group *group) { groups_.push_back(group); }
  const std::vector<test_group *> &groups() const { return groups_; }

private:
  registry() = default;
  std::vector<test_group *> groups_;
};

} // namespace lambdatech::networking::testing
