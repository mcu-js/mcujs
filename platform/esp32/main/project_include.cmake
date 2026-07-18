# ESP-IDF's reproducible mode maps IDF_PATH, PROJECT_DIR, BUILD_DIR, and each
# component. MCU.js also compiles shared sources above PROJECT_DIR plus
# JerryScript and compiler headers from separate roots. Normalize those roots
# globally because ESP-IDF embeds the complete ELF digest in the app image.
if(CONFIG_APP_REPRODUCIBLE_BUILD AND NOT BOOTLOADER_BUILD)
    get_filename_component(MCUJS_REPRO_ROOT "${COMPONENT_DIR}/../../.." ABSOLUTE)
    set(MCUJS_REPRO_JERRY "$ENV{JERRYSCRIPT_PATH}")
    set(MCUJS_REPRO_IDF_TOOLS "$ENV{IDF_TOOLS_PATH}")

    foreach(required_root
            "${MCUJS_REPRO_ROOT}"
            "${MCUJS_REPRO_JERRY}"
            "${MCUJS_REPRO_IDF_TOOLS}")
        if(NOT IS_DIRECTORY "${required_root}")
            message(FATAL_ERROR "Reproducible-build root is missing: ${required_root}")
        endif()
    endforeach()

    set(MCUJS_REPRO_COMPILE_OPTIONS
        "-fdebug-prefix-map=${MCUJS_REPRO_ROOT}=/MCUJS"
        "-fmacro-prefix-map=${MCUJS_REPRO_ROOT}=/MCUJS"
        "-fdebug-prefix-map=${MCUJS_REPRO_JERRY}=/JERRYSCRIPT"
        "-fmacro-prefix-map=${MCUJS_REPRO_JERRY}=/JERRYSCRIPT"
        "-fdebug-prefix-map=${MCUJS_REPRO_IDF_TOOLS}=/IDF_TOOLS"
        "-fmacro-prefix-map=${MCUJS_REPRO_IDF_TOOLS}=/IDF_TOOLS"
    )
    idf_build_set_property(COMPILE_OPTIONS "${MCUJS_REPRO_COMPILE_OPTIONS}" APPEND)
endif()
