# Small, Qt-free stand-in for an emulator used by tests and development runs.
add_executable(nexora-fake-emulator main.cpp)
target_link_libraries(nexora-fake-emulator PRIVATE nexora_warnings)

# Development configurations pointing at the freshly built fake emulator (opt-in with
# -DNEXORA_DEV_FAKE_EMULATOR_CONFIGS=ON), so that build/<preset>/bin/nexora-console-ui shows
# emulators to launch without a real one. Off by default: the Emulators tab then lists only the
# real definitions. A configure with the option off removes definitions an earlier configure wrote.
function(nexora_generate_dev_config id name args)
    if(NOT NEXORA_DEV_FAKE_EMULATOR_CONFIGS)
        file(REMOVE ${NEXORA_BUILD_CONFIG_DIR}/${id}.json)
        return()
    endif()
    file(GENERATE
        OUTPUT ${NEXORA_BUILD_CONFIG_DIR}/${id}.json
        CONTENT "{\n  \"id\": \"${id}\",\n  \"name\": \"${name}\",\n  \"executable\": \"$<TARGET_FILE:nexora-fake-emulator>\",\n  \"arguments\": [${args}],\n  \"enabled\": true\n}\n")
endfunction()

nexora_generate_dev_config(fake-ok "Fake Emulator (runs 3 s, exits OK)"
    "\"--stdout\", \"hello from fake emulator\", \"--sleep-ms\", \"3000\"")
nexora_generate_dev_config(fake-fail "Fake Emulator (exits with code 3)"
    "\"--sleep-ms\", \"1000\", \"--stderr\", \"simulated failure\", \"--exit-code\", \"3\"")
nexora_generate_dev_config(fake-crash "Fake Emulator (crashes)"
    "\"--sleep-ms\", \"1000\", \"--crash\", \"segv\"")
nexora_generate_dev_config(fake-long "Fake Emulator (long-running, ignores SIGTERM)"
    "\"--ignore-sigterm\", \"--sleep-ms\", \"600000\"")
