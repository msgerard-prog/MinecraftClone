# Writes BuildInfo.h with the project version and `git describe`. Runs on every build
# (cmake -P); configure_file only rewrites the header when its text changes, so an
# unchanged revision rebuilds nothing.
#   -DSRC=<source dir> -DOUT=<header> -DPROJECT_VERSION=<x.y.z>
execute_process(COMMAND git describe --tags --always --dirty
                WORKING_DIRECTORY "${SRC}" OUTPUT_VARIABLE MC_GIT_DESCRIBE
                OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE rc)
if(NOT rc EQUAL 0 OR MC_GIT_DESCRIBE STREQUAL "")
    set(MC_GIT_DESCRIBE "unknown")
endif()
string(REPLACE "." ";" parts "${PROJECT_VERSION}")
list(GET parts 0 PROJECT_VERSION_MAJOR)
list(GET parts 1 PROJECT_VERSION_MINOR)
list(GET parts 2 PROJECT_VERSION_PATCH)
configure_file("${SRC}/cmake/BuildInfo.h.in" "${OUT}" @ONLY)
