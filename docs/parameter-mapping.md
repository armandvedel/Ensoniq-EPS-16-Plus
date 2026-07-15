# EPS-16 Plus GUI parameter mapping

Status: draft 1, 2026-07-02

This document is the contract between the host GUI and the emulated EPS-16
Plus. The fidelity-first rule is: an OS-owned value is accessed through panel
automation until a direct RAM mapping has been independently located, verified
in more than one context, and covered by a read/write/restore test.

## Status vocabulary

- `GUI model`: implemented only in the diagnostic browser panel.
- `Transport proven`: panel bytes are consumed by the unmodified OS.
- `Manual verified`: page name, order, and direct number match the supplied
  EPS-16 Plus manual.
- `Round-trip pending`: the real OS value has not yet been read, changed, read
  back, and restored through the GUI bridge.
- `Not exposed`: internal implementation state, not a user parameter.

`Unknown` is intentional. It must not be replaced with an inferred RAM address
or numeric scale.

## Currently exposed GUI controls

| GUI parameter/control | Source | Read method | Write method | Value range | Scaling | Persistence | Known risks | Test status |
|---|---|---|---|---|---|---|---|---|
| Virtual disk image | Panel automation / host media adapter | Host-selected path; parsed EPS directory | Mount logical 800 KiB image; runtime disk swap | `.IMG`/decoded `.HFE` | None | Host session; image remains external | Wrong image, disk-change timing, writes not yet supported | Runtime swap and ED-001 directory verified by original OS |
| Master Volume fader | Hardware register | Read ES5505 parallel ADC channel 5 | Set virtual 10-bit analog input | `0..1023` | Left shift 6 to ES5505 bits 15..6 | Runtime/host state; not instrument data | Host gain must not be confused with OS instrument volume | ES5505 ADC read tested; live GUI bridge pending |
| Data Entry fader | EPS-16 Plus analog input / panel automation | Observe channel 3 during scanner phase `OPR & f0 = 90` and the selected OS field | Map GUI `0..1023` linearly to raw ADC `0..715`; serialize requests and coalesce pending browser events to the newest position | GUI `0..1023`; raw ADC `0..715`; calibrated span `28..687`; OS/Analog Test `0..255` | ROM subtracts `0x0700`, multiplies by `0x00c6`, doubles once, then applies field scaling; 28-count overtravel defeats endpoint hysteresis | Fader position is transient; selected OS parameter owns persistence | Sending the entire raw 10-bit ADC range creates a large upper dead zone; exact calibrated endpoints may not cross a hysteretic boundary after reversal | Original-ROM phase, zero reference, multiplier, and 0..255 scaling verified; repeated-sweep live retest pending |
| Mode: Load / Command / Edit | Panel automation | Decode OS mode indicator | Send four-byte matrix press/release packet | 3 states | Identity | OS session | Self-test glyphs are not wire indices | All three matrix indices live-verified |
| Page selection (14 buttons) | Panel automation | Decode OS page indicator | Send four-byte matrix press/release packet | 14 page IDs | Identity | OS session/context | Same physical buttons also act as numeric direct-select keys | All 14 matrix indices live-verified |
| Up / Down | Panel automation | Observe changed 22-character display | Send matrix indices `0a` / `0b` | Previous/next item or value step | OS-defined | Parameter-dependent | Auto-repeat starts on a 100 ms hold | Live-verified in ED-001 file browser; reversed labels corrected 2026-07-13 |
| Left / Right | Panel automation | Observe changed field/display | Send matrix indices `10` / `11` | Previous/next field; LOAD toggles name/blocks | OS-defined | Parameter-dependent | Meaning changes by page | Live-verified with `FILE 3 PIANO 241` / `241 BLKS` |
| Enter / Yes | Panel automation | Observe prompt/state transition | Send matrix index `23` press/release | Trigger | None | Operation-dependent | May execute destructive COMMAND operation | Press live-verified: ED-001 file selection reaches `PICK INSTRUMENT BUTTON` |
| Cancel / No | Panel automation | Observe return state | Send raw matrix index `21` (OS cursor value `23`) | Trigger | None | Operation-dependent | Context-sensitive cancellation | Mapping structurally derived from original-OS cursor-value sequence; live confirmation pending |
| Instrument/Track 1 | Panel automation | Read selected/loaded LED and display | Send matrix index `02` press/release | Track 1; context-sensitive press/release semantics | Identity | OS session; data persists only when saved | Replaces occupied slot during load | Release live-verified: loads ED-001 `JAZZ BASS` and reaches `FILE LOADED` |
| Instrument/Track 2-8 | Panel automation | Read selected/loaded LEDs and display | Send raw indices `08,0e,14,04,22,1c,16` | 2-8; context-sensitive press/release semantics | OS values are contiguous `01..07` | OS session; data persists only when saved | Replaces occupied slot during load | Structurally derived from original-OS translation table; live confirmation pending |
| Virtual keyboard note | KPC panel transport | Observe OS voice allocation and ES5505 registers | Send key index plus nonzero velocity/release byte | 61 key indices; velocity/pressure byte | OS adds `0x24` to key index | Runtime performance state | Loaded track must be selected after leaving LOAD mode | Middle C `98 64` / `18 01` live-verified with JAZZ BASS and nonzero stereo PCM |
| Record / Stop-Continue / Play | Panel automation | Read sequencer status indicator | Send transport packet | Trigger/toggle | None | Sequence/song state | Can modify an armed sequence | Manual verified; live round-trip pending |
| Sample | Browser microphone + panel automation | Browser PCM remains at the host rate; the device layer converts it to the sampling rate selected by the original OS and exposes conversions through ES5510 GPR 80; send Sample packet | Local `getUserMedia` stream and Sample trigger | Signed 16-bit PCM plus low-byte valid marker | GPR word `(sample << 8) | 1` | Instrument file after save | Browser permission is required; the original OS owns rate selection, allocation, recording and stop processing | Original OS receives the ordered Mac microphone waveform through its sampling-conversion polling path |
| Effect Select / Bypass | Panel automation | Read effect/status display | Send effect button packet | Effect-dependent | OS-defined | Instrument/bank/effect-file dependent | ES5510 execution and routing are implemented; live recorded-sample effect return still needs verification | Manual selection verified; audible round-trip pending |
| 22-character display | Panel automation | Decode DUART channel-B VFD stream | Read-only GUI mirror | 22 characters plus indicators | Character mapping | None | Cursor/control characters are only partially decoded | Boot display proven; full live decoder pending |

