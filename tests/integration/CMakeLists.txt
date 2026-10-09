nexora_add_test(tst_emulatormanager SOURCES tst_emulatormanager.cpp
    LIBS nexora_emulator_manager LABELS integration FAKE_EMULATOR)
nexora_add_test(tst_appcontroller SOURCES tst_appcontroller.cpp
    LIBS nexora_ui LABELS integration FAKE_EMULATOR)
nexora_add_test(tst_qmlsmoke SOURCES tst_qmlsmoke.cpp
    LIBS nexora_uiplugin nexora_ui Qt6::Quick Qt6::QuickControls2 LABELS integration GUI FAKE_EMULATOR)
