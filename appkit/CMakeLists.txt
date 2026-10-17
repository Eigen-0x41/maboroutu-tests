# =============================================================================
# appkit/ -- maboroutu_appkit（maboroutu::appkit）が提供する各モジュールのテスト。
#
# maboroutu_appkit は、maboroutu 0.0.3.8 まで本体の extension に含まれていた
# console・source_location_format を、別リポジトリ（maboroutu-appkit）へ
# 分離したものである。テストは、本体と同じく本リポジトリに置く。
# =============================================================================

function(maboroutu_add_appkit_test name)
   add_executable(${name} src/${name}.cpp)
   target_link_libraries(${name} PRIVATE
      maboroutu::appkit
      GTest::gtest
      GTest::gtest_main)
   set_target_properties(${name} PROPERTIES CXX_SCAN_FOR_MODULES ON)
   gtest_discover_tests(${name})
endfunction()

maboroutu_add_appkit_test(test_console)

# test_source_location_format.cpp が検証する std::formatter<std::source_location>
# の明示的特殊化は、Clang 18 + libc++ で実機検証した限り、モジュール越しに
# import しただけでは到達不能(「the supplied type is not formattable」)
# であることを確認済み（test_source_location_format.cpp 冒頭のコメント参照）。
# 現状のビルド環境ではこのターゲットは失敗する可能性が高いため、既定では
# 無効にしておく。GCC/libstdc++等の別環境で解消されている、または
# ライブラリ側の対応が入った場合は ON にして有効化できる。
option(MABOROUTU_TESTS_ENABLE_SOURCE_LOCATION_FORMAT_TEST
   "Enable test_source_location_format, which is known to fail to build \
under Clang+libc++ due to a cross-module std::formatter reachability \
limitation (see test_source_location_format.cpp for details)."
   OFF)
if(MABOROUTU_TESTS_ENABLE_SOURCE_LOCATION_FORMAT_TEST)
   maboroutu_add_appkit_test(test_source_location_format)
endif()
