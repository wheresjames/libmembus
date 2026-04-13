
find_program(GCC_PATH gcc)
find_program(GXX_PATH g++)

if (GCC_PATH AND GXX_PATH)
    message(STATUS " [wantGcc] Using GCC compiler")
    set(CMAKE_C_COMPILER ${GCC_PATH}) # CACHE STRING "C compiler" FORCE)
    set(CMAKE_CXX_COMPILER ${GXX_PATH}) # CACHE STRING "C++ compiler" FORCE)
    set(CMAKE_LINKER ${GXX_PATH}) # CACHE STRING "C++ linker" FORCE)
else()
    message(WARNING "[wantGcc] GCC compiler not found.")
endif()

