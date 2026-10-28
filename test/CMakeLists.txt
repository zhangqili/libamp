cmake_minimum_required(VERSION 3.14)

include(GoogleTest)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wstrict-prototypes -fdata-sections -ffunction-sections")

target_include_directories(libamp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/test_common
)

target_compile_definitions(libamp PRIVATE
    AUDIO_ENABLE
)

add_executable(libamp_tests
    test_common/keyboard_user.c
    test_common/test_fixture.cpp
    test_common/main.cpp
    analog/test_analog.cpp
    key/test_key.cpp
    advanced_key/test_advanced_key.cpp
    keyboard/test_keyboard.cpp
    keyboard/test_tick_script.cpp
    dynamic_key/test_dynamic_key.cpp
    event/test_event.cpp
    large_packet/test_large_packet.cpp
    macro/test_macro.cpp
    record/test_record.cpp
    record/test_runtime_packet.cpp
    mouse/test_mouse.cpp
    extra_key/test_extra_key.cpp
    joystick/test_joystick.cpp
    gamepad/test_gamepad.cpp
    packet/test_packet.cpp
    rgb/test_rgb.cpp
    console/test_console.cpp
    storage/test_storage.cpp
    layer/test_layer.cpp
    midi/test_midi.cpp
    nexus/test_nexus.cpp
    usb/test_usb_descriptor.cpp
    usb/test_usb_serial_number.cpp
)

target_link_libraries(libamp_tests
    PRIVATE
    libamp
    GTest::gtest_main
)

gtest_discover_tests(libamp_tests)

find_package(Threads REQUIRED)
foreach(tick_bits IN ITEMS 32 64)
    if(tick_bits EQUAL 64)
        set(tick_64bit_enable 1)
    else()
        set(tick_64bit_enable 0)
    endif()
    foreach(tick_rate IN ITEMS 1000 8000 125 1024)
        set(tick_target libamp_tick_${tick_bits}_${tick_rate}_tests)
        add_executable(${tick_target}
            keyboard/test_tick.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/../src/keyboard_tick.c
        )
        target_compile_definitions(${tick_target} PRIVATE
            POLLING_RATE=${tick_rate}
            KEYBOARD_TICK_64BIT_ENABLE=${tick_64bit_enable}
        )
        target_link_libraries(${tick_target} PRIVATE libamp GTest::gtest_main Threads::Threads)
        gtest_discover_tests(${tick_target} TEST_PREFIX "Tick${tick_bits}_${tick_rate}.")
    endforeach()

    foreach(tick_override IN ITEMS 32 64 both)
        set(tick_target libamp_tick_${tick_bits}_override_${tick_override}_tests)
        add_executable(${tick_target}
            keyboard/test_tick_override.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/../src/keyboard_tick.c
        )
        target_compile_definitions(${tick_target} PRIVATE
            KEYBOARD_TICK_64BIT_ENABLE=${tick_64bit_enable}
        )
        if(NOT tick_override STREQUAL "64")
            target_compile_definitions(${tick_target} PRIVATE TEST_OVERRIDE_TICK32)
        endif()
        if(NOT tick_override STREQUAL "32")
            target_compile_definitions(${tick_target} PRIVATE TEST_OVERRIDE_TICK64)
        endif()
        target_link_libraries(${tick_target} PRIVATE libamp GTest::gtest_main)
        gtest_discover_tests(${tick_target} TEST_PREFIX "Tick${tick_bits}Override${tick_override}.")
    endforeach()
endforeach()

add_executable(libamp_serial_override_tests
    usb/test_usb_serial_number_custom.cpp
)

target_link_libraries(libamp_serial_override_tests
    PRIVATE
    libamp
    GTest::gtest_main
)

gtest_discover_tests(libamp_serial_override_tests)

# Hot-path micro-benchmark; built with the tests but not registered with ctest.
add_executable(libamp_bench
    test_common/keyboard_user.c
    test_common/test_fixture.cpp
    bench/bench_hot_paths.cpp
)

target_link_libraries(libamp_bench
    PRIVATE
    libamp
)

# Select the MCU's fixed-width calibration arithmetic even on a 64-bit host.
# This copy satisfies the linker before libamp's native-word implementation.
add_executable(libamp_calibration_32bit_tests
    test_common/keyboard_user.c
    test_common/test_fixture.cpp
    test_common/main.cpp
    advanced_key/test_advanced_key.cpp
    advanced_key/advanced_key_32bit.c
)

target_link_libraries(libamp_calibration_32bit_tests
    PRIVATE
    libamp
    GTest::gtest_main
)

gtest_discover_tests(libamp_calibration_32bit_tests TEST_PREFIX "Calibration32.")

# nexus.c is built into libamp as a bitmap-mode master. The two variants below
# compile their own copy of nexus.c with the raw-sample and slave
# configurations; their definitions satisfy the linker before the archive's
# copy is considered.
add_executable(libamp_nexus_raw_tests
    test_common/keyboard_user.c
    test_common/test_fixture.cpp
    nexus/test_nexus_raw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/nexus.c
)

target_compile_definitions(libamp_nexus_raw_tests PRIVATE NEXUS_USE_RAW=1)

target_link_libraries(libamp_nexus_raw_tests
    PRIVATE
    libamp
    GTest::gtest_main
)

gtest_discover_tests(libamp_nexus_raw_tests)

add_executable(libamp_nexus_slave_tests
    test_common/keyboard_user.c
    test_common/test_fixture.cpp
    nexus/test_nexus_slave.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/nexus.c
)

target_compile_definitions(libamp_nexus_slave_tests PRIVATE NEXUS_IS_SLAVE=1)

target_link_libraries(libamp_nexus_slave_tests
    PRIVATE
    libamp
    GTest::gtest_main
)

gtest_discover_tests(libamp_nexus_slave_tests)

# Exercise bitmap-word boundaries with room for an additional empty slave.
add_executable(libamp_nexus_wide_tests
    test_common/keyboard_user.c
    test_common/test_fixture.cpp
    test_common/main.cpp
    nexus/test_nexus.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/nexus.c
)
target_compile_definitions(libamp_nexus_wide_tests PRIVATE
    NEXUS_SLICE_LENGTH_MAX=64 NEXUS_SLAVE_NUM=2
)
target_link_libraries(libamp_nexus_wide_tests PRIVATE libamp GTest::gtest_main)
gtest_discover_tests(libamp_nexus_wide_tests TEST_PREFIX "NexusWide.")
