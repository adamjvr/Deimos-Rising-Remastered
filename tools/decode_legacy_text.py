#!/usr/bin/env python3
"""Decode Deimos Rising legacy tagged-text resources without extracting archives."""
from __future__ import annotations
import argparse
from pathlib import Path

def decode(data: bytes) -> bytes:
    out = bytearray()
    for c in data:
        v = ((c & 0x07) << 4) | (c >> 4)
        if c & 0x08:
            v ^= 0x7F
        out.append(v)
    return bytes(out).rstrip(b"\0")

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("resource", type=Path)
    ap.add_argument("-o", "--output", type=Path)
    ns = ap.parse_args()
    decoded = decode(ns.resource.read_bytes())
    if ns.output:
        ns.output.write_bytes(decoded)
    else:
        print(decoded.decode("ascii"), end="")

if __name__ == "__main__":
    main()
