#====================================================================

set(EXAMPLE_SOURCES
    memmsg_pingpong.cpp
    memcmd_control.cpp
    memkv_state.cpp
    memvid_dummy_frames.cpp
)

foreach(EXAMPLE_SOURCE ${EXAMPLE_SOURCES})
    get_filename_component(EXAMPLE_NAME "${EXAMPLE_SOURCE}" NAME_WE)
    add_executable(example-${EXAMPLE_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_SOURCE}")
    target_include_directories(example-${EXAMPLE_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/include")
    target_link_libraries(example-${EXAMPLE_NAME} PRIVATE ${PROJECT_NAME})
endforeach()
