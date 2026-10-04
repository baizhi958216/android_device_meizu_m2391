#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Device adapter invoked by the standard LineageOS OTA/bacon build rule."""
import hashlib
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile

os.environ['PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION'] = 'python'
source = Path.cwd()
host = source / 'out/host/linux-x86'
sys.path.insert(0, str(host / 'bin/ota_from_target_files'))
import ota_from_target_files as ota
import common
import ota_utils
import check_target_files_vintf
import apex_utils
import ota_metadata_pb2

parts = {'boot', 'init_boot', 'vendor_boot', 'dtbo', 'vendor', 'odm',
         'vendor_dlkm', 'system_dlkm', 'system', 'system_ext', 'product',
         'vbmeta', 'vbmeta_system', 'recovery'}


def digest(path, length=None):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        while length is None or length:
            data = stream.read(8 << 20 if length is None else min(8 << 20, length))
            if not data:
                if length:
                    raise ValueError('Truncated image: ' + str(path))
                break
            result.update(data)
            if length is not None:
                length -= len(data)
    return result.hexdigest()


def payload_size(path):
    with path.open('rb') as stream:
        stream.seek(-64, 2)
        footer = stream.read(64)
    if footer[:4] != b'AVBf':
        raise ValueError('Missing stock AVB footer: ' + str(path))
    return struct.unpack_from('>Q', footer, 12)[0]


def read_info(path):
    return dict(line.split('=', 1) for line in path.read_text().splitlines() if '=' in line)


def write_info(path, info):
    path.write_text(''.join(f'{key}={value}\n' for key, value in sorted(info.items())))


native_metadata = ota.GetPackageMetadata


def device_metadata(target, previous=None):
    if target.device != 'm2391' or previous is not None:
        raise ValueError('This build adapter supports full m2391 OTA only')
    metadata = native_metadata(target, previous)
    if metadata.wipe or metadata.downgrade:
        raise ValueError('The full OTA must not request a wipe/downgrade')
    if 'meizu20Pro' not in metadata.precondition.device:
        metadata.precondition.device.append('meizu20Pro')
    return metadata


def main():
    argv = sys.argv[1:]
    if len(argv) < 2 or not Path(argv[-2]).is_dir():
        raise ValueError('Expected the native target-files directory and OTA output')
    original = Path(argv[-2]).resolve()
    # The build has already copied the prebuilts and built the framework images.
    inventory = source / 'vendor/meizu/m2391/proprietary-files.txt'
    originals = {}
    for line in inventory.read_text().splitlines():
        if not line or line.startswith('#'):
            continue
        _, destination, size, checksum = line.split('|')
        path = source / destination
        if path.stat().st_size != int(size) or digest(path) != checksum:
            raise ValueError('Prebuilt input mismatch: ' + destination)
        if path.suffix == '.img':
            originals[path.stem] = path
    images = original / 'IMAGES'
    if any(not (images / (part + '.img')).is_file() for part in parts):
        raise ValueError('Native target-files is missing a required partition image')
    ramdisk = original / 'RECOVERY/RAMDISK'
    for name in ('system/bin/recovery', 'system/bin/init', 'system/bin/adbd',
                 'system/bin/sh', 'system/etc/init/hw/init.rc'):
        if not (ramdisk / name).is_file():
            raise ValueError('Lineage Recovery ramdisk is incomplete: ' + name)
    for part, stock in originals.items():
        built = images / stock.name
        if part in {'boot', 'init_boot', 'vendor_boot', 'dtbo'}:
            size = payload_size(stock)
            if payload_size(built) != size or digest(stock, size) != digest(built, size):
                raise ValueError('Stock boot payload changed: ' + part)
        elif digest(stock) != digest(built):
            raise ValueError('Stock image changed: ' + part)

    # Do not change the native target-files tree while other build targets use it.
    with tempfile.TemporaryDirectory(prefix='m2391-ota-') as temporary:
        target = Path(temporary) / 'target'
        shutil.copytree(original, target, symlinks=True)
        for part in ('vendor', 'odm'):
            image = target / 'IMAGES' / (part + '.img')
            with image.open('rb') as stream:
                sparse = stream.read(4) == struct.pack('<I', 0xed26ff3a)
            if sparse:
                raw = Path(temporary) / (part + '.raw.img')
                subprocess.run([host / 'bin/simg2img', image, raw], check=True)
                image = raw
            tree = target / part.upper()
            if tree.exists():
                shutil.rmtree(tree)
            subprocess.run([host / 'bin/fsck.erofs', '--extract=' + str(tree),
                            '--no-preserve', image], check=True)
        misc = target / 'META/misc_info.txt'
        info = read_info(misc)
        if any(info.get(key) != 'true' for key in ('ab_update', 'use_dynamic_partitions', 'vintf_enforce')):
            raise ValueError('Native A/B, dynamic partitions and VINTF enforcement are required')
        ab = (target / 'META/ab_partitions.txt').read_text().split()
        if len(ab) != len(parts) or set(ab) != parts:
            raise ValueError('Native A/B partition set differs from this product')
        # Select the stock SKU rather than accidentally checking an empty manifest.
        info.update(vintf_vendor_manifest_skus='kalama', vintf_odm_manifest_skus='kalama',
                    vintf_include_empty_vendor_sku='false', vintf_include_empty_odm_sku='false',
                    virtual_ab_cow_version='2', virtual_ab_compression_method='gz')
        write_info(misc, info)
        for name in ('kernel_version.txt', 'kernel_configs.txt'):
            if not (target / 'META' / name).is_file():
                raise ValueError('Native kernel compatibility metadata missing: ' + name)
        dynamic_file = target / 'META/dynamic_partitions_info.txt'
        dynamic = read_info(dynamic_file)
        dynamic.update(virtual_ab_cow_version='2', virtual_ab_compression_method='gz')
        write_info(dynamic_file, dynamic)
        common.OPTIONS.search_path = str(host)
        apex = ota_metadata_pb2.ApexMetadata()
        apex.apex_info.extend(apex_utils.GetApexInfoFromTargetFiles(str(target)))
        (target / 'META/apex_info.pb').write_bytes(apex.SerializeToString())
        ota_utils.UNZIP_PATTERN.extend(check_target_files_vintf.GetVintfApexUnzipPatterns())
        ota.GetPackageMetadata = device_metadata
        ota.main([*argv[:-2], '--java_path', str(source / 'prebuilts/jdk/jdk21/linux-x86/bin/java'),
                  '--vabc_cow_version', '2', '--vabc_compression_param', 'gz',
                  '--enable_vabc_xor', 'false', str(target), argv[-1]])


try:
    common.CloseInheritedPipes()
    main()
finally:
    common.Cleanup()
