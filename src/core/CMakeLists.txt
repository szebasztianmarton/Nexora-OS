add_library(nexora_core STATIC
    logger.h logger.cpp
    jsonstore.h jsonstore.cpp
    appsettings.h appsettings.cpp
    emulatordefinition.h emulatordefinition.cpp
    emulatorregistry.h emulatorregistry.cpp
    emulatormanager.h emulatormanager.cpp
    gamemodel.h gamemodel.cpp
    gamefiltermodel.h gamefiltermodel.cpp
    gamelibrary.h gamelibrary.cpp
    controllermanager.h controllermanager.cpp
    directorybrowser.h directorybrowser.cpp
    diagnostics.h diagnostics.cpp
)

target_include_directories(nexora_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(nexora_core PUBLIC Qt6::Core Qt6::Gui Qt6::Concurrent)
target_compile_definitions(nexora_core PUBLIC
    NEXORA_VERSION="${PROJECT_VERSION}"
    NEXORA_BUILD_ID="${NEXORA_BUILD_ID}")

if(NEXORA_HAVE_SDL)
    target_link_libraries(nexora_core PRIVATE SDL2::SDL2)
    target_compile_definitions(nexora_core PRIVATE NEXORA_HAVE_SDL=1)
endif()

target_compile_options(nexora_core PRIVATE -Wall -Wextra)
