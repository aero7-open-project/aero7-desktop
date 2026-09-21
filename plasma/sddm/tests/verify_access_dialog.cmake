if(NOT DEFINED THEME_MAIN)
    message(FATAL_ERROR "THEME_MAIN is required")
endif()
if(NOT DEFINED THEME_CONFIG)
    message(FATAL_ERROR "THEME_CONFIG is required")
endif()

file(READ "${THEME_MAIN}" theme)
file(READ "${THEME_CONFIG}" theme_config)

foreach(required
        "id: accessDialog"
        "parent: root"
        "Hear text on screen read aloud (Narrator)"
        "Make items on the screen larger (Magnifier)"
        "See more contrast in colors (High Contrast)"
        "Type without the keyboard (On-Screen Keyboard)"
        "Press keyboard shortcuts one key at a time (Sticky Keys)"
        "Ignore brief or repeated keystrokes (Filter Keys)"
        "Desktop environment:"
        "/usr/bin/aero7-sddm-accessibility status"
        "/usr/bin/aero7-sddm-accessibility apply "
        "inputPanel.setActive(pendingKeyboard)"
        "Qt.callLater(function() { inputPanel.setActive(true) })"
        "session.index = desktopEnvironmentCombo.index"
        "arrowIcon: Qt.resolvedUrl(\"Assets/power-glyph-arrow.png\")"
        "Keys.onReturnPressed: function(event)"
        "scale: root.magnifierEnabled ? 1.18 : 1.0"
        "visible: !root.highContrastEnabled")
    string(FIND "${theme}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "SDDM Ease of Access contract is missing: ${required}")
    endif()
endforeach()

if(theme MATCHES "Keys\\.onReturnPressed:[ \t\r\n]*\\{")
    message(FATAL_ERROR "The SDDM theme still contains a Qt 6 implicit Return handler")
endif()

file(READ "${CMAKE_CURRENT_LIST_DIR}/../sddm-theme-mod/SMOD/ComboBox.qml" combo_box)
foreach(required
        "onWheel: function(wheel)"
        "Keys.onPressed: function(event)")
    string(FIND "${combo_box}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "SDDM ComboBox Qt 6 handler contract is missing: ${required}")
    endif()
endforeach()

if(theme MATCHES "else session\\.open\\(\\)")
    message(FATAL_ERROR "The access button still opens the old flat session menu")
endif()

string(FIND "${theme_config}"
    "GreeterEnvironment=QML_DISABLE_DISTANCEFIELD=1,QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1,QT_WAYLAND_SHELL_INTEGRATION=layer-shell"
    environment_found)
if(environment_found EQUAL -1)
    message(FATAL_ERROR "The installed SDDM environment does not enable accessibility and the Wayland shell")
endif()

message(STATUS "Aero7 SDDM Ease of Access dialog contract checks passed")
