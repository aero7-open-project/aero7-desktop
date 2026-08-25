cmake_minimum_required(VERSION 3.24)
project(Aero7InternetExplorer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

include(GNUInstallDirs)
include(CTest)
option(AERO7_IE_BUILD_UI "Build the Aero7 Internet Explorer launcher UI" ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(KF6Service REQUIRED)
find_package(KF6KIO REQUIRED COMPONENTS Gui)
if(AERO7_IE_BUILD_UI)
    find_package(Aero7Qt REQUIRED)
endif()

add_library(aero7-compat-launcher SHARED
    src/BrowserRegistry.cpp
    src/BrowserConfig.cpp
    src/DefaultApplications.cpp
    src/InternetExplorer.cpp
)
add_library(Aero7::CompatibilityLauncher ALIAS aero7-compat-launcher)
target_include_directories(aero7-compat-launcher
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(aero7-compat-launcher
    PUBLIC Qt6::Core Qt6::Gui KF6::Service KF6::KIOGui
)
set_target_properties(aero7-compat-launcher PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    OUTPUT_NAME aero7compatlauncher
)

if(AERO7_IE_BUILD_UI)
    add_executable(aero7-internet-explorer
        src/main.cpp
        src/SettingsDialog.cpp
    )
    target_link_libraries(aero7-internet-explorer PRIVATE
        Aero7::CompatibilityLauncher Qt6::Widgets Aero7Qt::Aero7Qt
    )
endif()

install(TARGETS aero7-compat-launcher
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
if(AERO7_IE_BUILD_UI)
    install(TARGETS aero7-internet-explorer
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()
install(DIRECTORY include/aero7compat
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
if(AERO7_IE_BUILD_UI)
    install(FILES data/aero7-internet-explorer.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )
    foreach(size 16 24 32 48 64 128 256)
        install(FILES icons/${size}x${size}/aero7-internet-explorer.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps
        )
    endforeach()
    install(FILES icons/256x256/aero7-internet-explorer.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/aero7/internet-explorer
        RENAME icon.png
    )
endif()
install(FILES LICENSE README.md docs/ARCHITECTURE.md docs/SECURITY.md
              docs/TASKBAR-IDENTITY.md docs/VM-TESTING.md
              docs/VM-RESULTS-2026-08-25.md
              docs/REQUIREMENTS-AUDIT.md
              docs/ASSET-PROVENANCE.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/aero7-internet-explorer
)
install(DIRECTORY docs/screenshots
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/aero7-internet-explorer
)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(aero7-internet-explorer-tests tests/InternetExplorerTests.cpp)
    target_link_libraries(aero7-internet-explorer-tests PRIVATE
        Aero7::CompatibilityLauncher Qt6::Test
    )
    add_test(NAME aero7-internet-explorer-tests
        COMMAND aero7-internet-explorer-tests
    )
    add_test(NAME aero7-internet-explorer-desktop-entry
        COMMAND ${CMAKE_COMMAND}
            -DDESKTOP_FILE=${CMAKE_CURRENT_SOURCE_DIR}/data/aero7-internet-explorer.desktop
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify-desktop-entry.cmake
    )
    set_tests_properties(aero7-internet-explorer-tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()
