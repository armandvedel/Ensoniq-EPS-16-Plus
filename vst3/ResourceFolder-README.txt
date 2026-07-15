EPS-16 Plus external files
==========================

Place the user-supplied files in this EPS_files folder, next to the VST3
plug-in. The plug-in never contains or redistributes these copyrighted files.

Preferred names:

  eps16plus-rom.bin   combined 128 KiB U28/U27 main ROM
  eps16plus-kpc.bin   32 KiB KPC 2.33 EPROM
  EPS130OS.img        819,200-byte logical OS disk

EPS130OS.hfe is also accepted. The original KPC filename
"Ensoniq EPS KPC2 v2.33 27c256.BIN" is recognized without renaming. If a
preferred name is absent, unique .bin/.rom files with the expected ROM sizes
and an .img with the expected disk size are detected.

Typical installed layout:

  ~/Library/Audio/Plug-Ins/VST3/
    EPS-16 Plus Prototype.vst3
    EPS_files/
      eps16plus-rom.bin
      eps16plus-kpc.bin
      EPS130OS.img
