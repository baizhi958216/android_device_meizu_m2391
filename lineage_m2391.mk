# SPDX-License-Identifier: Apache-2.0
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/generic_system.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/handheld_system_ext.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/telephony_system_ext.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/aosp_product.mk)
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)
$(call inherit-product, device/meizu/m2391/device.mk)

PRODUCT_NAME := lineage_m2391
PRODUCT_DEVICE := m2391
PRODUCT_MANUFACTURER := Meizu
PRODUCT_BRAND := Meizu
PRODUCT_MODEL := MEIZU 20 Pro
