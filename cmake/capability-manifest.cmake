# Called only after the platform has validated its selected Canvas profile.
function(mcujs_write_capability_manifest root board output configured_display)
    set(options)
    if(configured_display)
        list(APPEND options --configured-display)
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${root}/runtime/manifests/${board}.json" "${root}/runtime/device-capabilities.json"
        "${root}/scripts/build-capability-manifest.py")
    execute_process(COMMAND python3 "${root}/scripts/build-capability-manifest.py"
        --board "${board}" --output "${output}" ${options}
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Could not produce the selected build capability manifest")
    endif()
endfunction()
