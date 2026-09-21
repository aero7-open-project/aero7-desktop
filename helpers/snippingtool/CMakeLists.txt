include(CTest)

add_executable(aero7-snipping-tool main.cpp resources/icons.qrc)
target_link_libraries(aero7-snipping-tool PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    Qt6::Widgets
    KF6::GuiAddons
    KF6::GlobalAccel
    KF6::Notifications
)
install(TARGETS aero7-snipping-tool RUNTIME DESTINATION ${KDE_INSTALL_BINDIR})
install(FILES org.aero7.snippingtool.desktop
    DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES org.aero7.snippingtool-autostart.desktop
    DESTINATION ${KDE_INSTALL_AUTOSTARTDIR}
    RENAME org.aero7.snippingtool.desktop)
install(FILES snipping-recovery.conf
    DESTINATION ${KDE_INSTALL_LIBDIR}/systemd/user/app-org.aero7.snippingtool@autostart.service.d)
install(FILES aero7-snipping-tool.notifyrc
    DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})
foreach(size 16 24 32 48 256)
    install(FILES assets/icons/app/${size}x${size}/aero7-snipping-tool.png
        DESTINATION ${KDE_INSTALL_FULL_ICONDIR}/hicolor/${size}x${size}/apps)
endforeach()
install(FILES assets/icons/LICENSES.md assets/icons/UPSTREAM-LICENSE
    assets/icons/UPSTREAM-README.md
    DESTINATION ${KDE_INSTALL_DATADIR}/doc/aero7-snipping-tool)

if(BUILD_TESTING)
    add_executable(aero7-snipping-tool-clipboard-test tests/CaptureClipboardDataTest.cpp)
    target_link_libraries(aero7-snipping-tool-clipboard-test PRIVATE Qt6::Core Qt6::Gui)
    add_test(NAME aero7-snipping-tool-clipboard-data COMMAND aero7-snipping-tool-clipboard-test)
    add_test(NAME aero7-snipping-tool-application-identity
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_application_identity.cmake)
    add_executable(aero7-snipping-tool-pending-test tests/PendingCaptureWindowTest.cpp)
    target_link_libraries(aero7-snipping-tool-pending-test PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
    add_test(NAME aero7-snipping-tool-pending-capture COMMAND aero7-snipping-tool-pending-test)
    set_tests_properties(aero7-snipping-tool-pending-capture PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(aero7-snipping-tool-notification-test tests/CaptureNotificationGateTest.cpp)
    target_link_libraries(aero7-snipping-tool-notification-test PRIVATE Qt6::Core Qt6::DBus)
    add_test(NAME aero7-snipping-tool-notification-gate COMMAND aero7-snipping-tool-notification-test)
    add_executable(aero7-snipping-tool-result-test tests/CaptureResultTest.cpp)
    target_link_libraries(aero7-snipping-tool-result-test PRIVATE Qt6::Core)
    add_test(NAME aero7-snipping-tool-capture-result
        COMMAND aero7-snipping-tool-result-test)
    add_executable(aero7-snipping-tool-icon-test
        tests/IconIndependenceTest.cpp resources/icons.qrc)
    target_link_libraries(aero7-snipping-tool-icon-test PRIVATE
        Qt6::Core Qt6::Gui Qt6::Widgets)
    add_test(NAME aero7-snipping-tool-icon-independence
        COMMAND aero7-snipping-tool-icon-test)
    set_tests_properties(aero7-snipping-tool-icon-independence PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME aero7-snipping-tool-icon-policy
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_icon_policy.cmake)
endif()
