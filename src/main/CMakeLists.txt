
#====================================================================
# https://linux.die.net/man/1/nm
# nm -D ./bld/lib/*.so
# ldd -r ./bld/lib/*.so
# ldconfig -p

# Output
set(BINARY ${PROJECT_NAME})

# Include
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/headers")

# Source files
file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/cpp/*.cpp")

# Static or Shared
# add_library(${BINARY} ${SOURCES})

# Static and Shared
add_library(${BINARY}_obj OBJECT ${SOURCES})
add_library(${BINARY} SHARED $<TARGET_OBJECTS:${BINARY}_obj>)
add_library(${BINARY}_static STATIC $<TARGET_OBJECTS:${BINARY}_obj>)

#====================================================================
# Find boost
if (NOT CONAN_COMPILER)
    find_package(Boost)
    if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost not found")
    endif()
    message(STATUS "Found Boost ${Boost_VERSION}")
    include_directories("${Boost_INCLUDE_DIRS}")
    link_directories("${Boost_LIBRARIES}")
    target_link_libraries(${BINARY} LINK_PUBLIC ${Boost_LIBRARIES})
endif()

#====================================================================

include(GenerateExportHeader)
generate_export_header(${BINARY})

# Shared
set_target_properties(${BINARY} PROPERTIES PREFIX "")
set_target_properties(${BINARY} PROPERTIES VERSION ${APPVER} SOVERSION ${APPMAJ}.${APPMIN})
set_target_properties(${BINARY} PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Static
set_target_properties(${BINARY}_static PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
set_target_properties(${BINARY}_static PROPERTIES PREFIX "")
set_target_properties(${BINARY}_static PROPERTIES VERSION ${APPVER} SOVERSION ${APPMAJ}.${APPMIN})
set_target_properties(${BINARY}_static PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Install
install(TARGETS ${BINARY} DESTINATION lib COMPONENT lib)
install(TARGETS ${BINARY}_static DESTINATION lib COMPONENT lib)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/headers/" DESTINATION include COMPONENT lib)

