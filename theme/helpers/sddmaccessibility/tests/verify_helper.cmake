if(NOT DEFINED HELPER OR NOT DEFINED TEST_ROOT)
    message(FATAL_ERROR "HELPER and TEST_ROOT are required")
endif()

file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}/config" "${TEST_ROOT}/runtime")

file(READ "${CMAKE_CURRENT_LIST_DIR}/../speech-dispatcher-aero7.conf" speech_dropin)
if(NOT speech_dropin MATCHES "KillSignal=SIGINT")
    message(FATAL_ERROR "Speech Dispatcher must use the VM-verified clean SIGINT shutdown")
endif()

set(test_environment
    "XDG_CONFIG_HOME=${TEST_ROOT}/config"
    "XDG_RUNTIME_DIR=${TEST_ROOT}/runtime")

execute_process(
    COMMAND ${CMAKE_COMMAND} -E env ${test_environment} "${HELPER}" status
    RESULT_VARIABLE status_result
    OUTPUT_VARIABLE status_output
    ERROR_VARIABLE status_error)
if(NOT status_result EQUAL 0 OR NOT status_output MATCHES "\\\"stickyKeys\\\":false")
    message(FATAL_ERROR "Clean status failed: ${status_result} ${status_output} ${status_error}")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} -E env ${test_environment} "${HELPER}" apply 0 1 1
    RESULT_VARIABLE apply_result
    OUTPUT_VARIABLE apply_output
    ERROR_VARIABLE apply_error)
if(NOT apply_result EQUAL 0
   OR NOT apply_output MATCHES "\\\"stickyKeys\\\":true"
   OR NOT apply_output MATCHES "\\\"filterKeys\\\":true")
    message(FATAL_ERROR "Apply failed: ${apply_result} ${apply_output} ${apply_error}")
endif()

file(READ "${TEST_ROOT}/config/kaccessrc" config_contents)
foreach(expected
        "StickyKeys=true"
        "StickyKeysLatch=true"
        "SlowKeys=true"
        "SlowKeysDelay=500"
        "BounceKeys=true"
        "BounceKeysDelay=500"
        "Enabled=false")
    if(NOT config_contents MATCHES "${expected}")
        message(FATAL_ERROR "Missing ${expected} in kaccessrc:\n${config_contents}")
    endif()
endforeach()

execute_process(
    COMMAND ${CMAKE_COMMAND} -E env ${test_environment} "${HELPER}" apply 0 2 0
    RESULT_VARIABLE invalid_result
    OUTPUT_VARIABLE invalid_output)
if(invalid_result EQUAL 0 OR NOT invalid_output MATCHES "must be 0 or 1")
    message(FATAL_ERROR "Invalid boolean was accepted: ${invalid_result} ${invalid_output}")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} -E env ${test_environment} "${HELPER}" apply 0 0 0
    RESULT_VARIABLE reset_result
    OUTPUT_VARIABLE reset_output)
if(NOT reset_result EQUAL 0
   OR NOT reset_output MATCHES "\\\"stickyKeys\\\":false"
   OR NOT reset_output MATCHES "\\\"filterKeys\\\":false")
    message(FATAL_ERROR "Reset failed: ${reset_result} ${reset_output}")
endif()

message(STATUS "Aero7 SDDM accessibility helper state/apply/reset checks passed")
