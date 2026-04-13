
function(get_target_build_path out_var)
    # Detect system name and processor
    string(TOLOWER "${CMAKE_SYSTEM_NAME}" SYSTEM_NAME)
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" SYSTEM_ARCH)

    # Detect build type
    if (CMAKE_BUILD_TYPE)
        string(TOLOWER "${CMAKE_BUILD_TYPE}" BUILD_TYPE)
    else()
        # For multi-config generators, default to empty or choose one
        set(BUILD_TYPE "unknown")
    endif()

    # Construct final path
    set(PATH_STR "${SYSTEM_NAME}-${SYSTEM_ARCH}/${BUILD_TYPE}")
    set(${out_var} "${PATH_STR}" PARENT_SCOPE)
endfunction()
