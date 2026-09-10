# Resolve board firmware policy before configuring either the engine or its port.
include("${MCUJS_ROOT}/board/${MCUJS_BOARD}/board_config.cmake")

if(NOT "${MCUJS_JS_HEAP_KIB}" MATCHES "^[1-9][0-9]*$" OR
   MCUJS_JS_HEAP_KIB GREATER 512)
    message(FATAL_ERROR "JS heap must be an integer from 1 to 512 KiB (16-bit JerryScript pointers)")
endif()
if(MCUJS_JS_HEAP_REGION STREQUAL "external")
    set(MCUJS_JS_HEAP_EXTERNAL 1)
    set(MCUJS_JERRY_EXTERNAL_CONTEXT ON)
elseif(MCUJS_JS_HEAP_REGION STREQUAL "internal")
    set(MCUJS_JS_HEAP_EXTERNAL 0)
    set(MCUJS_JERRY_EXTERNAL_CONTEXT OFF)
else()
    message(FATAL_ERROR "JS heap region must be internal or external")
endif()
message(STATUS "MCU.js JS heap: ${MCUJS_JS_HEAP_KIB} KiB in ${MCUJS_JS_HEAP_REGION} memory")
