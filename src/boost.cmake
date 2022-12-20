
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

if(Boost_FOUND)

    message(STATUS "Found Boost        : ${Boost_VERSION}")
    message(STATUS "Boost_INCLUDE_DIRS : ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost_LIBRARY_DIRS : ${Boost_LIBRARY_DIRS}")
    message(STATUS "Boost_LIBRARIES    : ${Boost_LIBRARIES}")
    include_directories("${Boost_INCLUDE_DIRS}")
    link_directories("${Boost_LIBRARY_DIRS}")

else()
    message(FATAL_ERROR "Boost not found")

    # message(STATUS "Fetch boost")
    # include(FetchContent)
    # FetchContent_Declare(
    #     boost
    #     GIT_REPOSITORY      https://github.com/boostorg/boost.git
    #     GIT_TAG             master
    #     GIT_PROGRESS        TRUE
    # )
    # FetchContent_MakeAvailable(boost)

    # if(NOT boost_POPULATED)
    #     message(FATAL_ERROR "boost not found")
    # endif()

    # set(DEPENDCHAIN ${DEPENDCHAIN} boost)

    # message(STATUS "Boost_SOURCE_DIR : ${boost_SOURCE_DIR}")
    # message(STATUS "Boost_BINARY_DIR : ${boost_BINARY_DIR}")

    # include_directories(${boost_SOURCE_DIR})
    # link_directories(${boost_BINARY_DIR})

endif()

