add_library(nexora_library STATIC
    include/nexora/library/GameEntry.h
    include/nexora/library/LibraryScanner.h
    include/nexora/library/GameDatabase.h
    include/nexora/library/LibraryQuery.h
    include/nexora/library/GameLibrary.h
    include/nexora/library/XboxDiscProbe.h
    src/LibraryScanner.cpp
    src/GameDatabase.cpp
    src/LibraryQuery.cpp
    src/GameLibrary.cpp
    src/XboxDiscProbe.cpp
)
add_library(Nexora::Library ALIAS nexora_library)

target_include_directories(nexora_library PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nexora_library
    PUBLIC Qt6::Core Qt6::Sql nexora_shared
    PRIVATE nexora_warnings)
