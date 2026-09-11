# Offline tools (spec T-1, T-3, T-5, T-11). All JUCE-free; all drive the real
# DSP library with no host involved. Named bmo-tune-* as the spec names them.

function(bmo_tune_tool name source)
    bmo_add_dsp_tool(${name} ${CMAKE_CURRENT_SOURCE_DIR}/${source})
    target_link_libraries(${name} PRIVATE bmo_tune_dsp)
endfunction()

bmo_tune_tool(bmo-tune-cli cli/main.cpp)
bmo_tune_tool(bmo-tune-gen gen/main.cpp)
bmo_tune_tool(bmo-tune-score score/main.cpp)
bmo_tune_tool(bmo-tune-latency latency/main.cpp)
