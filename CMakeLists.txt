cmake_minimum_required(VERSION 3.22)
project(BMOTuneRT VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# BMO Tune RT is an LT3 Audio product but not a rack module: it is monophonic,
# takes MIDI, and has a latency contract the rack's slot model cannot carry.
# It still reuses the suite's shared code rather than forking it, so the
# JUCE-free headers it needs (core/dsp/ModuleDsp.h, core/state/ParamSpec.h)
# come straight out of a BMO Mix Rack checkout. Once this repository has a
# remote, that checkout becomes a submodule under libs/; until then it is the
# sibling folder.
set(BMO_RACK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../bmo-mix-rack-333"
    CACHE PATH "A BMO Mix Rack checkout, for the shared core/ headers")
get_filename_component(BMO_RACK_DIR "${BMO_RACK_DIR}" ABSOLUTE)

if(NOT EXISTS "${BMO_RACK_DIR}/core/state/ParamSpec.h")
    message(FATAL_ERROR
        "BMO Mix Rack not found at ${BMO_RACK_DIR}\n"
        "BMO Tune RT reuses the suite's shared JUCE-free headers. Point the "
        "build at a checkout with -DBMO_RACK_DIR=<path>.")
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

enable_testing()

add_subdirectory(modules)
add_subdirectory(tools)
add_subdirectory(tests)
