cmake_minimum_required(VERSION 3.16)
project(aerothemeplasma-desktop)

set(KF6_MIN_VERSION "6.0.0")
find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

set(PROJECT_VERSION "6.7.0")
set(PROJECT_VERSION_MAJOR 6)

option(INSTALL_X11_COMPONENTS "Install X11 session files" OFF)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)
find_package(KF6Config)
find_package(KF6GuiAddons REQUIRED)
find_package(KF6GlobalAccel REQUIRED)
find_package(KF6Notifications REQUIRED)
find_package(Plasma)
find_package(Qt6 REQUIRED COMPONENTS Core DBus Gui Widgets)

add_subdirectory(plasma)
add_subdirectory(helpers)
if(NOT INSTALL_X11_COMPONENTS)
    add_subdirectory(misc)
endif()

install()
