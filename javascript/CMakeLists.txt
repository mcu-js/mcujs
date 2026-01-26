cmake_minimum_required(VERSION 3.13)

# JerryScript Engine Build Configuration
# This directory only handles building/configuring the JerryScript engine itself.
# Host runtime code (engine wrapper, bindings, etc.) is in the /host directory.

# JerryScript source location (from Docker or environment)
if(DEFINED ENV{JERRYSCRIPT_PATH})
    set(JERRYSCRIPT_PATH $ENV{JERRYSCRIPT_PATH})
else()
    set(JERRYSCRIPT_PATH ${CMAKE_CURRENT_LIST_DIR}/jerryscript)
endif()

# Check if we have amalgamated sources or need to build from full source
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/jerryscript-core/jerryscript.c")
    # Use pre-generated amalgamated sources
    set(JERRY_AMALGAM TRUE)
    set(JERRY_CORE_SRC ${CMAKE_CURRENT_LIST_DIR}/jerryscript-core)
else()
    # Will be generated during build
    set(JERRY_AMALGAM FALSE)
endif()

# JerryScript core library (without port - port goes directly into executable)
if(JERRY_AMALGAM)
    add_library(mcujs_jerry STATIC
        ${JERRY_CORE_SRC}/jerryscript.c
    )
    target_include_directories(mcujs_jerry PUBLIC
        ${JERRY_CORE_SRC}
        ${CMAKE_CURRENT_LIST_DIR}
    )
    
    # JerryScript configuration for embedded
    target_compile_definitions(mcujs_jerry PUBLIC
        JERRY_CPOINTER_32_BIT=0
        JERRY_ERROR_MESSAGES=1
        JERRY_EXTERNAL_CONTEXT=0
        JERRY_GC_LIMIT=0
        JERRY_GC_MARK_LIMIT=8
        JERRY_GLOBAL_HEAP_SIZE=64
        JERRY_LCACHE=1
        JERRY_LINE_INFO=1
        JERRY_LOGGING=0
        JERRY_MEM_GC_BEFORE_EACH_ALLOC=0
        JERRY_MEM_STATS=0
        JERRY_NDEBUG=1
        JERRY_PARSER=1
        JERRY_PARSER_DUMP_BYTE_CODE=0
        JERRY_PROPRETY_HASHMAP=1
        JERRY_REGEXP_DUMP_BYTE_CODE=0
        JERRY_REGEXP_STRICT_MODE=0
        JERRY_SNAPSHOT_EXEC=0
        JERRY_SNAPSHOT_SAVE=0
        JERRY_SYSTEM_ALLOCATOR=0
        JERRY_VALGRIND=0
        JERRY_VM_HALT=0
        JERRY_VM_THROW=1
    )
    
    target_link_libraries(mcujs_jerry PUBLIC
        pico_stdlib
        pico_time
    )
else()
    # Build JerryScript from source with minimal profile
    if(EXISTS "${JERRYSCRIPT_PATH}/jerry-core")
        # Set JerryScript build options before adding subdirectory
        set(JERRY_CMDLINE OFF CACHE BOOL "" FORCE)
        set(JERRY_PORT OFF CACHE BOOL "" FORCE)  # We provide our own port
        set(JERRY_EXT OFF CACHE BOOL "" FORCE)
        set(JERRY_LIBM OFF CACHE BOOL "" FORCE)
        set(JERRY_GLOBAL_HEAP_SIZE 64 CACHE STRING "" FORCE)
        set(JERRY_GC_LIMIT 0 CACHE STRING "" FORCE)
        set(JERRY_GC_MARK_LIMIT 8 CACHE STRING "" FORCE)
        set(JERRY_ERROR_MESSAGES ON CACHE BOOL "" FORCE)
        set(JERRY_LINE_INFO ON CACHE BOOL "" FORCE)
        set(JERRY_PARSER ON CACHE BOOL "" FORCE)
        set(JERRY_SNAPSHOT_EXEC OFF CACHE BOOL "" FORCE)
        set(JERRY_SNAPSHOT_SAVE OFF CACHE BOOL "" FORCE)
        set(JERRY_VM_THROW ON CACHE BOOL "" FORCE)
        
        add_subdirectory(${JERRYSCRIPT_PATH} ${CMAKE_CURRENT_BINARY_DIR}/jerryscript EXCLUDE_FROM_ALL)
        
        # Create wrapper library that links jerry-core
        add_library(mcujs_jerry INTERFACE)
        target_link_libraries(mcujs_jerry INTERFACE
            jerry-core
            pico_stdlib
            pico_time
        )
        target_include_directories(mcujs_jerry INTERFACE
            ${CMAKE_CURRENT_LIST_DIR}
        )
    else()
        message(FATAL_ERROR "JerryScript not found. Please set JERRYSCRIPT_PATH or add jerryscript submodule.")
    endif()
endif()
