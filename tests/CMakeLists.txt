# DSP tests: plain executables, registered with CTest, JUCE-free.

function(bmo_tune_test name)
    bmo_add_dsp_tool(${name}_tests ${CMAKE_CURRENT_SOURCE_DIR}/dsp/${ARGN})
    target_link_libraries(${name}_tests PRIVATE bmo_tune_dsp)
    add_test(NAME ${name} COMMAND ${name}_tests)
endfunction()

bmo_tune_test(kernel       KernelTests.cpp)
bmo_tune_test(detector     DetectorTests.cpp)
bmo_tune_test(interpolator InterpolatorTests.cpp)
bmo_tune_test(correction   CorrectionTests.cpp)
bmo_tune_test(core         CoreTests.cpp)
bmo_tune_test(lpc          LpcTests.cpp)
bmo_tune_test(hybrid       HybridTests.cpp)
bmo_tune_test(schema       SchemaTests.cpp)
