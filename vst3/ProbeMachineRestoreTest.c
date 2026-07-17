#include "ProbeMachine.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr,
                "usage: %s COMBINED_ROM KPC_ROM OS_DISK SNAPSHOT\n", argv[0]);
        return 2;
    }
    Eps16ProbeMachine *machine = eps16_probe_machine_create();
    if (!machine || !eps16_probe_machine_begin(machine)) return 1;
    char error[256];
    if (!eps16_probe_machine_initialize(argv[1], argv[2], argv[3],
                                        error, sizeof(error))) {
        fprintf(stderr, "machine initialization failed: %s\n", error);
        return 1;
    }
    FILE *file = fopen(argv[4], "rb");
    if (!file || fseek(file, 0, SEEK_END)) return 1;
    const long file_size = ftell(file);
    if (file_size <= 0 || fseek(file, 0, SEEK_SET)) return 1;
    void *snapshot = malloc((size_t)file_size);
    if (!snapshot || fread(snapshot, 1, (size_t)file_size, file) !=
                         (size_t)file_size || fclose(file))
        return 1;
    if (!eps16_probe_machine_load_state(snapshot, (size_t)file_size,
                                        error, sizeof(error))) {
        fprintf(stderr, "snapshot restore failed: %s\n", error);
        return 1;
    }
    free(snapshot);
    char display[23];
    eps16_probe_machine_display(display);
    if (strncmp(display, "MODE=FORWARD-NO LOOP", 20)) {
        fprintf(stderr, "unexpected restored display: |%s|\n", display);
        return 1;
    }
    eps16_probe_machine_midi(0x80, 60, 0);
    eps16_probe_machine_run_until(eps16_probe_machine_cycles() + 1000000);
    eps16_probe_machine_midi(0x90, 60, 100);
    float peak = 0.0f;
    for (unsigned int block = 0; block < 100; ++block) {
        eps16_probe_machine_run_until(eps16_probe_machine_cycles() + 100000);
        float left = 0.0f;
        float right = 0.0f;
        eps16_probe_machine_stereo_output(&left, &right);
        if (fabsf(left) > peak) peak = fabsf(left);
        if (fabsf(right) > peak) peak = fabsf(right);
    }
    printf("restored_cycles=%llu display=|%s| playback_peak=%f illegal=%zu\n",
           (unsigned long long)eps16_probe_machine_cycles(), display, peak,
           eps16_probe_machine_illegal_instructions());
    if (peak < 0.00001f || eps16_probe_machine_illegal_instructions()) return 1;
    eps16_probe_machine_end(machine);
    eps16_probe_machine_destroy(machine);
    return 0;
}
