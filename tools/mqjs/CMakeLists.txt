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
if (EMSCRIPTEN)
    add_executable(mqjs_wasm mqjs.c ${MQJS_REPL_SRCS})
    add_dependencies(mqjs_wasm generate_mqjs_headers_task)
    target_link_libraries(mqjs_wasm PRIVATE libamp m)

    set_target_properties(mqjs_wasm PROPERTIES SUFFIX ".js")

    target_link_options(mqjs_wasm PRIVATE
        "-sWASM=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sEXPORTED_RUNTIME_METHODS=['callMain','FS']" 
        "-sMODULARIZE=1"
        "-sEXPORT_NAME='createMqjsCompiler'"
        "-sINVOKE_RUN=0"z
        "-sEXIT_RUNTIME=0"
        "-sEXPORT_ES6=1"
        "-sUSE_ES6_IMPORT_META=1"
        "-O3"
    )
else()
add_executable(mqjs mqjs.c ${MQJS_REPL_SRCS})
add_dependencies(mqjs generate_mqjs_headers_task)
target_link_libraries(mqjs PRIVATE libamp m)
endif()
