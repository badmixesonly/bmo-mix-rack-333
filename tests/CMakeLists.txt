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
bmo_tune_test(schema       SchemaTests.cpp)
bmo_tune_test(hardtune     HardTuneTests.cpp)

# The hard-tune target: BMO as close as Antares on a moving voice. It fails
# today -- that is the point of it (the house rule: fix nothing a test does not
# first fail on) -- so it is registered but DISABLED, which ctest lists as
# "Not Run (Disabled)" at the end of every run rather than hiding it. Enable
# it in the change that makes it pass. Run it by hand:
#   build/tests/Release/hardtune_tests --target
add_test(NAME hardtune_target COMMAND hardtune_tests --target)
set_tests_properties(hardtune_target PROPERTIES DISABLED TRUE LABELS open)

# The real panel, laid out with no display: needs JUCE and the product.
if(NOT BMO_DSP_ONLY)
    bmo_add_tool(panel_tests ${CMAKE_CURRENT_SOURCE_DIR}/plugin/PanelTests.cpp)
    target_include_directories(panel_tests PRIVATE ${BMO_INCLUDE_ROOTS})
    target_link_libraries(panel_tests PRIVATE bmo_tune)
    add_test(NAME panel COMMAND panel_tests)
endif()
