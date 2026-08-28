# Status

## 2026-08-27 — Phase 0 / Evidence Intake

### Complete

- Established evidence ID `DR-EVID-001`.
- Fingerprinted the original StuffIt archive with MD5, SHA-1, and SHA-256.
- Confirmed StuffIt 5 container structure.
- Parsed the full visible archive catalog without extracting proprietary payloads.
- Identified the primary payload as `deimos_rising.iso`.
- Recorded data/resource fork metadata, compression method, CRC16 values, offsets, and classic Finder type/creator codes.
- Established clean-room repository boundaries.
- Added a reusable StuffIt 5 catalog-inventory tool for subsequent archives.

### Current blocker / next operation

Extract StuffIt method-13 streams losslessly, validate CRC16, then inspect the ISO filesystem and inventory the original executable, resources, data files, audio, levels, graphics, and any registration/version metadata.

### Evidence expected next

Additional user-provided Deimos Rising archives will be assigned sequential evidence IDs and cross-compared rather than merged destructively.
