install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Windows7Aero DESTINATION ${CMAKE_INSTALL_PREFIX}/share/Kvantum)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Svg)
    add_executable(aero7-kvantum-svg-test tests/SvgReferencesTest.cpp)
    target_link_libraries(aero7-kvantum-svg-test PRIVATE Qt6::Gui Qt6::Svg)
    add_test(NAME aero7-kvantum-svg-references
        COMMAND aero7-kvantum-svg-test ${CMAKE_CURRENT_SOURCE_DIR}/Windows7Aero/Windows7Aero.svg)
    set_tests_properties(aero7-kvantum-svg-references PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
