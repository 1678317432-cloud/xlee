cmake_minimum_required(VERSION 3.22)

project(SuperBass VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../external/JUCE JUCE)

juce_add_plugin(SuperBass
    COMPANY_NAME "Xlee Audio"
    BUNDLE_ID "com.xleeaudio.superbass"
    PLUGIN_MANUFACTURER_CODE Xlee
    PLUGIN_CODE SuBs
    FORMATS AU VST3
    PRODUCT_NAME "Super Bass"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_AUTO_MANIFEST FALSE
)

target_sources(SuperBass
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_compile_definitions(SuperBass
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SuperBass
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
