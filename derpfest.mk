# SPDX-License-Identifier: Apache-2.0
# Keep DerpFest-specific configuration separate from the hardware configuration.
# This file is inherited before vendor/lineage/config/common_full_phone.mk so
# device feature selections take effect before DerpFest evaluates its defaults.

DERP_BOOTANIMATION := default
# Use the dark Monet animation regardless of the current system theme.
PRODUCT_PRODUCT_PROPERTIES += ro.product.bootanim.file=bootanimation-dark.zip

TARGET_FACE_UNLOCK_SUPPORTED := true
TARGET_SUPPORTS_BLUR := true
TARGET_SUPPORTS_QUICK_TAP := true
TARGET_INCLUDE_ACCORD := true

# This checkout supplies source Mainline modules, not Google-signed APEX prebuilts.
PRODUCT_MODULE_BUILD_FROM_SOURCE := true

PRODUCT_PACKAGES += \
    M2391CarrierConfig \
    M2391UpdaterOverlay
