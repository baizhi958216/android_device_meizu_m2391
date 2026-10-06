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
    namespace_imports=['hardware/qcom-caf/bootctrl'],
    blob_fixups={
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
