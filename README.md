# EPS-16 Plus reverse-engineering foundation

This project contains a clean-room toolchain for inspecting an HxC HFE disk
image supplied by the user. It does **not** contain the Ensoniq operating
system or any ROM data.

Current capabilities:

- parse HFE v1 images;
- deinterleave both floppy sides;
- decode IBM MFM address and data marks;
- validate every ID and data CRC;
- rebuild a sector-ordered 800 KiB image;
- accept validated EPS HFE v1 files directly wherever the native emulator
  accepts a logical IMG, without changing the IMG path;
- emit a machine-readable analysis report;
- parse the Ensoniq directory and 24-bit file-allocation table;
- extract OS, effects, instruments, banks and sequence files by their FAT chain;
- provide a small big-endian memory bus for the 68000 emulator;
- provide a deterministic SCSI-1 disk target with READ/WRITE (6/10), inquiry,
  sense and capacity commands.
- combine split U27/U28 ROMs and validate the 68000 reset vectors;
- execute the real v1.00F reset path with MMIO access tracing;
- boot the unmodified ROM from the supplied logical OS disk into the loaded
  scheduler using minimal DUART and WD1772 models;
- deliver vectored MC68681 timer interrupts to the running original OS;
- perform the EPS panel echo handshake and reconstruct its 1x22 display text.
- queue bidirectional panel packets for GUI-generated buttons and keys;
- model ES5510 host-side program/DRAM uploads;
- render ES5505 PCM voices with interpolation, exponential stereo volume,
  hardware loop modes and the four-pole K1/K2 filter.

Current reverse-engineering results are recorded in
[`docs/bringup.md`](docs/bringup.md). The GUI/OS ownership boundary, access
methods, ranges, persistence, risks, and verification state are tracked in
[`docs/parameter-mapping.md`](docs/parameter-mapping.md). The 36 physical front
panel controls, four-byte click framing, and currently verified wire indices are documented in
[`docs/panel-protocol.md`](docs/panel-protocol.md). Findings from the
user-supplied KPC 2.33 ROM are recorded without bundling it in
[`docs/kpc-reference.md`](docs/kpc-reference.md).

The separated first macOS VST3 host milestone, its exact limitations, build
instructions and the next authentic-engine extraction gate are documented in
[`docs/vst3-prototype.md`](docs/vst3-prototype.md). It is developed on the
`vst3-prototype` branch; the `vst3-prototype-base` tag marks its unchanged
authentic-engine baseline.

## Usage

```sh
python3 -m eps16.hfe /path/to/EPS130OS.hfe \
  --output work/generated/EPS130OS.img \
  --report work/generated/EPS130OS.json

python3 -m eps16.disk_compare /path/to/disk.hfe /path/to/reference.img \
  --report work/generated/hfe-vs-img-sectors.json

python3 -m eps16.inspect work/generated/EPS130OS.img \
  --report work/generated/inspection.json

python3 -m eps16.filesystem work/generated/EPS130OS.img \
  --output-dir work/generated/files \
  --report work/generated/filesystem.json

python3 -m eps16.rom --upper /path/to/upper.u28 --lower /path/to/lower.u27 \
  --output work/generated/eps16plus-rom.bin \
  --report work/generated/rom.json

python3 -m unittest discover -s tests -v
```

## Native 68000 core

