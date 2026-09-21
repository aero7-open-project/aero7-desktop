cmake_minimum_required(VERSION 3.24)
project(aero7-gadgets VERSION 3.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)
include(CTest)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core DBus Gui Network Svg Widgets Xml)
find_package(KF6WindowSystem REQUIRED)
find_package(LayerShellQt REQUIRED)

add_executable(aero7-gadget-host
    resources/icons.qrc
    src/main.cpp
    src/GadgetTypes.h
    src/GadgetManager.cpp
    src/GadgetManager.h
    src/GadgetWindow.cpp
    src/GadgetWindow.h
    src/GadgetPainter.cpp
    src/GadgetPainter.h
    src/GadgetGallery.cpp
    src/GadgetGallery.h
    src/GadgetOptionsDialog.cpp
    src/GadgetOptionsDialog.h
    src/RuntimeServices.cpp
    src/RuntimeServices.h
)

target_link_libraries(aero7-gadget-host PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    Qt6::Network
    Qt6::Svg
    Qt6::Widgets
    Qt6::Xml
    KF6::WindowSystem
    LayerShellQt::Interface
)

target_compile_definitions(aero7-gadget-host PRIVATE
    AERO7_GADGET_VERSION="${PROJECT_VERSION}"
    AERO7_GADGET_METADATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/gadgets"
)
target_compile_options(aero7-gadget-host PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS aero7-gadget-host RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(CODE "file(CREATE_LINK aero7-gadget-host \"\$ENV{DESTDIR}${CMAKE_INSTALL_FULL_BINDIR}/aero7-gadget-gallery\" SYMBOLIC)")
install(FILES
    data/org.aero7.GadgetHost.desktop
    data/org.aero7.GadgetGallery.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)
install(FILES data/org.aero7.GadgetHost-autostart.desktop
    DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/xdg/autostart
)
foreach(size 16 22 24 32 40 48 64 256)
    install(FILES assets/icons/app/${size}x${size}/aero7-gadget-gallery.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps)
endforeach()
install(FILES assets/icons/LICENSES.md assets/icons/UPSTREAM-LICENSE
    assets/icons/UPSTREAM-README.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/aero7-gadgets)
install(DIRECTORY data/gadgets DESTINATION ${CMAKE_INSTALL_DATADIR}/aero7)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    find_package(KF6Service REQUIRED)
    add_executable(gadget-wayland-permissions-test tests/WaylandPermissionsTest.cpp)
    target_link_libraries(gadget-wayland-permissions-test PRIVATE Qt6::Core KF6::Service)
    add_test(NAME gadget-wayland-permissions COMMAND gadget-wayland-permissions-test ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME gadget-runtime-core COMMAND aero7-gadget-host --self-test)
    set_tests_properties(gadget-runtime-core PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(gadget-icon-independence tests/IconIndependenceTest.cpp resources/icons.qrc)
    target_link_libraries(gadget-icon-independence PRIVATE Qt6::Widgets)
    add_test(NAME gadget-icon-independence COMMAND gadget-icon-independence)
    set_tests_properties(gadget-icon-independence PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME gadget-icon-policy COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_icon_policy.cmake)
    add_test(NAME gadget-wayland-contract COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_wayland_contract.cmake)
    # Exercise the real gallery and manager, not a duplicate widget fixture.
    get_target_property(gadget_gallery_sources aero7-gadget-host SOURCES)
    list(REMOVE_ITEM gadget_gallery_sources src/main.cpp)
    add_executable(gadget-gallery-test tests/GalleryTest.cpp ${gadget_gallery_sources})
    target_include_directories(gadget-gallery-test PRIVATE src)
    get_target_property(gadget_runtime_libraries aero7-gadget-host LINK_LIBRARIES)
    target_link_libraries(gadget-gallery-test PRIVATE ${gadget_runtime_libraries} Qt6::Test)
    find_program(DBUS_RUN_SESSION dbus-run-session REQUIRED)
    add_test(NAME gadget-gallery COMMAND ${DBUS_RUN_SESSION}
        --config-file=${CMAKE_CURRENT_SOURCE_DIR}/tests/isolated-session.conf
        $<TARGET_FILE:gadget-gallery-test>)
    set_tests_properties(gadget-gallery PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(gadget-runtime-services-test tests/RuntimeServicesTest.cpp src/RuntimeServices.cpp src/RuntimeServices.h)
    target_include_directories(gadget-runtime-services-test PRIVATE src)
    target_link_libraries(gadget-runtime-services-test PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::DBus Qt6::Test)
    add_test(NAME gadget-runtime-services COMMAND ${DBUS_RUN_SESSION}
        --config-file=${CMAKE_CURRENT_SOURCE_DIR}/tests/isolated-session.conf
        $<TARGET_FILE:gadget-runtime-services-test>)
    add_executable(gadget-media-services-test tests/MediaServicesTest.cpp src/RuntimeServices.cpp src/RuntimeServices.h)
    target_include_directories(gadget-media-services-test PRIVATE src)
    target_link_libraries(gadget-media-services-test PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::DBus Qt6::Test)
    add_test(NAME gadget-media-services COMMAND ${DBUS_RUN_SESSION}
        --config-file=${CMAKE_CURRENT_SOURCE_DIR}/tests/isolated-session.conf
        $<TARGET_FILE:gadget-media-services-test>)
    set_tests_properties(gadget-media-services PROPERTIES TIMEOUT 60)
    add_dependencies(gadget-gallery-test gadget-media-services-test)
endif()
