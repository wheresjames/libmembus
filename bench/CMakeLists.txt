#====================================================================

add_executable(bench-libmembus "${CMAKE_CURRENT_SOURCE_DIR}/bench_libmembus.cpp")
target_include_directories(bench-libmembus PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(bench-libmembus PRIVATE ${PROJECT_NAME})
