#
# Copyright (C) 2025 The Android Open Source Project
# Copyright (C) 2025 SebaUbuntu's TWRP device tree generator
#
# SPDX-License-Identifier: Apache-2.0
#

# Inherit from m2391 device
$(call inherit-product, device/meizu/m2391/device.mk)

# Assert
TARGET_OTA_ASSERT_DEVICE := m2391

PRODUCT_DEVICE := m2391
PRODUCT_NAME := twrp_m2391
PRODUCT_BRAND := MEIZU
PRODUCT_MODEL := MEIZU 20 Pro
PRODUCT_MANUFACTURER := MEIZU

PRODUCT_GMS_CLIENTID_BASE := android-meizu

PRODUCT_BUILD_PROP_OVERRIDES += \
    PRIVATE_BUILD_DESC="meizu_20Pro_CN-user 14 UKQ1.230917.001 1715712564 release-keys"

BUILD_FINGERPRINT := meizu/meizu_20Pro_CN/meizu20Pro:14/UKQ1.230917.001/1715712564:user/release-keys
