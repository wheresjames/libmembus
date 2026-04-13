
cmake_minimum_required(VERSION 3.30)

#-------------------------------------------------------------
# Options
set(CMAKE_CXX_STANDARD 20)
set(FETCHCONTENT_QUIET FALSE)

# Release or Debug
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

# Ensure debug symbols are included
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g")

#-------------------------------------------------------------
# Helper functions
include(${CMAKE_SOURCE_DIR}/cmake/debug.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/readProjectConfig.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/getTargetBuildPath.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/getLatestGitTag.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/getLatestStableGitTag.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/stripSymbolsPostbuild.cmake)


#-------------------------------------------------------------
# Read project config
readProjectConfig("${CMAKE_SOURCE_DIR}/PROJECT.txt")
dump_cmake_variables(APP)
project("${APPNAME}"
        VERSION "${APPVER}"
        DESCRIPTION "${APPDESC}"
        HOMEPAGE_URL "${APPURL}"
        LANGUAGES CXX
    )

# App Variables
add_compile_options(-DAPPNAME="${APPNAME}")
add_compile_options(-DAPPNAMERAW=${APPNAME})
add_compile_options(-DAPPVER="${APPVER}")
add_compile_options(-DAPPVERSION="${APPVERSION}")
add_compile_options(-DAPPBUILD="${APPBUILD}")
add_compile_options(-DAPPDESCRIPTION="${APPDESCRIPTION}")
add_compile_options(-DAPPURL="${APPURL}")

# Show compiler info
message(STATUS " [-] CMAKE_C_COMPILER=${CMAKE_C_COMPILER}")
message(STATUS " [-] CMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}")
message(STATUS " [-] CMAKE_LINKER=${CMAKE_LINKER}")
message(STATUS " [-] CMAKE_CXX_FLAGS_RELEASE=${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS " [-] CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
message(STATUS " [-] CMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}")


#-------------------------------------------------------------
# 3rd party libs

# Boost
# get_latest_git_tag("https://github.com/boostorg/boost.git" "^boost-?[0-9]+(\.[0-9]+){0,2}$" LIB3_BOOST_VERSION)
# set(BOOST_ENABLE_CMAKE ON CACHE BOOL "" FORCE)
# set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
# set(BOOST_INCLUDE_LIBRARIES "core;assert;stacktrace" CACHE STRING "" FORCE)

dump_cmake_variables(LIB3)

#-------------------------------------------------------------
# Add code
add_subdirectory(source)
add_subdirectory(test)


#-------------------------------------------------------------
# Install library and headers
install(TARGETS ${APPNAME}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h"
    )

