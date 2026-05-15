
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

# Include directories — build interface uses the source tree; install interface
# uses the destination set by cmake --install (resolved by GNUInstallDirs).
target_include_directories(${BINARY} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Precompiled headers
target_precompile_headers(${BINARY} PUBLIC "${CMAKE_SOURCE_DIR}/include/libmembus-internal.h")

# Link libraries
target_link_libraries(${BINARY} Boost::stacktrace_backtrace)
