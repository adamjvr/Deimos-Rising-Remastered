# Legacy tagged-text resource encoding

Status: **confirmed for the canonical 1.0.6 corpus**.

## Scope

The resources with extensions `leve`, `unde`, `plde`, `wede`, `idli`, `flli`, `coli`, `tefo`, `stli`, and `reli` are not opaque binary structs. Their contents are seven-bit text passed through a reversible per-byte transform.

A corpus pass successfully decodes all **473** canonical resources in these ten buckets to ASCII without replacement characters.

## Byte transform

For encoded byte `c`:

```text
v = ((c & 0x07) << 4) | (c >> 4)
if c & 0x08:
    v ^= 0x7f
```

`v` is the seven-bit decoded byte.

A useful property is that `decode(c) == decode(c ^ 0xff)`: two complementary byte values can represent the same seven-bit character. Therefore the clean implementation provides a canonical encoder for synthetic tests but does **not** claim that encoder reproduces the historical byte stream exactly.

## Text grammar

Most resource types use line records:

```text
#key <value>
```

Supported observed syntax includes:

- CR line endings and comments beginning with `//`;
- arbitrary indentation before records;
- whitespace/tabs between key and `<value>`;
- inline `//` comments after a value;
- keys containing spaces, especially ID and rect lists;
- bare lines for `.stli` string-list resources.

`U_Token.cc` diagnostics in the original executable independently refer to key lookup, integer, float, Boolean, `COLOR`, `RECT`, and four-byte ID parsing.

## Typed values

The clean core currently exposes proven primitives:

- integer;
- float;
- `TRUE` / `FALSE` Boolean;
- four-byte ID/FourCC;
- integer rectangle;
- 24-bit RGB hex.

FourCC whitespace is semantic and must never be trimmed. The canonical layer ID `air ` is one concrete example.

## Resource meanings

| Extension | Canonical count | Meaning |
| --- | ---: | --- |
| `leve` | 12 | Level definition |
| `unde` | 386 | Unit/entity definition |
| `plde` | 2 | Player definition |
| `wede` | 5 | Weapon definition |
| `idli` | 6 | ID list |
| `flli` | 1 | Float list/global constants |
| `coli` | 1 | Color list |
| `tefo` | 54 | Text format |
| `stli` | 5 | String list |
| `reli` | 1 | Rect list |

See `reverse/inventories/TAGGED_TEXT_CORPUS_1_0_6.json` for corpus statistics.

## Text-format quirk

`tefo` uses a field named `Format_ID`, but the canonical corpus proves it is not uniformly a FourCC: observed values are `LEFT`, `CENT`, `CEBU`, `RIGHT`, `3`, and `4`. The clean `TextFormatDefinition` therefore preserves this field as an opaque token instead of forcing an incorrect four-byte type.
