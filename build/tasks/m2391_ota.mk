# SPDX-License-Identifier: Apache-2.0
ifeq ($(TARGET_DEVICE),m2391)
# Compatibility with stock recovery's meizu20Pro device identifier only.
# Native target-files/image assembly, VINTF and OTA generation remain unchanged.
M2391_OTA_WRAPPER := device/meizu/m2391/releasetools/ota_from_target_files
$(INTERNAL_OTA_PACKAGE_TARGET): OTA_FROM_TARGET_FILES := $(M2391_OTA_WRAPPER)
$(INTERNAL_OTA_PACKAGE_TARGET): $(M2391_OTA_WRAPPER) \
    device/meizu/m2391/releasetools/ota.py
endif
