cmake_minimum_required(VERSION 3.22)
project(SpectralStrip VERSION 0.8.4 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

# JUCE source tree, patched with patches/juce-au-automatable-parameter-list.patch
# (scripts/generate-xcode.command applies the patch automatically).
set(JUCE_PATH "/Applications/JUCE" CACHE PATH "JUCE source directory")
if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found: set JUCE_PATH to your JUCE source directory.")
endif()
file(READ "${JUCE_PATH}/modules/juce_audio_plugin_client/juce_audio_plugin_client_AU_1.mm" SPECTRAL_AU_WRAPPER)
if(NOT SPECTRAL_AU_WRAPPER MATCHES "SpectralStrip: invalidate atomically")
    message(FATAL_ERROR "JUCE is missing patches/juce-au-automatable-parameter-list.patch. "
                        "Apply it, or run scripts/generate-xcode.command.")
endif()
add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")

set(SPECTRAL_DSP_SOURCES
    Source/SpectralEngine.h
    Source/ShimmerReverb.h
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp)
set(SPECTRAL_UI_SOURCES
    Source/PluginEditor.h
    Source/PluginEditor.cpp
    Source/UI/Theme.h
    Source/UI/EditorFonts.h
    Source/UI/StripLookAndFeel.h
    Source/UI/StripLookAndFeel.cpp
    Source/UI/Controls.h
    Source/UI/ModuleSpecs.h
    Source/UI/ModuleArt.h
    Source/UI/ModuleArt.cpp
    Source/UI/SpectrumView.h
    Source/UI/SpectrumView.cpp
    Source/UI/ModuleStrip.h
    Source/UI/ModuleStrip.cpp)
set(SPECTRAL_DEFINITIONS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)

#==============================================================================
# The plug-in: AU, VST3 and a standalone app.
juce_add_plugin(SpectralStrip
    COMPANY_NAME "Giorgio Audio"
    BUNDLE_ID "audio.giorgio.spectralstrip"
    PLUGIN_MANUFACTURER_CODE GiAu
    PLUGIN_CODE SpSt
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Spectral Strip"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Spectral Strip needs audio input to process sound.")
target_sources(SpectralStrip PRIVATE ${SPECTRAL_DSP_SOURCES} ${SPECTRAL_UI_SOURCES})
target_compile_definitions(SpectralStrip PUBLIC ${SPECTRAL_DEFINITIONS} JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(SpectralStrip
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

#==============================================================================
# Tests: a console runner (also used by ctest) and an editor preview app.
option(SPECTRAL_BUILD_TESTS "Build the test runner and the editor preview" ON)
if(SPECTRAL_BUILD_TESTS)
    juce_add_console_app(SpectralStripTests PRODUCT_NAME "SpectralStripTests")
    target_sources(SpectralStripTests PRIVATE
        Tests/TestEngine.h
        Tests/EngineTests.cpp
        Tests/SmearTiltTests.cpp
        Tests/ProcessorTests.cpp
        Tests/AuditTests.cpp
        Tests/ReverbTests.cpp
        ${SPECTRAL_DSP_SOURCES} ${SPECTRAL_UI_SOURCES})
    target_compile_definitions(SpectralStripTests PRIVATE ${SPECTRAL_DEFINITIONS})
    target_link_libraries(SpectralStripTests PRIVATE juce::juce_dsp juce::juce_audio_utils juce::juce_recommended_config_flags)
    enable_testing()
    add_test(NAME SpectralStrip COMMAND SpectralStripTests)

    juce_add_gui_app(SpectralStripPreview PRODUCT_NAME "Spectral Strip Preview" BUNDLE_ID "audio.giorgio.spectralstrip.preview")
    target_sources(SpectralStripPreview PRIVATE Tests/EditorPreview.cpp ${SPECTRAL_DSP_SOURCES} ${SPECTRAL_UI_SOURCES})
    target_compile_definitions(SpectralStripPreview PRIVATE ${SPECTRAL_DEFINITIONS})
    target_link_libraries(SpectralStripPreview PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
