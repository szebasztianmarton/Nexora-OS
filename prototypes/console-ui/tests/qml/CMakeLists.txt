find_package(Qt6 REQUIRED COMPONENTS QuickTest)

# QML UI tests: the real backend (temporary sandbox config with the mock
# emulator) plus the real Nexora QML module, driven by keyboard events.
add_executable(tst_qml_ui main.cpp)
target_link_libraries(tst_qml_ui PRIVATE nexora_backend nexora_uiplugin Qt6::QuickTest Qt6::Quick Qt6::QuickControls2)
target_compile_definitions(tst_qml_ui PRIVATE
    NEXORA_MOCK_EMULATOR="$<TARGET_FILE:nexora-mock-emulator>"
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_dependencies(tst_qml_ui nexora-mock-emulator)

add_test(NAME tst_qml_ui COMMAND tst_qml_ui -input ${CMAKE_CURRENT_SOURCE_DIR})
set_tests_properties(tst_qml_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 300)
