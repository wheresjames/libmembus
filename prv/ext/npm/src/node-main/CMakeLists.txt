
#====================================================================
# Includes
include(FetchContent)


#====================================================================
# Fetch NAPI
FetchContent_Declare(
  napi
  GIT_REPOSITORY https://github.com/nodejs/node-addon-api.git
  GIT_TAG        v1.5.0
)
FetchContent_MakeAvailable(napi)
include_directories("${napi_SOURCE_DIR}" "${napi_SOURCE_DIR}/src")


#====================================================================
set(BINARY ${PROJECT_NAME})


#====================================================================
# Library

# Include
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/headers")

# Source files
file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/cpp/*.cpp")

add_library(${BINARY} MODULE ${SOURCES})
set_target_properties(${BINARY} PROPERTIES PREFIX "")
set_target_properties(${BINARY} PROPERTIES SUFFIX ".node")
set_target_properties(${BINARY} PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")


#====================================================================
# Install

install(TARGETS ${BINARY} DESTINATION lib COMPONENT lib)

# Package file
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/package.json.in" "${CMAKE_BINARY_DIR}/package.json" @ONLY)
install(FILES "${CMAKE_BINARY_DIR}/package.json" DESTINATION lib COMPONENT lib)

# Copy package file for testing
file(COPY "${CMAKE_BINARY_DIR}/package.json" DESTINATION "${CMAKE_BINARY_DIR}/lib/node_modules/${PROJECT_NAME}")

# Copy package file for npm
file(COPY "${CMAKE_BINARY_DIR}/package.json" DESTINATION "${CMAKE_SOURCE_DIR}")

# Copy target for testing
add_custom_command(TARGET ${BINARY} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy 
                        "$<TARGET_FILE:${BINARY}>"
                        "${CMAKE_BINARY_DIR}/lib/node_modules/${PROJECT_NAME}/bin/$<TARGET_FILE_NAME:${BINARY}>")

# Copy for npm installation
add_custom_command(TARGET ${BINARY} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy 
                        "$<TARGET_FILE:${BINARY}>"
                        "${CMAKE_SOURCE_DIR}/bin/$<TARGET_FILE_NAME:${BINARY}>")
