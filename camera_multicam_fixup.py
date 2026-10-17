# SPDX-License-Identifier: Apache-2.0
"""Restore the third MCX mode in the m2391 CHI override (arm64).

The logical camera lists UW/wide/tele default transition map only
registers UW/wide. Reuse the existing three-mode descriptors and tag table from
kailua_mcx_uw_as_primary, while retaining m2391 sensor order, primary camera,
calibration and other use cases. Set boundaries relative to the main lens.

The small entry prelude runs after ELF constructors and before camera enumeration.
It performs data stores only, then branches to the original entry body. BTI/PAC,
stack guards, original code addresses, notes and unwind tables remain intact.
Requires the stock triple SAT engine at the dual SAT load path; the original dual
zoom translator must be retained. Only this exact firmware blob is accepted.
"""
import hashlib
import struct
from pathlib import Path

STOCK_SHA256 = '474eeab7f1137ee02f04ac1edb8705913d015b703d9c1f18263c322a6e4a2b83'
PATCHED_SHA256 = '59fcec3c692e4ac9b7ffd7bc0950c51b182be0e22b472ebde6d97bf941bf9e23'
MARKER = b'M2391-MCX-3CAM-V1\0'
ENTRY_BRANCH = 0xd1a04
ENTRY_BODY = 0x3038b0
MODE_POINTER = 0x45c658
THREE_MODES = 0x4856e0
THREE_TAG_TABLE = 0x485728
THREE_ZOOM_VECTOR = 0x4856f8
DEFAULT_TABLE_POINTER = 0x489570
BOUNDARIES = ((.6, .9), (.9, .99), (.99, 1.9), (1.9, 2.1), (2.1, 10.))


def _align(value, alignment):
    return (value + alignment - 1) & -alignment


def _branch(pc, target):
    delta = target - pc
    if delta % 4 or not -(1 << 27) <= delta < (1 << 27):
        raise ValueError('AArch64 branch outside range')
    return 0x14000000 | ((delta >> 2) & 0x3ffffff)


def _adr(register, pc, target, page=False):
    delta = ((target >> 12) - (pc >> 12)) if page else target - pc
    if not -(1 << 20) <= delta < (1 << 20):
        raise ValueError('AArch64 ADR outside range')
    return (0x90000000 if page else 0x10000000) | ((delta & 3) << 29) | (((delta >> 2) & 0x7ffff) << 5) | register


def _add(register, immediate):
    return 0x91000000 | (immediate << 10) | (register << 5) | register


def _memory(register, base, offset=0, load=False):
    if offset % 8 or not 0 <= offset < 32768:
        raise ValueError('Unaligned AArch64 memory offset')
    return (0xf9400000 if load else 0xf9000000) | ((offset // 8) << 10) | (base << 5) | register


def _prelude(address):
    words = [0xd503245f]  # bti c
    def emit(word):
        words.append(word)
    def pointer(register, target):
        emit(_adr(register, address + len(words) * 4, target, page=True))
        emit(_add(register, target & 0xfff))
    pointer(9, DEFAULT_TABLE_POINTER)
    pointer(10, THREE_TAG_TABLE)
    emit(_memory(10, 9))
    pointer(9, THREE_ZOOM_VECTOR)
    emit(_memory(9, 9, load=True))
    literal_instruction = len(words)
    emit(0)  # adr x11, boundaries; resolved below
    for i in range(len(BOUNDARIES)):
        emit(_memory(10, 11, i * 8, load=True))
        emit(_memory(10, 9, i * 160))
        emit(_memory(31, 9, i * 160 + 8))  # Preserve zero hysteresis offsets.
    emit(_branch(address + len(words) * 4, ENTRY_BODY))
    literal_offset = _align(len(words) * 4, 8)
    words[literal_instruction] = _adr(11, address + literal_instruction * 4, address + literal_offset)
    code = b''.join(struct.pack('<I', word) for word in words)
    code += bytes(literal_offset - len(code))
    return code + b''.join(struct.pack('<ff', *bounds) for bounds in BOUNDARIES) + MARKER


def patch_override(data):
    digest = hashlib.sha256(data).hexdigest()
    if digest == PATCHED_SHA256:
        return data
    if digest != STOCK_SHA256:
        raise ValueError('Unknown CHI override: revalidate the MCX layout for this firmware')
    result = bytearray(data)
    if result[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', result, 18)[0] != 183:
        raise ValueError('Expected AArch64 little-endian ELF')
    phoff = struct.unpack_from('<Q', result, 32)[0]
    phsize, phcount = struct.unpack_from('<HH', result, 54)
    if phsize != 56 or phcount != 12:
        raise ValueError('Unexpected ELF program header layout')
    headers = [list(struct.unpack_from('<II6Q', result, phoff + i * phsize)) for i in range(phcount)]
    end = max(h[3] + h[6] for h in headers if h[0] == 1)
    segment_address = _align(end, 4096)
    segment_offset = _align(len(result), 4096)
    header_bytes = (phcount + 1) * phsize
    code_offset = _align(header_bytes, 16)
    code_address = segment_address + code_offset
    payload = _prelude(code_address)
    segment_size = code_offset + len(payload)
    # Relocate the program header table into the added RX mapping. Existing LOAD
    # segments and every original section keep their addresses and file offsets.
    if headers[0][0] != 6:
        raise ValueError('Missing PT_PHDR')
    headers[0] = [6, 4, segment_offset, segment_address, segment_address, header_bytes, header_bytes, 8]
    headers.append([1, 5, segment_offset, segment_address, segment_address, segment_size, segment_size, 4096])
    struct.pack_into('<Q', result, 32, segment_offset)
    struct.pack_into('<H', result, 56, phcount + 1)
    if struct.unpack_from('<I', result, ENTRY_BRANCH)[0] != _branch(ENTRY_BRANCH, ENTRY_BODY):
        raise ValueError('Unexpected override entry instruction')
    if struct.unpack_from('<Q', result, MODE_POINTER)[0] != 0x486898:
        raise ValueError('Unexpected mode descriptor pointer')
    struct.pack_into('<I', result, ENTRY_BRANCH, _branch(ENTRY_BRANCH, code_address))
    struct.pack_into('<Q', result, MODE_POINTER, THREE_MODES)
    result += bytes(segment_offset - len(result))
    result += b''.join(struct.pack('<II6Q', *h) for h in headers)
    result += bytes(code_offset - header_bytes)
    result += payload
    return bytes(result)


def fix_camera_multicam(ctx, file, file_path, **kwargs):
    path = Path(file_path)
    path.write_bytes(patch_override(path.read_bytes()))


if __name__ == '__main__':
    import sys
    Path(sys.argv[2]).write_bytes(patch_override(Path(sys.argv[1]).read_bytes()))
