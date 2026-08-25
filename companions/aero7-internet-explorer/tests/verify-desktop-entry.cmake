if(NOT DEFINED DESKTOP_FILE)
    message(FATAL_ERROR "DESKTOP_FILE is required")
endif()

file(READ "${DESKTOP_FILE}" desktop_entry)

foreach(required_line
        "Name=Internet Explorer"
        "Exec=aero7-internet-explorer %U"
        "StartupNotify=false")
    string(FIND "${desktop_entry}" "${required_line}" line_position)
    if(line_position EQUAL -1)
        message(FATAL_ERROR "Missing required desktop-entry line: ${required_line}")
    endif()
endforeach()

string(FIND "${desktop_entry}" "StartupNotify=true" stale_startup_notify)
if(NOT stale_startup_notify EQUAL -1)
    message(FATAL_ERROR "Wrapper must not create a separate startup task")
endif()
