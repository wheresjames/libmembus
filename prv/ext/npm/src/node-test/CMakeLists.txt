
#====================================================================

set(TARGET ${PROJECT_NAME}-test)
set(BINARY ${PROJECT_BINARY_DIR}/lib/test.js)

add_custom_target(${TARGET} ALL
                  DEPENDS ${PROJECT_NAME}
                  COMMENT "Building ${TARGET}" VERBATIM)

enable_testing()

add_test(NAME ${TARGET}
         WORKING_DIRECTORY "${PROJECT_BINARY_DIR}/lib"
         COMMAND node ${BINARY})

set_tests_properties(${TARGET} PROPERTIES TIMEOUT 10)

# Copy test file
add_custom_command(TARGET ${TARGET} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy 
                        "${CMAKE_CURRENT_SOURCE_DIR}/js/test.js"
                        "${PROJECT_BINARY_DIR}/lib/test.js")

# Execute test
add_custom_command(TARGET ${TARGET} POST_BUILD
                  COMMAND ${CMAKE_CTEST_COMMAND} -C $<CONFIG> --output-on-failure --stop-on-failure) # --verbose)

