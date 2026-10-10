cmake_minimum_required(VERSION 3.21)

project(NexoraOS VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

option(NEXORA_BUILD_TESTS "Build unit and QML tests" ON)
option(NEXORA_WITH_SDL "Enable SDL2 game controller support" ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Concurrent)
qt_standard_project_setup()

# Build identifier shown in Diagnostics/About. Falls back to "development".
set(NEXORA_BUILD_ID "development" CACHE STRING "Build or image identifier")
find_package(Git QUIET)
if(GIT_FOUND AND NEXORA_BUILD_ID STREQUAL "development")
    execute_process(COMMAND ${GIT_EXECUTABLE} describe --always --dirty
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE _nexora_git_rev OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET RESULT_VARIABLE _nexora_git_rc)
    if(_nexora_git_rc EQUAL 0 AND _nexora_git_rev)
        set(NEXORA_BUILD_ID "git-${_nexora_git_rev}")
    endif()
endif()

set(NEXORA_HAVE_SDL OFF)
if(NEXORA_WITH_SDL)
    find_package(SDL2 QUIET)
    if(SDL2_FOUND)
        set(NEXORA_HAVE_SDL ON)
        message(STATUS "Nexora: SDL2 controller support enabled")
    else()
        message(STATUS "Nexora: SDL2 not found, controller support disabled")
    endif()
endif()

add_subdirectory(src/core)
add_subdirectory(tools/mock-emulator)
add_subdirectory(apps/console-ui)

if(NEXORA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
