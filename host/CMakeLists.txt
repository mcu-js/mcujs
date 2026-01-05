cmake_minimum_required(VERSION 3.13)

# Export the jerry_port.c path so it can be added directly to the executable
# (JerryScript port functions must be in the main executable to avoid LTO stripping)
set(JERRY_PORT_SOURCE ${CMAKE_CURRENT_LIST_DIR}/jerry_port.c PARENT_SCOPE)

# Host runtime library - mcujs APIs built on top of JerryScript
add_library(mcujs_host STATIC
    engine.c
    module_loader.c
)

target_link_libraries(mcujs_host PUBLIC
    mcujs_jerry
    pico_stdlib
)

target_include_directories(mcujs_host PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/filesystem
    ${CMAKE_SOURCE_DIR}/board
    ${CMAKE_SOURCE_DIR}/board/${BOARD}
    ${CMAKE_SOURCE_DIR}/javascript
    ${CMAKE_SOURCE_DIR}/javascript/jerryscript-core
)

# Add bindings subdirectory
add_subdirectory(bindings)