The native emulator uses the MIT-licensed
[Musashi](https://github.com/kstenerud/Musashi) core. It is intentionally an
external build dependency and is not copied into this repository:

```sh
git clone --depth 1 https://github.com/kstenerud/Musashi work/deps/Musashi
make test
```

A CMake build is also supplied for IDE integration.

The first native target executes a synthetic reset vector and verifies a real
68000 `MOVEQ`/`STOP` sequence. The ROM probe now follows the real reset path,
loads OS 1.30 from the virtual floppy and runs its interrupt-driven scheduler.
The front panel and audio devices are still partial stubs.

The second native target disassembles a loadable Ensoniq file without bundling
it into the project:

```sh
work/build/eps16_disasm work/generated/files/00_EPS-16+_O.S._1b.bin 0x204 64
work/build/eps16_disasm work/generated/eps16plus-rom.bin 0x11604 64 0xc00000
work/build/eps16_rom_probe work/generated/eps16plus-rom.bin 200000
work/build/eps16_rom_probe work/generated/eps16plus-rom.bin 100000000 \
  work/generated/EPS130OS.img

# Inject panel bytes 0x81,0x00 after 300 million cycles:
work/build/eps16_rom_probe work/generated/eps16plus-rom.bin 500000000 \
  work/generated/EPS130OS.img 8100 300000000

# Optional reverse-engineering snapshot of the loaded 64 KiB OS RAM:
EPS16_DUMP_OS_RAM=/tmp/eps16-osram.bin \
  work/build/eps16_rom_probe work/generated/eps16plus-rom.bin 100000000 \
  work/generated/EPS130OS.img
```

The probe also supports an in-session floppy swap and multiple timed panel
packets. This keeps the original OS running while a test replaces the boot
disk with an instrument disk:

```sh
EPS16_SWAP_DISK=/path/to/ED-001.IMG \
EPS16_SWAP_CYCLE=110000000 \
EPS16_PANEL_SCRIPT='350000000:a300,360000000:2300,500000000:8200,550000000:0200' \
  work/build/eps16_rom_probe work/generated/eps16plus-rom.bin 1500000000 \
  work/generated/EPS130OS.img
```

`LOGICAL_DISK_IMG` and `EPS16_SWAP_DISK` accept either the existing exact
819,200-byte IMG format or a CRC-valid EPS HFE v1 image. Format detection uses
the HFE signature, not the filename extension.

An external 32 KiB KPC 2.33 EPROM can be structurally validated at startup:

```sh
EPS16_KPC_ROM='/path/to/Ensoniq EPS KPC2 v2.33 27c256.BIN' \
  work/build/eps16_rom_probe work/generated/eps16plus-rom.bin 220000000 \
  work/generated/EPS130OS.img
```

Without another switch this reports `execution=legacy-model`: the ROM is
loaded and validated, but the established live behavior remains unchanged.
An isolated, no-browser/no-audio diagnostic can opt into the incomplete
68HC11 path with `EPS16_KPC_EXECUTE=1`. It currently verifies the real
normal-state `e7 -> ff` handshake; it is not yet suitable for the live UI
because the keyboard-scanner capture interrupts and physical matrix ports
remain incomplete. See `docs/kpc-migration.md`.

`EPS16_PANEL_SCRIPT` is a comma-separated list of `cycle:hex-bytes` events.
The example starts in the OS's boot-default LOAD/Instrument page, then uses
ENTER/YES and Instrument/Track-1. Loading starts on the Track-1 release.
Unknown panel codes are not treated as stable API.

After loading, select Track 1 once more, inject Middle C, and capture the live
ES5505 mix as a stereo WAV:

```sh
EPS16_AUDIO_WAV=work/generated/jazz-bass-middle-c-timing-fixed.wav \
EPS16_AUDIO_START_CYCLE=990000000 \
EPS16_AUDIO_END_CYCLE=1030000000 \
EPS16_SWAP_DISK=/path/to/ED-001.IMG \
EPS16_SWAP_CYCLE=110000000 \
EPS16_PANEL_SCRIPT='350000000:a300,360000000:2300,500000000:8200,550000000:0200,850000000:8200,860000000:0200,1000000000:9864,1010000000:1801' \
  work/build/eps16_rom_probe work/generated/eps16plus-rom.bin 1030000000 \
  work/generated/EPS130OS.img
```

`98 64` is Middle C press with velocity 100; `18 01` is its release. The WAV
uses the EPS-16 Plus ES5505 output rate derived from its 10 MHz clock and 21
active voice slots (29,761 Hz in this run).

An original-layout diagnostic panel is available at `panel/index.html`. It
implements the manual's LOAD/INSTRUMENT navigation, including the ED-001 file
catalog and block counts, and records the exact button sequence. Hardware codes
that are not yet confirmed remain visibly marked. It is intentionally separate
from the later streamlined plugin UI.

The EDIT and COMMAND navigation is sourced from the supplied 260-page German
EPS-16 PLUS manual and kept in `panel/menu-catalog.js`. Empty hardware pages are
left empty; effect parameter pages are explicitly dynamic because their fields
depend on the selected effect algorithm.

The extracted image is intentionally written outside this project by default.
Do not commit or redistribute copyrighted OS images.

## Confirmed properties of the supplied image

- HxC HFE v1, 80 cylinders, 2 sides, 250 kbit/s
- 10 sectors per track, 512 bytes per sector
- sector IDs use values 0 through 9 and physical skewing
- 1,600 of 1,600 sectors decode successfully
- 1,600 valid ID CRCs and 1,600 valid data CRCs
- logical image size: 819,200 bytes
- disk identifier: `EPS130OID`
- 68000 code begins in the first allocated data region

## Emulator boundary

The intended design keeps the original OS behind an emulated hardware bus:

```text
modern GUI <-> parameter adapter <-> 68000 + original OS <-> virtual hardware
```

The GUI will never depend on the original front-panel layout. Initially the
adapter can generate front-panel events. Once RAM structures are understood,
it can expose direct parameters and DAW automation without changing the audio
model.

## Fast sample loading

Floppy timing is not part of the audio model. The emulator can boot from HFE
for compatibility while exposing a virtual SCSI disk for normal use. SCSI
commands operate without host-side delays; the scheduler may fast-forward the
68000 until the OS finishes parsing and allocating a sample. A later direct
loader can bypass the OS file dialog, but should still hand the final state to
the original OS routines rather than guessing at live RAM invariants.
