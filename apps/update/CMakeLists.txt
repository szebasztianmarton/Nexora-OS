add_library(nexora_update STATIC
    include/nexora/update/Version.h
    include/nexora/update/UpdateManifest.h
    include/nexora/update/SignatureVerifier.h
    include/nexora/update/UpdateService.h
    src/Version.cpp
    src/UpdateManifest.cpp
    src/SignatureVerifier.cpp
    src/UpdateService.cpp
)
add_library(Nexora::Update ALIAS nexora_update)

target_include_directories(nexora_update PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nexora_update
    PUBLIC Qt6::Core nexora_shared
    PRIVATE OpenSSL::Crypto nexora_warnings)

# Verifies a bundle outside the running UI/service (CI, release scripts, manual checks).
# "sign" is intentionally not included here; signing uses the openssl CLI directly
# (scripts/release/sign-update.sh, Task 10) so the private key never has to be handled by
# a Nexora-authored binary.
add_executable(nexora-update-tool tools/nexora-update-tool.cpp)
target_link_libraries(nexora-update-tool PRIVATE nexora_update Qt6::Core nexora_warnings)
# Installed: the image's root update helper (buildroot-external .../nexora-updated) runs it
# to re-verify a bundle before writing it to the inactive root slot.
install(TARGETS nexora-update-tool RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
