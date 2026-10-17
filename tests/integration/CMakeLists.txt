nexora_add_test(tst_emulatormanager SOURCES tst_emulatormanager.cpp
    LIBS nexora_emulator_manager LABELS integration FAKE_EMULATOR)
nexora_add_test(tst_gamelibrary SOURCES tst_gamelibrary.cpp
    LIBS nexora_library LABELS integration)
nexora_add_test(tst_appcontroller SOURCES tst_appcontroller.cpp
    LIBS nexora_ui LABELS integration FAKE_EMULATOR)
nexora_add_test(tst_uicontext SOURCES tst_uicontext.cpp
    LIBS nexora_ui LABELS integration GUI FAKE_EMULATOR)
nexora_add_test(tst_updatecontroller SOURCES tst_updatecontroller.cpp
    LIBS nexora_ui nexora_update_testsupport OpenSSL::Crypto LABELS integration)
nexora_add_test(tst_qmlsmoke SOURCES tst_qmlsmoke.cpp
    LIBS nexora_uiplugin nexora_ui Qt6::Quick Qt6::QuickControls2 LABELS integration GUI FAKE_EMULATOR)
# Every case runs at 1280x720 and 1920x1080 (plus high-contrast/reduced-motion runs).
set_tests_properties(tst_qmlsmoke PROPERTIES TIMEOUT 300)
# Virtual controllers through /dev/uinput (skipped where uinput is not writable).
if(NEXORA_HAVE_LINUX_INPUT_H AND TARGET nexora-virtual-pad)
    nexora_add_test(tst_evdevhotplug SOURCES tst_evdevhotplug.cpp LIBS nexora_input LABELS integration)
    target_include_directories(tst_evdevhotplug PRIVATE ${PROJECT_SOURCE_DIR}/tests/fixtures/virtual-pad)
endif()
nexora_add_test(tst_backupservice SOURCES tst_backupservice.cpp
    LIBS nexora_system LABELS integration security)

# tst_updateservice doubles as the CTest FIXTURES_SETUP for test_update_tool.sh below: its
# writesToolFixture() test writes a good and a tampered signed bundle (plus the trusted
# key) to NEXORA_UPDATE_TOOL_FIXTURE_DIR, reusing the same bundle-building helper as the
# other update tests so nexora-update-tool can be exercised against real bundles.
# NEXORA_UPDATE_TOOL_FIXTURE_DIR is set in tests/CMakeLists.txt (tests/system uses it too).
nexora_add_test(tst_updateservice SOURCES tst_updateservice.cpp
    LIBS nexora_update nexora_update_testsupport OpenSSL::Crypto LABELS integration security)
target_compile_definitions(tst_updateservice PRIVATE
    NEXORA_UPDATE_FIXTURE_DIR="${NEXORA_UPDATE_TOOL_FIXTURE_DIR}")
set_tests_properties(tst_updateservice PROPERTIES FIXTURES_SETUP nexora_update_tool_fixture)

add_test(NAME test_update_tool COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/test_update_tool.sh")
set_tests_properties(test_update_tool PROPERTIES
    LABELS "integration;security"
    TIMEOUT 60
    FIXTURES_REQUIRED nexora_update_tool_fixture
    ENVIRONMENT "NEXORA_UPDATE_TOOL=$<TARGET_FILE:nexora-update-tool>;NEXORA_TEST_BUNDLE_DIR=${NEXORA_UPDATE_TOOL_FIXTURE_DIR}/good-bundle;NEXORA_TEST_BAD_BUNDLE_DIR=${NEXORA_UPDATE_TOOL_FIXTURE_DIR}/bad-bundle;NEXORA_TEST_KEYS_DIR=${NEXORA_UPDATE_TOOL_FIXTURE_DIR}/keys")

# Release engineering scripts (scripts/release): signing with a throwaway Ed25519 key in
# $TMPDIR, SHA256SUMS verification incl. tamper detection, refusal of keys inside a source
# work tree, and the documentation link checker on a scratch tree.
find_program(NEXORA_OPENSSL_EXECUTABLE openssl)
if(NEXORA_OPENSSL_EXECUTABLE)
    add_test(NAME test_release_scripts COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/test_release_scripts.sh")
    set_tests_properties(test_release_scripts PROPERTIES
        LABELS "integration;security"
        TIMEOUT 120)
else()
    message(STATUS "openssl command not found; skipping test_release_scripts")
endif()
