cmake_minimum_required(VERSION 3.24)
project(aero7-desktop VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

set(CMAKE_INSTALL_LIBDIR "lib" CACHE STRING "Library directory")
set(CMAKE_INSTALL_LIBEXECDIR "lib" CACHE STRING "Executable library directory")
set(CMAKE_INSTALL_SYSCONFDIR "/etc" CACHE PATH "System configuration directory")
include(GNUInstallDirs)
include(CTest)
option(AERO7_INSTALL_TEST_TOOLS "Install VM acceptance helpers (test images only)" OFF)
option(AERO7_BUILD_THEME "Build the included Aero7 Plasma shell, theme, SDDM and helpers" ON)

find_package(Qt6 6.10 REQUIRED COMPONENTS Core Concurrent DBus Gui Qml Quick Widgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBVLC REQUIRED IMPORTED_TARGET libvlc)
pkg_check_modules(TAGLIB REQUIRED IMPORTED_TARGET taglib)
find_program(MSGFMT_EXECUTABLE msgfmt REQUIRED)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dolphin-aero7.mo
    COMMAND ${MSGFMT_EXECUTABLE}
            -o ${CMAKE_CURRENT_BINARY_DIR}/dolphin-aero7.mo
            ${CMAKE_CURRENT_SOURCE_DIR}/integration/file-explorer/dolphin-aero7.po
    DEPENDS integration/file-explorer/dolphin-aero7.po
    VERBATIM
)
add_custom_target(aero7-file-explorer-locale ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/dolphin-aero7.mo)

qt_add_executable(aero7-recovery-ui
    shell/recovery/main.cpp
    shell/recovery/RecoveryController.cpp
    shell/recovery/RecoveryController.h
)
qt_add_resources(aero7-recovery-ui recovery-qml
    PREFIX /aero7/recovery
    BASE shell/recovery
    FILES shell/recovery/RecoveryView.qml
)
set_target_properties(aero7-recovery-ui PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN TRUE)
target_link_libraries(aero7-recovery-ui PRIVATE Qt6::Core Qt6::DBus Qt6::Gui Qt6::Qml Qt6::Quick)

qt_add_executable(aero7-media-player apps/media-player/main.cpp)
qt_add_resources(aero7-media-player aero7-media-icons
    PREFIX /media
    FILES
        assets/media-player/icons/app/64.png
        assets/media-player/icons/document-open.png
        assets/media-player/icons/list-add.png
        assets/media-player/icons/media-playback-pause.png
        assets/media-player/icons/media-playback-start.png
        assets/media-player/icons/media-playback-stop.png
        assets/media-player/icons/media-skip-backward.png
        assets/media-player/icons/media-skip-forward.png
        assets/media-player/icons/stock_volume-max.png
        assets/media-player/icons/stock_volume-mute.png
        assets/media-player/icons/view-fullscreen.png
        assets/media-player/icons/view-list-details.png
        assets/media-player/icons/library/audio-art.png
        assets/media-player/icons/library/back.png
        assets/media-player/icons/library/folder.png
        assets/media-player/icons/library/forward.png
        assets/media-player/icons/library/music.png
        assets/media-player/icons/library/search.png
        assets/media-player/icons/library/videos.png
)
target_link_libraries(aero7-media-player PRIVATE Qt6::Concurrent Qt6::Widgets PkgConfig::LIBVLC PkgConfig::TAGLIB)

if(BUILD_TESTING OR AERO7_INSTALL_TEST_TOOLS)
    qt_add_executable(aero7-test-status-notifier tests/integration/status_notifier_item.cpp)
    target_link_libraries(aero7-test-status-notifier PRIVATE Qt6::Core Qt6::DBus)

    qt_add_executable(aero7-screenshot-test tests/visual/screenshot_capture.cpp)
    target_link_libraries(aero7-screenshot-test PRIVATE Qt6::Core Qt6::DBus Qt6::Gui)
endif()

set(AERO7_LIBEXECDIR "${CMAKE_INSTALL_LIBEXECDIR}/aero7-desktop")
set(AERO7_DATADIR "${CMAKE_INSTALL_DATADIR}/aero7-desktop")