## OS editor parameters planned for the comfortable GUI

Unless a row says otherwise, the common mapping is:

- **Source:** panel automation.
- **Read:** select Mode/Page/direct number, then parse the real OS display;
  use Left/Right for additional fields.
- **Write:** select the same field and send Up/Down or emulated Data Entry.
- **Scaling:** identity in displayed OS units initially; normalized plug-in
  scaling is added only after endpoint and step-size tests.
- **Test status:** page catalog manual-verified; real OS round-trip pending.

### Instrument and system

| GUI parameter | Source | Read method | Write method | Value range | Scaling | Persistence | Known risks | Test status |
|---|---|---|---|---|---|---|---|---|
| Patch layer enable set | Panel automation | EDIT/Instrument/0 display | Cursor + Data Entry | OS-defined layer/patch set | Identity | Instrument file | Patch-select context | Manual verified; round-trip pending |
| Keydown layers | Panel automation | EDIT/Instrument/1 | Cursor + Data Entry | OS-defined layer set | Identity | Instrument file | Can silence keydown voices | Manual verified; round-trip pending |
| Keyup layers | Panel automation | EDIT/Instrument/2 | Cursor + Data Entry | OS-defined layer set | Identity | Instrument file | Release-trigger behavior | Manual verified; round-trip pending |
| MIDI out channel | Panel automation | EDIT/Instrument/3 | Data Entry | OS enumeration | Identity | Instrument file | Off/base/channel semantics | Manual verified; round-trip pending |
| MIDI out program | Panel automation | EDIT/Instrument/4 | Data Entry | OS enumeration | Identity | Instrument file | May transmit external program change | Manual verified; round-trip pending |
| Pressure mode | Panel automation | EDIT/Instrument/5 | Data Entry | OS enumeration | Enum | Instrument file | Poly/channel pressure distinction | Manual verified; round-trip pending |
| MIDI status | Panel automation | EDIT/Instrument/6 | Data Entry | OS enumeration | Enum | Instrument file | Local/MIDI routing changes | Manual verified; round-trip pending |
| Instrument size | Panel automation | EDIT/Instrument/7 | Read-only | Blocks | 1 block = 256 samples | Derived; not written | Must remain read-only | Manual verified; round-trip pending |
| Instrument name | Panel automation | EDIT/Instrument/8 | Character entry | OS name length/charset | Character mapping | Instrument file | Character encoding | Manual verified; round-trip pending |
| Patch Select assignment | Panel automation | EDIT/Instrument/9 | Cursor + Data Entry | OS enumeration | Enum | Instrument file | Four patch states | Manual verified; round-trip pending |
| Instrument key range | Panel automation | EDIT/Instrument, scroll | Cursor + Data Entry | Keyboard notes | Note-name mapping | Instrument file | Low/high ordering | Manual verified; round-trip pending |
| Instrument transpose | Panel automation | EDIT/Instrument, scroll | Data Entry | OS semitone range | Semitones | Instrument file | MIDI and audio behavior may differ | Manual verified; round-trip pending |
| System free blocks | Panel automation | EDIT/System-MIDI/0 | Read-only | Blocks | Identity | Derived | Must remain read-only | Manual verified; round-trip pending |
| Disk free blocks | Panel automation | EDIT/System-MIDI, scroll | Read-only | Blocks | Identity | Derived | Media changes invalidate value | Manual verified; round-trip pending |
| Master tune | Panel automation | EDIT/System-MIDI/1 | Data Entry | OS-defined | Display units initially | Global RAM; Save Global Parameters | Global audio impact | Manual verified; round-trip pending |
| Global bend range | Panel automation | EDIT/System-MIDI, scroll | Data Entry | OS-defined semitones | Semitones | Global RAM; optionally saved | Affects all instruments | Manual verified; round-trip pending |
| Touch response | Panel automation | EDIT/System-MIDI, scroll | Data Entry | OS enumeration | Enum | Global RAM; optionally saved | Velocity/pressure response | Manual verified; round-trip pending |
| Pedal mode | Panel automation | EDIT/System-MIDI/2 | Data Entry | Volume / Mod | Enum | Global RAM; optionally saved | Controller routing | Manual verified; round-trip pending |
| Sustain/Aux foot-switch modes | Panel automation | EDIT/System-MIDI, scroll | Data Entry | OS enumerations | Enum | Global RAM; optionally saved | Patch-select/start-stop side effects | Manual verified; round-trip pending |
| Auto-loop finding | Panel automation | EDIT/System-MIDI/3 | Data Entry | Off / On | Boolean | Global RAM; optionally saved | Sampling behavior | Manual verified; round-trip pending |
| FX send Bus2/Bus3 | Panel automation | EDIT/System-MIDI/4 | Cursor + Data Entry | OS-defined | Display units | Global RAM; optionally saved | DSP/routing dependency | Manual verified; round-trip pending |
| MIDI base channel and routing controls | Panel automation | EDIT/System-MIDI/5-9 + scroll | Cursor + Data Entry | OS MIDI enumerations | Enum/channel identity | Global RAM; optionally saved | External MIDI side effects | Manual verified; round-trip pending |

