cmake_minimum_required(VERSION 3.22)

project(TunersKick VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE pobiera sie automatycznie przy pierwszej konfiguracji (potrzebny git i internet).
# Nowsza wersje mozna wpisac w GIT_TAG (lista tagow: github.com/juce-framework/JUCE/tags).
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(TunersKick
    COMPANY_NAME "Tuners"
    PLUGIN_MANUFACTURER_CODE Tnrs
    PLUGIN_CODE Tkck
    FORMATS VST3 Standalone
    PRODUCT_NAME "Tuners Kick"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    # FALSE, bo kopiowanie do Program Files wymaga uprawnien administratora
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth)

target_sources(TunersKick PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/KickEngine.h)

target_compile_definitions(TunersKick PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(TunersKick
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
