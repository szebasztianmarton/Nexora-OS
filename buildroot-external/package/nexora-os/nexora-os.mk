################################################################################
#
# nexora-os
#
################################################################################

# Built from the source tree this BR2_EXTERNAL lives in (not downloaded).
NEXORA_OS_VERSION = 0.2.0
NEXORA_OS_SITE = $(BR2_EXTERNAL_NEXORA_PATH)/..
NEXORA_OS_SITE_METHOD = local
# Do not copy developer build trees or this BR2_EXTERNAL into the
# package build directory.
NEXORA_OS_OVERRIDE_SRCDIR_RSYNC_EXCLUSIONS = \
	--exclude /build \
	--exclude '/build-*' \
	--exclude /buildroot-external

# The project has no license yet (licensing decision pending), so
# there is no license file to point to.
NEXORA_OS_LICENSE = Unknown (licensing decision pending)

# The top-level CMakeLists.txt refuses in-source builds.
NEXORA_OS_SUPPORTS_IN_SOURCE_BUILD = NO

NEXORA_OS_DEPENDENCIES = \
	host-qt6base \
	host-qt6declarative \
	qt6base \
	qt6declarative

# BUILD_TESTING=OFF and NEXORA_BUILD_FAKE_EMULATOR=OFF skip tests/,
# the fake emulator and the generated development configurations.
# SDL3 is OFF because Buildroot 2026.02.x has no sdl3 package.
NEXORA_OS_CONF_OPTS = \
	-DQT_HOST_PATH=$(HOST_DIR) \
	-DBUILD_TESTING=OFF \
	-DNEXORA_BUILD_FAKE_EMULATOR=OFF \
	-DNEXORA_WARNINGS_AS_ERRORS=OFF \
	-DNEXORA_WITH_SDL3=OFF

$(eval $(cmake-package))
