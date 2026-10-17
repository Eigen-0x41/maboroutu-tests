# =============================================================================
# extension/ -- maboroutu_extension が提供する各モジュールのテスト。
#
# ライブラリ本体側の MABOROUTU_ENABLE_EXTENSION が OFF の場合、
# ルート CMakeLists.txt はこのディレクトリ自体を add_subdirectory しない
# （maboroutu::extension ターゲットが存在しないため）。
# =============================================================================

function(maboroutu_add_extension_test name)
   add_executable(${name} src/${name}.cpp)
   target_link_libraries(${name} PRIVATE
      maboroutu::extension
      maboroutu::core
      GTest::gtest
      GTest::gtest_main)
   set_target_properties(${name} PROPERTIES CXX_SCAN_FOR_MODULES ON)
   gtest_discover_tests(${name})
endfunction()

maboroutu_add_extension_test(test_slot_map)
maboroutu_add_extension_test(test_keyed_slot_map)
maboroutu_add_extension_test(test_stream_data_source)
maboroutu_add_extension_test(test_memory_data_buffer)
maboroutu_add_extension_test(test_binary_layout_handle)
