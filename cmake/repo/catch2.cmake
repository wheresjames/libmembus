
#====================================================================

include(FetchContent)

FetchContent_Declare(
        catch2
        GIT_REPOSITORY  https://github.com/catchorg/Catch2
        GIT_TAG         ${LIB3_CATCH2_VERSION}
        GIT_PROGRESS    TRUE
    )
FetchContent_MakeAvailable(catch2)

