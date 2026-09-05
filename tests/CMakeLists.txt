# DSP tests build everywhere, JUCE or not. Plugin tests need JUCE.

bmo_add_dsp_tool(eq_dsp_tests dsp/EqDspTests.cpp)
target_link_libraries(eq_dsp_tests PRIVATE bmo_eq_dsp)
add_test(NAME eq_dsp COMMAND eq_dsp_tests)

bmo_add_dsp_tool(sat_dsp_tests dsp/SatDspTests.cpp)
target_link_libraries(sat_dsp_tests PRIVATE bmo_sat_dsp)
add_test(NAME sat_dsp COMMAND sat_dsp_tests)

bmo_add_dsp_tool(util_dsp_tests dsp/UtilDspTests.cpp)
target_link_libraries(util_dsp_tests PRIVATE bmo_util_dsp)
add_test(NAME util_dsp COMMAND util_dsp_tests)

if(BMO_DSP_ONLY)
    return()
endif()

bmo_add_tool(eq_tests plugin/EqTests.cpp)
target_link_libraries(eq_tests PRIVATE bmo_eq)
add_test(NAME eq COMMAND eq_tests)

bmo_add_tool(sat_tests plugin/SatTests.cpp)
target_link_libraries(sat_tests PRIVATE bmo_sat)
add_test(NAME sat COMMAND sat_tests)

bmo_add_tool(util_tests plugin/UtilTests.cpp)
target_link_libraries(util_tests PRIVATE bmo_util)
add_test(NAME util COMMAND util_tests)

bmo_add_tool(rack_tests plugin/RackTests.cpp ${BMO_ROOT}/products/rack/Registry.cpp)
target_link_libraries(rack_tests PRIVATE bmo_eq bmo_sat bmo_util)
add_test(NAME rack COMMAND rack_tests)
