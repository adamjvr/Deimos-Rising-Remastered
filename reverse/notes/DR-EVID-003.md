# DR-EVID-003 — Windows build

## Confirmed

- Outer ZIP contains `Win_DeimosRising.exe` (35,270,167 bytes).
- Executable is PE32/i386, Windows GUI.
- It is an NSIS/Nullsoft self-extracting installer.
- NSIS first header begins at file offset 36,864 (`0x9000`).
- PE timestamp: 2003-02-10 17:58:53 UTC.

## Why this matters

The x86 build is not merely another target installer. Once expanded it can supply a second implementation of the same game rules and serialization. Cross-platform constants, strings, resource IDs, table layouts, and behavior can be matched against the PowerPC build to reduce ambiguity.

## Pending

- Expand the NSIS solid-compressed payload.
- Hash and inventory installed files.
- Identify executable version/build string.
- Match Windows and Mac resource/data payloads by hash and semantic tag.
- Establish function/string correspondence anchors.
