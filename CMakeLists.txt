cmake_minimum_required(VERSION 3.14)
# Set the project name

file(GLOB COMPONENT_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/usb/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/littlefs/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/midi/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/midi/bytequeue/*.c
)
# Add a library with the above sources
add_library(libamp STATIC ${COMPONENT_SRCS})

target_compile_definitions(libamp PRIVATE 
    #LFS_NO_MALLOC
    LFS_NO_ASSERT
    #LFS_YES_TRACE
    LFS_NO_DEBUG
    LFS_NO_ERROR
    LFS_NO_WARN
)

target_include_directories(libamp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/usb
    ${CMAKE_CURRENT_SOURCE_DIR}/src/midi
    ${CMAKE_CURRENT_SOURCE_DIR}/src/midi/bytequeue
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/littlefs
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/lufa
)

option(LIBAMP_BUILD_TESTS "Build libamp tests" OFF)
if(LIBAMP_BUILD_TESTS)
    project(libamp)
    enable_testing()
    add_subdirectory(${CMAKE_SOURCE_DIR}/lib/googletest)
    add_subdirectory(test)
endif()