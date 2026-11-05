# SPDX-License-Identifier: Apache-2.0
"""Make the OEM UCI transport pass its actual receive capacity to read(2).

phTmlUwb_spi_read(handle, buffer, capacity) overwrites capacity with 4, but
expects the shipping kernel to copy and return an entire UCI packet anyway.
That violates read(2)'s buffer bound. The TML reader supplies a 4200-byte
buffer and capacity; retain that argument instead of reinstating the kernel
overflow. Firmware download uses separate raw read calls and is unchanged.

Only the two audited stock blobs are accepted. Instruction sizes, function
addresses, branch targets, unwind metadata and BTI/PAC remain unchanged.
"""
import hashlib
from pathlib import Path


# SHA-256, file offset, original instruction, NOP. ARM32 .text VA = file + 0x1000.
PATCHES = (
    ('c838b296ca65794747d3b566f212c2809f2f8f812080afb5ee29f92288a207b1',
     0x13258, bytes.fromhex('04 22'), bytes.fromhex('00 bf')),
    ('a7b42e3d23bd49d6203cc2d3dfc478fd155a8a9cd20d31526ec23697227c9ec7',
     0x1afa0, bytes.fromhex('82 00 80 52'), bytes.fromhex('1f 20 03 d5')),
)


def patch_uwb_hal(data):
    digest = hashlib.sha256(data).hexdigest()
    for stock_hash, offset, before, after in PATCHES:
        if digest == stock_hash:
            if data[offset:offset + len(before)] != before:
                raise ValueError('Unexpected UWB SPI read instruction')
            return data[:offset] + after + data[offset + len(before):]
        # Accept only an exact prior application of this patch.
        if data[offset:offset + len(after)] == after:
            restored = data[:offset] + before + data[offset + len(after):]
            if hashlib.sha256(restored).hexdigest() == stock_hash:
                return data
    raise ValueError('Unknown UWB HAL: revalidate the receive buffer and instruction')


def fix_uwb_hal(ctx, file, file_path, **kwargs):
    path = Path(file_path)
    path.write_bytes(patch_uwb_hal(path.read_bytes()))


if __name__ == '__main__':
    import sys
    Path(sys.argv[2]).write_bytes(patch_uwb_hal(Path(sys.argv[1]).read_bytes()))
