
#====================================================================

# Output
set(BINARY test-${PROJECT_NAME})

# Source files
file(GLOB_RECURSE ${BINARY}_SOURCES LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/cpp/*.cpp")

# Add library
# add_library(${BINARY} ${${BINARY}_SOURCES})
add_executable(${BINARY} ${${BINARY}_SOURCES})

# Include directories
target_include_directories(${BINARY} PUBLIC ${CMAKE_SOURCE_DIR}/include)

# Precompiled headers
target_precompile_headers(${BINARY} PUBLIC ${CMAKE_SOURCE_DIR}/include/libmembus-internal.h)

# Link libraries
target_link_libraries(${BINARY} PRIVATE ${PROJECT_NAME})

enable_testing()
add_test(NAME ${BINARY} COMMAND ${BINARY})
set_tests_properties(${BINARY} PROPERTIES TIMEOUT 10)
add_custom_command(TARGET ${BINARY} POST_BUILD
                   COMMAND ${CMAKE_CTEST_COMMAND} -C $<CONFIG> --output-on-failure --build-target ${BINARY})

# install(TARGETS ${BINARY} DESTINATION share/${PROJECT_NAME} COMPONENT test)
