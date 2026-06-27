juce_add_plugin(LadderVoice
    COMPANY_NAME "Independent"
    PRODUCT_NAME "Ladder Voice"
    BUNDLE_ID "com.independent.laddervoice"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS Standalone)

target_sources(LadderVoice
    PRIVATE
        Source/AnalogLookAndFeel.cpp
        Source/PluginEditor.cpp
        Source/PluginProcessor.cpp)

juce_add_binary_data(LadderVoiceAssets
    SOURCES
        Resources/Knobs/knob_large_strip.png
        Resources/Knobs/knob_medium_strip.png
        Resources/Knobs/knob_small_strip.png)

target_include_directories(LadderVoice
    PRIVATE
        ${CMAKE_SOURCE_DIR}/synth_core/Include)

target_link_libraries(LadderVoice
    PRIVATE
        LadderVoiceAssets
        SynthCoreLib
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

target_compile_definitions(LadderVoice
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

set(TRUSTED_BIN_DIR "${CMAKE_SOURCE_DIR}/trusted_bin")
file(MAKE_DIRECTORY "${TRUSTED_BIN_DIR}")

option(LADDERVOICE_SIGN_STANDALONE "Code-sign the standalone executable for local Windows development." ON)
set(LADDERVOICE_CODESIGN_SUBJECT "Ladder Voice Local Dev" CACHE STRING "Certificate subject used to sign the standalone executable.")
set(LADDERVOICE_CODESIGN_PFX "${TRUSTED_BIN_DIR}/LadderVoiceLocalDev.pfx" CACHE FILEPATH "Local development code-signing PFX file.")
set(LADDERVOICE_CODESIGN_PFX_PASSWORD "LadderVoiceLocalDev" CACHE STRING "Password for the local development code-signing PFX file.")
set(LADDERVOICE_SIGNTOOL_EXECUTABLE "" CACHE FILEPATH "Path to signtool.exe.")

if(WIN32 AND LADDERVOICE_SIGN_STANDALONE AND NOT LADDERVOICE_SIGNTOOL_EXECUTABLE)
    file(GLOB _laddervoice_signtool_candidates
        LIST_DIRECTORIES false
        "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/signtool.exe")
    list(SORT _laddervoice_signtool_candidates COMPARE NATURAL ORDER DESCENDING)
    if(_laddervoice_signtool_candidates)
        list(GET _laddervoice_signtool_candidates 0 LADDERVOICE_SIGNTOOL_EXECUTABLE)
    endif()
endif()

set_target_properties(LadderVoice_Standalone
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${TRUSTED_BIN_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${TRUSTED_BIN_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${TRUSTED_BIN_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${TRUSTED_BIN_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${TRUSTED_BIN_DIR}")

add_custom_command(TARGET LadderVoice_Standalone POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${TRUSTED_BIN_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:LadderVoice_Standalone>"
            "${TRUSTED_BIN_DIR}/LadderVoice.exe"
    COMMENT "Creating no-space trusted launch copy: ${TRUSTED_BIN_DIR}/LadderVoice.exe")

if(WIN32 AND LADDERVOICE_SIGN_STANDALONE)
    if(NOT LADDERVOICE_SIGNTOOL_EXECUTABLE)
        message(FATAL_ERROR "LADDERVOICE_SIGN_STANDALONE is ON, but signtool.exe was not found. Install the Windows SDK or set LADDERVOICE_SIGNTOOL_EXECUTABLE.")
    endif()

    add_custom_command(TARGET LadderVoice_Standalone POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
                "-DSIGNTOOL_EXE=${LADDERVOICE_SIGNTOOL_EXECUTABLE}"
                "-DCERT_SUBJECT=${LADDERVOICE_CODESIGN_SUBJECT}"
                "-DCERT_PFX=${LADDERVOICE_CODESIGN_PFX}"
                "-DCERT_PFX_PASSWORD=${LADDERVOICE_CODESIGN_PFX_PASSWORD}"
                "-DFILE_TO_SIGN=$<TARGET_FILE:LadderVoice_Standalone>"
                -P "${CMAKE_SOURCE_DIR}/cmake/SignWindowsExecutable.cmake"
        COMMAND "${CMAKE_COMMAND}"
                "-DSIGNTOOL_EXE=${LADDERVOICE_SIGNTOOL_EXECUTABLE}"
                "-DCERT_SUBJECT=${LADDERVOICE_CODESIGN_SUBJECT}"
                "-DCERT_PFX=${LADDERVOICE_CODESIGN_PFX}"
                "-DCERT_PFX_PASSWORD=${LADDERVOICE_CODESIGN_PFX_PASSWORD}"
                "-DFILE_TO_SIGN=${TRUSTED_BIN_DIR}/LadderVoice.exe"
                -P "${CMAKE_SOURCE_DIR}/cmake/SignWindowsExecutable.cmake"
        COMMENT "Signing trusted standalone executables with '${LADDERVOICE_CODESIGN_SUBJECT}'")
endif()
