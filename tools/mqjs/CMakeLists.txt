cmake_minimum_required (VERSION 3.10)
set(CMAKE_COLOR_DIAGNOSTICS ON)

project(mqjs)

set(LIBAMP_SOURCE_DIR ${CMAKE_SOURCE_DIR}/../..)

set(MQJS_REPL_SRCS
    ${LIBAMP_SOURCE_DIR}/lib/mquickjs/readline.c
    ${LIBAMP_SOURCE_DIR}/lib/mquickjs/readline_tty.c
)
set(LIBAMP_INCLUDE_DIR ${LIBAMP_SOURCE_DIR}/test/test_common)
add_subdirectory(${LIBAMP_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/libamp)
add_executable(mqjs mqjs.c ${MQJS_REPL_SRCS})
add_dependencies(mqjs gen_mqjs_headers)
target_link_libraries(mqjs PRIVATE libamp m)

