# SPDX-License-Identifier: Apache-2.0
DEVICE_PATH := device/meizu/m2391

$(call inherit-product, $(SRC_TARGET_DIR)/product/generic_ramdisk.mk)
$(call inherit-product, $(DEVICE_PATH)/source-packages.mk)

# Qualcomm Boot HAL preserves the GPT-based A/B slot attributes.
PRODUCT_SOONG_NAMESPACES += hardware/qcom-caf/bootctrl

PRODUCT_SHIPPING_API_LEVEL := 33
PRODUCT_USE_DYNAMIC_PARTITIONS := true
PRODUCT_USE_DYNAMIC_PARTITION_SIZE := true
$(call inherit-product, $(SRC_TARGET_DIR)/product/virtual_ab_ota.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/virtual_ab_ota/compression.mk)

AB_OTA_UPDATER := true
AB_OTA_PARTITIONS := \
    boot dtbo init_boot odm product recovery system system_dlkm system_ext \
    vbmeta vbmeta_system vendor vendor_boot vendor_dlkm

PRODUCT_BUILD_BOOT_IMAGE := true
PRODUCT_BUILD_INIT_BOOT_IMAGE := true
PRODUCT_BUILD_VENDOR_BOOT_IMAGE := true
PRODUCT_BUILD_RECOVERY_IMAGE := true
PRODUCT_BUILD_VENDOR_IMAGE := true
PRODUCT_BUILD_ODM_IMAGE := true
PRODUCT_BUILD_VENDOR_DLKM_IMAGE := true
PRODUCT_BUILD_SYSTEM_DLKM_IMAGE := true
PRODUCT_BUILD_DEBUG_BOOT_IMAGE := false
PRODUCT_BUILD_DEBUG_VENDOR_BOOT_IMAGE := false
PRODUCT_BUILD_CACHE_IMAGE := false
PRODUCT_BUILD_USERDATA_IMAGE := false
PRODUCT_BUILD_SYSTEM_OTHER_IMAGE := false
PRODUCT_BUILD_SUPER_PARTITION := true

PRODUCT_EXTRA_VNDK_VERSIONS := 33
PRODUCT_COMPATIBLE_PROPERTY_OVERRIDE := true
PRODUCT_ENFORCE_VINTF_MANIFEST := true
PRODUCT_OTA_ENFORCE_VINTF_KERNEL_REQUIREMENTS := true
PRODUCT_ENABLE_UFFD_GC := true

PRODUCT_PACKAGES += \
    android.hardware.vibrator-service.m2391 \
    M2391FrameworkOverlay \
    M2391SystemUIOverlay \
    android.hidl.allocator@1.0-service \
    android.frameworks.sensorservice@1.0 \
    m2391_framework_compatibility_matrix_7 \
    update_engine \
    update_verifier \
    bootctl

# Keep the matched stock vendor policy/HAL set; base_vendor.mk would replace it.
PRODUCT_PACKAGES += \
    fs_config_dirs_nonsystem \
    fs_config_files_nonsystem \
    m2391_vendor_firmware_mnt \
    m2391_vendor_bt_firmware \
    m2391_vendor_dsp \
    vendor_compatibility_matrix.xml \
    odm-build.prop \
    vendor_dlkm-build.prop \
    system_dlkm-build.prop

PRODUCT_PACKAGES += \
    adbd.recovery \
    cgroups.recovery.json \
    charger.recovery \
    init_second_stage.recovery \
    ld.config.recovery.txt \
    linker.recovery \
    otacerts.recovery \
    recovery \
    servicemanager.recovery \
    shell_and_utilities_recovery \
    watchdogd.recovery \
    android.hardware.boot-service.qti.recovery \
    android.hardware.fastboot-service.example_recovery \
    android.hardware.health-service.example_recovery \
    fastbootd

PRODUCT_VENDOR_PROPERTIES += \
    ro.recovery.usb.vid?=18D1 \
    ro.recovery.usb.adb.pid?=D001 \
    ro.recovery.usb.fastboot.pid?=4EE0

PRODUCT_HOST_PACKAGES += \
    e2fsdroid \
    mke2fs \
    sload_f2fs \
    make_f2fs

PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/configs/fstab.qcom:$(TARGET_COPY_OUT_VENDOR_RAMDISK)/first_stage_ramdisk/fstab.qcom \
    $(DEVICE_PATH)/configs/init/init.recovery.qcom.rc:$(TARGET_COPY_OUT_RECOVERY)/root/init.recovery.qcom.rc

# Preserve upstream notices alongside the notices generated for this build.
PRODUCT_COPY_FILES += \
    vendor/meizu/m2391/proprietary/vendor/etc/NOTICE.xml.gz:$(TARGET_COPY_OUT_VENDOR)/etc/NOTICE.stock.xml.gz \
    vendor/meizu/m2391/proprietary/odm/etc/NOTICE.xml.gz:$(TARGET_COPY_OUT_ODM)/etc/NOTICE.stock.xml.gz \
    vendor/meizu/m2391/proprietary/vendor_dlkm/etc/NOTICE.xml.gz:$(TARGET_COPY_OUT_VENDOR_DLKM)/etc/NOTICE.stock.xml.gz \
    vendor/meizu/m2391/proprietary/system_dlkm/etc/NOTICE.xml.gz:$(TARGET_COPY_OUT_SYSTEM_DLKM)/etc/NOTICE.stock.xml.gz

$(call inherit-product, vendor/meizu/m2391/m2391-vendor.mk)

# COW v2/gzip matches the supported Android 12.1-based recovery.
PRODUCT_VIRTUAL_AB_COW_VERSION := 2
PRODUCT_VIRTUAL_AB_COMPRESSION_METHOD := gz
