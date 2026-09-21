add_executable(aero7-sddm-accessibility main.cpp)
target_link_libraries(aero7-sddm-accessibility PRIVATE
    Qt6::Core
    Qt6::DBus
    KF6::ConfigCore
)
install(TARGETS aero7-sddm-accessibility RUNTIME DESTINATION ${KDE_INSTALL_BINDIR})
install(FILES speech-dispatcher-aero7.conf
    DESTINATION lib/systemd/user/speech-dispatcher.service.d
    RENAME aero7-clean-stop.conf)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME aero7-sddm-accessibility-helper
        COMMAND ${CMAKE_COMMAND}
            -DHELPER=$<TARGET_FILE:aero7-sddm-accessibility>
            -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/test-profile
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_helper.cmake)
endif()
