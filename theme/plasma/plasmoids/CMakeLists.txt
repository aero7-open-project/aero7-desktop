add_subdirectory(src)

set(plasmoids
    io.gitgud.wackyideas.panel
    io.gitgud.wackyideas.battery
    io.gitgud.wackyideas.digitalclocklite
    io.gitgud.wackyideas.win7showdesktop
    io.gitgud.wackyideas.networkmanagement
    io.gitgud.wackyideas.volume
    io.gitgud.wackyideas.keyboardlayout

    io.gitgud.wackyideas.SevenStart
    io.gitgud.wackyideas.seventasks
    io.gitgud.wackyideas.desktopcontainment
)

foreach(plasmoid IN LISTS plasmoids)
    plasma_install_package(${plasmoid} ${plasmoid})
endforeach()

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Quick Qml Test)
    add_executable(aero7-start-search-load-test tests/StartSearchLoadTest.cpp)
    target_link_libraries(aero7-start-search-load-test PRIVATE Qt6::Quick Qt6::Qml Qt6::Test)
    target_compile_definitions(aero7-start-search-load-test PRIVATE
        SEARCH_QML_PATH="${CMAKE_CURRENT_SOURCE_DIR}/io.gitgud.wackyideas.SevenStart/contents/ui/SearchView.qml")
    add_test(NAME aero7-start-search-load COMMAND aero7-start-search-load-test)
    set_tests_properties(aero7-start-search-load PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 20)

    add_executable(aero7-task-tooltip-test tests/TaskTooltipTest.cpp)
    target_link_libraries(aero7-task-tooltip-test PRIVATE Qt6::Qml Qt6::Test)
    target_compile_definitions(aero7-task-tooltip-test PRIVATE
        TASK_QML_PATH="${CMAKE_CURRENT_SOURCE_DIR}/io.gitgud.wackyideas.seventasks/contents/ui/Task.qml")
    add_test(NAME aero7-task-tooltip-lifecycle COMMAND aero7-task-tooltip-test)
    set_tests_properties(aero7-task-tooltip-lifecycle PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 20)

    add_executable(aero7-group-preview-size-test tests/GroupPreviewSizeTest.cpp)
    target_link_libraries(aero7-group-preview-size-test PRIVATE Qt6::Quick Qt6::Qml Qt6::Test)
    target_compile_definitions(aero7-group-preview-size-test PRIVATE
        PREVIEW_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/io.gitgud.wackyideas.seventasks/contents/ui")
    add_test(NAME aero7-group-preview-size COMMAND aero7-group-preview-size-test)
    set_tests_properties(aero7-group-preview-size PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 20)
endif()
