add_library(nexora_shared STATIC
    include/nexora/shared/EmulatorConfig.h
    include/nexora/shared/ConfigLoader.h
    include/nexora/shared/Logging.h
    include/nexora/shared/Paths.h
    include/nexora/shared/Settings.h
    src/ConfigLoader.cpp
    src/Logging.cpp
    src/Paths.cpp
    src/Settings.cpp
)
add_library(Nexora::Shared ALIAS nexora_shared)

target_include_directories(nexora_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(nexora_shared PUBLIC cxx_std_20)
target_link_libraries(nexora_shared
    PUBLIC Qt6::Core
    PRIVATE nexora_warnings)
