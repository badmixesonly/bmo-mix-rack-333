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

bmo_add_dsp_tool(opto_dsp_tests dsp/OptoDspTests.cpp)
target_link_libraries(opto_dsp_tests PRIVATE bmo_opto_dsp)
add_test(NAME opto_dsp COMMAND opto_dsp_tests)

bmo_add_dsp_tool(dim_dsp_tests dsp/DimDspTests.cpp)
target_link_libraries(dim_dsp_tests PRIVATE bmo_dim_dsp)
add_test(NAME dim_dsp COMMAND dim_dsp_tests)

bmo_add_dsp_tool(deq_dsp_tests dsp/DeqDspTests.cpp)
target_link_libraries(deq_dsp_tests PRIVATE bmo_deq_dsp)
add_test(NAME deq_dsp COMMAND deq_dsp_tests)

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

bmo_add_tool(opto_tests plugin/OptoTests.cpp)
target_link_libraries(opto_tests PRIVATE bmo_opto)
add_test(NAME opto COMMAND opto_tests)

bmo_add_tool(dim_tests plugin/DimTests.cpp)
target_link_libraries(dim_tests PRIVATE bmo_dim)
add_test(NAME dim COMMAND dim_tests)

bmo_add_tool(rack_tests plugin/RackTests.cpp ${BMO_ROOT}/products/rack/Registry.cpp)
target_link_libraries(rack_tests PRIVATE bmo_eq bmo_sat bmo_util bmo_opto bmo_dim)
add_test(NAME rack COMMAND rack_tests)

# Layout assertions on the real panels. JUCE, not bmo_add_dsp_tool: no
# rendering happens, but Tokens.h includes juce_gui_basics, so a UI assertion
# cannot run in the DSP-only job whatever docs/ui-workflow-brief.md §4 says.
bmo_add_tool(ui_layout_tests ui/LayoutTests.cpp ${BMO_ROOT}/products/rack/Registry.cpp)
target_link_libraries(ui_layout_tests PRIVATE bmo_eq bmo_sat bmo_util bmo_opto bmo_dim)
add_test(NAME ui_layout COMMAND ui_layout_tests)
