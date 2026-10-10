# Project-wide warning flags and Qt hygiene defines.
#   nexora_warnings                      INTERFACE target for ordinary targets
#   nexora_apply_warnings(<sources...>)  per-source variant for targets that also compile
#                                        Qt-generated code (qmlcachegen output is not
#                                        -Wshadow clean)
set(NEXORA_WARNING_FLAGS "")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(NEXORA_WARNING_FLAGS
        -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor
        -Woverloaded-virtual -Wcast-align -Wformat=2)
    if(NEXORA_WARNINGS_AS_ERRORS)
        list(APPEND NEXORA_WARNING_FLAGS -Werror)
    endif()
endif()

add_library(nexora_warnings INTERFACE)
target_compile_options(nexora_warnings INTERFACE ${NEXORA_WARNING_FLAGS})
target_compile_definitions(nexora_warnings INTERFACE
    QT_NO_NARROWING_CONVERSIONS_IN_CONNECT
    QT_NO_URL_CAST_FROM_STRING)

function(nexora_apply_warnings)
    set_source_files_properties(${ARGN} PROPERTIES COMPILE_OPTIONS "${NEXORA_WARNING_FLAGS}")
endfunction()
