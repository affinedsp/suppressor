juce_add_binary_data(SuppressorAssets
    NAMESPACE SuppressorAssets
    HEADER_NAME SuppressorAssets.h
    SOURCES
        ../assets/fonts/BarlowCondensed-Regular.ttf
        ../assets/fonts/BarlowCondensed-SemiBold.ttf)

juce_add_plugin(Suppressor
    VERSION 0.1.0
    COMPANY_NAME "Suppressor Audio"
    COMPANY_WEBSITE "https://github.com/affinedsp/suppressor"
    COMPANY_EMAIL "dev@suppressor.audio"
    PLUGIN_MANUFACTURER_CODE SpAu
    PLUGIN_CODE Supr
    FORMATS VST3 Standalone
    PRODUCT_NAME "suppressor"
    DESCRIPTION "Zero-latency frequency-split noise suppressor for guitar DI"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics)

target_sources(Suppressor PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
    PluginLookAndFeel.cpp)

target_compile_definitions(Suppressor PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(Suppressor PRIVATE
    SuppressorAssets
    suppressor-dsp
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

target_compile_features(Suppressor PRIVATE cxx_std_17)

if (SUPPRESSOR_ENABLE_CLAP)
    clap_juce_extensions_plugin(TARGET Suppressor
        CLAP_ID "audio.suppressor.suppressor"
        CLAP_FEATURES audio-effect
        CLAP_PROCESS_EVENTS_RESOLUTION_SAMPLES 16
        CLAP_ALWAYS_JUCE_BRIDGE TRUE)
endif()

if (SUPPRESSOR_BUILD_TESTS)
    add_executable(SuppressorUITests)

    target_sources(SuppressorUITests PRIVATE
        ../Tests/test_ui_main.cpp
        ../Tests/test_ui.cpp)

    target_include_directories(SuppressorUITests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})

    target_link_libraries(SuppressorUITests PRIVATE
        Suppressor
        doctest::doctest
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

    target_compile_definitions(SuppressorUITests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_PLUGINHOST_VST3=1
        $<$<PLATFORM_ID:Windows>:_CONSOLE=1>)

    # Keep assertion context in CTest's failure log, including native-window exits.
    add_test(NAME suppressor-ui-unit COMMAND SuppressorUITests --success)
    set_tests_properties(suppressor-ui-unit PROPERTIES ENVIRONMENT
        "SUPPRESSOR_VST3_PATH=$<TARGET_GENEX_EVAL:Suppressor_VST3,$<TARGET_PROPERTY:Suppressor_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>")
    add_dependencies(SuppressorUITests Suppressor_VST3)
    if (SUPPRESSOR_ENABLE_CLAP)
        target_sources(SuppressorUITests PRIVATE ../Tests/test_clap_gui.cpp)
        target_link_libraries(SuppressorUITests PRIVATE clap-core)
        add_dependencies(SuppressorUITests Suppressor_CLAP)
        if (APPLE)
            set(clap_test_path "$<TARGET_BUNDLE_DIR:Suppressor_CLAP>")
        else()
            set(clap_test_path "$<TARGET_FILE:Suppressor_CLAP>")
        endif()
        set_property(TEST suppressor-ui-unit APPEND PROPERTY ENVIRONMENT
            "SUPPRESSOR_CLAP_PATH=${clap_test_path}")
    endif()
endif()
