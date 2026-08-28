# Terrain/image geometry observations

Status: **confirmed dimensions; semantic mapping partly strong**.

Canonical level image resources expose a simple fixed geometry relationship:

- level/background TGA: **480 × 3600**, 16-bit, uncompressed TGA type 2;
- media-mask TGA: **96 × 720**, 16-bit, uncompressed TGA type 2;
- level preview TGA: **146 × 306**, 16-bit;
- level-selection image: **640 × 480**, 16-bit;
- scorebar: **160 × 480**, 16-bit.

The media mask is exactly one fifth of the background in both dimensions. This is strong evidence for a 5×5 world-pixel region represented by each media-mask pixel, but the exact collision/media sampling rule remains to be proven from executable behavior.
