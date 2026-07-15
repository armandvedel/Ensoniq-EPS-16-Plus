# KPC 2.33 firmware reference

Source supplied by the user; the binary is not copied into this project.

## Identification

- File name: `Ensoniq EPS KPC2 v2.33 27c256.BIN`
- Size: 32,768 bytes, matching a 27C256 EPROM
- SHA-256: `c5410d29cad27f61d1481383700b3b63dff29b74051295afcf7a15ca5b6f157e`
- Programmed window: file offsets `0x6000..0x7fff`
- CPU: Motorola 68HC11-compatible code
- CPU address window: `0xe000..0xffff`
- Reset vector: `0xe005`
- Interrupt vectors: populated with targets in `0xe000..0xf79f`

The blank `0xff` area before offset `0x6000` is consistent with an 8 KiB
decode window inside a 32 KiB EPROM image; it is not evidence of truncation.

## Confirmed protocol behavior

The reset and command parser are coherent 68HC11 code. Relevant commands are:

| Command | Observed firmware behavior |
|---|---|
| `e7` | Early loader reports `c8`; normal firmware reports `c7` or `ff` according to KPC state |
| `f0` | Enters/updates normal KPC state and shares the normal response path |
| `fb` | Two-byte command; values at or above `c0` are table-translated |
| `fd` | Calibration/status path, including `c9`, `ca`, and `cb` replies |
| `e1`..`e4`, `f1`..`f5` | Keyboard-controller configuration/service operations |

This matches the EPS-16 boot ROM's special handling of `c8` after sending
`e7`, making the image a useful protocol reference.

## Confidence and limitation

The image is structurally valid and protocol-compatible. No public source with
a reference checksum was found, so byte-for-byte authenticity cannot be
independently proven. KPC 2.33 is documented for a foamless Poly-Key keyboard
with reference coil; it must not be assumed to match every physical EPS-16
Plus keybed revision. The emulator uses behavior derived from it, not the
copyrighted binary itself.

