cmake_minimum_required(VERSION 3.28)

# =============================================================================
# maboroutu-tests
#
# maboroutu (library_spec.md 記載名: mylib) の GTest 検証プロジェクト。
# library_spec.md 6章「検証方針」の記述:
#   「本ライブラリとは別プロジェクトにて、GTestを用いた検証を行う」
# に従い、本プロジェクトはライブラリ本体のリポジトリとは別プロジェクトとして
# 構成する。ライブラリ本体は FetchContent 経由で取得し、CMake の
# `FILE_SET ... TYPE CXX_MODULES` によって公開されたモジュール（BMI）を
# import して検証する。
#
# ディレクトリ構成（要望通り、ライブラリ本体の core/extension 構成をそのまま
# 踏襲する）:
#   maboroutu-tests/
#   ├ CMakeLists.txt          (本ファイル)
#   ├ core/
#   │ ├ CMakeLists.txt
#   │ └ src/                  (mylib.core が提供する各モジュールに1対1対応)
#   │   ├ test_core.cpp
#   │   ├ test_error.cpp
#   │   ├ test_data_source.cpp
#   │   ├ test_data_buffer.cpp
#   │   ├ test_sequential_source.cpp
#   │   ├ test_binary_convert.cpp
#   │   ├ test_sequential_view.cpp
#   │   ├ test_extension_store.cpp
#   │   └ test_io_pattern.cpp
#   ├ extension/
#   │ └ src/
#   │   ├ test_slot_map.cpp
#   │   ├ test_keyed_slot_map.cpp
#   │   ├ test_stream_data_source.cpp
#   │   ├ test_memory_data_buffer.cpp
#   │   └ test_binary_layout_handle.cpp
#   └ appkit/                 (maboroutu-appkit。別リポジトリで提供される)
#     └ src/
#       ├ test_console.cpp
#       └ test_source_location_format.cpp
# =============================================================================

project(maboroutu_tests LANGUAGES CXX)

# --- C++ modules を使うために Ninja / Visual Studio ジェネレータが必要 ---
if(NOT CMAKE_GENERATOR MATCHES "Ninja" AND NOT CMAKE_GENERATOR MATCHES "Visual Studio")
   message(WARNING
      "C++ modules (import maboroutu.*) require the Ninja or Visual Studio "
      "generator. Current generator: '${CMAKE_GENERATOR}'. Configure with "
      "'-G Ninja' (recommended, see .github/workflows/ci.yml).")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
   # clang++
   add_compile_options(--pedantic-errors)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
   # g++
   add_compile_options(--pedantic-errors)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
   # icc, icx
   add_compile_options(--pedantic-errors)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
   # cl, clang-cl
   add_compile_options(/permissive- /EHscr)
endif()

# import 文しか含まない（自身はモジュールを export しない）翻訳単位も
# 依存関係スキャンの対象にする。これが無いとテスト側の `import maboroutu.*;`
# が解決されない。
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

option(MABOROUTU_ENABLE_EXTENSION
   "Build/verify maboroutu_extension module tests (slot_map, keyed_slot_map, \
stream_data_source, memory_data_buffer, binary_layout_handle). Mirrors the \
library's own MABOROUTU_ENABLE_EXTENSION option; when OFF, the extension/ test \
directory is skipped entirely."
   ON)

option(MABOROUTU_ENABLE_APPKIT_TESTS
   "Fetch maboroutu-appkit and build/verify its module tests (console, \
source_location_format). When OFF, the appkit/ test directory is skipped \
and maboroutu-appkit is not fetched."
   ON)

# -----------------------------------------------------------------------
# 1. maboroutu 本体（テスト対象）の取得
# -----------------------------------------------------------------------
# 通常運用: FetchContent で GitHub から取得する。
# ローカル開発時にライブラリを既に手元にcloneしてある場合は、
# -DMABOROUTU_SOURCE_DIR=/path/to/maboroutu を指定するとネットワーク取得を
# 省略し、そのディレクトリを直接 add_subdirectory() する
# （FetchContent_Declare(... SOURCE_DIR ...) を使用）。
#
# NOTE（推測・要確認）: MABOROUTU_GIT_REPOSITORY の既定値は、アップロードされた
# vcpkg.json の "homepage" フィールド（https://github.com/Eigen-0x41/maboroutu）
# から採ったものであり、実際にテスト対象コードがそのURL・ブランチで公開されて
# いることを確認したものではない。CI・ローカルとも、実際の環境に合わせて
# MABOROUTU_GIT_REPOSITORY / MABOROUTU_GIT_TAG を必ず調整すること。
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
# NOTE: maboroutu 側の CMakeLists.txt は project(maboroutu LANGUAGES NONE) と
# option(MABOROUTU_ENABLE_EXTENSION ... ON) を持つ。option() は既にキャッシュ
# 済みの変数を上書きしないため、本プロジェクトが先に同名の変数を定義して
# おくことで、テスト側の MABOROUTU_ENABLE_EXTENSION の指定がライブラリ側の
# ビルド構成（extension サブディレクトリを含めるか）にもそのまま伝播する。
FetchContent_MakeAvailable(maboroutu)

