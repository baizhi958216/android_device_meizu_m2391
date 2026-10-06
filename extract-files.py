#!/usr/bin/env -S PYTHONPATH=../../../tools/extract-utils python3
# SPDX-License-Identifier: Apache-2.0

import shutil
from pathlib import Path
from tempfile import TemporaryDirectory

from extract_utils.extract_recovery import (
    extract_ramdisk,
    parse_mkbootimg_fragments,
    unpack_bootimg,
)
from extract_utils.fixups_lib import lib_fixups
from extract_utils.fixups_blob import blob_fixup
from extract_utils.main import ExtractUtils, ExtractUtilsModule
from extract_utils.module import ProprietaryFileType


def extract_boot(ctx, file_path, dump_dir):
    """Expose the kernel as a blob, without retaining the boot container."""
    destination = Path(dump_dir, 'boot')
    destination.mkdir(exist_ok=True)
    with TemporaryDirectory() as temporary:
        unpack_bootimg(file_path, temporary)
        shutil.copyfile(Path(temporary, 'kernel'), destination / 'kernel')
    return file_path


def extract_vendor_boot(ctx, file_path, dump_dir):
    """Extract DTB and the stock platform ramdisk's signed kernel modules."""
    destination = Path(dump_dir, 'vendor_boot')
    destination.mkdir(exist_ok=True)
    with TemporaryDirectory() as temporary:
        args = unpack_bootimg(file_path, temporary)
        shutil.copyfile(Path(temporary, 'dtb'), destination / 'dtb')
        fragments = parse_mkbootimg_fragments(args)
        platform = [fragment for fragment in fragments if fragment.ramdisk_type == 1]
        if len(platform) != 1:
            raise ValueError('Expected one stock platform vendor ramdisk')
        extract_ramdisk(platform[0].path, destination)
    return file_path


def lib_fixup_vendor_suffix(lib, partition):
    return f'{lib}_vendor' if partition == 'vendor' else None


module = ExtractUtilsModule(
    'm2391',
    'meizu',
    lib_fixups={
        **lib_fixups,
        'libvibrator': lib_fixup_vendor_suffix,
    },
    namespace_imports=['device/meizu/m2391', 'hardware/qcom-caf/bootctrl'],
    blob_fixups={
        # Its onrestart actions restart main/hal. Starting it before APEX
        # activation can pin those services to the bootstrap mount namespace
        # when libbinder's LLNDK dependencies are not available yet.
        'vendor/etc/init/vndservicemanager.m2391.rc': blob_fixup().regex_replace(
            r'(?m)^    class core\n', '    class core\n    updatable\n',
        ),
        (
            'vendor/bin/qguard',
            'vendor/lib64/libqfp-service.so',
            'vendor/lib/nfc_nci.nqx.default.hw.so',
            'vendor/lib64/nfc_nci.nqx.default.hw.so',
            'vendor/lib/hw/vendor.rongcard.hardware.eid_rk@1.0-impl.so',
            'vendor/lib64/hw/vendor.rongcard.hardware.eid_rk@1.0-impl.so',
        ): blob_fixup().replace_needed('libbase.so', 'libbase-v33.so'),
        (
            'vendor/bin/hw/android.hardware.security.keymint-service-qti',
            'vendor/bin/hw/android.hardware.security.keymint-service-spu-qti',
            'vendor/lib/libqtikeymint.so',
            'vendor/lib64/libqtikeymint.so',
            'vendor/lib/libspukeymint.so',
            'vendor/lib64/libspukeymint.so',
        ): blob_fixup().add_needed('android.hardware.security.rkp-V2-ndk.so'),
        (
            'vendor/lib/libqcodec2_core.so',
            'vendor/lib64/libqcodec2_core.so',
        ): blob_fixup().add_needed('libcodec2_m2391.so'),
        (
            'vendor/lib/vendor.libdpmframework.so',
            'vendor/lib64/vendor.libdpmframework.so',
        ): blob_fixup().replace_needed('libhidlbase.so', 'libhidlbase-v32.so'),
        'vendor/lib/vndk/libstagefright_omx.so': blob_fixup().replace_needed(
            'libstagefright_foundation.so', 'libstagefright_foundation-meizu.so',
        ),
        'vendor/bin/slim_daemon': blob_fixup().add_needed('libemutls_m2391.so'),
        'vendor/bin/hw/hostapd': blob_fixup().replace_needed(
            'libcrypto.so', 'libcrypto-v33.so',
        ),
        'vendor/bin/hw/wpa_supplicant.m2391': blob_fixup()
        .replace_needed('libcrypto.so', 'libcrypto-v33.so')
        .replace_needed('libkeystore-engine-wifi-hidl.so', 'libkeystore-engine-wifi-hidl-v33.so'),
        'vendor/lib64/libkeystore-engine-wifi-hidl-v33.so': blob_fixup()
        .replace_needed('libcrypto.so', 'libcrypto-v33.so'),
        # These proprietary HIDL interfaces have no source metadata for init's
        # checker. Keep their registration in the HAL, and start non-lazy ones
        # through their existing class rather than interface-triggered startup.
        'vendor/etc/init/qfp-daemon.rc': blob_fixup().regex_replace(
            r'(?m)^[ \t]*interface vendor\.qti\.hardware\.fingerprint@1\.0::.*\n', '',
        ),
        (
            'vendor/etc/init/vendor.aks.gamepad@1.0-service.rc',
            'vendor/etc/init/vendor.qti.hardware.wifi.wifilearner@1.0-service.rc',
        ): blob_fixup()
        .regex_replace(r'(?m)^[ \t]*interface .*\n', '')
        .regex_replace(r'(?m)^[ \t]*disabled\n', ''),
        # assemble_vintf supplies the version of the source-built policy.
        'vendor/etc/vintf/manifest_kalama.xml': blob_fixup().regex_replace(
            r'\s*<sepolicy>[\s\S]*?</sepolicy>', '',
        ),
        # V4 exports all Power symbols used by this service. The stock binary
        # redundantly links V3, which Soong rejects alongside V4.
        'vendor/bin/hw/android.hardware.power-service': blob_fixup()
        .remove_needed('android.hardware.power-V3-ndk.so'),
        # Keep the Meizu/QTI supplicant extensions without colliding with the
        # platform executable. Both stock service definitions use this path.
        (
            'vendor/etc/init/android.hardware.wifi.supplicant-service.rc',
            'vendor/etc/init/hw/init.qcom.rc',
        ): blob_fixup().regex_replace(
            '/vendor/bin/hw/wpa_supplicant ',
            '/vendor/bin/hw/wpa_supplicant.m2391 ',
        ),
    },
    extract_fns={
        r'^boot\.img$': extract_boot,
        r'^vendor_boot\.img$': extract_vendor_boot,
    },
)
# DTBO is extracted as firmware but installed by BOARD_PREBUILT_DTBOIMAGE.
module.add_proprietary_file(
    'proprietary-firmware.txt',
    vendor_rel_sub_path='radio',
    kind=ProprietaryFileType.FIRMWARE,
)

if __name__ == '__main__':
    ExtractUtils.device(module).run()
