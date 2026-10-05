#!/usr/bin/env -S PYTHONPATH=../../../tools/extract-utils python3
# SPDX-License-Identifier: Apache-2.0

import shutil
from pathlib import Path
from tempfile import TemporaryDirectory

from extract_utils.extract_recovery import extract_ramdisk, parse_mkbootimg_fragments, unpack_bootimg
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



module = ExtractUtilsModule(
    'm2391',
    'meizu',
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
