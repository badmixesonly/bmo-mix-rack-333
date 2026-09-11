cmake_minimum_required(VERSION 3.22)
project(BMOTuneRT VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# BMO Tune RT is an LT3 Audio product with a repository of its own, not a
# rack module. It reuses the suite's shared code rather than forking it, and
# that code comes from Kevin's main, pinned as a submodule at libs/bmo-mix-rack
# -- the same way the rack pins JUCE. Reading it from a sibling working copy
# instead would build against whatever branch that copy had checked out,
# which on 2026-09-10 was three different branches in one afternoon.
set(BMO_RACK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/bmo-mix-rack"
    CACHE PATH "BMO Mix Rack (Kevin's main), for the shared core/ code")
get_filename_component(BMO_RACK_DIR "${BMO_RACK_DIR}" ABSOLUTE)

if(NOT EXISTS "${BMO_RACK_DIR}/core/state/ParamSpec.h")
    message(FATAL_ERROR
        "BMO Mix Rack not found at ${BMO_RACK_DIR}\n"
        "BMO Tune RT reuses the suite's shared code from Kevin's main, pinned "
        "as a submodule. Fetch it with:\n"
        "    git submodule update --init libs/bmo-mix-rack\n"
        "(not --recursive: JUCE is only needed by the plugin wrapper).")
endif()

message(STATUS "BMO Mix Rack shared code: ${BMO_RACK_DIR}")

# Same switch, same meaning as the rack's: the DSP, its tests and every
# measurement tool build with no framework at all. The plugin wrapper is the
# only thing that needs JUCE, and it is off in this mode.
option(BMO_DSP_ONLY "Build only the JUCE-free DSP, tests and tools" ON)

# Two include roots, and they never collide: this repository owns modules/tune
# and tools/, the rack owns core/.
set(BMO_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(BMO_INCLUDE_ROOTS ${BMO_ROOT} ${BMO_RACK_DIR})

# Warnings as the rack gets them from juce_recommended_warning_flags, without
# needing JUCE to get them.
function(bmo_tune_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8
            /wd4100)   # unreferenced parameter: interface overrides
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wshadow -Wconversion
            -Wno-sign-conversion -Wno-unused-parameter)
    endif()
endfunction()

# A JUCE-free executable: DSP tests and measurement tools.
function(bmo_add_dsp_tool target)
    add_executable(${target} ${ARGN})
    target_include_directories(${target} PRIVATE ${BMO_INCLUDE_ROOTS})
    bmo_tune_warnings(${target})
    if(MSVC)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
endfunction()

# Sanitizers for the offline harness (spec T-1). MSVC has ASan only; UBSan,
# TSan and RealtimeSanitizer need clang and are wired for when one is used.
set(BMO_SANITIZE "" CACHE STRING "address | undefined | thread | realtime")
if(BMO_SANITIZE)
    if(MSVC)
        if(BMO_SANITIZE STREQUAL "address")
            add_compile_options(/fsanitize=address /Zi)
            add_link_options(/INCREMENTAL:NO)
        else()
            message(FATAL_ERROR "MSVC supports BMO_SANITIZE=address only")
        endif()
    else()
        add_compile_options(-fsanitize=${BMO_SANITIZE} -fno-omit-frame-pointer -g)
        add_link_options(-fsanitize=${BMO_SANITIZE})
    endif()
endif()

#==============================================================================
# The plugin: JUCE, the licensed fonts, and the suite's shared core, all from
# Kevin's main -- the same set-up as the rack's own CMakeLists.txt, which this
# follows line for line where it can. Off in BMO_DSP_ONLY.
#==============================================================================
if(NOT BMO_DSP_ONLY)
    if(NOT EXISTS "${BMO_RACK_DIR}/libs/JUCE/CMakeLists.txt")
        message(FATAL_ERROR
            "JUCE not found at ${BMO_RACK_DIR}/libs/JUCE\n"
            "It is the rack's own submodule, pinned by Kevin's main. From a local "
            "rack working copy that already has that commit (no download):\n"
            "    git -C libs/bmo-mix-rack -c protocol.file.allow=always "
            "-c submodule.libs/JUCE.url=<rack working copy>/libs/JUCE "
            "submodule update --init --no-recommend-shallow libs/JUCE\n"
            "or from GitHub:\n"
            "    git -C libs/bmo-mix-rack submodule update --init libs/JUCE")
    endif()

    # Static runtime, so a plugin does not depend on a redistributable being
    # installed on the listener's machine. Set before any target, so the DSP
    # library the plugin links is built against the same runtime.
    if(MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    add_subdirectory(${BMO_RACK_DIR}/libs/JUCE ${CMAKE_BINARY_DIR}/libs/JUCE)

    # The typefaces are licensed to individuals, not to the project, so the
    # .otf files live outside every repository. First hit wins, as in the
    # rack: -DBMO_FONT_DIR, then $ENV{BMO_FONT_DIR}, then this working copy's
    # gitignored .bmo-fontdir. See the rack's assets/fonts/README.md.
    set(BMO_FONT_DIR "" CACHE PATH "Directory holding the licensed .otf files")

    if(BMO_FONT_DIR)
        set(bmo_font_dir "${BMO_FONT_DIR}")
    elseif(DEFINED ENV{BMO_FONT_DIR})
        set(bmo_font_dir "$ENV{BMO_FONT_DIR}")
    elseif(EXISTS ${BMO_ROOT}/.bmo-fontdir)
        file(READ ${BMO_ROOT}/.bmo-fontdir bmo_font_dir)
        string(STRIP "${bmo_font_dir}" bmo_font_dir)
    else()
        message(FATAL_ERROR
            "No licensed-font folder recorded for this working copy.\n"
            "Write its path into ${BMO_ROOT}/.bmo-fontdir (gitignored), or pass "
            "-DBMO_FONT_DIR=<path>. The .otf files are never committed.")
    endif()

    file(TO_CMAKE_PATH "${bmo_font_dir}" bmo_font_dir)
    set(BMO_FONTS
        ${bmo_font_dir}/TG-MinervaBlack-Black.otf
        ${bmo_font_dir}/TG-Blender.otf)

    foreach(font ${BMO_FONTS})
        if(NOT EXISTS ${font})
            message(FATAL_ERROR "Missing licensed font: ${font}")
        endif()
    endforeach()

    message(STATUS "BMO fonts: ${bmo_font_dir}")

    juce_add_binary_data(BmoAssets SOURCES ${BMO_FONTS})
    set_target_properties(BmoAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

    # The suite's shared layer, compiled into this product the way the rack
    # compiles bmo_core into each of its own: an INTERFACE library with
    # sources, so no product's JucePlugin_* definitions leak into another.
    # The rack's list minus core/rack/, which is the rack's product and pulls
    # in its module registry.
    add_library(bmo_tune_core INTERFACE)
    target_include_directories(bmo_tune_core INTERFACE ${BMO_INCLUDE_ROOTS})
    target_sources(bmo_tune_core INTERFACE
        ${BMO_RACK_DIR}/core/state/PresetManager.cpp
        ${BMO_RACK_DIR}/core/ui/Tokens.cpp
        ${BMO_RACK_DIR}/core/ui/Fonts.cpp
        ${BMO_RACK_DIR}/core/ui/LookAndFeel.cpp
        ${BMO_RACK_DIR}/core/ui/Controls.cpp
        ${BMO_RACK_DIR}/core/ui/ModulePanel.cpp
        ${BMO_RACK_DIR}/core/ui/PresetBar.cpp
        ${BMO_RACK_DIR}/core/product/SingleModuleProcessor.cpp
        ${BMO_RACK_DIR}/core/product/ProductEditor.cpp)
    target_link_libraries(bmo_tune_core INTERFACE BmoAssets)

    # A plain executable that uses the shared code without being a plugin --
    # the snapshot tool. As the rack's bmo_add_tool, and for its reason: not
    # juce_add_console_app, which forces JUCE_STANDALONE_APPLICATION=1 and
    # collides with the plugin target.
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

enable_testing()

add_subdirectory(modules)
add_subdirectory(tools)
add_subdirectory(tests)

if(NOT BMO_DSP_ONLY)
    add_subdirectory(products/tune)
endif()