install(TARGETS aero7-recovery-ui aero7-media-player
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(AERO7_INSTALL_TEST_TOOLS)
    install(TARGETS aero7-test-status-notifier aero7-screenshot-test
        RUNTIME DESTINATION ${AERO7_LIBEXECDIR}
    )
endif()

install(PROGRAMS
    shell/session/aero7-session
    shell/recovery/aero7-recovery
    migrations/aero7-migrate
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dolphin-aero7.mo
    DESTINATION ${CMAKE_INSTALL_LOCALEDIR}/aero7/LC_MESSAGES
    RENAME dolphin.mo
)

install(PROGRAMS
    services/session/aero7-shell-service
    services/session/aero7-session-setup
    services/session/aero7-media-associations
    services/session/aero7-screenshot-identity
    services/session/aero7-portal-policy
    services/session/aero7-snap-control
    DESTINATION ${AERO7_LIBEXECDIR}
)
install(FILES services/session/aero7-wallpaper-defaults.js
    DESTINATION ${AERO7_LIBEXECDIR}
)

if(AERO7_INSTALL_TEST_TOOLS)
    install(PROGRAMS
        tests/vm/test-session.sh
        tests/vm/test-live-session.sh
        tests/vm/test-recovery.sh
        tests/vm/test-multimonitor.sh
        tests/vm/test-control-panel.sh
        tests/vm/test-file-explorer.sh
        tests/vm/test-personalization.sh
        tests/vm/test-shell-surfaces.sh
        tests/vm/test-window-management.sh
        tests/vm/test-failures.sh
        tests/vm/test-upgrade.sh
        tests/vm/test-reinstall.sh
        tests/vm/test-uninstall.sh
        DESTINATION ${AERO7_LIBEXECDIR}
    )
endif()

install(FILES
    packaging/session/aero7.desktop
    packaging/session/aero7-safe.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/wayland-sessions
)

if(AERO7_INSTALL_TEST_TOOLS)
    install(FILES
        tests/visual/org.aero7.visualtest.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )
    install(FILES tests/vm/99-aero7-ydotool.rules
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/udev/rules.d
    )
endif()

# Baloo's worker identifies itself as org.kde.baloo. Its upstream package
# supplies an autostart file but no matching application entry for the host
# portal. This hidden identity preserves normal portal registration without
# adding a launcher, changing indexing settings or masking diagnostics.
install(FILES packaging/applications/org.kde.baloo.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

# VLC skins2 archives are gzip-compressed tar files. Keep the original Aero7
# backdrop and the already-used icon pack's controls inside the player itself.
file(GLOB AERO7_MEDIA_ICON_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/media-player/icons/*.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/media-player/icons/app/*.png")
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/aero7-media-player.vlt
    COMMAND ${CMAKE_COMMAND} -E tar czf
            ${CMAKE_CURRENT_BINARY_DIR}/aero7-media-player.vlt
            theme.xml main.png playlist.png slider.png icons
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets/media-player
    DEPENDS assets/media-player/theme.xml
            assets/media-player/main.png
            assets/media-player/playlist.png
            assets/media-player/slider.png
            ${AERO7_MEDIA_ICON_FILES}
    VERBATIM
)
add_custom_target(aero7-media-player-skin ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/aero7-media-player.vlt)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/aero7-media-player.vlt
    DESTINATION ${CMAKE_INSTALL_DATADIR}/vlc/skins2)
install(FILES packaging/applications/aero7-media-player.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
foreach(size 16 24 32 48 64 256)
    install(FILES assets/media-player/icons/app/${size}.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps
        RENAME aero7-media-player.png)
endforeach()
install(FILES assets/media-player/ICON-LICENSE assets/media-player/ICON-PROVENANCE.md
    assets/media-player/ASSET-PROVENANCE.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/aero7-desktop/media-player)

install(FILES
    packaging/systemd/aero7-shell.service
    packaging/systemd/aero7-session-setup.service
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/user
)

install(FILES services/session/aero7-shell-layout.js
    DESTINATION ${AERO7_DATADIR}/shell
)

install(FILES assets/wallpapers/aero7-background.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/wallpapers/Aero7/contents/images
    RENAME 1672x941.png
)

install(FILES
    assets/branding/aero7-logo-circle.png
    assets/branding/aero7-logo-plain.png
    assets/branding/aero7-user.png
    DESTINATION ${AERO7_DATADIR}/branding
)

# The target unit is generated from x-atpootb.desktop, so its canonical unit
# name contains a literal systemd escape. CMake treats a backslash in an
# install destination as a path separator; the helper preserves the Linux
# filename exactly.
install(CODE "
    execute_process(
        COMMAND \"${CMAKE_CURRENT_SOURCE_DIR}/packaging/install-systemd-dropins.sh\"
                \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}\"
        RESULT_VARIABLE AERO7_DROPIN_RESULT)
    if(NOT AERO7_DROPIN_RESULT EQUAL 0)
        message(FATAL_ERROR \"Could not install Aero7 systemd drop-ins\")
    endif()
")

install(FILES defaults/shell.json
    DESTINATION ${AERO7_DATADIR}/defaults
)
install(FILES defaults/taskbar.json
    DESTINATION ${AERO7_DATADIR}/defaults
)
install(FILES defaults/start.json
    DESTINATION ${AERO7_DATADIR}/defaults
)
install(FILES defaults/desktop.json
    DESTINATION ${AERO7_DATADIR}/defaults
)
install(FILES "defaults/Recycle Bin.desktop"
    DESTINATION /etc/skel/Desktop
)
install(FILES compatibility/desktop-stack.json
    DESTINATION ${AERO7_DATADIR}/compatibility
)
install(FILES plasma/compatibility/aerothemeplasmarc
    DESTINATION /etc/xdg/aero7-desktop
)
install(FILES defaults/kwinrc
    DESTINATION /etc/xdg/aero7-desktop
)
install(FILES defaults/60-aero7-monospace.conf
    DESTINATION /etc/fonts/conf.d
)
install(DIRECTORY kwin/scripts/aero7shake
    DESTINATION ${CMAKE_INSTALL_DATADIR}/kwin/scripts
)
install(DIRECTORY kwin/scripts/aero7snap
    DESTINATION ${CMAKE_INSTALL_DATADIR}/kwin/scripts
)
if(BUILD_TESTING)
    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
    add_test(NAME portal-vault-policy
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/session/test_portal_policy.py)
    add_test(NAME unit-shell-service
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/unit/test_shell_service.py)
    add_test(NAME migration-idempotence
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/migration/test_migration.py)
    add_test(NAME session-artifacts
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/session/test_session_artifacts.py)
    add_test(NAME source-policy
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/unit/test_source_policy.py)
    add_test(NAME github-source-only-policy
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/unit/test_ci_policy.py)
    add_test(NAME source-snapshot
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/unit/test_source_snapshot.py)
    add_test(NAME snap-control
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/unit/test_snap_control.py)
    find_program(NODE_EXECUTABLE node)
    if(NODE_EXECUTABLE)
        add_test(NAME snap-behavior
            COMMAND ${NODE_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/unit/test_snap_behavior.js)
        add_test(NAME wallpaper-defaults
            COMMAND ${NODE_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/session/test_wallpaper_defaults.js)
    endif()
    add_test(NAME monospace-default
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/unit/test_monospace_default.py)
    add_test(NAME install-profiles
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/session/test_install_profiles.py)
    set_tests_properties(install-profiles PROPERTIES TIMEOUT 180)
    add_test(NAME screenshot-identity
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/session/test_screenshot_identity.py)
    add_test(NAME media-associations
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/unit/test_media_associations.py)
    add_test(NAME media-skin
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/unit/test_media_skin.py)
    add_test(NAME session-reconcile-execution
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/session/test_reconcile_execution.py)
    set_tests_properties(session-reconcile-execution PROPERTIES TIMEOUT 60)
    add_test(NAME tray-layout
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/session/test_tray_layout.py)
endif()

if(AERO7_BUILD_THEME)
    add_subdirectory(theme)
endif()