# maboroutu-appkit（console・source_location_format）。maboroutu 本体とは
# 独立したリポジトリであり、標準ライブラリのみに依存する。
if(MABOROUTU_ENABLE_APPKIT_TESTS)
   set(MABOROUTU_APPKIT_SOURCE_DIR "" CACHE PATH
      "Path to an existing local checkout of maboroutu-appkit. If set, \
FetchContent uses this directory directly instead of cloning over the network.")
   set(MABOROUTU_APPKIT_GIT_REPOSITORY
      "https://github.com/Eigen-0x41/maboroutu-appkit.git" CACHE STRING
      "Git URL of the maboroutu-appkit repository under test.")
   set(MABOROUTU_APPKIT_GIT_TAG "main" CACHE STRING
      "Git branch/tag/commit of maboroutu-appkit to check out and test.")
   if(MABOROUTU_APPKIT_SOURCE_DIR)
      FetchContent_Declare(maboroutu_appkit
         SOURCE_DIR "${MABOROUTU_APPKIT_SOURCE_DIR}")
   else()
      FetchContent_Declare(maboroutu_appkit
         GIT_REPOSITORY "${MABOROUTU_APPKIT_GIT_REPOSITORY}"
         GIT_TAG        "${MABOROUTU_APPKIT_GIT_TAG}")
   endif()
   FetchContent_MakeAvailable(maboroutu_appkit)
endif()

# -----------------------------------------------------------------------
# 2. GoogleTest の取得
# -----------------------------------------------------------------------
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(googletest
   GIT_REPOSITORY https://github.com/google/googletest.git
   GIT_TAG        v1.15.2)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)
enable_testing()

# -----------------------------------------------------------------------
# 2.5 ライフタイム解析（任意）
#
# コンパイラの解析を追加する。解析はバグの検出を補助するものであり、
# バグが無いことは保証しない。指定した解析を構成できない場合は、無効のまま
# 進めず、Configureを失敗させる。
#   - Clang: -Wlifetime-safety-permissive をテストコードへ追加する。
#   - MSVC : /analyze と /analyze:plugin EspXEngine.dll を追加する
#            （C++ Core Check は環境変数 Esp.Extensions=CppCoreCheck.dll で
#            有効になる）。モジュールをimportする翻訳単位の解析には、importされる
#            モジュールのIFCも /analyze でビルドされている必要があるため
#            （C1028）、ライブラリ本体とappkitにも追加する。
#            GoogleTest は /analyze でビルドできないため対象外とする。
#   この位置（GoogleTestの取得より後）で追加するため、GoogleTestには適用されない。
# -----------------------------------------------------------------------
option(MABOROUTU_TESTS_LIFETIME_SAFETY
   "Enable lifetime analysis (Clang -Wlifetime-safety-permissive, MSVC /analyze)." OFF)
if(MABOROUTU_TESTS_LIFETIME_SAFETY)
   if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
      include(CheckCXXCompilerFlag)
      check_cxx_compiler_flag(-Wlifetime-safety-permissive
         MABOROUTU_COMPILER_HAS_LIFETIME_SAFETY)
      if(NOT MABOROUTU_COMPILER_HAS_LIFETIME_SAFETY)
         message(FATAL_ERROR
            "MABOROUTU_TESTS_LIFETIME_SAFETY: this Clang "
            "(${CMAKE_CXX_COMPILER_VERSION}) does not support "
            "-Wlifetime-safety-permissive.")
      endif()
      add_compile_options(-Wlifetime-safety-permissive)
   elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
      # SHELL: を付けないと、CMakeが "EspXEngine.dll" を別の引数として扱い、
      # ソースファイルと解釈する。
      set(MABOROUTU_ANALYZE_FLAGS /analyze "SHELL:/analyze:plugin EspXEngine.dll")
      add_compile_options(${MABOROUTU_ANALYZE_FLAGS})
      foreach(target maboroutu_core maboroutu_extension maboroutu_appkit)
         if(TARGET ${target})
            target_compile_options(${target} PRIVATE ${MABOROUTU_ANALYZE_FLAGS})
         endif()
      endforeach()
   else()
      message(FATAL_ERROR
         "MABOROUTU_TESTS_LIFETIME_SAFETY: no lifetime analysis is configured "
         "for ${CMAKE_CXX_COMPILER_ID}.")
   endif()
endif()

# -----------------------------------------------------------------------
# 3. 各モジュールのテストディレクトリ
# -----------------------------------------------------------------------
add_subdirectory(core)
if(MABOROUTU_ENABLE_EXTENSION)
   add_subdirectory(extension)
endif()
if(MABOROUTU_ENABLE_APPKIT_TESTS)
   add_subdirectory(appkit)
endif()