### Voice, wavesample, and layer

| GUI parameter | Source | Read method | Write method | Value range | Scaling | Persistence | Known risks | Test status |
|---|---|---|---|---|---|---|---|---|
| Root key / fine tune | Panel automation | EDIT/Pitch/1, fields | Cursor + Data Entry | Note + OS fine units | Note/OS units | Instrument file | Selected wave/layer scope | Manual verified; round-trip pending |
| LFO pitch amount | Panel automation | EDIT/Pitch/2 | Data Entry | OS-defined bipolar | Display units | Instrument file | Modulation polarity | Manual verified; round-trip pending |
| ENV1 pitch amount | Panel automation | EDIT/Pitch/3 | Data Entry | OS-defined bipolar | Display units | Instrument file | Modulation polarity | Manual verified; round-trip pending |
| Random frequency / amount | Panel automation | EDIT/Pitch/5, fields | Cursor + Data Entry | OS-defined | Display units | Instrument file | Non-deterministic modulation | Manual verified; round-trip pending |
| Pitch-bend range | Panel automation | EDIT/Pitch/6 | Data Entry | OS-defined semitones | Semitones | Instrument file | Per-wave versus global bend | Manual verified; round-trip pending |
| Pitch mod source / amount | Panel automation | EDIT/Pitch/7, fields | Cursor + Data Entry | 15 modulators + bipolar amount | Enum/display units | Instrument file | Source enumeration | Manual verified; round-trip pending |
| Wavesample key range low/high | Panel automation | EDIT/Pitch/8, fields | Cursor + Data Entry | Keyboard notes | Note-name mapping | Instrument file | Low/high ordering | Manual verified; round-trip pending |
| ENV1/2/3 hard velocity levels 1-5 | Panel automation | EDIT/Env1-3/1, five fields | Cursor + Data Entry | 0-99 | Linear display units initially | Instrument file | Correct envelope/wave selection | Manual verified; round-trip pending |
| ENV1/2/3 soft velocity levels 1-5 | Panel automation | EDIT/Env1-3/2, five fields | Cursor + Data Entry | 0-99 | Linear display units initially | Instrument file | Velocity interpolation | Manual verified; round-trip pending |
| ENV1/2/3 times 1-5 | Panel automation | EDIT/Env1-3/3, five fields | Cursor + Data Entry | 0-99 | Nonlinear OS time table | Instrument file | Must not map linearly to seconds | Manual verified; round-trip pending |
| ENV1/2/3 second release time/level | Panel automation | EDIT/Env1-3/4, two fields | Cursor + Data Entry | OS-defined | Time table + level units | Instrument file | Two-stage release semantics | Manual verified; round-trip pending |
| ENV1/2/3 attack-time velocity | Panel automation | EDIT/Env1-3/5 | Data Entry | 0-99 | OS-defined response | Instrument file | Velocity-dependent timing | Manual verified; round-trip pending |
| ENV1/2/3 keyboard time scaling | Panel automation | EDIT/Env1-3/6 | Data Entry | OS-defined | OS-defined response | Instrument file | Root-key dependence | Manual verified; round-trip pending |
| ENV1/2/3 soft velocity curve | Panel automation | EDIT/Env1-3/7 | Data Entry | Off, Vel1, Vel2, Vel3 | Enum | Instrument file | Changes all soft/hard interpolation | Manual verified; round-trip pending |
| ENV1/2/3 mode | Panel automation | EDIT/Env1-3/8 | Data Entry | Normal, Cycle, Repeat | Enum | Instrument file | Repeat may sustain indefinitely | Manual verified; round-trip pending |
| ENV1/2/3 template | Panel automation | EDIT/Env1-3/0 | Data Entry | Current, Saved, 14 templates | Enum | Instrument file after save | Selecting/editing can replace current values | Manual verified; round-trip pending |
| LFO wave / speed | Panel automation | EDIT/LFO/1, fields | Cursor + Data Entry | 7 waves + OS speed range | Enum + OS rate units | Instrument file | Rate conversion unknown | Manual verified; round-trip pending |
| LFO depth / delay | Panel automation | EDIT/LFO/2, fields | Cursor + Data Entry | OS-defined | Display/time units | Instrument file | Delay time nonlinear | Manual verified; round-trip pending |
| LFO mode | Panel automation | EDIT/LFO/3 | Data Entry | OS enumeration | Enum | Instrument file | Key-sync/free-run semantics | Manual verified; round-trip pending |
| LFO depth-mod source/amount | Panel automation | EDIT/LFO/4, fields | Cursor + Data Entry | Modulator + bipolar amount | Enum/display units | Instrument file | Source enumeration | Manual verified; round-trip pending |
| LFO rate-mod source/amount | Panel automation | EDIT/LFO/5, fields | Cursor + Data Entry | Modulator + bipolar amount | Enum/display units | Instrument file | Source enumeration | Manual verified; round-trip pending |
| Filter mode | Panel automation | EDIT/Filter/0 | Data Entry | 4 OS modes | Enum | Instrument file | Must match ES5505 control topology | Manual verified; round-trip pending |
| F1/F2 cutoff | Panel automation | EDIT/Filter/1, fields | Cursor + Data Entry | OS-defined | OS cutoff table; not Hz yet | Instrument file | Direct K1/K2 register writes are prohibited | Manual verified; round-trip pending |
| F1/F2 ENV2 amount | Panel automation | EDIT/Filter/2, fields | Cursor + Data Entry | OS-defined bipolar | Display units | Instrument file | Dynamic per-voice result | Manual verified; round-trip pending |
| F1/F2 keyboard amount | Panel automation | EDIT/Filter/3, fields | Cursor + Data Entry | OS-defined bipolar | Display units | Instrument file | Key tracking | Manual verified; round-trip pending |
| F1/F2 mod source/amount | Panel automation | EDIT/Filter/7-8, fields | Cursor + Data Entry | Modulator + bipolar amount | Enum/display units | Instrument file | Source enumeration | Manual verified; round-trip pending |
| Wavesample volume / pan | Panel automation | EDIT/Amp/1-2, fields | Cursor + Data Entry | OS-defined | OS volume/pan law | Instrument file | ES5505 exponential volume is downstream | Manual verified; round-trip pending |
| Volume/Pan mod source/amount | Panel automation | EDIT/Amp/7-8, fields | Cursor + Data Entry | Modulator + bipolar amount | Enum/display units | Instrument file | Dynamic downstream values | Manual verified; round-trip pending |
| A-B fade-in / C-D fade-out / curve | Panel automation | EDIT/Amp/3-5 | Cursor + Data Entry | OS-defined | OS crossfade law | Instrument file | Key/velocity crossfade context | Manual verified; round-trip pending |
| Boost | Panel automation | EDIT/Amp/6 | Data Entry | Off / On (+12 dB) | Boolean | Instrument file | Clipping/headroom | Manual verified; round-trip pending |
| Output bus | Panel automation | EDIT/Amp/9 | Data Entry | OS output enumeration | Enum | Instrument file | Requires complete DSP/output routing | Manual verified; round-trip pending |
| Playback/loop mode | Panel automation | EDIT/Wave/0 | Data Entry | OS mode enumeration | Enum | Instrument file | Loop/reverse/transwave semantics | Manual verified; round-trip pending |
| Sample start/end | Panel automation | EDIT/Wave/1-2 | Data Entry | 0..sample length | Sample index | Instrument file | Must preserve start <= end | Manual verified; round-trip pending |
| Loop start/end/position | Panel automation | EDIT/Wave/3-5 | Data Entry | Within sample bounds | Sample index / OS position units | Instrument file | Bounds and mode dependency | Manual verified; round-trip pending |
| Wave modulation type/source/amount/range | Panel automation | EDIT/Wave/6-9, fields | Cursor + Data Entry | OS enumerations/ranges | Enum/display units | Instrument file | Transwave-specific behavior | Manual verified; round-trip pending |
| Layer glide mode/time and legato | Panel automation | EDIT/Layer/0-2 | Data Entry | OS enumerations/range | Enum/time units | Instrument file | Voice allocation behavior | Manual verified; round-trip pending |
| Layer velocity low/high | Panel automation | EDIT/Layer/3, fields | Cursor + Data Entry | MIDI velocity domain | Identity | Instrument file | Low/high ordering | Manual verified; round-trip pending |
| Pitch table | Panel automation | EDIT/Layer/4 | Data Entry | Table enumeration | Enum | Instrument file | Custom tuning dependency | Manual verified; round-trip pending |
| Layer name | Panel automation | EDIT/Layer/5 | Character entry | OS name length/charset | Character mapping | Instrument file | Character encoding | Manual verified; round-trip pending |
| Layer delay / velocity amount | Panel automation | EDIT/Layer/6, fields | Cursor + Data Entry | OS-defined | Time/display units | Instrument file | Trigger timing | Manual verified; round-trip pending |
| Layer restrike | Panel automation | EDIT/Layer/7 | Data Entry | OS enumeration | Enum | Instrument file | Voice retrigger behavior | Manual verified; round-trip pending |

