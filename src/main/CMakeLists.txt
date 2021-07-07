
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

if (NOT CONAN_COMPILER)
    include(../boost.cmake)
    target_link_libraries(${BINARY} LINK_PUBLIC ${Boost_LIBRARIES})
    target_link_libraries(${BINARY}_static LINK_PUBLIC ${Boost_LIBRARIES})
endif()


#====================================================================

include(GenerateExportHeader)
generate_export_header(${BINARY})

# Shared
set_target_properties(${BINARY} PROPERTIES PREFIX "")
set_target_properties(${BINARY} PROPERTIES VERSION ${APPVER} SOVERSION ${APPMAJ}.${APPMIN})
set_target_properties(${BINARY} PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
install(TARGETS ${BINARY} DESTINATION lib COMPONENT lib)

# Static
# set_target_properties(${BINARY}_static PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
set_target_properties(${BINARY}_static PROPERTIES PREFIX "")
set_target_properties(${BINARY}_static PROPERTIES VERSION ${APPVER} SOVERSION ${APPMAJ}.${APPMIN})
set_target_properties(${BINARY}_static PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
install(TARGETS ${BINARY}_static DESTINATION lib COMPONENT lib)

# Install source files
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/headers/" DESTINATION include COMPONENT lib)

