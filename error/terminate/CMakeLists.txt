cmake_minimum_required(VERSION 3.28)

project(maboroutu_test_error_terminate
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

option(MABOROUTU_ENABLE_EXTENSION
   "Build/verify maboroutu_extension module tests (slot_map, keyed_slot_map, \
stream_data_source, console, source_location_format). Mirrors the library's \
own MABOROUTU_ENABLE_EXTENSION option; when OFF, the extension/ test \
directory is skipped entirely."
   ON)


if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  # clang++
  add_compile_options(--pedantic-errors -fno-exceptions)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  # g++
  add_compile_options(--pedantic-errors -fno-exceptions)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
  # icc, icx
  add_compile_options(--pedantic-errors -fno-exceptions)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # cl, clang-cl
  add_compile_options(/permissive- /EHs-c-r-)
endif()

set(MABOROUTU_FATAL_TO_TERMINATE ON)
add_subdirectory(../lib)

### Main
add_executable(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/src/${PROJECT_NAME}.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)

### Dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE maboroutu_test_error_impl)

enable_testing()
add_test(NAME run_test_terminate COMMAND ${PROJECT_NAME})
