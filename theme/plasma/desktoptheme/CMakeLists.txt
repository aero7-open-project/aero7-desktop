install(
    DIRECTORY
        ${CMAKE_SOURCE_DIR}/plasma/desktoptheme/Aero7
        ${CMAKE_SOURCE_DIR}/plasma/desktoptheme/Seven-Black
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/plasma/desktoptheme
)

# Plasma Components 3 menus use widgets/background, while shell dialogs use
# dialogs/background. Install the same light Aero frame for both paths and for
# every compositing variant so neither can fall back to Seven-Black or Breeze.
install(
    FILES ${CMAKE_SOURCE_DIR}/plasma/desktoptheme/Aero7/dialogs/background.svg
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/plasma/desktoptheme/Aero7/widgets
)

foreach(surface_variant opaque solid translucent)
    install(
        FILES ${CMAKE_SOURCE_DIR}/plasma/desktoptheme/Aero7/dialogs/background.svg
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share/plasma/desktoptheme/Aero7/${surface_variant}/dialogs
    )
    install(
        FILES ${CMAKE_SOURCE_DIR}/plasma/desktoptheme/Aero7/dialogs/background.svg
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share/plasma/desktoptheme/Aero7/${surface_variant}/widgets
    )
endforeach()
