## ---------------------------------------------------------------------------
## PROPRIETARY CODE – Arthur de Araújo Farias 2025
## All rights reserved.  No part of this file may be reproduced, stored in a
## retrieval system, or transmitted in any form or by any means—electronic,
## mechanical, photocopying, recording, or otherwise—without the prior written
## permission of the copyright holder.
## ---------------------------------------------------------------------------

# C++ side of the cross-stack echo benchmark. Built only when
# LAMBDATECH_NETWORKING_BUILD_BENCHMARKS is ON (see the root CMakeLists).
#
#   lnw-bench-echo-server / lnw-bench-echo-client   always (core lib only)
#   asio-bench-echo-server / asio-bench-echo-client only if Boost.Asio is found

add_executable(lnw-bench-echo-server lnw_echo_server.cpp)
target_link_libraries(lnw-bench-echo-server PRIVATE lambdatech-networking)
target_include_directories(lnw-bench-echo-server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(lnw-bench-echo-server PRIVATE -O2)

add_executable(lnw-bench-echo-client lnw_echo_client.cpp)
target_link_libraries(lnw-bench-echo-client PRIVATE lambdatech-networking)
target_include_directories(lnw-bench-echo-client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(lnw-bench-echo-client PRIVATE -O2)

# Boost.Asio reference point - optional, detected here like a plugin.
find_package(Boost QUIET)
if(NOT Boost_FOUND AND EXISTS "/usr/include/boost/asio.hpp")
  # Header-only Asio: a full Boost CMake package isn't required.
  set(Boost_FOUND TRUE)
  set(Boost_INCLUDE_DIRS "/usr/include")
endif()

if(Boost_FOUND)
  foreach(role server client)
    add_executable(asio-bench-echo-${role} asio_echo_${role}.cpp)
    target_link_libraries(asio-bench-echo-${role} PRIVATE Threads::Threads)
    target_include_directories(asio-bench-echo-${role} PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR} ${Boost_INCLUDE_DIRS})
    target_compile_definitions(asio-bench-echo-${role} PRIVATE BOOST_ASIO_NO_DEPRECATED)
    target_compile_options(asio-bench-echo-${role} PRIVATE -O2)
  endforeach()
  message(STATUS "benchmarks: Boost.Asio found - asio-bench-echo-{server,client} enabled")
else()
  message(STATUS "benchmarks: Boost.Asio not found - skipping asio-bench-echo targets")
endif()
