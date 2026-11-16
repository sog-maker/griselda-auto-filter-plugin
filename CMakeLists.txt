cmake_minimum_required(VERSION 3.22)
project(GriseldaAutoFilter VERSION 2.2.3 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (MSVC)
    add_compile_options(/permissive- /W4 /bigobj)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    message(FATAL_ERROR "JUCE wurde nicht gefunden. Bitte zuerst JUCE nach ./JUCE klonen.")
endif()

juce_add_plugin(GriseldaAutoFilter
    VERSION 2.2.3
    COMPANY_NAME "Schlensog Audio Lab"
    BUNDLE_ID "com.schlensogaudiolab.griseldaautofilter"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    NEEDS_WEBVIEW2 TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Mslg
    PLUGIN_CODE Gaf2
    FORMATS VST3 Standalone
    PRODUCT_NAME "Griselda Auto Filter"
    VST3_CATEGORIES Fx Filter)

juce_generate_juce_header(GriseldaAutoFilter)

file(GLOB_RECURSE griselda_ui_files CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ui/dist-juce/*")
if (NOT griselda_ui_files)
    message(FATAL_ERROR "ui/dist-juce fehlt. Bitte zuerst im Ordner ui ausfuehren: VITE_JUCE_BUILD=1 pnpm build")
endif()
juce_add_binary_data(GriseldaWebUI SOURCES ${griselda_ui_files})

target_sources(GriseldaAutoFilter PRIVATE
    PluginProcessor.h
    PluginProcessor.cpp
    PluginEditor.h
    PluginEditor.cpp)

target_compile_definitions(GriseldaAutoFilter
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_MODAL_LOOPS_PERMITTED=1)

target_link_libraries(GriseldaAutoFilter
    PRIVATE
        GriseldaWebUI
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
