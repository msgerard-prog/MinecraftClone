mc_add_subsystem(audio DEPS mc_core)
# XAudio2 2.9 ships with Windows 10+ (ADR 0008): a system library, not a dependency.
if(WIN32)
    target_link_libraries(mc_audio PUBLIC xaudio2 ole32)
endif()
