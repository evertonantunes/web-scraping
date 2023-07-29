include(FetchContent)

message(STATUS "Not using system Curl, using built-in curl project instead.")

set(BUILD_CURL_EXE OFF CACHE INTERNAL "" FORCE)
set(BUILD_TESTING OFF CACHE INTERNAL "" FORCE)
set(HTTP_ONLY ON CACHE INTERNAL "" FORCE)

if (USE_WINSSL OR USE_OPENSSL)
    set(SSL_ENABLED ON CACHE INTERNAL "" FORCE)
else()
    set(CURL_CA_PATH "none" CACHE INTERNAL "" FORCE)
endif()

if(USE_WINSSL)
    set(CMAKE_USE_WINSSL ON CACHE INTERNAL "" FORCE)
    set(CURL_CA_PATH "none" CACHE INTERNAL "" FORCE)
endif()

if(USE_OPENSSL)
    set(CMAKE_USE_OPENSSL ON CACHE INTERNAL "" FORCE)
endif()

# Show progress of FetchContent:
set(FETCHCONTENT_QUIET OFF CACHE INTERNAL "" FORCE)
FetchContent_Declare(curl
    GIT_REPOSITORY         https://github.com/curl/curl.git
    GIT_TAG                b81e0b07784dc4c1e8d0a86194b9d28776d071c0 # the hash for curl-7_69_1
    GIT_PROGRESS           TRUE
    USES_TERMINAL_DOWNLOAD TRUE
)

FetchContent_MakeAvailable(curl)

add_library(curl_int INTERFACE)
target_link_libraries(curl_int INTERFACE libcurl)
target_include_directories(curl_int INTERFACE ${curl_SOURCE_DIR}/include ${curl_BINARY_DIR}/include/curl)
add_library(CURL::libcurl ALIAS curl_int)

if(BUILD_CURL_EXE)
    set_property(TARGET curl PROPERTY FOLDER "external")
endif()

set_property(TARGET libcurl PROPERTY FOLDER "external")
