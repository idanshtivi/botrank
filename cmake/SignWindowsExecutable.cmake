if(NOT DEFINED SIGNTOOL_EXE OR SIGNTOOL_EXE STREQUAL "")
    message(FATAL_ERROR "SIGNTOOL_EXE is required.")
endif()

if(NOT DEFINED CERT_SUBJECT OR CERT_SUBJECT STREQUAL "")
    message(FATAL_ERROR "CERT_SUBJECT is required.")
endif()

if(NOT DEFINED FILE_TO_SIGN OR FILE_TO_SIGN STREQUAL "")
    message(FATAL_ERROR "FILE_TO_SIGN is required.")
endif()

if(NOT EXISTS "${SIGNTOOL_EXE}")
    message(FATAL_ERROR "signtool.exe was not found: ${SIGNTOOL_EXE}")
endif()

if(NOT EXISTS "${FILE_TO_SIGN}")
    message(FATAL_ERROR "Cannot sign missing executable: ${FILE_TO_SIGN}")
endif()

set(_cert_args /n "${CERT_SUBJECT}")
if(DEFINED CERT_PFX AND NOT CERT_PFX STREQUAL "" AND EXISTS "${CERT_PFX}")
    set(_cert_args /f "${CERT_PFX}")
    if(DEFINED CERT_PFX_PASSWORD AND NOT CERT_PFX_PASSWORD STREQUAL "")
        list(APPEND _cert_args /p "${CERT_PFX_PASSWORD}")
    endif()
endif()

execute_process(
    COMMAND "${SIGNTOOL_EXE}" sign ${_cert_args} /fd SHA256 "${FILE_TO_SIGN}"
    RESULT_VARIABLE _sign_result
    OUTPUT_VARIABLE _sign_output
    ERROR_VARIABLE _sign_error)

if(NOT _sign_result EQUAL 0)
    message(FATAL_ERROR
        "Failed to sign ${FILE_TO_SIGN} with certificate '${CERT_SUBJECT}'.\n"
        "Run scripts/Install-LadderVoiceDevCertificate.ps1 once, then rebuild.\n"
        "${_sign_output}\n${_sign_error}")
endif()

message(STATUS "Signed: ${FILE_TO_SIGN}")
