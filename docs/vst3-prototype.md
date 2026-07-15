# macOS VST3 prototype

Status: milestone 1 on branch `vst3-prototype`, based on tag `topsupergold`
and commit `8371356`.

## Architecture finding

The verified emulator is not yet an instantiable library. CPU bus state,
peripheral state, boot, scheduling, CLI diagnostics, browser transport,
CoreMIDI and AudioQueue are all owned by the single translation unit
`native/rom_probe.c` and its `main` loop. Directly compiling that loop into a
plug-in callback would retain wall-clock and singleton assumptions, would not
be safe for multiple plug-in instances, and would violate the DAW-clock
boundary.

The smallest safe executable milestone is therefore a real VST3 bundle plus a
strict, tested emulator-sink boundary. It establishes the host contract before
the verified machine is mechanically extracted from `rom_probe.c`.

## Implemented in milestone 1

- Apple Silicon/macOS VST3 bundle with stereo input, stereo output and MIDI
  input buses.
- No HTTP server, Web Audio, AudioQueue or CoreMIDI link in the plug-in.
- Fixed-capacity message-thread-to-audio-thread queue for physical panel and
  analog transitions; the audio callback performs no queue allocation.
- Sample-offset MIDI delivery and deterministic conversion of DAW sample time
  to the EPS 10 MHz CPU clock using an integer remainder accumulator.
- Stereo DAW input delivery to the emulator sampling boundary for every audio
  sample.
- Stereo output delivery exclusively through the plug-in process callback.
- Native original-layout panel surface. Buttons send raw KPC transitions only;
  they do not implement modes, menus or display state. Unverified RECORD,
  STOP/CONTINUE and PLAY mappings are visible but disabled.
- The VFD is blank until a real KPC/OS sink publishes it. No placeholder OS
  message is inserted into the display.
- External selectors for combined ROM, KPC ROM and OS disk paths. The selected
  paths are plug-in state; no copyrighted image is in the source or bundle.
- VOLUME maps to analog channel 5. DATA ENTRY uses the documented GUI
  `0..1023` to raw ADC `0..715` mapping on channel 3.

The current sink is deliberately silent. This milestone is a scan/load/UI/I/O
and scheduling prototype, not yet an audible EPS emulator. It must not be
presented as the completed emulator.

## Deterministic callback contract

For every DAW block, `EmulatorBridge`:

1. timestamps queued physical controls at the block boundary;
2. timestamps DAW MIDI at its sample offset;
3. submits the stereo DAW sample to the sampling input boundary;
4. advances the machine to the exact accumulated 10 MHz cycle target;
5. asks the machine for one stereo output sample.

After exactly one second at 44.1 or 48 kHz, the target is exactly 10,000,000
CPU cycles. Block partitioning does not alter the total.

## Build and test

JUCE is an external build dependency, like Musashi. Put a local JUCE 8 checkout
at `work/deps/JUCE` or set `EPS16_JUCE_DIR`.

```sh
cmake -S . -B work/vst3-build \
  -DEPS16_BUILD_VST3=ON \
  -DEPS16_JUCE_DIR="$PWD/work/deps/JUCE" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build work/vst3-build --target Eps16Plus_VST3 -j 6
cmake --build work/vst3-build --target eps16_vst3_package
ctest --test-dir work/vst3-build --output-on-failure
```

The raw bundle is written below
`work/vst3-build/vst3/Eps16Plus_artefacts/Release/VST3/`. The package target
copies it without Finder/resource-fork metadata, ad-hoc signs and strictly
verifies it, then writes
`work/vst3-build/vst3-package/EPS-16-Plus-Prototype-arm64.zip`. The archive
contains no ROM, KPC ROM or OS disk.

## Next milestone: authentic engine extraction

The next change should not rewrite working device behavior. It should move the
existing verified state and loop into an instance-owned `Eps16Machine` in
small compiling steps:

1. Separate CLI diagnostics and `live_host` calls from machine state.
2. Make RAM, MMIO, KPC, DMA, DUART, FDC, ES5505, ES5510 and Musashi context
   instance-owned. Remove all file-static machine globals.
3. Expose external resource loading, physical panel bytes, MIDI bytes, stereo
   sampling input, `runUntil(cycle)` and stereo rendering through the sink
   implemented in this milestone.
4. Replace the silent sink only after a deterministic boot regression reaches
   the same original-OS display and audio hashes as `rom_probe`.
5. Keep the legacy KPC path opt-in rules from `docs/kpc-migration.md`; do not
   delete provisional behavior until its ten acceptance tests pass.

No Enhanced parameter or direct-RAM feature belongs in this extraction.
