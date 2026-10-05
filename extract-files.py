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
from extract_utils.module import FirmwareProprietaryFile


class NativeImageFirmware(FirmwareProprietaryFile):
    def write_makefiles(self, module, ctx):
        # Extraction still treats DTBO as firmware. BOARD_PREBUILT_DTBOIMAGE
        # owns its image/AVB/OTA rules, so add-radio-file must not install it too.
        if any(file.dst != 'dtbo.img' for file in self.file_list.all_files):
            raise ValueError('Review native image rules before adding firmware')


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
    native_source = source.with_name('native-symlinks.txt')
    native = dict(line.split('|', 1) for line in native_source.read_text().splitlines()
                  if line and not line.startswith('#'))
    reused = set()
    for line in source.read_text().splitlines():
        if not line or line.startswith('#'):
            continue
        path, target = line.split('|', 1)
        if path in native:
            reused.add(path)
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
    if reused != native.keys():
        raise ValueError(f'Native links missing from stock inventory: {native.keys() - reused}')
    include_packages(ctx, names + sorted(set(native.values())))


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


def write_stock_etc(ctx, packages_ctx):
    """Select stock policy/configuration through native prebuilt selection."""
    # Soong supports third-party plugins under vendor/. Keep their sources in
    # the device tree and reproduce them with the generated vendor definitions.
    plugin_source = Path(__file__).parent / 'build/soong'
    plugin_destination = Path(packages_ctx.vendor_prop_path).parent / 'build/soong'
    plugin_destination.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(plugin_source / 'Android.bp.in', plugin_destination / 'Android.bp')
    shutil.copyfile(plugin_source / 'stock_etc.go', plugin_destination / 'stock_etc.go')
    entries = json.loads((Path(__file__).parent /
                          'configs/stock-etc-modules.json').read_text())
    for entry in entries:
        path = Path(entry['path'])
        if path.parts[:2] != ('vendor', 'etc'):
            raise ValueError(f'Unexpected stock configuration path: {path}')
        kind = 'm2391_prebuilt_etc_common' if entry['common'] else 'm2391_prebuilt_etc'
        properties = {
            'name': entry['module'],
            'src': 'proprietary/' + str(path),
            'filename': path.name,
            'relative_install_path': str(path.parent.relative_to('vendor/etc')),
            'soc_specific': True,
            'prefer': True,
        }
        ctx.bp_out.write('\n' + kind + ' {\n')
        for key, value in properties.items():
            ctx.bp_out.write(f'    {key}: {json.dumps(value)},\n')
        ctx.bp_out.write('}\n')
    # This file is a 64-bit ELF despite its stock lib/ location. Preserve the
    # original file and directory without creating a false 32-bit link target.
    ctx.bp_out.write('\nm2391_prebuilt_lib {\n'
                     '    name: "m2391_qesdk_vendor_lib",\n'
                     '    src: "proprietary/vendor/lib/libqti-qesdk-secure.so",\n'
                     '    filename: "libqti-qesdk-secure.so",\n'
                     '    soc_specific: true,\n'
                     '}\n')
    include_packages(ctx, [entry['module'] for entry in entries] +
                     ['m2391_qesdk_vendor_lib'])


module = ExtractUtilsModule(
    'm2391',
    'meizu',
    extract_fns={
        r'^boot\.img$': extract_boot,
        r'^vendor_boot\.img$': extract_vendor_boot,
    },
)
module.proprietary_files.insert(0, NativeImageFirmware(
    module.proprietary_file_path('proprietary-firmware.txt')))
module.proprietary_files[-1].add_post_makefile_generation_fn(write_symlinks)
module.proprietary_files[-1].add_post_makefile_generation_fn(write_stock_overlays)
module.proprietary_files[-1].add_post_makefile_generation_fn(write_stock_etc)

if __name__ == '__main__':
    ExtractUtils.device(module).run()
