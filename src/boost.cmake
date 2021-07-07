
#====================================================================
# Find boost
# https://cmake.org/cmake/help/latest/module/FindBoost.html

set(Boost_USE_STATIC_LIBS        ON)
set(Boost_USE_DEBUG_LIBS        OFF)
set(Boost_USE_RELEASE_LIBS       ON)
set(Boost_USE_MULTITHREADED      ON)
set(Boost_USE_STATIC_RUNTIME     ON)
set(Boost_PYTHON_STATIC_LIB      ON)

if(UNIX)
    find_package(Boost)
else()
    find_package(Boost COMPONENTS date_time)
endif()

if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost not found")
endif()

include_directories("${Boost_INCLUDE_DIRS}")
link_directories("${Boost_LIBRARY_DIRS}")

message(STATUS "Found Boost        : ${Boost_VERSION}")
message(STATUS "Boost_INCLUDE_DIRS : ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost_LIBRARY_DIRS : ${Boost_LIBRARY_DIRS}")
message(STATUS "Boost_LIBRARIES    : ${Boost_LIBRARIES}")

