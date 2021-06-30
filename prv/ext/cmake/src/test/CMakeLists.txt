
#====================================================================

# Output
set(BINARY ${PROJECT_NAME}-test)

include_directories("${PROJECT_SOURCE_DIR}/src/main/headers")

file(GLOB_RECURSE SOURCES "cpp/*.cpp")

add_executable(${BINARY} ${SOURCES})

target_link_libraries(${BINARY} PRIVATE ${PROJECT_NAME})

if(UNIX)
    target_link_libraries(${BINARY} PRIVATE pthread rt)
endif()

set_target_properties(${BINARY} PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

enable_testing()
add_test(NAME ${BINARY} COMMAND ${BINARY})
set_tests_properties(${BINARY} PROPERTIES TIMEOUT 10)
add_custom_command(TARGET ${BINARY} POST_BUILD
                   COMMAND ${CMAKE_CTEST_COMMAND} -C $<CONFIG> --output-on-failure --build-target ${BINARY})

install(TARGETS ${BINARY} DESTINATION share/${PROJECT_NAME} COMPONENT test)
