cmake_minimum_required(VERSION 3.14)
# Set the project name
project (libamp)
include(ExternalProject)
include(CheckTypeSize)
check_type_size("void*" SIZEOF_VOID_P)
if(SIZEOF_VOID_P EQUAL 4)
    set(M32_FLAG -m32)
endif()
if("${SIZEOF_VOID_P}" STREQUAL "")
    set(M32_FLAG -m32)
endif()
find_program(HOST_GCC gcc DOC "Host GCC for generator tools")
if(NOT HOST_GCC)
    set(HOST_GCC "gcc")
endif()

if(NOT DEFINED LIBAMP_INCLUDE_DIR AND NOT DEFINED LIBAMP_BUILD_TESTS)
    message(FATAL_ERROR "Error: LIBAMP_INCLUDE_DIR is not defined!\n")
endif()
if(DEFINED LIBAMP_BUILD_TESTS)
    set(LIBAMP_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/test/test_common")
endif()

set(MQJS_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}")
file(MAKE_DIRECTORY ${MQJS_GEN_DIR})

set(GEN_EXE_NAME "mqjs_header_generator")
if(CMAKE_HOST_WIN32)
    set(MQJS_GEN_TOOL_EXE "${MQJS_GEN_DIR}/${GEN_EXE_NAME}.exe")
else()
    set(MQJS_GEN_TOOL_EXE "${MQJS_GEN_DIR}/${GEN_EXE_NAME}")
endif()

ExternalProject_Add(host_generator_project
    SOURCE_DIR "${PROJECT_SOURCE_DIR}/tools/mqjs_header_generator"
    BINARY_DIR "${MQJS_GEN_DIR}"
    INSTALL_COMMAND ""

    CMAKE_ARGS 
        "-DCMAKE_TOOLCHAIN_FILE=" 
        "-DCMAKE_C_COMPILER=${HOST_GCC}"
        "-DCMAKE_BUILD_TYPE=Release"
        "-DLIBAMP_INCLUDE_DIR=${LIBAMP_INCLUDE_DIR}" 
    
    BUILD_ALWAYS 1
)

set(MQJS_GEN_HEADER_ATOM "${MQJS_GEN_DIR}/mquickjs_atom.h")
set(MQJS_GEN_HEADER_LIB  "${MQJS_GEN_DIR}/mqjs_stdlib.h")

if(CMAKE_HOST_WIN32)
    set(SHELL_WRAPPER cmd /c)
else()
    set(SHELL_WRAPPER sh -c)
endif()
add_custom_command(
    OUTPUT ${MQJS_GEN_HEADER_ATOM} ${MQJS_GEN_HEADER_LIB}
    COMMAND ${SHELL_WRAPPER} "${MQJS_GEN_TOOL_EXE} -a ${M32_FLAG} > ${MQJS_GEN_HEADER_ATOM}"
    COMMAND ${SHELL_WRAPPER} "${MQJS_GEN_TOOL_EXE} ${M32_FLAG} > ${MQJS_GEN_HEADER_LIB}"
    DEPENDS host_generator_project
    COMMENT "Host: Generating QuickJS headers..."
    VERBATIM
)

add_custom_target(generate_mqjs_headers_task
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
    ${PROJECT_SOURCE_DIR}/lib/mquickjs/dtoa.c
    ${PROJECT_SOURCE_DIR}/lib/mquickjs/libm.c
    ${PROJECT_SOURCE_DIR}/lib/mquickjs/mquickjs.c
)
# Add a library with the above sources
add_library(${PROJECT_NAME} ${COMPONENT_SRCS} ${MQJS_SRCS})
add_dependencies(${PROJECT_NAME} generate_mqjs_headers_task)
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