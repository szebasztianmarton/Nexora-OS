# Controller input: gamepad backends (evdev, optional SDL3) feeding a backend-neutral
# navigator that turns gamepad state into UI navigation actions / synthesized key events.
include(CheckIncludeFileCXX)

add_library(nexora_input STATIC
    NavAction.h
    GamepadInput.h
    GamepadNavigator.h
    GamepadNavigator.cpp
    InputBackend.h
    InputController.h
    InputController.cpp
)
add_library(Nexora::Input ALIAS nexora_input)

# Consumers include headers as "input/InputController.h".
target_include_directories(nexora_input PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(nexora_input
    PUBLIC Qt6::Core Qt6::Gui nexora_shared
    PRIVATE nexora_warnings)

# --- evdev backend (Linux only) ----------------------------------------------------------
set(_nexora_evdev OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    check_include_file_cxx(linux/input.h NEXORA_HAVE_LINUX_INPUT_H)
    if(NEXORA_HAVE_LINUX_INPUT_H)
        set(_nexora_evdev ON)
    endif()
endif()

if(_nexora_evdev)
    target_sources(nexora_input PRIVATE
        EvdevMapping.h
        EvdevMapping.cpp
        EvdevGamepadBackend.h
        EvdevGamepadBackend.cpp)
    # PUBLIC so tests can conditionally exercise the evdev mapping helpers.
    target_compile_definitions(nexora_input PUBLIC NEXORA_HAVE_EVDEV=1)
endif()

# --- SDL3 backend (optional) -----------------------------------------------------------
if(NOT DEFINED NEXORA_WITH_SDL3)
    set(NEXORA_WITH_SDL3 "AUTO")
endif()
string(TOUPPER "${NEXORA_WITH_SDL3}" _nexora_sdl3_mode)

set(_nexora_sdl3 OFF)
if(_nexora_sdl3_mode STREQUAL "ON")
    find_package(SDL3 CONFIG)
    if(NOT SDL3_FOUND)
        message(FATAL_ERROR
            "NEXORA_WITH_SDL3=ON but SDL3 was not found.\n"
            "Install the SDL3 development package (e.g. `sudo apt install libsdl3-dev` where "
            "available) or build SDL3 from source and pass -DCMAKE_PREFIX_PATH=/path/to/sdl3. "
            "Use -DNEXORA_WITH_SDL3=AUTO or OFF to build without it.")
    endif()
    set(_nexora_sdl3 ON)
elseif(_nexora_sdl3_mode STREQUAL "AUTO")
    find_package(SDL3 CONFIG QUIET)
    if(SDL3_FOUND)
        set(_nexora_sdl3 ON)
    endif()
elseif(NOT _nexora_sdl3_mode STREQUAL "OFF")
    message(FATAL_ERROR "NEXORA_WITH_SDL3 must be AUTO, ON or OFF (got '${NEXORA_WITH_SDL3}').")
endif()

if(_nexora_sdl3)
    target_sources(nexora_input PRIVATE
        Sdl3GamepadBackend.h
        Sdl3GamepadBackend.cpp)
    target_link_libraries(nexora_input PRIVATE SDL3::SDL3)
    target_compile_definitions(nexora_input PUBLIC NEXORA_HAVE_SDL3=1)
endif()

message(STATUS "Nexora: gamepad backends: evdev=${_nexora_evdev} sdl3=${_nexora_sdl3}")
