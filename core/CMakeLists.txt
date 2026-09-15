# =============================================================================
# core/ -- maboroutu_core が提供する各モジュールのテスト。
#
# library_spec.md 3章のモジュール一覧に対応させ、`src/` 配下に
# 「モジュール名1つにつきtestファイル1つ」の対応を取る
# （ライブラリ本体の core/src/*.cppm 構成をそのまま踏襲）。
#
# 各テストは独立した実行ファイル（test_<module>）としてビルドし、
# CTest には gtest_discover_tests() でテストケース単位まで登録する。
# =============================================================================

# maboroutu::core が要求するコンパイラ機能（cxx_std_26 等）をそのまま
# 引き継がせるため、target_link_libraries 経由で PUBLIC 伝播に頼り、
# ここで独自に target_compile_features は指定しない。
function(maboroutu_add_core_test name)
   add_executable(${name} src/${name}.cpp)
   target_link_libraries(${name} PRIVATE
      maboroutu::core
      GTest::gtest
      GTest::gtest_main)
   # import 文を含む翻訳単位であることをスキャナに伝える
   # （CMAKE_CXX_SCAN_FOR_MODULES ON をルートで設定済みだが、明示しておく）。
   set_target_properties(${name} PROPERTIES CXX_SCAN_FOR_MODULES ON)
   gtest_discover_tests(${name})
endfunction()

maboroutu_add_core_test(test_core)
maboroutu_add_core_test(test_error)
maboroutu_add_core_test(test_data_source)
maboroutu_add_core_test(test_data_buffer)
maboroutu_add_core_test(test_sequential_source)
maboroutu_add_core_test(test_binary_convert)
maboroutu_add_core_test(test_sequential_view)
maboroutu_add_core_test(test_extension_store)
maboroutu_add_core_test(test_io_pattern)
