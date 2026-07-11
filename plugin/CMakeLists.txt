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
    FORMATS Standalone VST3)

target_sources(LadderVoice
    PRIVATE
        Source/AnalogLookAndFeel.cpp
        Source/PluginEditor.cpp
        Source/PluginProcessor.cpp
        Source/PresetManager.cpp
        Source/PresetBrowser.cpp
        Source/Typography.cpp)

juce_add_binary_data(LadderVoiceAssets
    SOURCES
        Resources/Knobs/knob_large_strip.png
        Resources/Knobs/knob_medium_strip.png
        Resources/Knobs/knob_small_strip.png)

# Embedded UI typefaces (Archivo + IBM Plex, SIL OFL). Optional: Typography.cpp
# falls back to platform fonts if Resources/Fonts is ever empty, so the build
# remains usable without the intended instrument typefaces.
file(GLOB LADDERVOICE_FONT_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Fonts/*.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Fonts/*.otf")
if(LADDERVOICE_FONT_FILES)
    # Distinct NAMESPACE/HEADER_NAME so this target's generated header never
    # collides with LadderVoiceAssets' own BinaryData.h/BinaryData:: symbols.
    juce_add_binary_data(LadderVoiceFonts
        NAMESPACE FontData
        HEADER_NAME FontBinaryData.h
        SOURCES
            ${LADDERVOICE_FONT_FILES})
endif()

# Embedded factory preset library (Resources/... style bundling, but sourced
# from the top-level presets/ tree so preset authoring stays plain .ladderpreset
# XML files on disk — no hand-typed C++ literals). Compiled straight into the
# plugin binary via the same juce_add_binary_data mechanism already used for
# fonts, so every end user gets the full library with zero install/import step.
file(GLOB_RECURSE LADDERVOICE_PRESET_FILES
    "${CMAKE_SOURCE_DIR}/presets/*.ladderpreset")
if(LADDERVOICE_PRESET_FILES)
    # Distinct NAMESPACE/HEADER_NAME: avoids colliding with both the plain
    # BinaryData:: symbols (LadderVoiceAssets) and the PresetData struct
    # already declared in PresetManager.h.
    juce_add_binary_data(LadderVoicePresetData
        NAMESPACE PresetBinaryData
        HEADER_NAME PresetBinaryData.h
        SOURCES
            ${LADDERVOICE_PRESET_FILES})
endif()

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

if(LADDERVOICE_FONT_FILES)
    target_link_libraries(LadderVoice PRIVATE LadderVoiceFonts)
    target_compile_definitions(LadderVoice PRIVATE LADDERVOICE_HAS_EMBEDDED_FONTS=1)
endif()

if(LADDERVOICE_PRESET_FILES)
    target_link_libraries(LadderVoice PRIVATE LadderVoicePresetData)
    target_compile_definitions(LadderVoice PRIVATE LADDERVOICE_HAS_EMBEDDED_PRESETS=1)
endif()

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
        OUTPUT_NAME "LadderVoice"
        RUNTIME_OUTPUT_DIRECTORY "${TRUSTED_BIN_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${TRUSTED_BIN_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${TRUSTED_BIN_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${TRUSTED_BIN_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${TRUSTED_BIN_DIR}")

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
        COMMENT "Signing trusted standalone executable with '${LADDERVOICE_CODESIGN_SUBJECT}'")
endif()

# Remove Mark-of-the-Web (Zone.Identifier ADS) so Smart App Control / SmartScreen
# does not treat locally-built executables as downloaded-from-internet.
# Two COMMAND entries — one per file — keep the inline script simple and avoid
# quoting issues with paths that contain spaces.
if(WIN32)
    add_custom_command(TARGET LadderVoice_Standalone POST_BUILD
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -Command
                "& { $f='$<TARGET_FILE:LadderVoice_Standalone>'; if(Test-Path -LiteralPath $f){Remove-Item -LiteralPath ($f+':Zone.Identifier') -ErrorAction SilentlyContinue; Unblock-File -LiteralPath $f -ErrorAction SilentlyContinue} }"
        COMMENT "Removing Mark-of-the-Web from standalone executable")

    add_custom_command(TARGET LadderVoice_Standalone POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${CMAKE_CURRENT_BINARY_DIR}/LadderVoice_artefacts/$<CONFIG>/Standalone"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:LadderVoice_Standalone>"
                "${CMAKE_CURRENT_BINARY_DIR}/LadderVoice_artefacts/$<CONFIG>/Standalone/Ladder Voice.exe"
        COMMENT "Updating legacy standalone launch path")
endif()
