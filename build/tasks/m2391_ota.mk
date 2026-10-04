# SPDX-License-Identifier: Apache-2.0
ifeq ($(TARGET_DEVICE),m2391)

# AOSP's optional prebuilt ramdisk roots can be absent. Keep fs_config's
# normal behavior for existing roots without letting a failed cd scan $TOP.
# Derived from the Apache-2.0 AOSP fs_config recipe.
define fs_config
(if [ -d "$(1)" ]; then cd "$(1)" && { find . -type d | sed 's,$$,/,'; find . \! -type d; }; fi) | cut -c 3- | sort | sed 's,^,$(2),' | $(HOST_OUT_EXECUTABLES)/fs_config -C -D $(TARGET_OUT) -R "$(2)"
endef

# The native OTA rule and Lineage bacon target still generate/sign the package.
# Only supply a device adapter for whole-image vendor/ODM target-files inputs.
M2391_OTA_WRAPPER := device/meizu/m2391/releasetools/ota_from_target_files
$(INTERNAL_OTA_PACKAGE_TARGET): OTA_FROM_TARGET_FILES := $(M2391_OTA_WRAPPER)
$(INTERNAL_OTA_PACKAGE_TARGET): $(M2391_OTA_WRAPPER) \
    device/meizu/m2391/releasetools/ota.py \
    vendor/meizu/m2391/proprietary-files.txt \
    $(HOST_OUT_EXECUTABLES)/fsck.erofs $(HOST_OUT_EXECUTABLES)/simg2img

endif
