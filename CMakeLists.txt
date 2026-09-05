cmake_minimum_required(VERSION 3.22)
project(BMOMixRack VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The DSP of every module has no JUCE dependency, so its tests and the
# measurement tools build and run on a machine with no audio framework at all,
# including a CI container. Configure with -DBMO_DSP_ONLY=ON to skip the
# plugins and everything JUCE brings with them.
option(BMO_DSP_ONLY "Build only the JUCE-free DSP tests and measurement tools" OFF)

# Every file in the tree includes from the root: "core/...", "modules/eq/...".
set(BMO_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

#==============================================================================
# JUCE and the plugin set-up shared by every product
#==============================================================================
if(NOT BMO_DSP_ONLY)
    set(BMO_FORMATS VST3 Standalone)

    if(APPLE)
        list(APPEND BMO_FORMATS AU)

        # Dev builds are native arm64. For a release universal binary,
        # configure with -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64".
        if(NOT CMAKE_OSX_ARCHITECTURES)
            set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
        endif()

        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
    endif()

    # Only development builds install into the user's plugin folders. A
    # Release build is for packaging and must not clobber what is being tested.
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(BMO_INSTALL_AFTER_BUILD FALSE)
    else()
        set(BMO_INSTALL_AFTER_BUILD TRUE)
    endif()

    if(MSVC)
        # Static runtime, so a plugin does not depend on a redistributable
        # being installed on the listener's machine.
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    add_subdirectory(libs/JUCE)

    # The typefaces are licensed and not in the repository. See
    # assets/fonts/README.md; CI restores them from secrets.
    set(BMO_FONTS
        ${BMO_ROOT}/assets/fonts/TG-MinervaBlack-Black.otf
        ${BMO_ROOT}/assets/fonts/TG-Blender.otf)

    foreach(font ${BMO_FONTS})
        if(NOT EXISTS ${font})
            message(FATAL_ERROR "Missing font ${font} -- see assets/fonts/README.md")
        endif()
    endforeach()

    juce_add_binary_data(BmoAssets SOURCES ${BMO_FONTS})
    set_target_properties(BmoAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

    # One plugin target per product. The manufacturer code is shared by the
    # whole suite and is an input to the VST3 and AU identity: changing it
    # would orphan every session that references any LT3 Audio plugin. Each
    # product's plugin code is equally permanent. See plans/ and AGENTS.md.
    function(bmo_add_plugin target)
        set(one PRODUCT_NAME PLUGIN_CODE BUNDLE_ID)
        set(multi CATEGORIES SOURCES LIBRARIES)
        cmake_parse_arguments(ARG "" "${one}" "${multi}" ${ARGN})

        juce_add_plugin(${target}
            COMPANY_NAME                "LT3 Audio"
            BUNDLE_ID                   ${ARG_BUNDLE_ID}
            PLUGIN_MANUFACTURER_CODE    LT3a
            PLUGIN_CODE                 ${ARG_PLUGIN_CODE}
            FORMATS                     ${BMO_FORMATS}
            PRODUCT_NAME                ${ARG_PRODUCT_NAME}
            IS_SYNTH                    FALSE
            NEEDS_MIDI_INPUT            FALSE
            NEEDS_MIDI_OUTPUT           FALSE
            IS_MIDI_EFFECT              FALSE
            EDITOR_WANTS_KEYBOARD_FOCUS FALSE
            COPY_PLUGIN_AFTER_BUILD     ${BMO_INSTALL_AFTER_BUILD}
            VST3_CATEGORIES             ${ARG_CATEGORIES}
            AU_MAIN_TYPE                kAudioUnitType_Effect)

        target_sources(${target} PRIVATE ${ARG_SOURCES})

        target_compile_definitions(${target} PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_REPORT_APP_USAGE=0)

        target_link_libraries(${target}
            PRIVATE
                ${ARG_LIBRARIES}
                juce::juce_audio_utils
                juce::juce_dsp
            PUBLIC
                juce::juce_recommended_config_flags
                juce::juce_recommended_lto_flags
                juce::juce_recommended_warning_flags)
    endfunction()

    # A plain executable that uses the shared code without being a plugin:
    # tests and the snapshot tool. Not juce_add_console_app, which forces
    # JUCE_STANDALONE_APPLICATION=1 and collides with the plugin targets.
    function(bmo_add_tool target)
        add_executable(${target} ${ARGN})
        target_compile_definitions(${target} PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_STANDALONE_APPLICATION=1
            JUCE_REPORT_APP_USAGE=0)
        target_link_libraries(${target} PRIVATE
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_gui_basics
            juce::juce_dsp
            juce::juce_events
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
    endfunction()
endif()

# A JUCE-free executable: DSP tests and measurement tools.
function(bmo_add_dsp_tool target)
    add_executable(${target} ${ARGN})
    target_include_directories(${target} PRIVATE ${BMO_ROOT})
    if(NOT BMO_DSP_ONLY)
        target_link_libraries(${target} PRIVATE
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
    endif()
endfunction()

enable_testing()

add_subdirectory(core)
add_subdirectory(modules)
add_subdirectory(products)
add_subdirectory(tools)
add_subdirectory(tests)
