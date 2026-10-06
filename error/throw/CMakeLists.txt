cmake_minimum_required(VERSION 3.28)

project(maboroutu_test_error_throw
    VERSION 0.0.0.0
    # COMPAT_VERSION 0.0.0.0
    # SPDX_LICENSE  MIT
    DESCRIPTION "TODO: ADD DESCRIPTION"
    HOMEPAGE_URL ""
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  # clang++
  add_compile_options(
     $<IF:$<CONFIG:Debug>,-O0,-O2>
     --pedantic-errors)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  # g++
  add_compile_options(
     $<IF:$<CONFIG:Debug>,-O0,-O2>
     --pedantic-errors)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
  # icc, icx
  add_compile_options(
     $<IF:$<CONFIG:Debug>,-O0,-O2>
     --pedantic-errors)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # cl, clang-cl
  add_compile_options(
     $<IF:$<CONFIG:Debug>,/Od,/O2>
     /permissive-
     /EHscr)
endif()


add_compile_definitions(MABOROUTU_FATAL_TO_TERMINATE)
add_compile_definitions(_HAS_EXCEPTIONS=1)
add_subdirectory(../lib ${CMAKE_CURRENT_BINARY_DIR}/lib)

### Main
add_executable(${PROJECT_NAME}0 ${PROJECT_SOURCE_DIR}/../src/test.cpp)
target_include_directories(${PROJECT_NAME}0 PUBLIC ${PROJECT_SOURCE_DIR}/include)

### Dependencies
target_link_libraries(${PROJECT_NAME}0 PRIVATE maboroutu_test_error_impl)


### Main
add_executable(${PROJECT_NAME}1 ${PROJECT_SOURCE_DIR}/../src/try_catch.cpp)
target_include_directories(${PROJECT_NAME}1 PUBLIC ${PROJECT_SOURCE_DIR}/include)

### Dependencies
target_link_libraries(${PROJECT_NAME}1 PRIVATE maboroutu_test_error_impl)
include(FetchContent)
set(MABOROUTU_SOURCE_DIR "" CACHE PATH
   "Path to an existing local checkout of the maboroutu library. If set, \
FetchContent uses this directory directly instead of cloning over the \
network (useful for local development against a working copy).")
set(MABOROUTU_GIT_REPOSITORY "https://github.com/Eigen-0x41/maboroutu.git" CACHE STRING
   "Git URL of the maboroutu library repository under test. \
NOTE: taken from vcpkg.json's \"homepage\" field; not independently \
verified as the canonical source location. Adjust as needed.")
set(MABOROUTU_GIT_TAG "main" CACHE STRING
   "Git branch/tag/commit of the maboroutu library to check out and test. \
Pin this to a specific commit/tag for reproducible CI runs.")
if(MABOROUTU_SOURCE_DIR)
   FetchContent_Declare(maboroutu SOURCE_DIR "${MABOROUTU_SOURCE_DIR}")
else()
   FetchContent_Declare(maboroutu
      GIT_REPOSITORY "${MABOROUTU_GIT_REPOSITORY}"
      GIT_TAG        "${MABOROUTU_GIT_TAG}")
endif()
FetchContent_MakeAvailable(maboroutu)
target_link_libraries(${PROJECT_NAME}1 PRIVATE maboroutu::core)



enable_testing()
add_test(NAME run_test_throw COMMAND ${PROJECT_NAME}0)
add_test(NAME run_test_throw COMMAND ${PROJECT_NAME}1)
