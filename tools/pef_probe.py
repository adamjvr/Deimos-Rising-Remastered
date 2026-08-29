#!/usr/bin/env python3
"""Read-only PEF container, packed-data, import, and relocation probe.

This tool is intentionally independent of the remaster runtime.  It exists to
turn classic Mac OS PEF evidence into reproducible structural observations.
It can unpack pattern-initialized data and execute the documented PEF
relocation bytecode against synthetic section/import addresses so that TOC,
transition-vector, and pointer relationships can be inspected without running
original code.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
import struct
from pathlib import Path
from typing import Any

PEF_TAGS = b"Joy!peff"
SECTION_CODE = 0
SECTION_UNPACKED_DATA = 1
SECTION_PATTERN_DATA = 2
SECTION_CONSTANT = 3
SECTION_LOADER = 4
SECTION_EXEC_DATA = 6
INSTANTIATED_KINDS = {
    SECTION_CODE,
    SECTION_UNPACKED_DATA,
    SECTION_PATTERN_DATA,
    SECTION_CONSTANT,
    SECTION_EXEC_DATA,
}


def be16(b: bytes | bytearray, o: int) -> int:
    return struct.unpack_from(">H", b, o)[0]


def be32(b: bytes | bytearray, o: int) -> int:
    return struct.unpack_from(">I", b, o)[0]


def s32(b: bytes | bytearray, o: int) -> int:
    return struct.unpack_from(">i", b, o)[0]


def put_be32(b: bytearray, o: int, value: int) -> None:
    struct.pack_into(">I", b, o, value & 0xFFFFFFFF)


def c_string(b: bytes, o: int) -> str:
    if o < 0 or o >= len(b):
        return ""
    end = b.find(b"\0", o)
    if end < 0:
        end = len(b)
    return b[o:end].decode("mac_roman", "replace")


@dataclass(frozen=True)
class Section:
    index: int
    name_offset: int
    default_address: int
    total_size: int
    unpacked_size: int
    packed_size: int
    container_offset: int
    section_kind: int
    share_kind: int
    alignment: int

    @property
    def instantiated(self) -> bool:
        return self.section_kind in INSTANTIATED_KINDS

    def as_dict(self) -> dict[str, Any]:
        return {
            "index": self.index,
            "name_offset": self.name_offset,
            "default_address": self.default_address,
            "total_size": self.total_size,
            "unpacked_size": self.unpacked_size,
            "packed_size": self.packed_size,
            "container_offset": self.container_offset,
            "section_kind": self.section_kind,
            "share_kind": self.share_kind,
            "alignment": self.alignment,
            "instantiated": self.instantiated,
        }


def parse_sections(b: bytes) -> list[Section]:
    section_count = be16(b, 32)
    sections: list[Section] = []
    for i in range(section_count):
        o = 40 + i * 28
        sections.append(
            Section(
                index=i,
                name_offset=s32(b, o),
                default_address=be32(b, o + 4),
                total_size=be32(b, o + 8),
                unpacked_size=be32(b, o + 12),
                packed_size=be32(b, o + 16),
                container_offset=be32(b, o + 20),
                section_kind=b[o + 24],
                share_kind=b[o + 25],
                alignment=b[o + 26],
            )
        )
    return sections


def read_pidata_arg(src: bytes, pos: int, inline_count: int | None = None) -> tuple[int, int]:
    """Read the PEF 7-bit big-endian packed-data argument encoding."""
    if inline_count is not None and inline_count != 0:
        return inline_count, pos
    value = 0
    for _ in range(5):
        if pos >= len(src):
            raise ValueError("truncated pattern-data argument")
        byte = src[pos]
        pos += 1
        value = ((value << 7) | (byte & 0x7F)) & 0xFFFFFFFF
        if not (byte & 0x80):
            return value, pos
    raise ValueError("pattern-data argument exceeds 5 bytes")


def unpack_pattern_data(src: bytes, expected_size: int | None = None) -> tuple[bytes, dict[str, int]]:
    """Execute PEF pattern-initialization bytecode.

    Opcode semantics follow Inside Macintosh: Mac OS Runtime Architectures,
    Chapter 8.  The returned bytes are the initialized portion only; callers
    may append zero-fill up to the section's totalSize for its BSS tail.
    """
    pos = 0
    out = bytearray()
    counts = {
        "zero": 0,
        "block_copy": 0,
        "repeated_block": 0,
        "interleave_block_copy": 0,
        "interleave_zero": 0,
    }

    def take(n: int) -> bytes:
        nonlocal pos
        if n < 0 or pos + n > len(src):
            raise ValueError("pattern-data raw block exceeds packed section")
        data = src[pos : pos + n]
        pos += n
        return data

    while pos < len(src):
        instruction = src[pos]
        pos += 1
        opcode = instruction >> 5
        first, pos = read_pidata_arg(src, pos, instruction & 0x1F)

        if opcode == 0:  # zero(count)
            counts["zero"] += 1
            out.extend(b"\0" * first)
        elif opcode == 1:  # blockCopy(blockSize, raw)
            counts["block_copy"] += 1
            out.extend(take(first))
        elif opcode == 2:  # repeatedBlock(blockSize, repeatCount-1, raw)
            counts["repeated_block"] += 1
            repeat_minus_one, pos = read_pidata_arg(src, pos)
            raw = take(first)
            out.extend(raw * (repeat_minus_one + 1))
        elif opcode == 3:  # interleave common + custom
            counts["interleave_block_copy"] += 1
            custom_size, pos = read_pidata_arg(src, pos)
            repeat_count, pos = read_pidata_arg(src, pos)
            common = take(first)
            out.extend(common)
            for _ in range(repeat_count):
                out.extend(take(custom_size))
                out.extend(common)
        elif opcode == 4:  # interleave zero + custom
            counts["interleave_zero"] += 1
            custom_size, pos = read_pidata_arg(src, pos)
            repeat_count, pos = read_pidata_arg(src, pos)
            zeros = b"\0" * first
            out.extend(zeros)
            for _ in range(repeat_count):
                out.extend(take(custom_size))
                out.extend(zeros)
        else:
            raise ValueError(f"reserved pattern-data opcode {opcode} at packed offset {pos - 1:#x}")

        if expected_size is not None and len(out) > expected_size:
            raise ValueError(
                f"pattern-data output exceeded declared unpacked size: {len(out)} > {expected_size}"
            )

    if expected_size is not None and len(out) != expected_size:
        raise ValueError(
            f"pattern-data output size mismatch: got {len(out)}, expected {expected_size}"
        )
    return bytes(out), counts


def section_contents(b: bytes, section: Section) -> tuple[bytearray, dict[str, int] | None]:
    if not section.instantiated:
        return bytearray(), None
    raw = b[section.container_offset : section.container_offset + section.packed_size]
    if len(raw) != section.packed_size:
        raise ValueError(f"section {section.index} is truncated")
    pidata_counts = None
    if section.section_kind == SECTION_PATTERN_DATA:
        initialized, pidata_counts = unpack_pattern_data(raw, section.unpacked_size)
    else:
        if section.unpacked_size != section.packed_size:
            raise ValueError(
                f"unsupported non-pattern packed section {section.index}: "
                f"packed={section.packed_size}, unpacked={section.unpacked_size}"
            )
        initialized = raw
    if len(initialized) != section.unpacked_size:
        raise ValueError(f"section {section.index}: initialized size mismatch")
    image = bytearray(initialized)
    if section.total_size < len(image):
        raise ValueError(f"section {section.index}: totalSize smaller than unpackedSize")
    image.extend(b"\0" * (section.total_size - len(image)))
    return image, pidata_counts


def loader_info(b: bytes, sections: list[Section]) -> dict[str, Any] | None:
    loader = next((s for s in sections if s.section_kind == SECTION_LOADER), None)
    if loader is None:
        return None
    base = loader.container_offset
    if loader.packed_size < 56:
        raise ValueError("truncated loader header")
    header = {
        "main_section": s32(b, base),
        "main_offset": be32(b, base + 4),
        "init_section": s32(b, base + 8),
        "init_offset": be32(b, base + 12),
        "term_section": s32(b, base + 16),
        "term_offset": be32(b, base + 20),
        "imported_library_count": be32(b, base + 24),
        "total_imported_symbol_count": be32(b, base + 28),
        "reloc_section_count": be32(b, base + 32),
        "reloc_instr_offset": be32(b, base + 36),
        "loader_strings_offset": be32(b, base + 40),
        "export_hash_offset": be32(b, base + 44),
        "export_hash_table_power": be32(b, base + 48),
        "exported_symbol_count": be32(b, base + 52),
    }
    loader_blob = b[base : base + loader.packed_size]
    strings_base = header["loader_strings_offset"]

    libraries = []
    libraries_start = 56
    for i in range(header["imported_library_count"]):
        o = libraries_start + i * 24
        if o + 24 > len(loader_blob):
            raise ValueError("imported library table exceeds loader section")
        name_off = be32(loader_blob, o)
        libraries.append(
            {
                "index": i,
                "name_offset": name_off,
                "name": c_string(loader_blob, strings_base + name_off),
                "old_import_version": be32(loader_blob, o + 4),
                "current_version": be32(loader_blob, o + 8),
                "imported_symbol_count": be32(loader_blob, o + 12),
                "first_imported_symbol": be32(loader_blob, o + 16),
                "options": loader_blob[o + 20],
            }
        )

    imports_start = libraries_start + len(libraries) * 24
    imports = []
    for i in range(header["total_imported_symbol_count"]):
        o = imports_start + i * 4
        if o + 4 > len(loader_blob):
            raise ValueError("imported symbol table exceeds loader section")
        raw = be32(loader_blob, o)
        symbol_class = (raw >> 24) & 0xFF
        name_off = raw & 0x00FFFFFF
        library_index = None
        for lib in libraries:
            first = lib["first_imported_symbol"]
            count = lib["imported_symbol_count"]
            if first <= i < first + count:
                library_index = lib["index"]
                break
        imports.append(
            {
                "index": i,
                "class_byte": symbol_class,
                "symbol_class": symbol_class & 0x0F,
                "weak": bool(symbol_class & 0x80),
                "name_offset": name_off,
                "name": c_string(loader_blob, strings_base + name_off),
                "library_index": library_index,
                "library": libraries[library_index]["name"] if library_index is not None else None,
            }
        )

    reloc_headers_start = imports_start + len(imports) * 4
    reloc_headers = []
    for i in range(header["reloc_section_count"]):
        o = reloc_headers_start + i * 12
        if o + 12 > len(loader_blob):
            raise ValueError("relocation header table exceeds loader section")
        reloc_headers.append(
            {
                "index": i,
                "section_index": be16(loader_blob, o),
                "reserved": be16(loader_blob, o + 2),
                "reloc_count_blocks": be32(loader_blob, o + 4),
                "first_reloc_offset": be32(loader_blob, o + 8),
            }
        )

    return {
        "section_index": loader.index,
        "header": header,
        "libraries": libraries,
        "imports": imports,
        "relocation_headers": reloc_headers,
        "loader_blob": loader_blob,
    }


class Relocator:
    """Execute PEF relocation bytecode against synthetic runtime addresses."""

    def __init__(
        self,
        sections: list[Section],
        images: dict[int, bytearray],
        imports: list[dict[str, Any]],
        section_bases: dict[int, int],
    ) -> None:
        self.sections = sections
        self.images = images
        self.imports = imports
        self.section_bases = section_bases
        self.fixups: list[dict[str, Any]] = []
        self.instruction_counts: dict[str, int] = {}
        self.section_c = self._section_delta(0)
        self.section_d = self._section_delta(1)
        self.import_index = 0
        self.reloc_pos = 0
        self.target_section = -1

    def _section_delta(self, index: int) -> int:
        if index < 0 or index >= len(self.sections) or not self.sections[index].instantiated:
            return 0
        return (self.section_bases[index] - self.sections[index].default_address) & 0xFFFFFFFF

    def _import_address(self, index: int) -> int:
        if index < 0 or index >= len(self.imports):
            raise ValueError(f"import index {index} out of range")
        # Synthetic and unmistakable.  The fixup log carries the real symbol identity.
        return (0xE0000000 + index * 4) & 0xFFFFFFFF

    def _bump(self, name: str) -> None:
        self.instruction_counts[name] = self.instruction_counts.get(name, 0) + 1

    def _add_word(self, addend: int, kind: str, **extra: Any) -> None:
        image = self.images[self.target_section]
        if self.reloc_pos < 0 or self.reloc_pos + 4 > len(image):
            raise ValueError(
                f"relocation address {self.reloc_pos:#x} outside section {self.target_section}"
            )
        before = be32(image, self.reloc_pos)
        after = (before + addend) & 0xFFFFFFFF
        put_be32(image, self.reloc_pos, after)
        row = {
            "section_index": self.target_section,
            "offset": self.reloc_pos,
            "kind": kind,
            "before": before,
            "addend": addend,
            "after": after,
        }
        row.update(extra)
        self.fixups.append(row)
        self.reloc_pos += 4

    def _set_section(self, which: str, index: int) -> None:
        value = self._section_delta(index)
        if which == "C":
            self.section_c = value
        else:
            self.section_d = value

    def _execute_blocks(self, words: list[int], start: int, end: int, allow_repeat: bool) -> int:
        i = start
        while i < end:
            word = words[i]
            top7 = word >> 9

            if (word >> 14) == 0b00:
                self._bump("RelocBySectDWithSkip")
                skip_count = (word >> 6) & 0xFF
                reloc_count = word & 0x3F
                self.reloc_pos += skip_count * 4
                for _ in range(reloc_count):
                    self._add_word(self.section_d, "section_d")
                i += 1
                continue

            if (word >> 13) == 0b010:
                sub = (word >> 9) & 0xF
                run = (word & 0x1FF) + 1
                names = {
                    0: "RelocBySectC",
                    1: "RelocBySectD",
                    2: "RelocTVector12",
                    3: "RelocTVector8",
                    4: "RelocVTable8",
                    5: "RelocImportRun",
                }
                if sub not in names:
                    raise ValueError(f"reserved RelocateValue subopcode {sub} at block {i}")
                self._bump(names[sub])
                if sub == 0:
                    for _ in range(run):
                        self._add_word(self.section_c, "section_c")
                elif sub == 1:
                    for _ in range(run):
                        self._add_word(self.section_d, "section_d")
                elif sub == 2:
                    for _ in range(run):
                        self._add_word(self.section_c, "tvector_code")
                        self._add_word(self.section_d, "tvector_toc")
                        self.reloc_pos += 4
                elif sub == 3:
                    for _ in range(run):
                        self._add_word(self.section_c, "tvector_code")
                        self._add_word(self.section_d, "tvector_toc")
                elif sub == 4:
                    for _ in range(run):
                        self._add_word(self.section_d, "vtable_data")
                        self.reloc_pos += 4
                else:
                    for _ in range(run):
                        idx = self.import_index
                        imp = self.imports[idx]
                        self._add_word(
                            self._import_address(idx),
                            "import",
                            import_index=idx,
                            import_name=imp["name"],
                            import_library=imp["library"],
                        )
                        self.import_index += 1
                i += 1
                continue

            if (word >> 13) == 0b011:
                sub = (word >> 9) & 0xF
                index = word & 0x1FF
                names = {
                    0: "RelocSmByImport",
                    1: "RelocSmSetSectC",
                    2: "RelocSmSetSectD",
                    3: "RelocSmBySection",
                }
                if sub not in names:
                    raise ValueError(f"reserved RelocateByIndex subopcode {sub} at block {i}")
                self._bump(names[sub])
                if sub == 0:
                    imp = self.imports[index]
                    self._add_word(
                        self._import_address(index),
                        "import",
                        import_index=index,
                        import_name=imp["name"],
                        import_library=imp["library"],
                    )
                    self.import_index = index + 1
                elif sub == 1:
                    self._set_section("C", index)
                elif sub == 2:
                    self._set_section("D", index)
                else:
                    self._add_word(self._section_delta(index), "section", source_section=index)
                i += 1
                continue

            if (word >> 12) == 0b1000:
                self._bump("RelocIncrPosition")
                self.reloc_pos += (word & 0xFFF) + 1
                i += 1
                continue

            if (word >> 12) == 0b1001:
                if not allow_repeat:
                    raise ValueError("nested RelocSmRepeat is not permitted")
                self._bump("RelocSmRepeat")
                block_count = ((word >> 8) & 0xF) + 1
                repeat_count = (word & 0xFF) + 1
                repeat_start = i - block_count
                if repeat_start < start:
                    raise ValueError("RelocSmRepeat references blocks before current range")
                for _ in range(repeat_count):
                    consumed = self._execute_blocks(words, repeat_start, i, False)
                    if consumed != i:
                        raise ValueError("repeat block did not end on instruction boundary")
                i += 1
                continue

            op6 = word >> 10
            if op6 == 0b101000:  # set position
                if i + 1 >= end:
                    raise ValueError("truncated RelocSetPosition")
                self._bump("RelocSetPosition")
                self.reloc_pos = ((word & 0x3FF) << 16) | words[i + 1]
                i += 2
                continue
            if op6 == 0b101001:  # large import
                if i + 1 >= end:
                    raise ValueError("truncated RelocLgByImport")
                self._bump("RelocLgByImport")
                index = ((word & 0x3FF) << 16) | words[i + 1]
                imp = self.imports[index]
                self._add_word(
                    self._import_address(index),
                    "import",
                    import_index=index,
                    import_name=imp["name"],
                    import_library=imp["library"],
                )
                self.import_index = index + 1
                i += 2
                continue
            if op6 == 0b101100:  # large repeat
                if not allow_repeat:
                    raise ValueError("nested RelocLgRepeat is not permitted")
                if i + 1 >= end:
                    raise ValueError("truncated RelocLgRepeat")
                self._bump("RelocLgRepeat")
                block_count = ((word >> 6) & 0xF) + 1
                repeat_count = ((word & 0x3F) << 16) | words[i + 1]
                repeat_start = i - block_count
                if repeat_start < start:
                    raise ValueError("RelocLgRepeat references blocks before current range")
                for _ in range(repeat_count):
                    consumed = self._execute_blocks(words, repeat_start, i, False)
                    if consumed != i:
                        raise ValueError("large repeat block did not end on instruction boundary")
                i += 2
                continue
            if op6 == 0b101101:  # large section operation
                if i + 1 >= end:
                    raise ValueError("truncated RelocLgSetOrBySection")
                sub = (word >> 6) & 0xF
                index = ((word & 0x3F) << 16) | words[i + 1]
                names = {
                    0: "RelocLgBySection",
                    1: "RelocLgSetSectC",
                    2: "RelocLgSetSectD",
                }
                if sub not in names:
                    raise ValueError(f"reserved RelocLgSetOrBySection subopcode {sub}")
                self._bump(names[sub])
                if sub == 0:
                    self._add_word(self._section_delta(index), "section", source_section=index)
                elif sub == 1:
                    self._set_section("C", index)
                else:
                    self._set_section("D", index)
                i += 2
                continue

            raise ValueError(f"unknown relocation opcode top7={top7:#x} at block {i}")
        return i

    def execute(self, words: list[int], target_section: int) -> None:
        if target_section not in self.images:
            raise ValueError(f"relocation target section {target_section} is not instantiated")
        self.target_section = target_section
        self.reloc_pos = 0
        self.import_index = 0
        self.section_c = self._section_delta(0)
        self.section_d = self._section_delta(1)
        self._execute_blocks(words, 0, len(words), True)


def relocation_analysis(
    b: bytes,
    sections: list[Section],
    loader: dict[str, Any],
) -> tuple[dict[str, Any], dict[int, bytearray]]:
    images: dict[int, bytearray] = {}
    pidata_stats: dict[int, dict[str, int]] = {}
    for s in sections:
        if s.instantiated:
            image, stats = section_contents(b, s)
            images[s.index] = image
            if stats is not None:
                pidata_stats[s.index] = stats

    # Stable synthetic addresses make relocation effects deterministic and easy
    # to distinguish in reports.  PEF default addresses for this target are 0.
    section_bases = {i: 0x10000000 + i * 0x10000000 for i in images}

    relocator = Relocator(sections, images, loader["imports"], section_bases)
    loader_blob = loader["loader_blob"]
    reloc_area = loader["header"]["reloc_instr_offset"]
    relocation_sections = []
    for rh in loader["relocation_headers"]:
        start = reloc_area + rh["first_reloc_offset"]
        byte_count = rh["reloc_count_blocks"] * 2
        raw = loader_blob[start : start + byte_count]
        if len(raw) != byte_count:
            raise ValueError("relocation instruction stream exceeds loader section")
        words = list(struct.unpack(f">{rh['reloc_count_blocks']}H", raw))
        before_fixups = len(relocator.fixups)
        before_counts = dict(relocator.instruction_counts)
        relocator.execute(words, rh["section_index"])
        delta_counts = {
            k: relocator.instruction_counts.get(k, 0) - before_counts.get(k, 0)
            for k in relocator.instruction_counts
            if relocator.instruction_counts.get(k, 0) != before_counts.get(k, 0)
        }
        relocation_sections.append(
            {
                **rh,
                "decoded_instruction_counts": delta_counts,
                "fixup_count": len(relocator.fixups) - before_fixups,
                "final_reloc_offset": relocator.reloc_pos,
                "final_import_index": relocator.import_index,
            }
        )

    result: dict[str, Any] = {
        "synthetic_section_bases": {str(k): v for k, v in section_bases.items()},
        "pattern_data_opcode_counts": {str(k): v for k, v in pidata_stats.items()},
        "relocation_sections": relocation_sections,
        "instruction_counts": relocator.instruction_counts,
        "fixup_count": len(relocator.fixups),
        "import_fixup_count": sum(1 for f in relocator.fixups if f["kind"] == "import"),
        "internal_fixup_count": sum(1 for f in relocator.fixups if f["kind"] != "import"),
        "fixups": relocator.fixups,
    }

    h = loader["header"]
    main_section = h["main_section"]
    main_offset = h["main_offset"]
    if main_section in images and main_offset + 8 <= len(images[main_section]):
        code_addr = be32(images[main_section], main_offset)
        toc_addr = be32(images[main_section], main_offset + 4)
        code_base = section_bases.get(0, 0)
        data_base = section_bases.get(main_section, 0)
        result["main_transition_vector"] = {
            "section_index": main_section,
            "offset": main_offset,
            "code_address": code_addr,
            "toc_address": toc_addr,
            "code_section_offset": (code_addr - code_base) & 0xFFFFFFFF,
            "toc_section_offset": (toc_addr - data_base) & 0xFFFFFFFF,
        }
    return result, images


def probe(path: Path, include_fixups: bool = False) -> tuple[dict[str, Any], dict[int, bytearray]]:
    b = path.read_bytes()
    if len(b) < 40 or b[:8] != PEF_TAGS:
        raise SystemExit("not a PEF container")
    arch = b[8:12].decode("mac_roman", "replace")
    version = be32(b, 16)
    sections = parse_sections(b)
    result: dict[str, Any] = {
        "path": str(path),
        "architecture": arch,
        "format_version": version,
        "section_count": len(sections),
        "instantiated_section_count": be16(b, 34),
        "sections": [s.as_dict() for s in sections],
    }

    loader = loader_info(b, sections)
    images: dict[int, bytearray] = {}
    if loader:
        # Keep the JSON report compact by dropping the private raw loader blob.
        public_loader = {k: v for k, v in loader.items() if k != "loader_blob"}
        result["loader"] = public_loader
        reloc_result, images = relocation_analysis(b, sections, loader)
        if not include_fixups:
            reloc_result.pop("fixups", None)
        result["relocations"] = reloc_result
    else:
        for s in sections:
            if s.instantiated:
                image, _ = section_contents(b, s)
                images[s.index] = image
    return result, images


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("pef", type=Path)
    ap.add_argument("-o", "--output", type=Path)
    ap.add_argument("--include-fixups", action="store_true", help="include every relocation fixup in JSON")
    ap.add_argument(
        "--dump-section",
        action="append",
        type=int,
        default=[],
        help="write an instantiated/relocated section image as <dump-dir>/section-N.bin",
    )
    ap.add_argument("--dump-dir", type=Path, default=Path("."))
    ns = ap.parse_args()
    result, images = probe(ns.pef, ns.include_fixups)
    out = json.dumps(result, indent=2, ensure_ascii=False) + "\n"
    if ns.output:
        ns.output.parent.mkdir(parents=True, exist_ok=True)
        ns.output.write_text(out)
    else:
        print(out, end="")

    if ns.dump_section:
        ns.dump_dir.mkdir(parents=True, exist_ok=True)
        for index in ns.dump_section:
            if index not in images:
                raise SystemExit(f"section {index} is not instantiated")
            target = ns.dump_dir / f"section-{index}.bin"
            target.write_bytes(images[index])


if __name__ == "__main__":
    main()
