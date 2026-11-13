cmake_minimum_required(VERSION 3.22)

# macOS: universal binary (Apple Silicon + Intel), macOS 11.0 or later. Ignored on Windows.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
if(NOT DEFINED CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
endif()

project(PON VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime: the plugin works without the VC++ Redistributable (drop-in ZIP distribution).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(PON_BUILD_TESTS "Build the offline render/verification tool" OFF)

# JUCE 8.0.4 (same as HazePad / Cheapbeat). Reuse HazePad's already-downloaded copy read-only if present,
# otherwise download the same zip.
set(_hazepad_juce "${CMAKE_CURRENT_SOURCE_DIR}/../hazepad/build/_deps/juce-src")
if(NOT DEFINED FETCHCONTENT_SOURCE_DIR_JUCE AND EXISTS "${_hazepad_juce}/CMakeLists.txt")
    set(FETCHCONTENT_SOURCE_DIR_JUCE "${_hazepad_juce}" CACHE PATH "JUCE source dir")
endif()

include(FetchContent)
FetchContent_Declare(
    JUCE
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.4.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(PON_FORMATS VST3 AU)
else()
    set(PON_FORMATS VST3)
endif()

juce_add_plugin(PON
    COMPANY_NAME "PON"
    PRODUCT_NAME "PON"
    BUNDLE_ID "com.pon.pon"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Pnfx
    PLUGIN_CODE Pon1
    AU_MAIN_TYPE kAudioUnitType_Effect
    FORMATS ${PON_FORMATS}
    VST3_CATEGORIES Fx Modulation Delay)

set(PON_SOURCES
    Source/PresetData.cpp
    Source/FactoryPresets.cpp
    Source/BandFx.cpp
    Source/SyncDelay.cpp
    Source/Engine.cpp
    Source/Params.cpp
    Source/PluginProcessor.cpp
    Source/PadComponent.cpp
    Source/DetailPanel.cpp
    Source/PluginEditor.cpp)

target_sources(PON PRIVATE ${PON_SOURCES})

target_compile_definitions(PON PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

if(MSVC)
    target_compile_options(PON PRIVATE /utf-8)
endif()

target_link_libraries(PON
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(PON_BUILD_TESTS)
    juce_add_console_app(PONRenderTest PRODUCT_NAME "PONRenderTest")
    target_sources(PONRenderTest PRIVATE Tools/RenderTest.cpp ${PON_SOURCES})
    target_include_directories(PONRenderTest PRIVATE Source)
    target_compile_definitions(PONRenderTest PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    if(MSVC)
        target_compile_options(PONRenderTest PRIVATE /utf-8)
    endif()
    target_link_libraries(PONRenderTest
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()
