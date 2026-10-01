add_executable(aero7-shell-action main.cpp)
find_package(PkgConfig REQUIRED)
pkg_check_modules(ShellActionJournal REQUIRED IMPORTED_TARGET libsystemd)
target_link_libraries(aero7-shell-action PRIVATE Qt6::Core Qt6::DBus PkgConfig::ShellActionJournal)
install(TARGETS aero7-shell-action RUNTIME DESTINATION ${KDE_INSTALL_BINDIR})

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(aero7-delayed-output-fixture tests/DelayedOutputFixture.cpp)
    target_link_libraries(aero7-delayed-output-fixture PRIVATE Qt6::Core)
    add_executable(aero7-detached-launch-test tests/DetachedLaunchTest.cpp)
    target_link_libraries(aero7-detached-launch-test PRIVATE Qt6::Core Qt6::Test)
    target_compile_definitions(aero7-detached-launch-test PRIVATE
        FIXTURE_PATH="$<TARGET_FILE:aero7-delayed-output-fixture>"
        LAUNCHER_PATH="$<TARGET_FILE:aero7-shell-action>")
    add_dependencies(aero7-detached-launch-test aero7-shell-action aero7-delayed-output-fixture)
    add_test(NAME aero7-detached-shell-launch COMMAND aero7-detached-launch-test)
    set_tests_properties(aero7-detached-shell-launch PROPERTIES TIMEOUT 30)
    add_executable(aero7-shell-action-no-journal main.cpp tests/JournalUnavailable.cpp)
    target_link_libraries(aero7-shell-action-no-journal PRIVATE Qt6::Core Qt6::DBus PkgConfig::ShellActionJournal)
    target_link_options(aero7-shell-action-no-journal PRIVATE "-Wl,--wrap=sd_journal_stream_fd")
    add_executable(aero7-detached-launch-no-journal-test tests/DetachedLaunchTest.cpp)
    target_link_libraries(aero7-detached-launch-no-journal-test PRIVATE Qt6::Core Qt6::Test)
    target_compile_definitions(aero7-detached-launch-no-journal-test PRIVATE
        EXPECT_JOURNAL_UNAVAILABLE
        FIXTURE_PATH="$<TARGET_FILE:aero7-delayed-output-fixture>"
        LAUNCHER_PATH="$<TARGET_FILE:aero7-shell-action-no-journal>")
    add_dependencies(aero7-detached-launch-no-journal-test aero7-shell-action-no-journal aero7-delayed-output-fixture)
    add_test(NAME aero7-detached-shell-launch-no-journal COMMAND aero7-detached-launch-no-journal-test)
    set_tests_properties(aero7-detached-shell-launch-no-journal PROPERTIES TIMEOUT 30)
endif()
