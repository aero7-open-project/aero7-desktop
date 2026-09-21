cmake_minimum_required(VERSION 3.29)
project(Aero7VaultPresentationTests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
enable_testing()
find_package(ECM 6.29 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
find_package(Qt6 REQUIRED COMPONENTS Widgets Test)
find_package(KF6I18n REQUIRED)
find_package(KF6WidgetsAddons REQUIRED)
find_package(Gpgmepp REQUIRED)
include(ECMQtDeclareLoggingCategory)
if(NOT EXISTS "${KWALLET_SOURCE_DIR}/src/runtime/ksecretd/knewwalletdialog.cpp")
    message(FATAL_ERROR "Set KWALLET_SOURCE_DIR to the prepared KWallet 6.29.0 source")
endif()
set(daemon_dir "${KWALLET_SOURCE_DIR}/src/runtime/ksecretd")
add_executable(vault-presentation-test PresentationTest.cpp
    "${daemon_dir}/knewwalletdialog.cpp" "${daemon_dir}/knewwalletdialog.h"
    "${daemon_dir}/kbetterthankdialog.cpp" "${daemon_dir}/kbetterthankdialog.h")
ecm_qt_declare_logging_category(vault-presentation-test
    HEADER ksecretd_debug.h IDENTIFIER KSECRETD_LOG CATEGORY_NAME aero7.qa.wallet)
ki18n_wrap_ui(vault-presentation-test
    "${daemon_dir}/knewwalletdialogintro.ui" "${daemon_dir}/knewwalletdialoggpg.ui"
    "${daemon_dir}/kbetterthankdialogbase.ui")
target_include_directories(vault-presentation-test PRIVATE "${daemon_dir}")
target_link_libraries(vault-presentation-test PRIVATE Qt6::Widgets Qt6::Test KF6::I18n KF6::WidgetsAddons Gpgmepp)
target_compile_definitions(vault-presentation-test PRIVATE TRANSLATION_DOMAIN="kwalletd6")
qt_add_resources(vault-presentation-test aero7_vault_icon PREFIX /aero7-vault
    BASE "${daemon_dir}" FILES "${daemon_dir}/credential-vault.png")
add_test(NAME vault-presentation COMMAND vault-presentation-test)
set_tests_properties(vault-presentation PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;LANGUAGE=en_US" TIMEOUT 30)
