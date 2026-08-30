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

# The smoke app may use C++ helpers in an anonymous namespace, but every
# Objective-C declaration must appear after that namespace has closed.
smoke = root / 'src/platform/apple/apple_host_smoke_app.mm'
smoke_text = smoke.read_text(encoding='utf-8')
close_pos = smoke_text.find('} // namespace')
interface_pos = smoke_text.find('@interface')
implementation_pos = smoke_text.find('@implementation')
if close_pos < 0 or interface_pos < 0 or implementation_pos < 0:
    raise SystemExit('apple_host_smoke_app.mm: missing namespace close or Objective-C declarations')
if interface_pos < close_pos or implementation_pos < close_pos:
    raise SystemExit(
        'apple_host_smoke_app.mm: Objective-C declarations must remain outside the C++ namespace')

# The macOS playable wrapper must not silently fall back to a static preview.
# Input is routed through the key window responder so weapon presses reach the
# semantic live-world input path even when no local event monitor is installed.
for token in (
    'PLAYABLE WIP 3',
    'live-world bootstrap failed:',
    '@interface DeimosGameWindow : NSWindow',
    '- (void)keyDown:(NSEvent*)event',
    'WeaponAction::FireAir',
    'case 49:  // Space',
    'case 6:   // Z: primary air fire',
    'Deimos AIR FIRE accepted',
):
    if token not in smoke_text:
        raise SystemExit(f'apple_host_smoke_app.mm: missing playable-host contract token: {token}')
if 'addLocalMonitorForEventsMatchingMask' in smoke_text:
    raise SystemExit('apple_host_smoke_app.mm: obsolete local key monitor must not drive playable input')
print('Apple playable-host input/fail-fast contract PASS')
