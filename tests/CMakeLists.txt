# One doctest executable for all subsystems. Files are globbed, so a new
# tests/<subsystem>_*.cpp is picked up without editing this file.
file(GLOB test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(mc_tests ${test_sources})
# rendering is linked for its GL-free parts (meshing); tests never create a GL context.
target_link_libraries(mc_tests PRIVATE doctest::doctest mc_core mc_world mc_gameplay mc_rendering mc_ui)
mc_set_warnings(mc_tests)
add_test(NAME mc_tests COMMAND mc_tests)
target_compile_definitions(mc_tests PRIVATE MC_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
