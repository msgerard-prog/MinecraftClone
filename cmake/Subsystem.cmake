# Strict warnings for our own code only (never third_party).
function(mc_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

# mc_add_subsystem(<name> [DEPS <targets...>])
# Creates target mc_<name> from every .cpp/.h under the current folder.
# Sources are globbed (CONFIGURE_DEPENDS), so adding a file needs no CMake edit.
# Headers are included as "<subsystem>/File.h" from src/.
function(mc_add_subsystem name)
    cmake_parse_arguments(ARG "" "" "DEPS" ${ARGN})
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
    set(target mc_${name})
    list(FILTER sources INCLUDE REGEX "\\.cpp$")
    if(sources)
        add_library(${target} STATIC ${sources})
        target_include_directories(${target} PUBLIC "${PROJECT_SOURCE_DIR}/src")
        target_link_libraries(${target} PUBLIC ${ARG_DEPS})
        mc_set_warnings(${target})
    else()
        # No code yet: header-only placeholder so the layer graph exists from day one.
        add_library(${target} INTERFACE)
        target_include_directories(${target} INTERFACE "${PROJECT_SOURCE_DIR}/src")
        target_link_libraries(${target} INTERFACE ${ARG_DEPS})
    endif()
endfunction()
