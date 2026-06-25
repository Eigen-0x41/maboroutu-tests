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

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(binarystream
    VERSION 0.0.0.0
    # COMPAT_VERSION 0.0.0.0
    # SPDX_LICENSE  MIT
    DESCRIPTION "maboroutu test binarystream"
    HOMEPAGE_URL ""
    LANGUAGES CXX)


### Project Dependencies
# find_package(${DEPEND} REQUIRED)

### Main
add_executable(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/src/${PROJECT_NAME}.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
#target_compile_options(${PROJECT_NAME} PRIVATE -pthread)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  # clang++
  target_compile_options(${PROJECT_NAME} PRIVATE
      $<IF:$<CONFIG:Debug>,-g,-O2>
      -Wall
      -Wextra
      --pedantic-errors
      # -fno-rtti
      # -pthread
  )
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  # g++
  target_compile_options(${PROJECT_NAME} PRIVATE
      $<IF:$<CONFIG:Debug>,-g,-O2>
      -Wall
      -Wextra
      --pedantic-errors
      # -fno-rtti
      # -pthread
  )
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
  # icc, icx
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # cl, clang-cl
  target_compile_options(${PROJECT_NAME} PRIVATE
      $<IF:$<CONFIG:Debug>,/Z7,/O2>
      /EHsc
      /W4
      /permissive-
      /GR-
  )
endif()

### Dependencies
# find_package(maboroutu CONFIG REQUIRED)
# target_link_libraries(${PROJECT_NAME} PRIVATE maboroutu::maboroutu)
include(FetchContent)
FetchContent_Declare(
  maboroutu
  # GIT_REPOSITORY https://github.com/Eigen-0x41/maboroutu.git
  GIT_REPOSITORY ${PROJECT_SOURCE_DIR}/../../../maboroutu
  GIT_TAG 32fcd4085e9eb73fb0927bbf01925341065c8b4c
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(maboroutu)
target_link_libraries(${PROJECT_NAME} PRIVATE maboroutu)
find_package(GTest CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
