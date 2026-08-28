#!/usr/bin/env python3
"""Minimal read-only StuffIt 5 catalog inventory tool.

It does NOT decompress payloads. Its purpose is evidence intake: identify the
container, enumerate entries/forks, and record offsets/sizes/metadata without
modifying the source archive.
"""
from __future__ import annotations
import argparse, datetime as dt, hashlib, json, struct
from pathlib import Path

SIT5_ID = 0xA5A5A5A5
DIR = 0x40

def u16(b, o): return struct.unpack_from(">H", b, o)[0]
def u32(b, o): return struct.unpack_from(">I", b, o)[0]

def mac_time(v: int) -> str:
    epoch = dt.datetime(1904, 1, 1, tzinfo=dt.timezone.utc)
    return (epoch + dt.timedelta(seconds=v)).isoformat()

def decode_name(x: bytes) -> str:
    return x.decode("mac_roman", "replace")

def parse(path: Path) -> dict:
    b = path.read_bytes()
    if len(b) < 100 or not b.startswith(b"StuffIt (c)1997-"):
        raise SystemExit("not a recognized StuffIt 5 archive")

    version = b[82]
    flags = b[83]
    if version != 5:
        raise SystemExit(f"unsupported StuffIt archive version {version}")

    total = u32(b, 84)
    root_count = u16(b, 92)
    first = u32(b, 94)

    entries = []
    dirs = {}
    todo = root_count
    off = first
    i = 0

    while i < todo:
        start = off
        if u32(b, off) != SIT5_ID:
            raise SystemExit(f"bad entry ID at offset {off}")

        eversion = b[off+4]
        headersize = u16(b, off+6)
        eflags = b[off+9]
        created = u32(b, off+10)
        modified = u32(b, off+14)
        nextoff = u32(b, off+22)
        diroff = u32(b, off+26)
        namelen = u16(b, off+30)
        headercrc = u16(b, off+32)
        datalen = u32(b, off+34)
        datacomp = u32(b, off+38)
        datacrc = u16(b, off+42)
        pos = off + 46

        if eflags & DIR:
            children = u16(b, pos)
            pos += 2
            if datalen == 0xFFFFFFFF:
                # StuffIt 5 emits a short post-directory marker entry.
                todo += 1
                off = pos
                i += 1
                continue
            datamethod = None
            passlen = 0
        else:
            children = None
            datamethod = b[pos]
            passlen = b[pos+1]
            pos += 2 + passlen

        name = decode_name(b[pos:pos+namelen])
        pos += namelen

        if pos < start + headersize:
            commentsz = u16(b, pos)
            pos += 4
            comment = decode_name(b[pos:pos+commentsz])
            pos += commentsz
        else:
            comment = ""

        something = u16(b, pos)
        pos += 4
        ftype = decode_name(b[pos:pos+4]); pos += 4
        creator = decode_name(b[pos:pos+4]); pos += 4
        finderflags = u16(b, pos); pos += 2
        pos += 22 if eversion == 1 else 18

        rsrc = None
        if something & 1:
            rlen = u32(b, pos)
            rcomp = u32(b, pos+4)
            rcrc = u16(b, pos+8)
            rmethod = b[pos+12]
            rpass = b[pos+13]
            pos += 14 + rpass
            rsrc = {
                "uncompressed_size": rlen,
                "compressed_size": rcomp,
                "crc16": rcrc,
                "compression_method": rmethod,
                "data_offset": pos,
            }

        parent = dirs.get(diroff, "")
        full = f"{parent}/{name}".strip("/")

        row = {
            "path": full,
            "archive_offset": start,
            "flags_hex": hex(eflags),
            "creation_time_utc": mac_time(created),
            "modification_time_utc": mac_time(modified),
            "header_size": headersize,
            "header_crc16": headercrc,
            "finder_type": ftype,
            "finder_creator": creator,
            "finder_flags": finderflags,
            "comment": comment,
        }

        if eflags & DIR:
            row["kind"] = "directory"
            row["children_declared"] = children
            dirs[start] = full
            entries.append(row)
            todo += children
            off = pos
        else:
            row["kind"] = "file"
            row["resource_fork"] = rsrc
            data_start = pos + (rsrc["compressed_size"] if rsrc else 0)
            row["data_fork"] = {
                "uncompressed_size": datalen,
                "compressed_size": datacomp,
                "crc16": datacrc,
                "compression_method": datamethod,
                "data_offset": data_start,
            }
            entries.append(row)
            off = data_start + datacomp

        i += 1

    return {
        "path": str(path),
        "size_bytes": len(b),
        "hashes": {
            "md5": hashlib.md5(b).hexdigest(),
            "sha1": hashlib.sha1(b).hexdigest(),
            "sha256": hashlib.sha256(b).hexdigest(),
        },
        "container": {
            "format": "StuffIt 5",
            "version": version,
            "flags_hex": hex(flags),
            "declared_total_size": total,
            "root_entry_count": root_count,
            "first_entry_offset": first,
        },
        "entries": entries,
    }

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("archive", type=Path)
    ap.add_argument("-o", "--output", type=Path)
    ns = ap.parse_args()
    result = parse(ns.archive)
    out = json.dumps(result, indent=2, ensure_ascii=False) + "\n"
    if ns.output:
        ns.output.write_text(out)
    else:
        print(out, end="")

if __name__ == "__main__":
    main()
