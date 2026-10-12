# nexora_add_test(<name> SOURCES <files...> LIBS <targets...> LABELS <labels...> [GUI] [FAKE_EMULATOR])
#   GUI            the test needs a QGuiApplication (runs on the offscreen platform)
#   FAKE_EMULATOR  defines NEXORA_FAKE_EMULATOR="<path to nexora-fake-emulator>"
function(nexora_add_test name)
    cmake_parse_arguments(ARG "GUI;FAKE_EMULATOR" "" "SOURCES;LIBS;LABELS" ${ARGN})
    qt_add_executable(${name} ${ARG_SOURCES})
    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests)
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARG_LIBS} nexora_warnings)
    if(ARG_FAKE_EMULATOR)
        add_dependencies(${name} nexora-fake-emulator)
        target_compile_definitions(${name} PRIVATE
            NEXORA_FAKE_EMULATOR="$<TARGET_FILE:nexora-fake-emulator>")
    endif()
    add_test(NAME ${name} COMMAND ${name})
    # Set per test (not only in presets/CI) so a plain `ctest` behaves the same everywhere.
    set_tests_properties(${name} PROPERTIES
        LABELS "${ARG_LABELS}"
        TIMEOUT 120
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LOGGING_RULES=nexora.*.debug=true")
endfunction()

add_subdirectory(support)
add_subdirectory(unit)
add_subdirectory(integration)
add_subdirectory(qml)
