# DSP tests: plain executables, registered with CTest, JUCE-free.

function(bmo_tune_test name)
    bmo_add_dsp_tool(${name}_tests ${CMAKE_CURRENT_SOURCE_DIR}/dsp/${ARGN})
    target_link_libraries(${name}_tests PRIVATE bmo_tune_dsp)
    add_test(NAME ${name} COMMAND ${name}_tests)
endfunction()

bmo_tune_test(kernel   KernelTests.cpp)
bmo_tune_test(detector DetectorTests.cpp)

bmo_add_dsp_tool(probe ${CMAKE_CURRENT_SOURCE_DIR}/dsp/Probe.cpp)
target_link_libraries(probe PRIVATE bmo_tune_dsp)

bmo_tune_test(interpolator InterpolatorTests.cpp)
bmo_tune_test(core CoreTests.cpp)
bmo_tune_test(correction CorrectionTests.cpp)
