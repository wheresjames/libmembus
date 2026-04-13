
#====================================================================

# Output
set(BINARY test-${PROJECT_NAME})

# 3rd party libraries
include(${CMAKE_SOURCE_DIR}/cmake/repo/catch2.cmake)

# Enable Catch2 CMake integration
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

# Source files
file(GLOB_RECURSE ${BINARY}_SOURCES LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/cpp/*.cpp")

# Add executable
add_executable(${BINARY} ${${BINARY}_SOURCES})

# Include directories
target_include_directories(${BINARY} PUBLIC ${CMAKE_SOURCE_DIR}/include)

# Precompiled headers
target_precompile_headers(${BINARY} PUBLIC ${CMAKE_SOURCE_DIR}/include/libmembus-internal.h)

# Link libraries
target_link_libraries(${BINARY} PRIVATE ${PROJECT_NAME} Catch2::Catch2WithMain)

# Register tests with CTest via Catch2 discovery
enable_testing()
catch_discover_tests(${BINARY})

# Auto-run tests after each build
add_custom_command(TARGET ${BINARY} POST_BUILD
                   COMMAND ${CMAKE_CTEST_COMMAND} -C $<CONFIG> --output-on-failure)
