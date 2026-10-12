# Reusable signed-bundle test fixture builder (keys + manifest + image), shared by the
# update unit/integration tests and reusable by the Buildroot update-helper tests.
add_library(nexora_update_testsupport STATIC
    UpdateBundleFixture.h
    UpdateBundleFixture.cpp
)
target_include_directories(nexora_update_testsupport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(nexora_update_testsupport
    PUBLIC Qt6::Core
    PRIVATE OpenSSL::Crypto nexora_warnings)
