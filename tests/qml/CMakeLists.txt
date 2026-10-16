# QtQuickTest discovers every *.qml file below at run time; this one executable covers both
# tst_theme.qml (the Theme singleton alone) and tst_settings_ui.qml (the real SettingsScreen
# against a real sandboxed backend, built by main.cpp's Setup class). Only built when
# Qt6::QuickTest is available (BUILD_TESTING, see the top-level CMakeLists.txt).
if(NOT TARGET Qt6::QuickTest)
    message(STATUS "Qt6::QuickTest not found; skipping tests/qml (QML behaviour tests)")
    return()
endif()

qt_add_executable(tst_console_qml main.cpp)
set_target_properties(tst_console_qml PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests)
target_link_libraries(tst_console_qml PRIVATE
    nexora_uiplugin nexora_ui Qt6::QuickTest Qt6::Quick Qt6::QuickControls2 nexora_warnings)
target_compile_definitions(tst_console_qml PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    NEXORA_FAKE_EMULATOR="$<TARGET_FILE:nexora-fake-emulator>")
add_dependencies(tst_console_qml nexora-fake-emulator)

add_test(NAME tst_console_qml COMMAND tst_console_qml -input ${CMAKE_CURRENT_SOURCE_DIR})
set_tests_properties(tst_console_qml PROPERTIES
    LABELS "integration;GUI"
    TIMEOUT 120
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LOGGING_RULES=nexora.*.debug=true")
