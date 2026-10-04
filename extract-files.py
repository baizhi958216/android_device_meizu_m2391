#!/usr/bin/env -S PYTHONPATH=../../../tools/extract-utils python3
# SPDX-License-Identifier: Apache-2.0

import json
import re
import shutil
from pathlib import Path
from tempfile import TemporaryDirectory

from extract_utils.extract_recovery import (
    extract_ramdisk,
    parse_mkbootimg_fragments,
    unpack_bootimg,
)
from extract_utils.main import ExtractUtils, ExtractUtilsModule


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


def include_packages(ctx, names):
    if len(names) != len(set(names)):
        raise ValueError('Duplicate generated module names')
    continuation = ' \\' + '\n    '
    ctx.product_mk_out.write('\nPRODUCT_PACKAGES +=' + continuation + continuation.join(names) + '\n')


def write_symlinks(ctx, packages_ctx):
    """Keep relative and dangling stock links; they are not copyable blobs."""
    names = []
    source = Path(__file__).parent / 'configs/vendor-symlinks.txt'
    for line in source.read_text().splitlines():
        if not line or line.startswith('#'):
            continue
        path, target = line.split('|', 1)
        # These links are installed by the source-built toolbox_vendor module.
        if target == 'toolbox' and path.rsplit('/', 1)[-1] in {
            'getevent', 'getprop', 'modprobe', 'setprop', 'start', 'stop',
        }:
            continue
        partition, location = path.split('/', 1)
        if partition != 'vendor':
            raise ValueError(f'Unexpected symlink partition: {path}')
        name = 'm2391_link_' + re.sub(r'[^a-zA-Z0-9_]', '_', location)
        # JSON strings are valid Blueprint string literals.
        ctx.bp_out.write('\ninstall_symlink {\n'
                         f'    name: {json.dumps(name)},\n'
                         '    soc_specific: true,\n'
                         f'    installed_location: {json.dumps(location)},\n'
                         f'    symlink_target: {json.dumps(target)},\n'
                         '}\n')
        names.append(name)
    include_packages(ctx, names)


def write_stock_overlays(ctx, packages_ctx):
    # Preserve signed, compiled stock RROs at /vendor/overlay, not /vendor/app.
    names = []
    for source in sorted(Path(packages_ctx.vendor_prop_path, 'vendor/overlay').glob('*.apk')):
        name = 'm2391_' + source.stem
        ctx.bp_out.write('\nprebuilt_overlay {\n'
                         f'    name: {json.dumps(name)},\n'
                         '    soc_specific: true,\n'
                         f'    src: "proprietary/vendor/overlay/{source.name}",\n'
                         f'    filename: {json.dumps(source.name)},\n'
                         '}\n')
        names.append(name)
    include_packages(ctx, names)


module = ExtractUtilsModule(
    'm2391',
    'meizu',
    extract_fns={
        r'^boot\.img$': extract_boot,
        r'^vendor_boot\.img$': extract_vendor_boot,
    },
    add_firmware_proprietary_file=True,
)
module.proprietary_files[-1].add_post_makefile_generation_fn(write_symlinks)
module.proprietary_files[-1].add_post_makefile_generation_fn(write_stock_overlays)

if __name__ == '__main__':
    ExtractUtils.device(module).run()
