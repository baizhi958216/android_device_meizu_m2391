# SPDX-License-Identifier: Apache-2.0
"""Keep the stock IMS implementation but remove its QSSI overlay selection gate."""
import struct
import zipfile
from pathlib import Path


def remove_ims_overlay(ctx, file, file_path, **kwargs):
    # The stock IMS APK also declares itself an overlay gated on a QSSI boot
    # property. PackageParser skips the entire APK when that property is absent.
    # Retain its application, resources and DEX; Soong signs the result with the
    # platform certificate. No vendor boot property is forged for this purpose.
    with zipfile.ZipFile(file_path) as archive:
        entries = [(info, archive.read(info.filename)) for info in archive.infolist()]
    manifest = next(data for info, data in entries if info.filename == 'AndroidManifest.xml')
    kind, header, size = struct.unpack_from('<HHI', manifest)
    if kind != 3 or size != len(manifest):
        raise ValueError('Unexpected IMS binary XML header')
    strings = []
    chunks = []
    offset = header
    removed = 0
    depth = 0
    while offset < len(manifest):
        kind, chunk_header, length = struct.unpack_from('<HHI', manifest, offset)
        if length < chunk_header or length < 8 or offset + length > len(manifest):
            raise ValueError('Invalid IMS manifest chunk')
        chunk = manifest[offset:offset + length]
        if kind == 1:
            count, _, flags, start = struct.unpack_from('<IIII', chunk, 8)
            for i in range(count):
                pos = start + struct.unpack_from('<I', chunk, chunk_header + 4 * i)[0]
                if flags & 0x100:
                    # UTF-8 pools have UTF-16 character and UTF-8 byte lengths.
                    for _ in range(2):
                        n = chunk[pos]; pos += 1
                        if n & 0x80:
                            n = ((n & 0x7f) << 8) | chunk[pos]; pos += 1
                    strings.append(chunk[pos:pos + n].decode('utf-8'))
                else:
                    n = struct.unpack_from('<H', chunk, pos)[0]; pos += 2
                    if n & 0x8000:
                        n = ((n & 0x7fff) << 16) | struct.unpack_from('<H', chunk, pos)[0]; pos += 2
                    strings.append(chunk[pos:pos + n * 2].decode('utf-16le'))
        if kind == 0x102:
            name = strings[struct.unpack_from('<I', chunk, 20)[0]]
            if depth or name == 'overlay':
                if not depth:
                    removed += 1
                depth += 1
                offset += length
                continue
        if depth:
            if kind == 0x103:
                depth -= 1
        else:
            chunks.append(chunk)
        offset += length
    if removed != 1 or depth:
        raise ValueError('Expected exactly one complete stock IMS overlay element')
    patched = bytearray(manifest[:header] + b''.join(chunks))
    struct.pack_into('<I', patched, 4, len(patched))
    temporary = Path(file_path + '.ims-tmp')
    with zipfile.ZipFile(temporary, 'w') as archive:
        for info, data in entries:
            # The original signing data is no longer valid after the manifest edit.
            if info.filename.startswith('META-INF/'):
                continue
            archive.writestr(info, patched if info.filename == 'AndroidManifest.xml' else data)
    temporary.replace(file_path)
