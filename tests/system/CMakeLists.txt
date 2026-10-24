# Host-side checks of the Buildroot image tree (buildroot-external/), label "system".
# They run against scratch files only (never real disks, no root, no VM) and are registered
# only when the host tools they need exist; check_line_endings is always registered.
set(NEXORA_BR_EXTERNAL ${PROJECT_SOURCE_DIR}/buildroot-external)
set(NEXORA_BR_BOARD ${NEXORA_BR_EXTERNAL}/board/nexora-x86_64)
set(NEXORA_BR_OVERLAY ${NEXORA_BR_BOARD}/rootfs-overlay)

find_program(NEXORA_BASH bash)
find_program(NEXORA_GRUB_EDITENV grub-editenv)
find_program(NEXORA_GRUB_SCRIPT_CHECK grub-script-check)
find_program(NEXORA_SHELLCHECK shellcheck)

# nexora-updated (root update helper) and nexora-bootok (trial-boot confirmation) in their
# test modes, against scratch A/B slot files, a scratch grubenv and the signed bundles of
# the nexora_update_tool_fixture.
if(NEXORA_BASH AND NEXORA_GRUB_EDITENV)
    add_test(NAME test_update_helper COMMAND ${NEXORA_BASH} ${NEXORA_BR_EXTERNAL}/tests/test-update-helper.sh)
    set_tests_properties(test_update_helper PROPERTIES
        LABELS system
        TIMEOUT 180
        FIXTURES_REQUIRED nexora_update_tool_fixture
        ENVIRONMENT "NEXORA_UPDATED=${NEXORA_BR_OVERLAY}/usr/libexec/nexora/nexora-updated;NEXORA_UPDATE_TOOL=$<TARGET_FILE:nexora-update-tool>;NEXORA_TEST_BUNDLE_DIR=${NEXORA_UPDATE_TOOL_FIXTURE_DIR}/good-bundle;NEXORA_TEST_BAD_BUNDLE_DIR=${NEXORA_UPDATE_TOOL_FIXTURE_DIR}/bad-bundle;NEXORA_TEST_KEYS_DIR=${NEXORA_UPDATE_TOOL_FIXTURE_DIR}/keys")
else()
    message(STATUS "test_update_helper not registered (needs bash and grub-editenv)")
endif()

# GRUB script syntax of the A/B boot-counting grub.cfg.
if(NEXORA_GRUB_SCRIPT_CHECK)
    add_test(NAME grub_cfg_syntax COMMAND ${NEXORA_GRUB_SCRIPT_CHECK} ${NEXORA_BR_BOARD}/grub.cfg)
    set_tests_properties(grub_cfg_syntax PROPERTIES LABELS system TIMEOUT 30)
else()
    message(STATUS "grub_cfg_syntax not registered (needs grub-script-check from grub-common)")
endif()

# On-target scripts run under BusyBox sh: no bashisms (shellcheck -s sh).
if(NEXORA_SHELLCHECK)
    add_test(NAME shellcheck_image_scripts
        COMMAND ${NEXORA_SHELLCHECK} -s sh -x -P SCRIPTDIR
            ${NEXORA_BR_OVERLAY}/etc/init.d/S01nexora-data
            ${NEXORA_BR_OVERLAY}/etc/init.d/S80nexora-powerd
            ${NEXORA_BR_OVERLAY}/etc/init.d/S85nexora-updated
            ${NEXORA_BR_OVERLAY}/etc/init.d/S90nexora
            ${NEXORA_BR_OVERLAY}/etc/init.d/S99nexora-bootok
            ${NEXORA_BR_OVERLAY}/usr/libexec/nexora/nexora-functions
            ${NEXORA_BR_OVERLAY}/usr/libexec/nexora/nexora-updated
            ${NEXORA_BR_OVERLAY}/usr/libexec/nexora/nexora-bootok
            ${NEXORA_BR_OVERLAY}/usr/libexec/nexora/nexora-recovery
            ${NEXORA_BR_OVERLAY}/usr/libexec/nexora/nexora-session
            ${NEXORA_BR_OVERLAY}/usr/libexec/nexora/nexora-powerd
            ${NEXORA_BR_BOARD}/post-build.sh
            ${NEXORA_BR_BOARD}/post-image.sh
            ${NEXORA_BR_EXTERNAL}/scripts/check-defconfig.sh
            ${NEXORA_BR_EXTERNAL}/scripts/check-kernel-fragment.sh)
    set_tests_properties(shellcheck_image_scripts PROPERTIES LABELS system TIMEOUT 60)
else()
    message(STATUS "shellcheck_image_scripts not registered (needs shellcheck)")
endif()

# No tracked file may have CRLF line endings (a CRLF script breaks the Buildroot build).
# Skipped (exit 77) when the source tree is not a git checkout git can read.
add_test(NAME check_line_endings COMMAND sh ${PROJECT_SOURCE_DIR}/scripts/check-line-endings.sh)
set_tests_properties(check_line_endings PROPERTIES LABELS system TIMEOUT 60 SKIP_RETURN_CODE 77)
