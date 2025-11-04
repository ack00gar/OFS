cmake_minimum_required(VERSION 3.16)

project(scripter)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

# =============
# == OPTIONS ==
# =============
option(OFS_PROFILE OFF)
option(OFS_AVX OFF)
option(OFS_BUILD_UNIVERSAL "Build universal binary for macOS (x86_64 + arm64)" OFF)

# macOS specific settings
if(APPLE)
    # Set minimum macOS version to 11.0 (Big Sur) for ARM64 support
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment version")

    # On Apple Silicon, build ARM64-only by default for better performance
    if(OFS_BUILD_UNIVERSAL)
        set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures for macOS")
        message(STATUS "Building universal binary (x86_64 + arm64)")
    else()
        # Build ARM64-only on Apple Silicon
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures for macOS")
        message(STATUS "Building ARM64-only (Apple Silicon native)")
    endif()
endif()

if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    if(OFS_AVX)
        message("OFS AVX ENABLED")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX")
    endif()
endif()

# ====================
# === DEPENDENCIES ===
# ====================
add_subdirectory("lib/")

# =====================
# ==== BUILD-TOOLS ====
# =====================
add_subdirectory("localization/")

# ==============
# ==== SRC ====
# ==============
add_subdirectory("OFS-lib/")
add_subdirectory("src/")

