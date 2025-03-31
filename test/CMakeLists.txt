cmake_minimum_required(VERSION 3.14)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.16.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP true
)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wstrict-prototypes -fdata-sections -ffunction-sections")

target_include_directories(libamp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/test_common
)

add_executable(libamp_tests
    test_common/keyboard_user.c
    test_common/lfs_port.c
    test_common/main.cpp
    key/test_key.cpp
    advanced_key/test_advanced_key.cpp
    keyboard/test_keyboard.cpp
    dynamic_key/test_dynamic_key.cpp
    mouse/test_mouse.cpp
    extra_key/test_extra_key.cpp
    joystick/test_joystick.cpp
    command/test_command.cpp
)

target_link_libraries(libamp_tests
    PRIVATE
    libamp
    GTest::gtest_main
)

gtest_discover_tests(libamp_tests)