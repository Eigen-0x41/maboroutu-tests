cmake_minimum_required(VERSION 3.28)

option(MABOROUTU_ENABLE_EXTENSION
   "Build/verify maboroutu_extension module tests (slot_map, keyed_slot_map, \
stream_data_source, console, source_location_format). Mirrors the library's \
own MABOROUTU_ENABLE_EXTENSION option; when OFF, the extension/ test \
directory is skipped entirely."
   ON)

project(maboroutu_test_error_impl
    VERSION 0.0.0.0
    # COMPAT_VERSION 0.0.0.0
    # SPDX_LICENSE  MIT
    DESCRIPTION "TODO: ADD DESCRIPTION"
    HOMEPAGE_URL ""
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED WORKSPACE_NAME)
  set(WORKSPACE_NAME "${PROJECT_NAME}")
endif()

option(${PROJECT_NAME}_SHARED_LIBS "Build ${PROJECT_NAME} as a shared library." OFF)

if(${PROJECT_NAME}_SHARED_LIBS)
  add_library(${PROJECT_NAME} SHARED)
else()
  add_library(${PROJECT_NAME} STATIC)
endif()

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_26)

file(GLOB_RECURSE ${PROJECT_NAME}_MODULE_INTERFACE
  "${PROJECT_SOURCE_DIR}/src/*.cppm")
file(GLOB_RECURSE ${PROJECT_NAME}_MODULE_IMPLEMENTATION
  "${PROJECT_SOURCE_DIR}/src/*.cpp")
target_sources(${PROJECT_NAME}
  PUBLIC
  FILE_SET ${PROJECT_NAME}_FILE_SET
  TYPE CXX_MODULES
  BASE_DIRS ${PROJECT_SOURCE_DIR}/src
  FILES ${${PROJECT_NAME}_MODULE_INTERFACE})
target_sources(${PROJECT_NAME}
  PRIVATE
  ${${PROJECT_NAME}_MODULE_IMPLEMENTATION})

### Dependencies
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
target_link_libraries(${PROJECT_NAME} PRIVATE maboroutu::core)


### Transparency
target_compile_definitions(${PROJECT_NAME} PUBLIC ${PROJECT_NAME}_PUBLIC)
target_compile_definitions(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_PRIVATE)
