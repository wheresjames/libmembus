
#====================================================================

function(readProjectConfig)

    file(READ ${ARGV0} CFGSTR)
    string(REPLACE "\r" "\n" CFGSTR ${CFGSTR})
    set(CFGSTR "\n${CFGSTR}")
    string(REPLACE ";" ":" CFGSTR "${CFGSTR}")
    string(REPLACE "\n" ";" CFGSTR "${CFGSTR}")

    foreach(line IN LISTS CFGSTR)
        if("${line}" STREQUAL "")
            continue()
        endif()
        string(REGEX MATCH "^[ \t]*([a-zA-Z0-9_]+)[ \t]+(.+)$" _ ${line})
        set(key "${CMAKE_MATCH_1}")
        string(TOUPPER ${key} key)
        set(value "${CMAKE_MATCH_2}")
        set("APP${key}" "${value}" PARENT_SCOPE)
    endforeach()

    if("${APPBUILD}" STREQUAL "")
        string(TIMESTAMP APPBUILD "%y.%m.%d.%H%M")
        string(REPLACE ".0" "." APPBUILD ${APPBUILD})
        set("APPBUILD" "${APPBUILD}" PARENT_SCOPE)
    endif()

endfunction()
