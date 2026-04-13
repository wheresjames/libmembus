
#====================================================================

include(FetchContent)

FetchContent_Declare(
        backward
        GIT_REPOSITORY  https://github.com/bombela/backward-cpp.git
        GIT_TAG         ${LIB3_BACKWARD_VERSION}
        GIT_PROGRESS    TRUE
    )
FetchContent_MakeAvailable(backward)

