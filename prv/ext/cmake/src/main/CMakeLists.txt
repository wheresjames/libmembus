
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

add_library(${BINARY} ${SOURCES})

include(GenerateExportHeader)
generate_export_header(${BINARY})

set_target_properties(${BINARY} PROPERTIES PREFIX "")
set_target_properties(${BINARY} PROPERTIES VERSION ${APPVER} SOVERSION ${APPMAJ}.${APPMIN})
set_target_properties(${BINARY} PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Install
install(TARGETS ${BINARY} DESTINATION lib COMPONENT lib)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/headers/" DESTINATION include COMPONENT lib)
