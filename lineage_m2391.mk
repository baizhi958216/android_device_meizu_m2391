#
# Copyright (C) 2024 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

# Inherit from those products. Most specific first.
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)

# Inherit some common Lineage stuff.
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)

# Inherit from m2391 device
$(call inherit-product, device/meizu/m2391/device.mk)

PRODUCT_DEVICE := m2391
PRODUCT_NAME := lineage_m2391
PRODUCT_BRAND := meizu
PRODUCT_MODEL := MEIZU 20 Pro
PRODUCT_MANUFACTURER := meizu

PRODUCT_GMS_CLIENTID_BASE := android-meizu

PRODUCT_BUILD_PROP_OVERRIDES += \
    PRIVATE_BUILD_DESC="meizu_20Pro_CN-user 14 UKQ1.230917.001 1715714532 release-keys"

BUILD_FINGERPRINT := meizu/meizu_20Pro_CN/meizu20Pro:14/UKQ1.230917.001/1715714532:user/release-keys
