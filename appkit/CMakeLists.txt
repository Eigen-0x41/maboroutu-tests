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

maboroutu_add_appkit_test(test_source_location_format)
