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
    add_test(NAME gadget-runtime-core COMMAND aero7-gadget-host --self-test)
    set_tests_properties(gadget-runtime-core PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(gadget-icon-independence tests/IconIndependenceTest.cpp resources/icons.qrc)
    target_link_libraries(gadget-icon-independence PRIVATE Qt6::Widgets)
    add_test(NAME gadget-icon-independence COMMAND gadget-icon-independence)
    set_tests_properties(gadget-icon-independence PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME gadget-icon-policy COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_icon_policy.cmake)
endif()
