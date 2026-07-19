cmake_minimum_required(VERSION 3.13)

# Host runtime library - mcujs APIs built on top of JerryScript
add_library(mcujs_host STATIC
    engine.c
    module_loader.c
    runtime_registry.c
)

target_link_libraries(mcujs_host PUBLIC
    mcujs_jerry
)

mcujs_platform_configure_host(mcujs_host)

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
