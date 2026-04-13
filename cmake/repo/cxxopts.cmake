
#====================================================================

include(FetchContent)

FetchContent_Declare(
        cxxopts
        GIT_REPOSITORY  https://github.com/jarro2783/cxxopts.git
        GIT_TAG         ${LIB3_CXXOPTS_VERSION}
        GIT_PROGRESS    TRUE
    )
FetchContent_MakeAvailable(cxxopts)

