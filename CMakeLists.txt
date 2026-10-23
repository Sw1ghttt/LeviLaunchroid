cmake_minimum_required(VERSION 3.22)
project(dynamicam LANGUAGES CXX)

# ── C++ standard ──────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Locate preloader-android SDK ───────────────────────────────
get_filename_component(LL_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(PRELOADER_ANDROID_ROOT
    "${LL_REPO_ROOT}/app/src/main/cpp/preloader"
    CACHE PATH "Path to LiteLDev/preloader-android")

if(NOT EXISTS "${PRELOADER_ANDROID_ROOT}/include/pl/Config.hpp")
    message(FATAL_ERROR
        "PRELOADER_ANDROID_ROOT must point to a preloader-android checkout. "
        "Missing: ${PRELOADER_ANDROID_ROOT}/include/pl/Config.hpp")
endif()

message(STATUS "Dynamicam using preloader from: ${PRELOADER_ANDROID_ROOT}")

# ── External dependencies via FetchContent ──────────────────────
include(FetchContent)

set(DYNAMICAM_DEPS)
if(NOT TARGET nlohmann_json::nlohmann_json)
    FetchContent_Declare(nlohmann_json GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.11.3)
    list(APPEND DYNAMICAM_DEPS nlohmann_json)
endif()
if(NOT TARGET Boost::pfr)
    FetchContent_Declare(boost_pfr GIT_REPOSITORY https://github.com/boostorg/pfr.git GIT_TAG boost-1.88.0)
    list(APPEND DYNAMICAM_DEPS boost_pfr)
endif()
if(NOT TARGET magic_enum::magic_enum)
    FetchContent_Declare(magic_enum GIT_REPOSITORY https://github.com/Neargye/magic_enum.git GIT_TAG v0.9.7)
    list(APPEND DYNAMICAM_DEPS magic_enum)
endif()
if(NOT TARGET fmt::fmt)
    FetchContent_Declare(fmt GIT_REPOSITORY https://github.com/fmtlib/fmt.git GIT_TAG 11.2.0)
    set(FMT_DOC OFF CACHE BOOL "" FORCE)
    set(FMT_TEST OFF CACHE BOOL "" FORCE)
    set(FMT_INSTALL OFF CACHE BOOL "" FORCE)
    list(APPEND DYNAMICAM_DEPS fmt)
endif()
if(DYNAMICAM_DEPS)
    FetchContent_MakeAvailable(${DYNAMICAM_DEPS})
endif()

# ── Common configuration function ──────────────────────────────
function(configure_dynamicam_common target_name)
    target_include_directories("${target_name}" PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/module"
        "${PRELOADER_ANDROID_ROOT}/include")
    target_link_libraries("${target_name}" PRIVATE
        nlohmann_json::nlohmann_json
        Boost::pfr
        magic_enum::magic_enum
        fmt::fmt)
    target_compile_options("${target_name}" PRIVATE
        $<$<COMPILE_LANG_AND_ID:CXX,Clang,GNU>:-Wall -Wextra -Wpedantic>)
endfunction()

# ── Non-Android: config generator (host) ──────────────────────
if(NOT ANDROID)
    add_executable(dynamicam_config_gen src/module/Dynamicam.cpp src/module/CameraManager.cpp)
    configure_dynamicam_common(dynamicam_config_gen)
    target_compile_definitions(dynamicam_config_gen PRIVATE PL_CONFIG_NO_RUNTIME)
endif()

# ── Android: native .so library ───────────────────────────────
if(ANDROID)
    option(LL_DYNAMICAM_LINK_PRELOADER
           "Build and link the local preloader target" ON)

    if(LL_DYNAMICAM_LINK_PRELOADER AND NOT TARGET preloader)
        add_subdirectory("${PRELOADER_ANDROID_ROOT}"
                         "${CMAKE_BINARY_DIR}/_preloader_android")
    endif()

    if(ANDROID_ABI)
        set(LL_DYNAMICAM_OUTPUT_ABI "${ANDROID_ABI}")
    else()
        set(LL_DYNAMICAM_OUTPUT_ABI "android")
    endif()
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY
        "${CMAKE_BINARY_DIR}/out/${LL_DYNAMICAM_OUTPUT_ABI}")

    find_library(LL_LOG_LIB log)

    add_library(dynamicam SHARED
        src/module/Dynamicam.cpp
        src/module/CameraManager.cpp
        src/module/RenderHooks.cpp
        src/module/PerspectiveSystem.cpp
        src/module/EmoteFreecam.cpp
        src/module/HotItemRender.cpp
        src/module/SignatureScan.cpp
    )
    set_target_properties(dynamicam PROPERTIES OUTPUT_NAME "dynamicam")
    configure_dynamicam_common(dynamicam)
    target_compile_options(dynamicam PRIVATE
        -O2 -fvisibility=hidden -fvisibility-inlines-hidden
        -ffunction-sections -fdata-sections -fexceptions)
    target_compile_options(dynamicam PRIVATE
        $<$<COMPILE_LANG_AND_ID:CXX,Clang,GNU>:-Wall -Wextra -Wpedantic>)

    if(LL_LOG_LIB)
        target_link_libraries(dynamicam PRIVATE "${LL_LOG_LIB}")
    endif()
    if(TARGET preloader)
        target_link_libraries(dynamicam PRIVATE preloader)
    else()
        target_link_options(dynamicam PRIVATE -Wl,--unresolved-symbols=ignore-all)
    endif()

    # Link OpenGL/EGL/JNI libs
    find_library(LL_EGL_LIB EGL)
    find_library(LL_GLES3_LIB GLESv3)
    find_library(LL_JNI_LIB jnigraphics)
    if(LL_EGL_LIB)   target_link_libraries(dynamicam PRIVATE ${LL_EGL_LIB})
    if(LL_GLES3_LIB) target_link_libraries(dynamicam PRIVATE ${LL_GLES3_LIB})
    if(LL_JNI_LIB)   target_link_libraries(dynamicam PRIVATE ${LL_JNI_LIB})
endif()
