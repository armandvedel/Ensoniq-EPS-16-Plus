#include "ProbeMachine.h"

#include <stdint.h>
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc != 4) return 2;
    char error[256];
    if (!eps16_probe_machine_initialize(argv[1], argv[2], argv[3],
                                        error, sizeof(error))) {
        fprintf(stderr, "%s\n", error);
        return 1;
    }
    eps16_probe_machine_run_until(220000000);
    const uint64_t before = eps16_probe_machine_midi_input_bytes();
    eps16_probe_machine_midi(0x90, 60, 100);    /* Note on */
    eps16_probe_machine_midi(0xb0, 1, 96);      /* Mod wheel */
    eps16_probe_machine_midi(0xe0, 0x00, 0x60); /* Pitch bend */
    eps16_probe_machine_midi(0xa0, 60, 72);     /* Poly pressure */
    eps16_probe_machine_midi(0xd0, 80, 0);      /* Channel pressure */
    eps16_probe_machine_midi(0x80, 60, 0);      /* Note off */
    eps16_probe_machine_run_until(222000000);
    const uint64_t consumed = eps16_probe_machine_midi_input_bytes() - before;
    printf("midi_input_bytes=%llu\n", (unsigned long long)consumed);
    /* Note on/off currently use the documented KPC compatibility path; all
       controller bytes use the original OS MIDI receiver. */
    return consumed == 11 && !eps16_probe_machine_illegal_instructions() ? 0 : 1;
}
