
### Main
add_executable(${PROJECT_NAME}_error ${CMAKE_CURRENT_LIST_DIR}/src/error.cpp)
target_compile_features(${PROJECT_NAME}_error PRIVATE cxx_std_23)

### Dependencies
target_link_libraries(${PROJECT_NAME}_error PRIVATE maboroutu::maboroutu)
target_link_libraries(${PROJECT_NAME}_error PRIVATE GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)

add_test(NAME ${PROJECT_NAME}_error COMMAND ${PROJECT_NAME}_error)
