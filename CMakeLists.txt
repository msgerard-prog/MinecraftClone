cmake_minimum_required(VERSION 3.28)
# Version: MAJOR.MINOR.PATCH (semantic). Before 1.0 the minor number follows the last
# finished milestone (0.12 = M12); v1.0.0 is tagged when M22 is done. Each build also
# records `git describe` (BuildInfo.h), shown in the log, F3 and --version.
project(MinecraftClone VERSION 0.22.0 LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Debug info embedded in each object (/Z7) instead of a shared PDB per target (/Zi):
# parallel compiles then don't depend on the per-user mspdbsrv, which other projects'
# builds on this machine can leave hung. The linker still writes the .exe's PDB.
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
# The release preset is RelWithDebInfo, whose default /Ob1 inlines only functions
# marked inline: allow MSVC to inline any small function (noise, lerp, palette reads).
if(MSVC)
    string(REPLACE "/Ob1" "/Ob2" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Dependencies)
include(Subsystem)

add_subdirectory(third_party)

# Build info (version + git revision), regenerated on every build.
set(MC_BUILDINFO_DIR "${CMAKE_BINARY_DIR}/generated")
add_custom_target(mc_buildinfo ALL
    COMMAND "${CMAKE_COMMAND}" -DSRC="${CMAKE_SOURCE_DIR}" -DOUT="${MC_BUILDINFO_DIR}/BuildInfo.h"
            -DPROJECT_VERSION=${PROJECT_VERSION} -P "${CMAKE_SOURCE_DIR}/cmake/GenBuildInfo.cmake"
    BYPRODUCTS "${MC_BUILDINFO_DIR}/BuildInfo.h"
    COMMENT "Build info")

# Subsystems, in dependency order (a subsystem may only depend on ones above it).
# See docs/architecture.md for the layer rules.
add_subdirectory(src/core)
add_dependencies(mc_core mc_buildinfo)
target_include_directories(mc_core PRIVATE "${MC_BUILDINFO_DIR}") # core/Version.cpp only
add_subdirectory(src/world)
add_subdirectory(src/rendering)
add_subdirectory(src/audio)
add_subdirectory(src/gameplay)
add_subdirectory(src/ui)

add_executable(MinecraftClone src/main.cpp src/MinecraftClone.rc)
add_dependencies(MinecraftClone mc_buildinfo)
target_include_directories(MinecraftClone PRIVATE "${MC_BUILDINFO_DIR}")
target_link_libraries(MinecraftClone PRIVATE mc_core mc_world mc_rendering mc_audio mc_gameplay mc_ui)
mc_set_warnings(MinecraftClone)

enable_testing()
add_subdirectory(tests)
