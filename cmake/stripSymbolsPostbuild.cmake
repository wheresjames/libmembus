
if(NOT DEFINED _STRIP_COMMAND_INDEX)
    set(_STRIP_COMMAND_INDEX 0)
endif()

function(strip_symbols_postbuild target_name)

    math(EXPR _STRIP_COMMAND_INDEX "${_STRIP_COMMAND_INDEX} + 1")
    set(unique_id "${_STRIP_COMMAND_INDEX}")

    set(_var "postbuild_${unique_id}_target")
    set(${_var} "${target_name}" PARENT_SCOPE)

    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMENT "Stripping debug symbols for ${target_name}\nFROM: $<TARGET_FILE:${target_name}>\nTO:   $<TARGET_FILE:${target_name}>.symbols"
        COMMAND ${CMAKE_OBJCOPY} --only-keep-debug $<TARGET_FILE:${target_name}> $<TARGET_FILE:${target_name}>.symbols
        COMMAND ${CMAKE_STRIP} --strip-unneeded $<TARGET_FILE:${target_name}>
        COMMAND ${CMAKE_OBJCOPY} --add-gnu-debuglink=$<TARGET_FILE:${target_name}>.symbols $<TARGET_FILE:${target_name}>
    )

endfunction()
