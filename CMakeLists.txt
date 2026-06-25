cmake_minimum_required(VERSION 4.2)

option(CMAKE_EXPORT_COMPILE_COMMANDS "Export compile_commands.json." ON)

#option(ENABLE_FOO_TESTING "Enable tests" OFF)
#if(ENABLE_FOO_TESTING)
#  list(APPEND VCPKG_MANIFEST_FEATURES "FOO-testing")
#endif()
if(DEFINED ENV{VCPKG_ROOT})
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Use the vcpkg package manager")
endif()

#if(NOT CMAKE_BUILD_TYPE)
#  set(CMAKE_BUILD_TYPE Release)
#endif()

project(maboroutu-test
    VERSION 0.0.0.0
    # COMPAT_VERSION 0.0.0.0
    # SPDX_LICENSE  MIT
    DESCRIPTION "TODO: ADD DESCRIPTION"
    HOMEPAGE_URL ""
    LANGUAGES CXX)

add_subdirectory(test/binarystream)
add_subdirectory(test/slot_map)
