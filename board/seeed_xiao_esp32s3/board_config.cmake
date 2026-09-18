# Firmware JS memory policy; physical PSRAM setup belongs to ESP-IDF.
set(MCUJS_JS_HEAP_KIB 256)
set(MCUJS_JS_HEAP_REGION external)

include(${CMAKE_CURRENT_LIST_DIR}/../../src/generated/sd_config.cmake)
