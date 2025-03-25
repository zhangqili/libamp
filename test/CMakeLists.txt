cmake_minimum_required(VERSION 3.10)
project(libamp_tests)

# 找到 GoogleTest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# 设置 keyboard_user.c 和 keyboard_conf.h 的路径
set(KEYBOARD_CONF ${CMAKE_CURRENT_SOURCE_DIR}/test_commonc/keyboard_conf.h)
set(KEYBOARD_USER ${CMAKE_CURRENT_SOURCE_DIR}/test_commonc/keyboard_user.c)

# 添加测试源文件
file(GLOB TEST_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/key/test_key.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/advanced_key/test_advanced_key.cpp
)

# 创建测试可执行文件
add_executable(runTests ${TEST_SOURCES} ${KEYBOARD_USER})

# 链接 GoogleTest 和 libamp
target_link_libraries(runTests PRIVATE ${PROJECT_NAME} GTest::GTest GTest::Main pthread)

# 添加头文件路径
target_include_directories(runTests PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/usb
        ${PROJECT_SOURCE_DIR}/littlefs
        ${PROJECT_SOURCE_DIR}/src/midi
        ${PROJECT_SOURCE_DIR}/src/midi/bytequeue
        ${CMAKE_CURRENT_SOURCE_DIR}/test_commonc
)

# 启用CTest
add_test(NAME libamp_tests COMMAND runTests)
