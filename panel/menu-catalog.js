// Transcribed from the supplied EPS-16 PLUS manual. Each entry represents one
// display page; semicolon-separated labels are fields on the same display.
const envPages = [
  { number: 1, name: 'HARD VEL LEVELS' },
  { number: 2, name: 'SOFT VEL LEVELS' },
  { number: 3, name: 'TIMES' },
  { number: 4, name: '2ND RELEASE TIME; LEVEL' },
  { number: 5, name: 'ATTACK TIME VELOCITY' },
  { number: 6, name: 'KBD TIME SCALING' },
  { number: 7, name: 'SOFT VEL CURVE' },
  { number: 8, name: 'ENVELOPE MODE' },
  { number: 0, name: 'ENVELOPE=' }
];

window.EPS16_MENU_CATALOG = {
  EDIT: {
    INSTRUMENT: [
      { number: 0, name: 'PATCH' },
      { number: 1, name: 'KEYDOWN LAYERS' },
      { number: 2, name: 'KEYUP LAYERS' },
      { number: 3, name: 'MIDI OUT CHANNEL' },
      { number: 4, name: 'MIDI OUT PROGRAM' },
      { number: 5, name: 'PRESSURE MODE' },
      { number: 6, name: 'MIDI STATUS' },
      { number: 7, name: 'SIZE' },
      { number: 8, name: 'INST NAME' },
      { number: 9, name: 'PATCH SELECT' },
      { name: 'INSTRUMENT KEY RANGE' },
      { name: 'TRANSPOSE' }
    ],
    'SEQ-SONG': [
      { number: 0, name: 'CURRENT SEQ/SONG; GOTO' },
      { number: 1, name: 'TEMPO; LOOP' },
      { number: 2, name: 'CLOCK SOURCE' },
      { number: 3, name: 'CLICK' },
      { number: 4, name: 'CLICK VOLUME' },
      { number: 5, name: 'CLICK PAN; OUTPUT' },
      { number: 6, name: 'SEQ COUNTOFF' },
      { number: 7, name: 'RECORD MODE' },
      { number: 8, name: 'RECORD SOURCE' }
    ],
    'SYSTEM-MIDI': [
      { number: 0, name: 'FREE SYSTEM BLOCKS' },
      { name: 'FREE DISK BLOCKS' },
      { number: 1, name: 'MASTER TUNE' },
      { name: 'GLOBAL BEND RANGE' },
      { name: 'TOUCH' },
      { number: 2, name: 'PEDAL' },
      { name: 'SUSTAIN FT SWITCH' },
      { name: 'AUX FT SWITCH' },
      { number: 3, name: 'AUTO-LOOP FINDING' },
      { number: 4, name: 'FX SEND BUS2; BUS3' },
      { number: 5, name: 'MIDI BASE CHANNEL' },
      { number: 6, name: 'TRANSMIT ON' },
      { name: 'BASE CHANNEL PRESSURE' },
      { number: 7, name: 'MIDI IN MODE' },
      { name: 'GLOBAL CONTROLLERS' },
      { name: 'MULTI CONTROLLERS' },
      { number: 8, name: 'MIDI CONTROLLERS' },
      { name: 'MIDI SYS-EX' },
      { number: 9, name: 'MIDI PROGRAM CHANGE' },
      { name: 'MIDI SONG SELECT' },
      { name: 'MIDI XCTRL NUMBER' }
    ],
    EFFECTS: [{ name: 'EFFECT PARAMETERS: TYPE DEPENDENT' }],
    '1-ENV1': envPages,
    '2-ENV2': envPages,
    '3-ENV3': envPages,
    '4-PITCH': [
      { number: 1, name: 'ROOT KEY; FINE' },
      { number: 2, name: 'LFO AMOUNT' },
      { number: 3, name: 'ENV1 AMOUNT' },
      { number: 5, name: 'RANDOM FREQ; AMOUNT' },
      { number: 6, name: 'PITCH BEND RANGE' },
      { number: 7, name: 'PITCH MOD; AMOUNT' },
      { number: 8, name: 'WS RANGE LO; HI' }
    ],
    '5-FILTER': [
      { number: 0, name: 'MODE' },
      { number: 1, name: 'F1 CUTOFF; F2 CUTOFF' },
      { number: 2, name: 'F1 ENV2; F2 ENV2' },
      { number: 3, name: 'F1 KBD; F2 KBD' },
      { number: 7, name: 'F1 MOD; AMOUNT' },
      { number: 8, name: 'F2 MOD; AMOUNT' }
    ],
    '6-AMP': [
      { direct: [1, 2], name: 'WS VOLUME; PAN' },
      { number: 7, name: 'VOLUME MOD; AMOUNT' },
      { number: 8, name: 'PAN MOD; AMOUNT' },
      { number: 3, name: 'A-B FADE IN' },
      { number: 4, name: 'C-D FADE OUT' },
      { number: 5, name: 'FADECURVE' },
      { number: 6, name: 'BOOST' },
      { number: 9, name: 'OUT' }
    ],
    '7-LFO': [
      { number: 1, name: 'LFO WAVE; SPEED' },
      { number: 2, name: 'LFO DEPTH; DELAY' },
      { number: 3, name: 'LFO MODE' },
      { number: 4, name: 'LFO MOD; AMOUNT' },
      { number: 5, name: 'RATE MOD; AMOUNT' }
    ],
    '8-WAVE': [
      { number: 0, name: 'MODE' },
      { number: 1, name: 'SAMPLE START' },
      { number: 2, name: 'SAMPLE END' },
      { number: 3, name: 'LOOP START' },
      { number: 4, name: 'LOOP END' },
      { number: 5, name: 'LOOP POSITION' },
      { direct: [6, 7], name: 'WAVE MOD TYPE; SOURCE' },
      { direct: [8, 9], name: 'WAVE MOD AMOUNT; RANGE' }
    ],
    '9-LAYER': [
      { number: 0, name: 'LAYER GLIDE MODE' },
      { number: 1, name: 'LAYER GLIDE TIME' },
      { number: 2, name: 'LEGATO LAYER' },
      { number: 3, name: 'LAYER VEL LO; HI' },
      { number: 4, name: 'PITCH TABLE' },
      { number: 5, name: 'LAYER NAME' },
      { number: 6, name: 'DELAY; VELOCITY AMT' },
      { number: 7, name: 'LAYER RESTRIKE' }
    ],
    '0-TRACK': [
      { number: 0, name: 'TRACK STATUS' },
      { number: 1, name: 'TRACK MIX; PAN' },
      { number: 2, name: 'TRACK OUTPUT' },
      { number: 3, name: 'EFFECT CONTROL' },
      { number: 4, name: 'MULTI-IN MIDI CHANNEL' }
    ]
  },
  COMMAND: {
    INSTRUMENT: [
      { number: 0, name: 'CREATE NEW INSTRUMENT' },
      { number: 1, name: 'COPY INSTRUMENT' },
      { number: 2, name: 'DELETE INSTRUMENT' },
      { number: 3, name: 'SAVE INSTRUMENT' },
      { number: 4, name: 'SAVE BANK' },
      { number: 5, name: 'CREATE PRESET' },
      { number: 6, name: 'DELETE INST EFFECT' }
    ],
    'SEQ-SONG': [
      { number: 0, name: 'CREATE NEW SEQUENCE' },
      { number: 1, name: 'COPY SEQUENCE' },
      { number: 2, name: 'DELETE SEQUENCE' },
      { number: 3, name: 'SAVE CURRENT SEQUENCE' },
      { number: 4, name: 'SAVE SONG + ALL SEQS' },
      { number: 5, name: 'RENAME SONG/SEQUENCE' },
      { number: 6, name: 'SEQUENCER INFORMATION' },
      { number: 7, name: 'ERASE SONG + ALL SEQS' },
      { number: 8, name: 'APPEND SEQUENCE' },
      { number: 9, name: 'CHANGE SEQUENCE LENGTH' },
      { name: 'SELECT LOADABLE INST' },
      { name: 'EDIT SONG STEPS' }
    ],
    'SYSTEM-MIDI': [
      { number: 0, name: 'FORMAT FLOPPY DISK' },
      { number: 1, name: 'COPY O.S. TO DISK' },
      { number: 2, name: 'SAVE GLOBAL PARAMETERS' },
      { number: 3, name: 'LOAD GLOBAL PARAMETERS' },
      { number: 4, name: 'CREATE DIRECTORY' },
      { number: 5, name: 'CHANGE STORAGE DEVICE' },
      { number: 6, name: 'SAVE MACRO FILE' },
      { number: 7, name: 'COPY FLOPPY DISK' },
      { number: 8, name: 'FORMAT FLASH BANK' },
      { number: 9, name: 'MIDI SYS-EX RECORDER' },
      { name: 'WRITE DISK LABEL' },
      { name: 'LOAD MIRAGE-DSK SOUND' },
      { name: 'FORMAT SCSI DRIVE' }
    ],
    EFFECTS: [
      { number: 0, name: 'SAVE BANK EFFECT' },
      { number: 1, name: 'COPY CURRENT EFFECT' }
    ],
    '4-PITCH': [
      { number: 0, name: 'EDIT PITCH TABLE' },
      { number: 1, name: 'COPY PITCH TABLE' },
      { number: 2, name: 'DELETE PITCH TABLE' },
      { number: 3, name: 'EXTRAPOLATE PITCH TABLE' }
    ],
    '6-AMP': [
      { number: 0, name: 'NORMALIZE GAIN' },
      { number: 1, name: 'VOLUME SMOOTHING' },
      { number: 2, name: 'MIX WAVESAMPLES' },
      { number: 3, name: 'MERGE WAVESAMPLES' },
      { number: 4, name: 'SPLICE WAVESAMPLES' },
      { number: 5, name: 'FADE IN' },
      { number: 6, name: 'FADE OUT' }
    ],
    '7-LFO': [
      { number: 0, name: 'CLEAR DATA' },
      { number: 1, name: 'COPY DATA' },
      { number: 2, name: 'REPLICATE DATA' },
      { number: 3, name: 'REVERSE DATA' },
      { number: 4, name: 'INVERT DATA' },
      { number: 5, name: 'ADD DATA' },
      { number: 6, name: 'SCALE DATA' }
    ],
    '8-WAVE': [
      { number: 0, name: 'CREATE NEW WAVESAMPLE' },
      { number: 1, name: 'COPY WAVESAMPLE' },
      { number: 2, name: 'DELETE WAVESAMPLE' },
      { number: 3, name: 'WAVESAMPLE INFORMATION' },
      { number: 4, name: 'TRUNCATE WAVESAMPLE' },
      { number: 5, name: 'CROSS FADE LOOP' },
      { number: 6, name: 'REVERSE CROSS FADE' },
      { number: 7, name: 'ENSEMBLE CROSS FADE' },
      { number: 8, name: 'BOWTIE CROSS FADE LOOP' },
      { number: 9, name: 'BIDIRECTIONAL X-FADE' },
      { name: 'MAKE LOOP LONGER' },
      { name: 'SYNTHESIZED LOOP' },
      { name: 'CONVERT SAMPLE RATE' },
      { name: 'RESAMPLE WITH EFFECT' },
      { name: 'COPY WAVE PARAMETERS' }
    ],
    '9-LAYER': [
      { number: 0, name: 'CREATE NEW LAYER' },
      { number: 1, name: 'COPY LAYER' },
      { number: 2, name: 'DELETE LAYER' }
    ],
    '0-TRACK': [
      { number: 0, name: 'QUANTIZE TRACK' },
      { number: 1, name: 'COPY TRACK' },
      { number: 2, name: 'ERASE/UNDEFINE TRACK' },
      { number: 3, name: 'FILTER EVENT' },
      { number: 4, name: 'MERGE TWO TRACKS' },
      { number: 5, name: 'EVENT EDIT TRACKS' },
      { number: 6, name: 'TRANSPOSE TRACK' },
      { number: 7, name: 'SCALE EVENT' },
      { number: 8, name: 'SHIFT TRACK BY CLOCKS' }
    ]
  }
};
