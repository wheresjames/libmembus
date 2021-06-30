
#====================================================================
set(TARGET ${PROJECT_NAME}-test)
set(BINARY ${PROJECT_BINARY_DIR}/lib/test.py)


#====================================================================
# Must have python

set(NEWPY "YES")

if("${PYVER}" STREQUAL "")
    set(PYVER "=${PYTHON_VERSION_STRING}")
endif()

if(${PYVER} MATCHES "=")
    string(REPLACE "=" "" PYVER "${PYVER}")
    if(NOT "${PYVER}" STREQUAL "")
        set(PYEQ "EXACT")
    endif()    
endif()

message(STATUS "[INFO] PYVER: ${PYVER}, PYEQ: ${PYEQ}, NEWPY: ${NEWPY}")

if(${NEWPY})
    find_package (Python3 ${PYVER} ${PYEQ} COMPONENTS Interpreter Development)
    if(NOT Python3_FOUND OR NOT Python3_Development_FOUND)
        if("${PYEQ}" STREQUAL "")
            message(FATAL_ERROR "Python not found : ${PYVER}")
        else()
            message(STATUS "!!!! Exact Python version not found : ${PYVER}")
            find_package (Python3 COMPONENTS Interpreter Development)
            if(NOT Python3_FOUND OR NOT Python3_Development_FOUND)
                message(FATAL_ERROR "Python not found : ${PYVER}")
            endif()
        endif()
    endif()
    set(PYEXE ${Python3_EXECUTABLE})
else()
    find_package(PythonExtensions REQUIRED)
    set(PYEXE ${PYTHON_EXECUTABLE})
endif()


#====================================================================
add_custom_target(${TARGET} ALL
                    DEPENDS ${PROJECT_NAME}
                    COMMENT "Building ${TARGET}" VERBATIM)

enable_testing()
add_test(NAME ${TARGET}
            WORKING_DIRECTORY "${PROJECT_BINARY_DIR}/lib"
            COMMAND ${PYEXE} ${BINARY})

set_tests_properties(${TARGET} PROPERTIES TIMEOUT 10)

# Copy test file
add_custom_command(TARGET ${TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy 
                    "${CMAKE_CURRENT_SOURCE_DIR}/py/test.py" 
                    "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")

# Execute test
add_custom_command(TARGET ${TARGET} POST_BUILD
                COMMAND ${CMAKE_CTEST_COMMAND} -C $<CONFIG> --output-on-failure --stop-on-failure) # --verbose)

