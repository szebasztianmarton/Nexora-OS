add_library(nexora_emulator_manager STATIC
    include/nexora/emulator/EmulatorProcessState.h
    include/nexora/emulator/EmulatorManager.h
    include/nexora/emulator/EmulatorHealth.h
    include/nexora/emulator/LaunchHistory.h
    src/EmulatorProcessState.cpp
    src/EmulatorManager.cpp
    src/EmulatorHealth.cpp
    src/LaunchHistory.cpp
)
add_library(Nexora::EmulatorManager ALIAS nexora_emulator_manager)

target_include_directories(nexora_emulator_manager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nexora_emulator_manager
    PUBLIC Qt6::Core nexora_shared
    PRIVATE nexora_warnings)
