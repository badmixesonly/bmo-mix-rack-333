# BMO Tune RT's DSP: a real static library, JUCE-free, the same shape as a
# rack module's bmo_<id>_dsp. The tests, the CLI and every measurement tool
# link this; the plugin wrapper will too.
add_library(bmo_tune_dsp STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/tune/dsp/Detector.cpp)

target_include_directories(bmo_tune_dsp PUBLIC ${BMO_INCLUDE_ROOTS})
set_target_properties(bmo_tune_dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)
bmo_tune_warnings(bmo_tune_dsp)
