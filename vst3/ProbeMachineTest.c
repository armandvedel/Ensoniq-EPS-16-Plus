#include "ProbeMachine.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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

static int display_is_blank(void) {
    char display[23];
    eps16_probe_machine_display(display);
    printf("cycles=%llu recording_display=|%s|\n",
           (unsigned long long)eps16_probe_machine_cycles(), display);
    return strspn(display, " ") == 22;
}

static void print_indicators(const char *label) {
    printf("indicators[%s]=%04x/%04x %04x/%04x %04x/%04x\n", label,
           eps16_probe_machine_indicator_on(0),
           eps16_probe_machine_indicator_flash(0),
           eps16_probe_machine_indicator_on(1),
           eps16_probe_machine_indicator_flash(1),
           eps16_probe_machine_indicator_on(2),
           eps16_probe_machine_indicator_flash(2));
}

int main(int argc, char **argv) {
    if (argc != 4 && argc != 5) {
        fprintf(stderr, "usage: %s COMBINED_ROM KPC_ROM OS_DISK [SNAPSHOT]\n",
                argv[0]);
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
    eps16_probe_machine_run_until(220000000);
    if (!display_starts_with("NO INSTRUMENTS")) return 1;
    print_indicators("load-instrument");
    if (eps16_probe_machine_illegal_instructions()) return 1;

    click(0x05);
    click(0x1b);
    run_for(10000000);
    display_starts_with("");
    print_indicators("edit-system-midi");

    /* Exercise the same raw physical KPC transitions emitted by the VST GUI.
       SAMPLE and Track 1 must remain decisions of the original firmware/OS. */
    click(0x20);
    if (!display_starts_with("PICK SAMPLE INSTRUMENT")) return 1;
    run_for(5000000);
    print_indicators("pick-sample-instrument");
    click(0x02);
    eps16_probe_machine_sampling_input(0.25f, -0.125f);
    run_for(50000000);
    display_starts_with("");
    print_indicators("sample-track1");
    if ((eps16_probe_machine_indicator_on(0) & 0x0101U) != 0x0101U)
        return 1;
    if (eps16_probe_machine_indicator_on(1) ||
        eps16_probe_machine_indicator_on(2))
        return 1;

    /* The sampling level meter uses non-character VFD traffic. ENTER release
       is the original OS/KPC transition into RECORD; a later ENTER press is
       the deliberate stop transition. */
    const uint64_t writes_before =
        eps16_probe_machine_sample_ram_write_bytes();
    const uint64_t conversions_before =
        eps16_probe_machine_sampling_input_conversions();
    click(0x23);
    run_for(100000000);
    if (!display_is_blank()) return 1;
    print_indicators("recording");
    /* Bank 0 contains the loaded/selected Track LEDs. The original OS turns
       on only the fixed REC legend (right annunciator bank, index 3) once the
       recording transition has completed. */
    if (eps16_probe_machine_indicator_on(1) ||
        eps16_probe_machine_indicator_on(2) != 0x0008U)
        return 1;
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
    Eps16ProbeAudioFrame queued[64];
    uint64_t previous_audio_cycle = 0;
    size_t queued_audio_frames = 0;
    for (;;) {
        const size_t count = eps16_probe_machine_drain_audio(queued, 64);
        for (size_t index = 0; index < count; ++index) {
            if (queued[index].cpu_cycle < previous_audio_cycle ||
                queued[index].clock_divider < 16 ||
                queued[index].clock_divider > 512 ||
                queued[index].clock_divider % 16)
                return 1;
            previous_audio_cycle = queued[index].cpu_cycle;
        }
        queued_audio_frames += count;
        if (count < 64) break;
    }
    printf("queued_audio_frames=%zu last_audio_cycle=%llu\n",
           queued_audio_frames, (unsigned long long)previous_audio_cycle);
    if (!queued_audio_frames) return 1;

    const size_t snapshot_size = eps16_probe_machine_state_size();
    void *snapshot = malloc(snapshot_size);
    if (!snapshot || !eps16_probe_machine_save_state(snapshot, snapshot_size))
        return 1;
    if (argc == 5) {
        FILE *snapshot_file = fopen(argv[4], "wb");
        if (!snapshot_file ||
            fwrite(snapshot, 1, snapshot_size, snapshot_file) != snapshot_size ||
            fclose(snapshot_file))
            return 1;
    }
    const uint64_t saved_cycle = eps16_probe_machine_cycles();
    const uint64_t saved_writes = eps16_probe_machine_sample_ram_write_bytes();
    char saved_display[23];
    eps16_probe_machine_display(saved_display);
    run_for(5000000);
    if (eps16_probe_machine_cycles() == saved_cycle) return 1;
    char restore_error[256];
    if (!eps16_probe_machine_load_state(snapshot, snapshot_size,
                                        restore_error, sizeof(restore_error))) {
        fprintf(stderr, "snapshot restore failed: %s\n", restore_error);
        return 1;
    }
    free(snapshot);
    if (eps16_probe_machine_cycles() != saved_cycle ||
        eps16_probe_machine_sample_ram_write_bytes() != saved_writes)
        return 1;
    char restored_display[23];
    eps16_probe_machine_display(restored_display);
    if (memcmp(saved_display, restored_display, sizeof(saved_display))) return 1;
    run_for(1000000);
    if (eps16_probe_machine_illegal_instructions()) return 1;
    printf("snapshot_size=%zu restored_cycle=%llu\n", snapshot_size,
           (unsigned long long)saved_cycle);
    eps16_probe_machine_midi(0x80, 60, 0);
    run_for(1000000);
    eps16_probe_machine_end(machine);
    eps16_probe_machine_destroy(machine);
    return 0;
}
