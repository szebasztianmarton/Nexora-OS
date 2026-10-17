################################################################################
#
# nexora-os
#
################################################################################

# Built from the source tree this BR2_EXTERNAL lives in (not downloaded).
# Keep in sync with project(... VERSION ...) in the top-level
# CMakeLists.txt; /etc/nexora-release is filled from the version compiled
# into the application (share/nexora/build-info), not from this value.
NEXORA_OS_VERSION = 0.4.0
NEXORA_OS_SITE = $(BR2_EXTERNAL_NEXORA_PATH)/..
NEXORA_OS_SITE_METHOD = local
# Do not copy developer build trees or this BR2_EXTERNAL into the
# package build directory.
NEXORA_OS_OVERRIDE_SRCDIR_RSYNC_EXCLUSIONS = \
	--exclude /build \
	--exclude '/build-*' \
	--exclude /buildroot-external

# SPDX identifier; make legal-info copies LICENSE.
NEXORA_OS_LICENSE = GPL-3.0-or-later
NEXORA_OS_LICENSE_FILES = LICENSE

# The top-level CMakeLists.txt refuses in-source builds.
NEXORA_OS_SUPPORTS_IN_SOURCE_BUILD = NO

NEXORA_OS_DEPENDENCIES = \
	host-qt6base \
	host-qt6declarative \
	openssl \
	qt6base \
	qt6declarative

# The rsynced copy has no .git, so the build id (git describe) is taken
# from the source tree here and passed to CMake explicitly.
NEXORA_OS_BUILD_ID := $(shell git -C $(BR2_EXTERNAL_NEXORA_PATH)/.. describe --always --dirty 2>/dev/null)

# BUILD_TESTING=OFF and NEXORA_BUILD_FAKE_EMULATOR=OFF skip tests/,
# the fake emulator and the generated development configurations.
# SDL3 is OFF because Buildroot 2026.02.x has no sdl3 package.
NEXORA_OS_CONF_OPTS = \
	-DQT_HOST_PATH=$(HOST_DIR) \
	-DBUILD_TESTING=OFF \
	-DNEXORA_BUILD_FAKE_EMULATOR=OFF \
	-DNEXORA_WARNINGS_AS_ERRORS=OFF \
	-DNEXORA_WITH_SDL3=OFF \
	$(if $(NEXORA_OS_BUILD_ID),-DNEXORA_BUILD_ID=$(NEXORA_OS_BUILD_ID))

# Trusted update signing keys: public keys only (*.pem). The directory in
# the repository holds a README and no key; release builds drop the
# release public key(s) there. No key = updates are refused.
NEXORA_OS_UPDATE_KEYS_DIR = $(BR2_EXTERNAL_NEXORA_PATH)/board/nexora-x86_64/update-keys

define NEXORA_OS_INSTALL_UPDATE_KEYS
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/usr/share/nexora/update-keys
	$(foreach key,$(wildcard $(NEXORA_OS_UPDATE_KEYS_DIR)/*.pem), \
		$(INSTALL) -m 0644 $(key) \
			$(TARGET_DIR)/usr/share/nexora/update-keys/$(notdir $(key))$(sep))
endef
NEXORA_OS_POST_INSTALL_TARGET_HOOKS += NEXORA_OS_INSTALL_UPDATE_KEYS

$(eval $(cmake-package))
