# KPC firmware migration

## Goal

Replace the provisional `KpcLegacy` behavior with execution of the original
KPC 2.33 68HC11 firmware.  The live adapter must remain a physical transport:
it supplies matrix transitions, keyboard events and analog inputs, while the
KPC firmware and original EPS-16 Plus OS own protocol state and navigation.

The legacy path remains the default until the firmware path passes every
acceptance test below.  Development runs use a separate HTTP port with
`EPS16_LIVE_NO_AUDIO=1`; they must not replace or stop the user's port-8160
process.

## Provisional behavior to remove

1. `KPC_LOAD_READY_POLLS` and the bounded load-ready burst.
2. Guessed display-frame drain counts and ready-search byte limits.
3. GPR-81-derived ENTER context plus host sampling start/stop state.
4. The `0xffebc8` instruction hook that writes low RAM `0x0b72`.
5. The `0xffe4f0` instruction hook that changes KPC and input-buffer state.
6. Display-text-triggered KPC state changes such as `FILE LOADED`.

Tracing at those addresses may remain, but it must not alter CPU, RAM, KPC,
panel, disk, sampling or audio state.

## Firmware-core boundary

The KPC implementation needs:

- 68HC11 registers, condition codes, stack, interrupts and instruction pages;
- the `0xe000..0xffff` 8 KiB EPROM window and internal RAM/register map;
- reset and interrupt vectors from the user-supplied 32 KiB EPROM image;
- SCI receive/transmit timing connected to the EPS DUART channel B;
- timer and port behavior used by the firmware;
- physical matrix/key events as KPC inputs, never as direct OS events;
- VFD and lamp output emitted by firmware-visible KPC hardware.

The copyrighted EPROM is never copied into the project. Tests receive its
external path, while the loader verifies size, blank prefix, program window
and reset vector. The supplied reference image was separately verified as
SHA-256
`c5410d29cad27f61d1481383700b3b63dff29b74051295afcf7a15ca5b6f157e`.

## Current implementation status

- `native/m68hc11_core.c` implements the reset path and the instruction subset
  reached so far by the real firmware. Unknown opcodes stop the core instead
  of being guessed or treated as NOPs.
- `native/kpc_device.c` maps the external EPROM, writable address space and
  SCI receive/transmit queues. With no received byte, KPC 2.33 remains in its
  genuine `f50f/f512/f514` receive polling loop.
- The isolated ROM test executes the original normal reset/parser/transmit
  path. Input `e7` produces `ff`, written by the firmware through its SCI data
  register. AddressSanitizer and UndefinedBehaviorSanitizer report no errors
  on the implemented path.
- `rom_probe.c` can connect this device to DUART-B only when
  `EPS16_KPC_EXECUTE=1` is explicitly set. The normal emulator still reports
  and uses `execution=legacy-model`; port 8160 is therefore unchanged.
- The firmware selects its reset path from the 68HC11 `CONFIG` register at
  `0x103f`. The KPC hardware value is `0x0d`: any other value enters the
  serial bootstrap parser and produces the early `e7 -> c8` response. Modeling
  `CONFIG=0x0d` reaches normal initialization and produces the genuine
  normal-state `e7 -> ff` response without a legacy reply.
- SPI, SCI, timer compare/capture and interrupt return paths now execute far
  enough for a combined 68000/KPC boot to reach `NO INSTRUMENTS` without an
  unknown 68HC11 opcode or an illegal 68000 instruction. The keypad/display
  board's idle synchronous return level is `0x00`; using `0xff` creates genuine
  repeated phantom matrix events in the KPC firmware.
- Physical matrix transitions are encoded on the synchronous KPC input rather
  than inserted into DUART-B. End-to-end CLI checks show the original firmware
  producing the UART events and the original OS reaching `FILE 1  PARALLEL
  EFX`, `CREATE NEW INSTRUMENT`, and `FREE SYSTEM BLKS=4085`.
- `run-kpc-test.command` starts an opt-in, no-audio browser test on port 8161.
  The legacy port-8160 path remains unchanged while the acceptance list is
  worked through.

The isolated handshake can be repeated without starting a browser or audio:

```sh
make work/build/eps16_kpc_boot_test
work/build/eps16_kpc_boot_test \
  '/Users/dbk/Downloads/Ensoniq EPS KPC2 v2.33 27c256.BIN' e7
```

The opt-in combined boot diagnostic also runs without a browser or audio:

```sh
EPS16_KPC_ROM='/Users/dbk/Downloads/Ensoniq EPS KPC2 v2.33 27c256.BIN' \
EPS16_KPC_EXECUTE=1 \
  work/build/eps16_rom_probe work/generated/eps16plus-rom.bin 220000000 \
  work/generated/EPS130OS.img
```

This switch remains diagnostic scaffolding, not the default live replacement.

## Acceptance tests before switching

1. Boot reaches `TUNING KBD - HANDS OFF` and `KEYBOARD TUNED` without a
   synthetic ready token.
2. Boot and ordinary page displays appear atomically and retain all 22 cells.
3. Every verified raw matrix code produces one press and one release event.
4. LEFT/RIGHT advance exactly one OS-owned page per click; repeated navigation
   never creates stray `*` characters.
5. Loading an instrument completes without display-text inspection.
6. Sampling target selection reaches the level/threshold display without a
   synthetic ready burst.
7. ENTER release starts recording once; ENTER press stops once; revisiting the
   wavesample shows the threshold page rather than entering RECORD directly.
8. A deterministic tone recorded through GPR 80 and played through the ES5505
   has the OS-selected duration and pitch.
9. A browser-microphone recording has no inserted zero islands, missing tail
   or duplicated ADC conversions.
10. Effect selection and normal keyboard playback remain functional.

Only after all ten pass may `KpcLegacy` and the six provisional mechanisms be
deleted.  The original input-filter control is then derived from KPC/OS-visible
hardware state and verified separately; no display string may select a cutoff.