### Sequencer and effects

| GUI parameter | Source | Read method | Write method | Value range | Scaling | Persistence | Known risks | Test status |
|---|---|---|---|---|---|---|---|---|
| Current sequence/song and Goto | Panel automation | EDIT/Seq-Song/0 | Cursor + Data Entry | Existing sequence/song positions | Enum/position | Song/bank | Changes editing context | Manual verified; round-trip pending |
| Tempo / loop | Panel automation | EDIT/Seq-Song/1, fields | Cursor + Data Entry | OS-defined BPM / loop enum | BPM + enum | Sequence/song | Playback timing | Manual verified; round-trip pending |
| Clock source | Panel automation | EDIT/Seq-Song/2 | Data Entry | OS enumeration | Enum | Sequence/song/global context | External sync dependency | Manual verified; round-trip pending |
| Click, volume, pan, output | Panel automation | EDIT/Seq-Song/3-5 | Cursor + Data Entry | OS-defined | OS units | Sequence/song | Audio routing dependency | Manual verified; round-trip pending |
| Countoff / record mode / record source | Panel automation | EDIT/Seq-Song/6-8 | Data Entry | OS enumerations | Enum | Sequence/song | Can alter recording behavior | Manual verified; round-trip pending |
| Track status | Panel automation | EDIT/Track/0 | Data Entry | Mute / Play / Solo | Enum | Sequence/song | Audible state | Manual verified; round-trip pending |
| Track mix / pan / output | Panel automation | EDIT/Track/1-2 | Cursor + Data Entry | OS-defined | OS volume/pan/output law | Sequence/song | Routing and headroom | Manual verified; round-trip pending |
| Track effect control | Panel automation | EDIT/Track/3 | Data Entry | OS-defined | Enum/display units | Sequence/song | DSP dependency | Manual verified; round-trip pending |
| Multi-In MIDI channel | Panel automation | EDIT/Track/4 | Data Entry | MIDI channel enumeration | Identity | Sequence/song | External MIDI routing | Manual verified; round-trip pending |
| Effect algorithm parameters | Panel automation | Effects page; dynamic fields | Cursor + Data Entry | Algorithm-dependent | Algorithm-specific | Instrument/bank/effect file | Cannot use one fixed parameter schema | Manual verified at algorithm level; round-trip pending |

