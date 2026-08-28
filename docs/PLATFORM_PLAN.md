# Platform Plan

## Final targets

- macOS
- iPadOS
- Linux
- Windows

## Architecture rule

The simulation, game rules, file parsers, deterministic timing, entity model, collision, and save/project state should remain platform-independent.

Platform layers provide:

- rendering/surface/window integration;
- audio device output;
- keyboard/controller/touch input;
- filesystem/user-data paths;
- packaging.

## Priority

1. macOS
2. iPadOS
3. Linux
4. Windows

Linux and Windows are not afterthought ports; they are final supported platforms and should be continuously protected by portable-core tests.
