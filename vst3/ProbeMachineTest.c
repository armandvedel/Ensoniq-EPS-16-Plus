#include "ProbeMachine.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static void run_for(uint64_t cycles) {
    eps16_probe_machine_run_until(eps16_probe_machine_cycles() + cycles);
}

static void click(uint8_t raw_code) {
    eps16_probe_machine_panel_byte((uint8_t)(raw_code | 0x80));
    eps16_probe_machine_panel_byte(0);
    run_for(1000000);
    eps16_probe_machine_panel_byte(raw_code);
    eps16_probe_machine_panel_byte(0);
    run_for(30000000);
}

static int display_starts_with(const char *expected) {
    char display[23];
    eps16_probe_machine_display(display);
    printf("cycles=%llu display=|%s| illegal=%zu\n",
           (unsigned long long)eps16_probe_machine_cycles(), display,
           eps16_probe_machine_illegal_instructions());
    return !strncmp(display, expected, strlen(expected));
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s COMBINED_ROM KPC_ROM OS_DISK\n", argv[0]);
        return 2;
    }
    char error[256];
    if (!eps16_probe_machine_initialize(argv[1], argv[2], argv[3],
                                        error, sizeof(error))) {
        fprintf(stderr, "machine initialization failed: %s\n", error);
        return 1;
    }
    eps16_probe_machine_run_until(220000000);
    if (!display_starts_with("NO INSTRUMENTS")) return 1;
    if (eps16_probe_machine_illegal_instructions()) return 1;

    /* Exercise the same raw physical KPC transitions emitted by the VST GUI.
       SAMPLE and Track 1 must remain decisions of the original firmware/OS. */
    click(0x20);
    if (!display_starts_with("PICK SAMPLE INSTRUMENT")) return 1;
    click(0x02);
    eps16_probe_machine_sampling_input(0.25f, -0.125f);
    run_for(50000000);
    display_starts_with("");

    /* The sampling level meter uses non-character VFD traffic. ENTER release
       is the original OS/KPC transition into RECORD; a later ENTER press is
       the deliberate stop transition. */
    const uint64_t writes_before =
        eps16_probe_machine_sample_ram_write_bytes();
    const uint64_t conversions_before =
        eps16_probe_machine_sampling_input_conversions();
    click(0x23);
    run_for(100000000);
    display_starts_with("");
    const uint64_t recorded_bytes =
        eps16_probe_machine_sample_ram_write_bytes() - writes_before;
    const uint64_t input_conversions =
        eps16_probe_machine_sampling_input_conversions() - conversions_before;
    printf("recorded_bytes=%llu input_conversions=%llu\n",
           (unsigned long long)recorded_bytes,
           (unsigned long long)input_conversions);
    if (!recorded_bytes || !input_conversions) return 1;
    click(0x23);
    run_for(100000000);
    if (!display_starts_with("PLAY ROOT KEY")) return 1;
    eps16_probe_machine_midi(0x90, 60, 100);
    float playback_peak = 0.0f;
    for (unsigned int block = 0; block < 300; ++block) {
        run_for(100000);
        float left = 0.0f;
        float right = 0.0f;
        eps16_probe_machine_stereo_output(&left, &right);
        if (fabsf(left) > playback_peak) playback_peak = fabsf(left);
        if (fabsf(right) > playback_peak) playback_peak = fabsf(right);
    }
    display_starts_with("");
    int cursor_start = -1;
    int cursor_end = -1;
    eps16_probe_machine_cursor(&cursor_start, &cursor_end);
    printf("cursor=%d..%d\n", cursor_start, cursor_end);
    if (cursor_start < 0 || cursor_end <= cursor_start || cursor_end > 22)
        return 1;
    printf("playback_peak=%f\n", playback_peak);
    if (playback_peak < 0.00001f) return 1;
    eps16_probe_machine_midi(0x80, 60, 0);
    run_for(1000000);
    return 0;
}
