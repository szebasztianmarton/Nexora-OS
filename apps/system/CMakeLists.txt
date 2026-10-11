add_library(nexora_system STATIC
    include/nexora/system/BackupService.h
    include/nexora/system/DiagnosticReport.h
    include/nexora/system/HardwareInfo.h
    include/nexora/system/SystemIntegration.h
    include/nexora/system/SingleInstanceGuard.h
    include/nexora/system/UnixSignalWatcher.h
    src/BackupService.cpp
    src/DiagnosticReport.cpp
    src/HardwareInfo.cpp
    src/SystemIntegration.cpp
    src/SingleInstanceGuard.cpp
    src/UnixSignalWatcher.cpp
)
add_library(Nexora::System ALIAS nexora_system)

target_include_directories(nexora_system PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nexora_system
    PUBLIC Qt6::Core nexora_shared
    PRIVATE nexora_warnings)
