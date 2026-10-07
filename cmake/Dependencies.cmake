# Third-party dependencies, pinned by version and SHA-256.
# Changing a version is a "never change without asking" item (see CLAUDE.md).
include(FetchContent)
set(FETCHCONTENT_QUIET ON)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glfw
    URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
    URL_HASH SHA256=b5ec004b2712fd08e8861dc271428f048775200a2df719ccf575143ba749a3e9
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    SYSTEM)

FetchContent_Declare(glm
    URL https://github.com/g-truc/glm/archive/refs/tags/1.0.1.zip
    URL_HASH SHA256=09c5716296787e1f7fcb87b1cbdbf26814ec1288ed6259ccd30d5d9795809fa5
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    SYSTEM)

FetchContent_Declare(doctest
    URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.12.zip
    URL_HASH SHA256=7a7afb5f70d0b749d49ddfcb8a454299a8fcd53e9db9c131abe99b456e88a1fe
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    SYSTEM)

# stb has no releases; pinned to a commit.
FetchContent_Declare(stb
    URL https://github.com/nothings/stb/archive/2c980bb59875b0d32144a71867fbdebb2f77cd20.zip
    URL_HASH SHA256=8e59f72b0780690cda64726804269f638a3be77b9d1506ea95f443f7964bccf0
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    SYSTEM)

FetchContent_MakeAvailable(glfw glm doctest)

FetchContent_MakeAvailable(stb)
add_library(stb INTERFACE)
target_include_directories(stb SYSTEM INTERFACE "${stb_SOURCE_DIR}")
