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
add_subdirectory(../lib ${CMAKE_CURRENT_BINARY_DIR}/lib)

### Main
add_executable(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/../src/test.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)

### Dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE maboroutu_test_error_impl)

enable_testing()
add_test(NAME run_test_throw COMMAND ${PROJECT_NAME})
