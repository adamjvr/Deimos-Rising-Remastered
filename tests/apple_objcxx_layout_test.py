#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
source = root / 'src/platform/apple/apple_metal_host_view.mm'
text = source.read_text(encoding='utf-8')
namespace_pos = text.find('namespace deimos')
if namespace_pos < 0:
    raise SystemExit('apple_metal_host_view.mm: missing namespace deimos')
for token in ('@interface', '@implementation'):
    positions = []
    start = 0
    while True:
        pos = text.find(token, start)
        if pos < 0:
            break
        positions.append(pos)
        start = pos + len(token)
    if not positions:
        raise SystemExit(f'apple_metal_host_view.mm: missing {token}')
    bad = [p for p in positions if p > namespace_pos]
    if bad:
        raise SystemExit(
            f'apple_metal_host_view.mm: {token} must remain at Objective-C++ file-global scope, '
            f'not inside namespace deimos')
print('Apple Objective-C++ declaration layout PASS')
