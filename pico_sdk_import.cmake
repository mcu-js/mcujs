# This file is used to import the Pico SDK into a CMake project
# Copy this file to your project or set PICO_SDK_PATH environment variable

# Check for PICO_SDK_PATH environment variable
if(DEFINED ENV{PICO_SDK_PATH} AND NOT PICO_SDK_PATH)
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
endif()

if(NOT PICO_SDK_PATH)
    message(FATAL_ERROR "PICO_SDK_PATH is not set. Please set it to the Pico SDK location.")
endif()

# Ensure the SDK path exists
if(NOT EXISTS "${PICO_SDK_PATH}")
    message(FATAL_ERROR "PICO_SDK_PATH does not exist: ${PICO_SDK_PATH}")
endif()

# Include the SDK's external project import
set(PICO_SDK_INIT_CMAKE_FILE ${PICO_SDK_PATH}/pico_sdk_init.cmake)
if(NOT EXISTS "${PICO_SDK_INIT_CMAKE_FILE}")
    message(FATAL_ERROR "Cannot find pico_sdk_init.cmake in ${PICO_SDK_PATH}")
endif()

include(${PICO_SDK_INIT_CMAKE_FILE})
