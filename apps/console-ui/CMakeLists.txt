# Backend bootstrap shared by the application and the QML test runner.
add_library(nexora_backend STATIC src/backend.h src/backend.cpp)
target_include_directories(nexora_backend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(nexora_backend PUBLIC nexora_core Qt6::Qml Qt6::Gui)

# The QML user interface as a static QML module (URI "Nexora").
# Files keep their sub-folders in the source tree but are flattened into the
# module root in resources so the generated qmldir covers all of them
# (singletons resolve correctly from every file).
file(GLOB NEXORA_QML_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/*.qml
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/*/*.qml)
file(GLOB NEXORA_QML_SINGLETONS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/styles/*.qml
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/singletons/*.qml)
foreach(_file IN LISTS NEXORA_QML_FILES)
    get_filename_component(_name ${_file} NAME)
    set_source_files_properties(${_file} PROPERTIES QT_RESOURCE_ALIAS ${_name})
endforeach()
set_source_files_properties(${NEXORA_QML_SINGLETONS} PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_library(nexora_ui STATIC)
qt_add_qml_module(nexora_ui
    URI Nexora
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Nexora
    RESOURCE_PREFIX /qt/qml
    QML_FILES ${NEXORA_QML_FILES}
)
target_link_libraries(nexora_ui PRIVATE Qt6::Quick Qt6::QuickControls2)

qt_add_executable(nexora-console-ui src/main.cpp)
target_link_libraries(nexora-console-ui PRIVATE
    nexora_backend nexora_uiplugin Qt6::Quick Qt6::QuickControls2)

install(TARGETS nexora-console-ui RUNTIME DESTINATION bin)
