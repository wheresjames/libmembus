
find_program(CLANG_PATH clang)
find_program(CLANGXX_PATH clang++)

if (CLANG_PATH AND CLANGXX_PATH)
    message(STATUS " [wantClang] Using Clang compiler")
    set(CMAKE_C_COMPILER ${CLANG_PATH}) # CACHE STRING "C compiler" FORCE)
    set(CMAKE_CXX_COMPILER ${CLANGXX_PATH}) # CACHE STRING "C++ compiler" FORCE)
    set(CMAKE_OBJ_COMPILER ${CLANGXX_PATH}) # CACHE STRING "Objective C compiler" FORCE)
    set(CMAKE_OBJCXX_COMPILER ${CLANGXX_PATH}) # CACHE STRING "Objective C++ compiler" FORCE)
    set(CMAKE_LINKER ${CLANGXX_PATH}) #  CACHE STRING "C++ linker" FORCE)
else()
    message(WARNING "[wantClang] Clang compiler not found.")
endif()

