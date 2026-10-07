cmake_minimum_required(VERSION 3.28)
project(MinecraftClone LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Dependencies)
include(Subsystem)

add_subdirectory(third_party)

# Subsystems, in dependency order (a subsystem may only depend on ones above it).
# See docs/architecture.md for the layer rules.
add_subdirectory(src/core)
add_subdirectory(src/world)
add_subdirectory(src/rendering)
add_subdirectory(src/audio)
add_subdirectory(src/gameplay)
add_subdirectory(src/ui)

add_executable(MinecraftClone src/main.cpp)
target_link_libraries(MinecraftClone PRIVATE mc_core mc_world mc_rendering mc_audio mc_gameplay mc_ui)
mc_set_warnings(MinecraftClone)

enable_testing()
add_subdirectory(tests)
