# BMO Tune RT's DSP: a real static library, JUCE-free, the same shape as a
# rack module's bmo_<id>_dsp. The tests, the CLI and every measurement tool
# link this; the plugin wrapper will too.
add_library(bmo_tune_dsp STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/tune/dsp/Detector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tune/dsp/CorrectionLaw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tune/dsp/ClassicEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tune/dsp/HybridEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tune/dsp/TuneCore.cpp)

target_include_directories(bmo_tune_dsp PUBLIC ${BMO_INCLUDE_ROOTS})
set_target_properties(bmo_tune_dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)
bmo_tune_warnings(bmo_tune_dsp)

# The whole module for the plugin: its definition and its panel, on the
# shared core, as an INTERFACE library the product compiles in -- the rack's
# bmo_<id> shape.
if(NOT BMO_DSP_ONLY)
    add_library(bmo_tune INTERFACE)
    target_sources(bmo_tune INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/tune/Module.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tune/panel/TunePanel.cpp)
    target_link_libraries(bmo_tune INTERFACE bmo_tune_core bmo_tune_dsp)
endif()
