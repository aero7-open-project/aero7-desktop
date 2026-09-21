add_subdirectory(login-sessions)

if(NOT INSTALL_X11_COMPONENTS)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/plasma/sddm/sddm-theme-mod DESTINATION ${CMAKE_INSTALL_PREFIX}/share/sddm/themes)
endif()

install(FILES aero7-accessibility.conf
    DESTINATION lib/sddm/sddm.conf.d
    RENAME 90-aero7-accessibility.conf)

if(BUILD_TESTING)
    add_test(NAME aero7-sddm-ease-of-access-contract
        COMMAND ${CMAKE_COMMAND}
            -DTHEME_MAIN=${CMAKE_CURRENT_SOURCE_DIR}/sddm-theme-mod/Main.qml
            -DTHEME_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}/aero7-accessibility.conf
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_access_dialog.cmake)
endif()
