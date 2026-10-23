cmake_minimum_required(VERSION 3.22)
project(Dynamicam LANGUAGES C CXX)

# ── C++ standard ──────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Optimization flags (tiny .so size) ────────────────────────
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fvisibility=hidden -ffunction-sections -fdata-sections -fexceptions")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -fvisibility=hidden -ffunction-sections -fdata-sections")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gc-sections,--strip-all -s")

# ── Fetch preloader-android SDK ───────────────────────────────
include(FetchContent)
FetchContent_Declare(
    preloader_android
    GIT_REPOSITORY https://github.com/LiteLDev/preloader-android.git
    GIT_TAG main
)
FetchContent_MakeAvailable(preloader_android)

# ── Include directories ───────────────────────────────────────
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)

# ── All source files ──────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/pluginkit/Dynamicam.cpp
    src/pluginkit/CameraManager.cpp
    src/pluginkit/RenderHooks.cpp
    src/pluginkit/PerspectiveSystem.cpp
    src/pluginkit/EmoteFreecam.cpp
    src/pluginkit/HotItemRender.cpp
    src/pluginkit/SignatureScan.cpp
)

# ── Build shared library (.so) ────────────────────────────────
add_library(dynamicam SHARED ${SOURCES})

target_link_libraries(dynamicam
    preloader
    log
    android
    EGL
    GLESv3
    GLESv2
    jnigraphics
)
