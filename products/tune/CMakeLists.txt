# BMO Tune RT as a plugin: the rack's bmo_add_plugin, written out here because
# this product is not built by the rack's CMakeLists.txt.
#
# The manufacturer code is the suite's and the plugin code is this product's.
# Both are inputs to the VST3 identity, so both are permanent: changing either
# orphans every session that references the plugin. Btun and
# com.lt3audio.bmotunert were allocated by Frosty on 2026-09-10.
#
# Never copied into the system plugin folder by the build. Install by hand
# from build-plugin/products/tune/BmoTuneRT_artefacts/Release/VST3.

juce_add_plugin(BmoTuneRT
    COMPANY_NAME                "LT3 Audio"
    BUNDLE_ID                   "com.lt3audio.bmotunert"
    PLUGIN_MANUFACTURER_CODE    LT3a
    PLUGIN_CODE                 Btun
    FORMATS                     VST3 Standalone
    PRODUCT_NAME                "BMO Tune RT"
    VERSION                     ${PROJECT_VERSION}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             "Fx" "Pitch Shift"
    AU_MAIN_TYPE                kAudioUnitType_Effect)

target_sources(BmoTuneRT PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

target_compile_definitions(BmoTuneRT PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(BmoTuneRT
    PRIVATE
        bmo_tune
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
