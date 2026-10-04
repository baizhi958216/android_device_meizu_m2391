#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Retain the stock recovery device alias in full A/B OTA metadata only.

All target-files, partition images, VINTF checks and payloads are handled by
Lineage's unmodified ota_from_target_files. Remove this adapter when migration
from stock recovery (which reports meizu20Pro) is no longer supported.
"""
import argparse
import os
from pathlib import Path
import sys

parser = argparse.ArgumentParser(add_help=False)
parser.add_argument('-p', '--path', default=os.environ.get('ANDROID_HOST_OUT', 'out/host/linux-x86'))
options, _ = parser.parse_known_args()
sys.path.insert(0, str(Path(options.path).resolve() / 'bin/ota_from_target_files'))

import common
import ota_from_target_files as ota

native_metadata = ota.GetPackageMetadata


def device_metadata(target, previous=None):
    metadata = native_metadata(target, previous)
    if target.device == 'm2391' and previous is None:
        if 'meizu20Pro' not in metadata.precondition.device:
            metadata.precondition.device.append('meizu20Pro')
    return metadata


if __name__ == '__main__':
    try:
        common.CloseInheritedPipes()
        ota.GetPackageMetadata = device_metadata
        ota.main(sys.argv[1:])
    finally:
        common.Cleanup()
