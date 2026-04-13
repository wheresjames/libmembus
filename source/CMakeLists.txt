
#====================================================================

# Output
set(BINARY ${PROJECT_NAME})

# 3rd party
# include(${CMAKE_SOURCE_DIR}/cmake/repo/boost.cmake)
find_package(Boost REQUIRED COMPONENTS stacktrace_backtrace)

# Source files
file(GLOB_RECURSE ${BINARY}_SOURCES LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/cpp/*.cpp")

# Add library
add_library(${BINARY} ${${BINARY}_SOURCES})

# Include directories
target_include_directories(${BINARY} PUBLIC "${CMAKE_SOURCE_DIR}/include")

# Precompiled headers
target_precompile_headers(${BINARY} PUBLIC "${CMAKE_SOURCE_DIR}/include/libmembus-internal.h")

# Link libraries
target_link_libraries(${BINARY} Boost::stacktrace_backtrace)
