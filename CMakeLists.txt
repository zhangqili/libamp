cmake_minimum_required(VERSION 3.14)
# Set the project name
project (libamp)
include(CheckTypeSize)
check_type_size("size_t" SIZEOF_SIZE_T)
if(${SIZEOF_SIZE_T} EQUAL 4)
    set(M32_FLAG -m32)
endif()

if(NOT DEFINED LIBAMP_INCLUDE_DIR AND NOT DEFINED LIBAMP_BUILD_TESTS)
    message(FATAL_ERROR "Error: LIBAMP_INCLUDE_DIR is not defined!\n")
endif()
if(DEFINED LIBAMP_BUILD_TESTS)
    set(LIBAMP_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/test/test_common")
endif()

set(MQJS_GEN_SRC_1 "${PROJECT_SOURCE_DIR}/src/mquickjs/mqjs_libamp_stdlib.c")
set(MQJS_GEN_SRC_2 "${PROJECT_SOURCE_DIR}/lib/mquickjs/mquickjs_build.c")
set(MQJS_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}")
file(MAKE_DIRECTORY ${MQJS_GEN_DIR})
set(MQJS_GEN_TOOL_EXE "${CMAKE_CURRENT_BINARY_DIR}/mqjs_libamp_stdlib")
set(MQJS_GEN_HEADER_ATOM "${MQJS_GEN_DIR}/mquickjs_atom.h")
set(MQJS_GEN_HEADER_LIB  "${MQJS_GEN_DIR}/mqjs_stdlib.h")

add_custom_command(
    OUTPUT ${MQJS_GEN_TOOL_EXE}
    COMMAND gcc -O2 -o ${MQJS_GEN_TOOL_EXE} ${MQJS_GEN_SRC_1} ${MQJS_GEN_SRC_2} -I ${PROJECT_SOURCE_DIR}/lib/mquickjs/ -I ${LIBAMP_INCLUDE_DIR}
    DEPENDS ${MQJS_GEN_SRC_1} ${MQJS_GEN_SRC_2}
    COMMENT "Host: Compiling QuickJS generator tool..."
    VERBATIM
)

add_custom_command(
    OUTPUT ${MQJS_GEN_HEADER_ATOM} ${MQJS_GEN_HEADER_LIB}
    COMMAND sh -c "${MQJS_GEN_TOOL_EXE} -a ${M32_FLAG} > ${MQJS_GEN_HEADER_ATOM}"
    COMMAND sh -c "${MQJS_GEN_TOOL_EXE} ${M32_FLAG} > ${MQJS_GEN_HEADER_LIB}"
    DEPENDS ${MQJS_GEN_TOOL_EXE}
    COMMENT "Host: Generating QuickJS headers..."
    VERBATIM
)

add_custom_target(gen_mqjs_headers
    DEPENDS ${MQJS_GEN_HEADER_ATOM} ${MQJS_GEN_HEADER_LIB}
)

file(GLOB COMPONENT_SRCS
    ${PROJECT_SOURCE_DIR}/src/*.c
    ${PROJECT_SOURCE_DIR}/usb/*.c
    ${PROJECT_SOURCE_DIR}/lib/littlefs/*.c
    ${PROJECT_SOURCE_DIR}/src/midi/*.c
    ${PROJECT_SOURCE_DIR}/src/midi/bytequeue/*.c
    ${PROJECT_SOURCE_DIR}/src/lamp_array/*.c
)

file(GLOB MQJS_SRCS
    ${PROJECT_SOURCE_DIR}/lib/mquickjs/cutils.c
    ${PROJECT_SOURCE_DIR}/lib/mquickjs/cutils.c
    ${PROJECT_SOURCE_DIR}/lib/mquickjs/dtoa.c
    ${PROJECT_SOURCE_DIR}/lib/mquickjs/libm.c
    ${PROJECT_SOURCE_DIR}/lib/mquickjs/mquickjs.c
)
# Add a library with the above sources
add_library(${PROJECT_NAME} ${COMPONENT_SRCS} ${MQJS_SRCS})
add_dependencies(${PROJECT_NAME} gen_mqjs_headers)
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
    ${PROJECT_SOURCE_DIR}/lib/mquickjs
    ${LIBAMP_INCLUDE_DIR}
    ${MQJS_GEN_DIR}
)

target_include_directories( ${PROJECT_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/src/mquickjs
)

option(LIBAMP_BUILD_TESTS "Build libamp tests" OFF)
if(LIBAMP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(${CMAKE_SOURCE_DIR}/lib/googletest)
    add_subdirectory(test)
endif()