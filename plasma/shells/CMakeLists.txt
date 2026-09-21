install(DIRECTORY ${CMAKE_SOURCE_DIR}/plasma/shells/io.gitgud.wackyideas.desktop DESTINATION ${CMAKE_INSTALL_PREFIX}/share/plasma/shells)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Quick Qml Test)
    add_executable(aero7-panel-shadow-test tests/PanelShadowMarginsTest.cpp)
    target_link_libraries(aero7-panel-shadow-test PRIVATE Qt6::Quick Qt6::Qml Qt6::Test)
    target_compile_definitions(aero7-panel-shadow-test PRIVATE
        PANEL_QML_PATH="${CMAKE_CURRENT_SOURCE_DIR}/io.gitgud.wackyideas.desktop/contents/views/Panel.qml")
    add_test(NAME aero7-panel-shadow-margins COMMAND aero7-panel-shadow-test)
    set_tests_properties(aero7-panel-shadow-margins PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 20)
endif()
