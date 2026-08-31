// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <format>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include <lambdatech/networking/testing/registry.hpp>
#include <lambdatech/networking/testing/test_group.hpp>

namespace lambdatech::networking::testing {

namespace detail {

inline int run_single(const std::string &group_name, const std::string &case_name) {
  for (auto *group : registry::instance().groups()) {
    if (group->name() != group_name) {
      continue;
    }
    for (const auto &test_case : group->cases()) {
      if (test_case.name() != case_name) {
        continue;
      }
      test_context ctx;
      test_case.run(ctx);
      return ctx.failed() ? 1 : 0;
    }
  }
  std::cerr << "no such test: " << group_name << "::" << case_name << "\n";
  return 2;
}

inline bool run_isolated(const std::string &executable, const std::string &group_name, const std::string &case_name,
                         bool expect_crash) {
  std::string combined = std::format("{}::{}", group_name, case_name);

  pid_t child = fork();
  if (child == 0) {
    execl(executable.c_str(), executable.c_str(), "--run", combined.c_str(), static_cast<char *>(nullptr));
    _exit(127); // execl only returns on failure
  }

  int status = 0;
  waitpid(child, &status, 0);

  bool crashed = WIFSIGNALED(status);
  bool exited_cleanly = WIFEXITED(status) && WEXITSTATUS(status) == 0;

  return expect_crash ? crashed : exited_cleanly;
}

} // namespace detail

inline int run_all(int argc, char **argv) {
  if (argc == 3 && std::string(argv[1]) == "--run") {
    std::string arg = argv[2];
    auto separator = arg.find("::");
    return detail::run_single(arg.substr(0, separator), arg.substr(separator + 2));
  }

  int total = 0;
  int passed = 0;

  for (auto *group : registry::instance().groups()) {
    for (const auto &test_case : group->cases()) {
      ++total;
      bool ok = false;
      std::vector<std::string> failures;

      if (test_case.needs_subprocess()) {
        ok = detail::run_isolated(argv[0], group->name(), test_case.name(), test_case.expects_crash());
      } else {
        test_context ctx;
        try {
          test_case.run(ctx);
        } catch (const std::exception &e) {
          ctx.fail(std::source_location::current(), std::format("uncaught exception: {}", e.what()));
        } catch (...) {
          ctx.fail(std::source_location::current(), "uncaught non-exception value");
        }
        ok = !ctx.failed();
        failures = ctx.failures();
      }

      std::cout << (ok ? "[  OK  ] " : "[ FAIL ] ") << group->name() << "::" << test_case.name() << "\n";
      for (const auto &failure : failures) {
        std::cout << "         " << failure << "\n";
      }

      passed += ok ? 1 : 0;
    }
  }

  std::cout << "\n" << passed << "/" << total << " tests passed\n";
  return passed == total ? 0 : 1;
}

} // namespace lambdatech::networking::testing
