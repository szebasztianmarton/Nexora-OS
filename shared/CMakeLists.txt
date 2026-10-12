add_library(nexora_shared STATIC
    include/nexora/shared/AtomicFile.h
    include/nexora/shared/DataLayout.h
    include/nexora/shared/EmulatorConfig.h
    include/nexora/shared/ConfigLoader.h
    include/nexora/shared/Logging.h
    include/nexora/shared/Paths.h
    include/nexora/shared/Redaction.h
    include/nexora/shared/Settings.h
    include/nexora/shared/SystemCatalog.h
    src/AtomicFile.cpp
    src/ConfigLoader.cpp
    src/DataLayout.cpp
    src/Logging.cpp
    src/Paths.cpp
    src/Redaction.cpp
    src/Settings.cpp
    src/SystemCatalog.cpp
)
add_library(Nexora::Shared ALIAS nexora_shared)

configure_file(include/nexora/shared/BuildInfo.h.in
               ${PROJECT_BINARY_DIR}/generated/nexora/shared/BuildInfo.h @ONLY)

target_include_directories(nexora_shared PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/generated)
target_compile_features(nexora_shared PUBLIC cxx_std_20)
target_link_libraries(nexora_shared
    PUBLIC Qt6::Core
    PRIVATE nexora_warnings)

# The system catalog is compiled in as :/nexora/systems.json so it is always available.
set_source_files_properties(${PROJECT_SOURCE_DIR}/config/systems/systems.json
                            PROPERTIES QT_RESOURCE_ALIAS systems.json)
qt_add_resources(nexora_shared nexora_shared_resources
    PREFIX /nexora
    FILES ${PROJECT_SOURCE_DIR}/config/systems/systems.json)
