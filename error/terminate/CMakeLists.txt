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

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
   # clang++
   add_compile_options(
     $<IF:$<CONFIG:Debug>,-O0,-O2>
     --pedantic-errors
     -fno-exceptions)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
   # g++
   add_compile_options(
     $<IF:$<CONFIG:Debug>,-O0,-O2>
     --pedantic-errors
     -fno-exceptions)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
   # icc, icx
   add_compile_options(
     $<IF:$<CONFIG:Debug>,-O0,-O2>
     --pedantic-errors
     -fno-exceptions)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
   # cl, clang-cl
   add_compile_options(
     $<IF:$<CONFIG:Debug>,/Od,/O2>
     /permissive-
     /EHs-c-r-)
endif()

# enter_fatal の診断表示。MABOROUTU_FATAL_DIAGNOSTICS を指定した場合はその値を、
# 未指定の場合はビルド種別(NDEBUGの有無)から期待値を決める。
# マルチコンフィグ生成器でビルド種別が不定の場合は判定しない。
set(MABOROUTU_FATAL_DIAGNOSTICS "" CACHE STRING
   "Force enter_fatal diagnostics on (1) or off (0). Empty: follow NDEBUG.")
get_property(_maboroutu_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
set(_maboroutu_expect_diag "")
if(NOT MABOROUTU_FATAL_DIAGNOSTICS STREQUAL "")
   add_compile_definitions(MABOROUTU_FATAL_DIAGNOSTICS=${MABOROUTU_FATAL_DIAGNOSTICS})
   set(_maboroutu_expect_diag ${MABOROUTU_FATAL_DIAGNOSTICS})
elseif(NOT _maboroutu_multi_config)
   if(CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo|MinSizeRel)$")
      set(_maboroutu_expect_diag 0)
   else()
      set(_maboroutu_expect_diag 1)
   endif()
endif()

add_compile_definitions(MABOROUTU_FATAL_TO_TERMINATE)
add_compile_definitions(_HAS_EXCEPTIONS=0)
add_subdirectory(../lib ${CMAKE_CURRENT_BINARY_DIR}/lib)

### Main
add_executable(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/../src/test.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)

### Dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE maboroutu_test_error_impl)

enable_testing()
add_test(NAME run_test_terminate COMMAND ${PROJECT_NAME})
set_tests_properties(run_test_terminate PROPERTIES PASS_REGULAR_EXPRESSION "TERMINATE_HANDLER")
set_tests_properties(run_test_terminate PROPERTIES FAIL_REGULAR_EXPRESSION  "ABORT_SIGNAL|UNREACHABLE")

if(_maboroutu_expect_diag STREQUAL "1")
   add_test(NAME run_test_terminate_diagnostic COMMAND ${PROJECT_NAME})
   set_tests_properties(run_test_terminate_diagnostic PROPERTIES
      PASS_REGULAR_EXPRESSION "maboroutu: fatal error \\(exceptions disabled\\): terminate")
elseif(_maboroutu_expect_diag STREQUAL "0")
   add_test(NAME run_test_terminate_no_diagnostic COMMAND ${PROJECT_NAME})
   set_tests_properties(run_test_terminate_no_diagnostic PROPERTIES
      PASS_REGULAR_EXPRESSION "TERMINATE_HANDLER"
      FAIL_REGULAR_EXPRESSION "maboroutu: fatal error")
endif()