## Low-level hardware observability (not direct GUI ownership)

| Internal value | Source | Read method | Write method | Value range | Scaling | Persistence | Known risks | Test status |
|---|---|---|---|---|---|---|---|---|
| ES5505 voice control | Hardware register `0x200000`, paged reg 0 | Emulator register read/trace | OS writes only | 16-bit bitfield | Bitfield | Reconstructed by OS | Direct GUI write can desynchronize OS | Core read/write tested; not exposed |
| ES5505 frequency | Hardware register, paged reg 1 | Emulator trace | OS writes only | 15 effective bits, stored shifted | Phase increment | Runtime voice | Not equivalent to edited pitch parameter | Core tested; not exposed |
| ES5505 start/end/accumulator | Hardware registers, paged regs 2-5/10-11 | Emulator trace | OS writes only | 31-bit normalized address | 20 integer + internal fraction | Runtime voice | Transient; unsafe as editor state | Core tested; not exposed |
| ES5505 K1/K2 | Hardware registers, paged regs 7/6 | Emulator trace | OS writes only | `0x0000..0xfff0`, step `0x10` | Chip coefficient, not Hz | Runtime voice | Modulated per voice; not stored cutoff | Core topology tested; not exposed |
| ES5505 left/right volume | Hardware registers, paged regs 8/9 | Emulator trace | OS writes only | `0x00..0xff` | 4-bit exponent + 4-bit mantissa | Runtime voice | Exponential, dynamic, downstream value | Volume law tested; not exposed |
| OS RAM parameter structures | RAM address | Targeted before/after snapshots | No GUI writes permitted yet | Unknown | Unknown | OS session / file-dependent | Layout, relocation, selection context unknown | Discovery not started |
| Enhanced resonance/features | Enhanced engine | N/A | N/A | N/A | N/A | N/A | Outside current scope | Not exposed / deferred |

## Required promotion test for a direct RAM mapping

A parameter may move from panel automation to direct RAM access only after all
of the following pass:

1. Capture before/after RAM snapshots while changing only that parameter.
2. Repeat with at least two instruments, two layers/wavesamples where relevant,
   and two non-adjacent values.
3. Identify selection/context pointers and prove the address is not incidental.
4. Read the value without changing OS behavior.
5. Write a value, confirm it in the original OS display and audio/register
   output, then restore the original value.
6. Save and reload the owning file and confirm persistence.
7. Add an automated regression test before the GUI uses the RAM path.
