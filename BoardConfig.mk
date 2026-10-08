# SPDX-License-Identifier: Apache-2.0
DEVICE_PATH := device/meizu/m2391

include build/make/target/board/BoardConfigMainlineCommon.mk

TARGET_ARCH := arm64
TARGET_ARCH_VARIANT := armv8-2a
TARGET_CPU_ABI := arm64-v8a
TARGET_CPU_VARIANT := generic
TARGET_2ND_ARCH := arm
TARGET_2ND_ARCH_VARIANT := armv8-a
TARGET_2ND_CPU_ABI := armeabi-v7a
TARGET_2ND_CPU_ABI2 := armeabi
TARGET_2ND_CPU_VARIANT := generic
TARGET_BOARD_PLATFORM := kalama
TARGET_BOOTLOADER_BOARD_NAME := kalama
TARGET_NO_BOOTLOADER := true
TARGET_NO_RECOVERY := false
TARGET_SCREEN_WIDTH := 1440
TARGET_SCREEN_HEIGHT := 3200

include kernel/meizu/m2391/BoardConfigKernel.mk
include vendor/meizu/m2391/BoardConfigVendor.mk

TARGET_FS_CONFIG_GEN := $(DEVICE_PATH)/configs/config.fs

# Stock QSEE/camera/display binaries use the legacy ION allocator ABI.
$(call soong_config_set_bool,libion,legacy_impl,true)

# Preserve the Android 13 audio metadata/port ABI used by the stock HIDL HAL.
$(call soong_config_set_bool,android_hardware_audio,skip_speaker_layout_channel_mask_field,true)
$(call soong_config_set_bool,android_hardware_audio,skip_codec_provenance_field,true)

BOARD_BOOTIMAGE_PARTITION_SIZE := 100663296
BOARD_INIT_BOOT_IMAGE_PARTITION_SIZE := 8388608
BOARD_VENDOR_BOOTIMAGE_PARTITION_SIZE := 100663296
BOARD_DTBOIMG_PARTITION_SIZE := 25165824
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 104857600
BOARD_FLASH_BLOCK_SIZE := 262144
BOARD_BOOT_HEADER_VERSION := 4
BOARD_KERNEL_PAGESIZE := 4096
BOARD_USES_GENERIC_KERNEL_IMAGE := true
BOARD_USES_RECOVERY_AS_BOOT := false
BOARD_MOVE_RECOVERY_RESOURCES_TO_VENDOR_BOOT := false
BOARD_INCLUDE_RECOVERY_RAMDISK_IN_VENDOR_BOOT := false
BOARD_EXCLUDE_KERNEL_FROM_RECOVERY_IMAGE := true
BOARD_RAMDISK_USE_LZ4 := true
BOARD_INIT_BOOT_HEADER_VERSION := 4
BOARD_MKBOOTIMG_INIT_ARGS += --header_version $(BOARD_INIT_BOOT_HEADER_VERSION)
BOARD_RECOVERY_MKBOOTIMG_ARGS := --header_version 4 --os_version 13.0.0 --os_patch_level 2023-10
TARGET_RECOVERY_PIXEL_FORMAT := RGBX_8888
BOARD_USES_METADATA_PARTITION := true
# dex_preopt_config.mk treats any nonempty value (including "false") as enabled.
BOARD_USES_SYSTEM_OTHER_ODEX :=

BOARD_SUPER_PARTITION_SIZE := 8589934592
BOARD_SUPER_PARTITION_GROUPS := qti_dynamic_partitions
BOARD_QTI_DYNAMIC_PARTITIONS_SIZE := 8585740288
BOARD_QTI_DYNAMIC_PARTITIONS_PARTITION_LIST := \
    odm product system system_dlkm system_ext vendor vendor_dlkm

TARGET_USERIMAGES_USE_EXT4 := true
TARGET_USERIMAGES_USE_F2FS := true
BOARD_SYSTEMIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_SYSTEM_EXTIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_PRODUCTIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_ODMIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_VENDOR_DLKMIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_SYSTEM_DLKMIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_USES_VENDOR_DLKMIMAGE := true
BOARD_USES_SYSTEM_DLKMIMAGE := true
BOARD_USERDATAIMAGE_FILE_SYSTEM_TYPE := f2fs
TARGET_COPY_OUT_SYSTEM_EXT := system_ext
TARGET_COPY_OUT_PRODUCT := product
TARGET_COPY_OUT_VENDOR := vendor
TARGET_COPY_OUT_ODM := odm
TARGET_COPY_OUT_VENDOR_DLKM := vendor_dlkm
TARGET_COPY_OUT_SYSTEM_DLKM := system_dlkm
BOARD_SYSTEMIMAGE_PARTITION_RESERVED_SIZE := 67108864
BOARD_SYSTEM_EXTIMAGE_PARTITION_RESERVED_SIZE := 67108864
BOARD_PRODUCTIMAGE_PARTITION_RESERVED_SIZE := 67108864

# Generate AVB metadata for all rebuilt partitions on an unlocked device.
BOARD_AVB_ENABLE := true
BOARD_AVB_RECOVERY_ADD_HASH_FOOTER_ARGS += --rollback_index 1
BOARD_AVB_ROLLBACK_INDEX := 0
BOARD_AVB_MAKE_VBMETA_IMAGE_ARGS += --flags 3
BOARD_AVB_VBMETA_SYSTEM := system system_ext product
BOARD_AVB_VBMETA_SYSTEM_KEY_PATH := external/avb/test/data/testkey_rsa2048.pem
BOARD_AVB_VBMETA_SYSTEM_ALGORITHM := SHA256_RSA2048
BOARD_AVB_VBMETA_SYSTEM_ROLLBACK_INDEX := 0
BOARD_AVB_VBMETA_SYSTEM_ROLLBACK_INDEX_LOCATION := 1

# Shipping level and firmware patch level of the proprietary hardware stack.
BOARD_SHIPPING_API_LEVEL := 33
VENDOR_SECURITY_PATCH := 2023-10-01
BOOT_SECURITY_PATCH := 2023-10-01
include device/qcom/sepolicy_vndr/SEPolicy.mk

SYSTEM_EXT_PRIVATE_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy/system_ext/private
BOARD_VENDOR_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy/vendor

DEVICE_MANIFEST_SKUS := kalama
DEVICE_MANIFEST_KALAMA_FILES := \
    vendor/meizu/m2391/proprietary/vendor/etc/vintf/manifest_kalama.xml \
    $(sort $(wildcard vendor/meizu/m2391/proprietary/vendor/etc/vintf/manifest/*.xml))
DEVICE_MATRIX_FILE := vendor/meizu/m2391/proprietary/vendor/etc/vintf/compatibility_matrix.xml

DEVICE_FRAMEWORK_COMPATIBILITY_MATRIX_FILE += $(DEVICE_PATH)/configs/vintf/framework_compatibility_matrix.xml
TARGET_SYSTEM_PROP += $(DEVICE_PATH)/configs/properties/system.prop
TARGET_SYSTEM_EXT_PROP += $(DEVICE_PATH)/configs/properties/system_ext.prop
TARGET_VENDOR_PROP += $(DEVICE_PATH)/configs/properties/vendor.prop
TARGET_ODM_PROP += $(DEVICE_PATH)/configs/properties/odm.prop
TARGET_RECOVERY_FSTAB := $(DEVICE_PATH)/configs/fstab.qcom
