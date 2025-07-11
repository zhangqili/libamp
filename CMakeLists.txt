cmake_minimum_required(VERSION 3.14)
# Set the project name
project (libamp)

file(GLOB COMPONENT_SRCS
    ${PROJECT_SOURCE_DIR}/src/*.c
    ${PROJECT_SOURCE_DIR}/usb/*.c
    ${PROJECT_SOURCE_DIR}/lib/littlefs/*.c
    ${PROJECT_SOURCE_DIR}/src/midi/*.c
    ${PROJECT_SOURCE_DIR}/src/midi/bytequeue/*.c
    ${PROJECT_SOURCE_DIR}/src/lamp_array/*.c
)
# Add a library with the above sources
add_library(${PROJECT_NAME} ${COMPONENT_SRCS})

target_compile_definitions( ${PROJECT_NAME} PRIVATE 
    #LFS_NO_MALLOC
    LFS_NO_ASSERT
    #LFS_YES_TRACE
    LFS_NO_DEBUG
    LFS_NO_ERROR
    LFS_NO_WARN
)

target_include_directories( ${PROJECT_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/usb
    ${PROJECT_SOURCE_DIR}/src/midi
    ${PROJECT_SOURCE_DIR}/src/midi/bytequeue
    ${PROJECT_SOURCE_DIR}/src/lamp_array
    ${PROJECT_SOURCE_DIR}/lib/littlefs
    ${PROJECT_SOURCE_DIR}/lib/lufa
)

option(LIBAMP_BUILD_TESTS "Build libamp tests" OFF)
if(LIBAMP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(${CMAKE_SOURCE_DIR}/lib/googletest)
    add_subdirectory(test)
endif()