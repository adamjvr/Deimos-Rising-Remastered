# Testing and Validation

## Evidence intake tests

For each archive:

1. hash before any transformation;
2. parse container header;
3. enumerate every entry and fork;
4. record original uncompressed/compressed sizes and checksums;
5. extract losslessly;
6. validate per-fork CRC/checksum where available;
7. hash extracted outputs;
8. compare duplicates across evidence sets by content hash, not filename.

## Reconstruction tests

- parser tests use synthetic fixtures;
- behavior tests record explicit expected values;
- discrepancies are logged rather than silently normalized;
- confidence is tagged as confirmed / strong / tentative / unknown.
