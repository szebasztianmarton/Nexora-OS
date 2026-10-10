add_subdirectory(src/input)

# --- UI library + QML module -------------------------------------------------------------
qt_add_library(nexora_ui STATIC)

set(NEXORA_QML_FILES
    qml/Main.qml
    qml/Theme.qml
    qml/Header.qml
    qml/NavTabBar.qml
    qml/HintBar.qml
    qml/Toast.qml
    qml/NxButton.qml
    qml/SettingRow.qml
    qml/GameTile.qml
    qml/GameRow.qml
    qml/HomeScreen.qml
    qml/GamesScreen.qml
    qml/EmulatorsScreen.qml
    qml/EmulatorCard.qml
    qml/EmptyState.qml
    qml/SettingsScreen.qml
    qml/DiagnosticsScreen.qml
    qml/RunningOverlay.qml
    qml/TutorialOverlay.qml
)
# Serve the files at qrc:/qt/qml/Nexora/Console/<Name>.qml (without the qml/ subdirectory).
foreach(qml_file IN LISTS NEXORA_QML_FILES)
    get_filename_component(qml_name ${qml_file} NAME)
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_name})
endforeach()
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(nexora_ui
    URI Nexora.Console
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/qml/Nexora/Console
    QML_FILES ${NEXORA_QML_FILES}
    SOURCES
        src/AppController.h src/AppController.cpp
        src/EmulatorListModel.h src/EmulatorListModel.cpp
        src/UiBootstrap.h src/UiBootstrap.cpp
        src/GameListModel.h src/GameListModel.cpp
        src/LibraryController.h src/LibraryController.cpp
        src/SettingsController.h src/SettingsController.cpp
        src/SystemController.h src/SystemController.cpp
        src/UiContext.h src/UiContext.cpp
)

# qmltyperegistrar includes the QML_ELEMENT headers by bare name.
target_include_directories(nexora_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(nexora_ui PUBLIC NEXORA_VERSION="${PROJECT_VERSION}")
target_link_libraries(nexora_ui
    PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick nexora_shared nexora_emulator_manager nexora_input nexora_library nexora_system)
target_compile_definitions(nexora_ui PRIVATE
    QT_NO_NARROWING_CONVERSIONS_IN_CONNECT QT_NO_URL_CAST_FROM_STRING)
# Warnings only for hand-written sources; qmlcachegen output is not -Wshadow clean.
nexora_apply_warnings(src/AppController.cpp src/EmulatorListModel.cpp src/UiBootstrap.cpp
    src/GameListModel.cpp src/LibraryController.cpp src/SettingsController.cpp src/SystemController.cpp
    src/UiContext.cpp)

# --- Application ---------------------------------------------------------------------------
qt_add_executable(nexora-console-ui src/main.cpp)
target_link_libraries(nexora-console-ui PRIVATE
    nexora_ui nexora_uiplugin Qt6::QuickControls2 nexora_warnings)

install(TARGETS nexora-console-ui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
