
function(get_latest_stable_git_tag REPO_URL TAG_PREFIX RESULT_VAR)

    # First see if the value is cached
    if (DEFINED ${RESULT_VAR})
        message(STATUS " [cached] ${RESULT_VAR}=${${RESULT_VAR}}")
        return()
    endif()

    execute_process(
        COMMAND ${CMAKE_SOURCE_DIR}/sh/get-latest-stable-git-tag.sh ${REPO_URL} ${TAG_PREFIX}
        OUTPUT_VARIABLE result
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if (result)
        set(${RESULT_VAR} ${result} CACHE STRING "${RESULT_VAR} from ${REPO_URL}")
    else()
        message(FATAL_ERROR "Failed to get latest stable tag from ${REPO_URL}")
    endif()
endfunction()
