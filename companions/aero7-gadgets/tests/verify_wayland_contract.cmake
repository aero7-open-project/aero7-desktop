# KWindowSystem receives showingDesktopChanged through this restricted
# protocol. The executable's desktop entry must request it from KWin.
foreach(entry IN ITEMS org.aero7.GadgetHost org.aero7.GadgetGallery org.aero7.GadgetHost-autostart)
    file(STRINGS "${SOURCE_DIR}/data/${entry}.desktop" declared_interfaces
        REGEX "^X-KDE-Wayland-Interfaces=")
    if(NOT declared_interfaces STREQUAL "X-KDE-Wayland-Interfaces=org_kde_plasma_window_management")
        message(FATAL_ERROR "${entry} must request exactly the window-management interface for Show Desktop")
    endif()
    file(STRINGS "${SOURCE_DIR}/data/${entry}.desktop" executable REGEX "^Exec=")
    if(NOT executable MATCHES "^Exec=/usr/bin/aero7-gadget-host( --gallery)?$")
        message(FATAL_ERROR "${entry} must identify the installed executable for KWin's canonical-path permission lookup")
    endif()
endforeach()
file(READ "${SOURCE_DIR}/src/main.cpp" main_source)
if(NOT main_source MATCHES "setDesktopFileName\\(QStringLiteral\\(\"org.aero7.GadgetHost\"\\)\\)")
    message(FATAL_ERROR "Runtime desktop identity must match the permission-bearing desktop entry")
endif()
