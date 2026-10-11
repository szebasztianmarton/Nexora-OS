cmake_minimum_required(VERSION 3.22)

project(NexoraOS
    VERSION 0.4.0
    DESCRIPTION "Controller-first Linux gaming environment (MVP 4)"
    LANGUAGES CXX)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not supported. Use e.g. `cmake --preset debug`.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(BUILD_TESTING "Build the Nexora test suite" ON)
option(NEXORA_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(NEXORA_BUILD_FAKE_EMULATOR "Build the fake emulator used by tests and dev runs" ON)
set(NEXORA_WITH_SDL3 "AUTO" CACHE STRING "Build the optional SDL3 gamepad backend (AUTO, ON, OFF)")
set_property(CACHE NEXORA_WITH_SDL3 PROPERTY STRINGS AUTO ON OFF)

set(NEXORA_BOARD "nexora-x86_64" CACHE STRING "Board / hardware profile name reported by the OS")
if(NOT DEFINED NEXORA_BUILD_ID)
    set(NEXORA_BUILD_ID "dev")
    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(COMMAND ${GIT_EXECUTABLE} describe --always --dirty
                        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                        OUTPUT_VARIABLE NEXORA_GIT_DESCRIBE
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                        ERROR_QUIET
                        RESULT_VARIABLE NEXORA_GIT_RESULT)
        if(NEXORA_GIT_RESULT EQUAL 0 AND NEXORA_GIT_DESCRIBE)
            set(NEXORA_BUILD_ID "${NEXORA_GIT_DESCRIBE}")
        endif()
    endif()
endif()
set(NEXORA_BUILD_ID "${NEXORA_BUILD_ID}" CACHE STRING "Build identifier (default: git describe at configure time)")

set(NEXORA_QT_MIN_VERSION 6.4)
set(NEXORA_QT_COMPONENTS Core Gui Qml Quick QuickControls2 Sql)
if(BUILD_TESTING)
    list(APPEND NEXORA_QT_COMPONENTS Test)
endif()

find_package(Qt6 ${NEXORA_QT_MIN_VERSION} COMPONENTS ${NEXORA_QT_COMPONENTS})
if(NOT Qt6_FOUND)
    message(FATAL_ERROR
        "Qt ${NEXORA_QT_MIN_VERSION}+ with components [${NEXORA_QT_COMPONENTS}] was not found.\n"
        "On Ubuntu 24.04 install:\n"
        "  sudo apt install qt6-base-dev qt6-declarative-dev libqt6sql6-sqlite qml6-module-qtquick \\\n"
        "       qml6-module-qtquick-controls qml6-module-qtquick-templates \\\n"
        "       qml6-module-qtquick-layouts qml6-module-qtquick-window qml6-module-qtqml-workerscript\n"
        "For a custom Qt installation pass -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64.")
endif()

qt_standard_project_setup()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
# Dev-run configuration lives next to bin/ so the default lookup
# (<appDir>/../share/nexora/emulators) works from the build tree.
set(NEXORA_BUILD_CONFIG_DIR ${PROJECT_BINARY_DIR}/share/nexora/emulators)

include(GNUInstallDirs)
include(cmake/NexoraCompilerWarnings.cmake)

add_subdirectory(shared)
add_subdirectory(apps/emulator-manager)
add_subdirectory(apps/game-library)
add_subdirectory(apps/system)
add_subdirectory(apps/console-ui)

if(NEXORA_BUILD_FAKE_EMULATOR OR BUILD_TESTING)
    add_subdirectory(tests/fixtures/fake-emulator)
endif()

configure_file(config/emulators/example.json ${NEXORA_BUILD_CONFIG_DIR}/example.json COPYONLY)
install(FILES config/emulators/example.json
        DESTINATION ${CMAKE_INSTALL_DATADIR}/nexora/emulators)

configure_file(config/systems/systems.json ${PROJECT_BINARY_DIR}/share/nexora/systems.json COPYONLY)
install(FILES config/systems/systems.json
        DESTINATION ${CMAKE_INSTALL_DATADIR}/nexora)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
