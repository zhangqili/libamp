cmake_minimum_required(VERSION 3.10)
project(MQSJHeaderGenerator)

set(LIBAMP_SOURCE_DIR ${CMAKE_SOURCE_DIR}/../..)

if(NOT DEFINED LIBAMP_INCLUDE_DIR)
    set(LIBAMP_INCLUDE_DIR "${LIBAMP_SOURCE_DIR}/test/test_common")
endif()

add_executable(mqjs_header_generator 
    ${LIBAMP_SOURCE_DIR}/src/mquickjs/mqjs_libamp_stdlib.c
    ${LIBAMP_SOURCE_DIR}/lib/mquickjs/mquickjs_build.c
    )
target_include_directories(mqjs_header_generator PRIVATE
    ${LIBAMP_SOURCE_DIR}/lib/mquickjs/ 
    ${LIBAMP_INCLUDE_DIR}
)
