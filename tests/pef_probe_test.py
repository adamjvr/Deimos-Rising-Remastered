#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("deimos_pef_probe", ROOT / "tools" / "pef_probe.py")
assert spec and spec.loader
pef = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = pef
spec.loader.exec_module(pef)


def vararg(value: int) -> bytes:
    groups = [value & 0x7F]
    value >>= 7
    while value:
        groups.append(value & 0x7F)
        value >>= 7
    groups.reverse()
    return bytes((x | 0x80) if i + 1 < len(groups) else x for i, x in enumerate(groups))


def main() -> None:
    packed = bytearray()
    packed += bytes([0b000_00011])                       # zero(3)
    packed += bytes([0b001_00100]) + b"ABCD"           # blockCopy(4)
    packed += bytes([0b010_00010]) + vararg(2) + b"xy" # repeat 2 bytes 3 times
    packed += bytes([0b011_00001]) + vararg(2) + vararg(2) + b"Cabde"
    packed += bytes([0b100_00001]) + vararg(1) + vararg(2) + b"XY"
    expected = b"\0\0\0ABCDxyxyxyCabCdeC\0X\0Y\0"
    decoded, stats = pef.unpack_pattern_data(bytes(packed), len(expected))
    assert decoded == expected
    assert stats == {
        "zero": 1,
        "block_copy": 1,
        "repeated_block": 1,
        "interleave_block_copy": 1,
        "interleave_zero": 1,
    }

    # A minimal relocation program: one 8-byte transition vector followed by
    # one imported pointer.  Synthetic addresses make the result deterministic.
    sections = [
        pef.Section(0, -1, 0, 32, 32, 32, 0, pef.SECTION_CODE, 4, 4),
        pef.Section(1, -1, 0, 32, 32, 32, 0, pef.SECTION_UNPACKED_DATA, 1, 4),
    ]
    images = {0: bytearray(32), 1: bytearray(32)}
    imports = [{"name": "ExampleImport", "library": "ExampleLib"}]
    reloc = pef.Relocator(sections, images, imports, {0: 0x10000000, 1: 0x20000000})
    tvector8_run1 = 0x4000 | (3 << 9) | 0
    import_run1 = 0x4000 | (5 << 9) | 0
    reloc.execute([tvector8_run1, import_run1], 1)
    assert pef.be32(images[1], 0) == 0x10000000
    assert pef.be32(images[1], 4) == 0x20000000
    assert pef.be32(images[1], 8) == 0xE0000000
    assert reloc.import_index == 1
    assert len(reloc.fixups) == 3

    print("pef_probe_test PASS")


if __name__ == "__main__":
    main()
