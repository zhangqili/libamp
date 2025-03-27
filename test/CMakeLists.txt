cmake_minimum_required(VERSION 3.14)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.16.0.zip
)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

target_include_directories(libamp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/test_common
)

add_executable(libamp_tests
    test_common/keyboard_user.c
    test_common/lfs_port.c
    key/test_key.cpp
    advanced_key/test_advanced_key.cpp
    keyboard/test_keyboard.cpp
)

target_link_libraries(libamp_tests
    PRIVATE
    libamp
    GTest::gtest_main
)

gtest_discover_tests(libamp_tests)