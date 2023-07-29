include(FetchContent)

set(BOOST_ENABLE_CMAKE ON)
FetchContent_Declare(
        Boost
        GIT_REPOSITORY "https://github.com/boostorg/boost"
        GIT_TAG "5df8086b733798c8e08e316626a16babe11bd0d2" #https://github.com/boostorg/boost/releases/tag/boost-1.79.0
        USES_TERMINAL_DOWNLOAD TRUE
)

FetchContent_MakeAvailable(Boost)
