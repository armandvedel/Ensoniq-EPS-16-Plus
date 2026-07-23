#include "ProbeMachine.h"

#include <stdint.h>
#include <stdio.h>

static void run_for(uint64_t cycles) {
    eps16_probe_machine_run_until(eps16_probe_machine_cycles() + cycles);
}

static int expect_analog(unsigned int channel, unsigned int expected) {
    const unsigned int actual = eps16_probe_machine_analog_value(channel);
    printf("analog[%u]=%u expected=%u\n", channel, actual, expected);
    return actual == expected;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s COMBINED_ROM KPC_ROM OS_DISK\n", argv[0]);
        return 2;
    }

    Eps16ProbeMachine *machine = eps16_probe_machine_create();
    char error[256] = {0};
    if (!machine || !eps16_probe_machine_begin(machine) ||
        !eps16_probe_machine_initialize(argv[1], argv[2], argv[3], error,
                                        sizeof(error))) {
        fprintf(stderr, "machine initialization failed: %s\n", error);
        return 1;
    }
    eps16_probe_machine_run_until(220000000);

    int failed = 0;
    eps16_probe_machine_midi(0xe0, 0x00, 0x00);
    if (!expect_analog(0, 1023)) failed = 1;
    eps16_probe_machine_midi(0xe0, 0x00, 0x40);
    if (!expect_analog(0, 511)) failed = 1;
    eps16_probe_machine_midi(0xe0, 0x7f, 0x7f);
    if (!expect_analog(0, 0)) failed = 1;

    eps16_probe_machine_midi(0xb0, 1, 0);
    if (!expect_analog(2, 1023)) failed = 1;
    eps16_probe_machine_midi(0xb0, 1, 127);
    if (!expect_analog(2, 0)) failed = 1;

    /* Host transport is serialized into the real MC68681 channel-A receive
       path. The original OS must consume the realtime bytes; ordinary notes
       below deliberately retain their established direct KPC path. */
    const size_t clock_before = eps16_probe_machine_midi_rx_consumed();
    eps16_probe_machine_midi(0xfa, 0, 0);
    eps16_probe_machine_midi(0xf8, 0, 0);
    eps16_probe_machine_midi(0xfc, 0, 0);
    run_for(2000000);
    const size_t clock_after = eps16_probe_machine_midi_rx_consumed();
    printf("midi_clock_bytes=%zu/%zu\n", clock_before, clock_after);
    if (clock_after != clock_before + 3) failed = 1;

    const size_t before = eps16_probe_machine_panel_rx_consumed();
    eps16_probe_machine_midi(0x90, 60, 100);
    run_for(20000000);
    const size_t after_note = eps16_probe_machine_panel_rx_consumed();
    const size_t pressure_midi_before =
        eps16_probe_machine_midi_rx_consumed();
    eps16_probe_machine_midi(0xa0, 60, 50);
    run_for(20000000);
    const size_t after_pressure = eps16_probe_machine_panel_rx_consumed();
    const size_t pressure_midi_after =
        eps16_probe_machine_midi_rx_consumed();
    eps16_probe_machine_midi(0x80, 60, 0);
    run_for(20000000);
    const size_t after_release = eps16_probe_machine_panel_rx_consumed();
    eps16_probe_machine_midi(0xa0, 60, 0);
    run_for(20000000);
    const size_t after_released_pressure =
        eps16_probe_machine_panel_rx_consumed();

    /* Conventional channel-1 poly pressure enters the original MIDI receive
       path. Even a dense stream cannot delay the established direct KPC
       release path or turn pressure into another local key-down packet. */
    eps16_probe_machine_midi(0x90, 64, 100);
    run_for(20000000);
    const size_t before_dense_pressure =
        eps16_probe_machine_panel_rx_consumed();
    const size_t before_dense_pressure_midi =
        eps16_probe_machine_midi_rx_consumed();
    for (unsigned int value = 0; value < 64; ++value)
        eps16_probe_machine_midi(0xa0, 64, (uint8_t)value);
    eps16_probe_machine_midi(0x80, 64, 0);
    run_for(20000000);
    const size_t after_dense_release =
        eps16_probe_machine_panel_rx_consumed();
    const size_t after_dense_pressure_midi =
        eps16_probe_machine_midi_rx_consumed();

    /* Push-class MPE traffic arrives densely: note events use member channels,
       followed by a stream of per-note channel pressure and pitch bend.  The
       EPS has no MPE input, so those expression messages must not turn into
       repeated KPC key-down packets or alter its global physical pitch wheel.
       In particular, the release must remain the next keyboard packet. */
    eps16_probe_machine_midi(0xe0, 0x00, 0x40);
    eps16_probe_machine_midi(0x91, 62, 90);
    const size_t after_mpe_note = eps16_probe_machine_panel_rx_consumed();
    for (unsigned int value = 0; value < 128; ++value) {
        /* Ableton's VST3 path presents Push pressure as a PolyPressureEvent,
           which JUCE converts to ordinary A0 before processBlock(). */
        eps16_probe_machine_midi(0xa1, 62, (uint8_t)value);
        eps16_probe_machine_midi(0xd1, (uint8_t)value, 0);
        eps16_probe_machine_midi(0xe1, (uint8_t)(value & 0x7f),
                                (uint8_t)(value >> 1));
    }
    const size_t after_mpe_pressure = eps16_probe_machine_panel_rx_consumed();
    eps16_probe_machine_midi(0x81, 62, 0);
    run_for(20000000);
    const size_t after_mpe_release = eps16_probe_machine_panel_rx_consumed();
    eps16_probe_machine_midi(0xd1, 0, 0);
    run_for(20000000);
    const size_t after_released_mpe_pressure =
        eps16_probe_machine_panel_rx_consumed();

    printf("keyboard_bytes=%zu/%zu/%zu/%zu/%zu pressure_midi=%zu/%zu "
           "dense=%zu/%zu midi=%zu/%zu "
           "mpe=%zu/%zu/%zu/%zu illegal=%zu\n",
           before, after_note, after_pressure, after_release,
           after_released_pressure, pressure_midi_before, pressure_midi_after,
           before_dense_pressure, after_dense_release,
           before_dense_pressure_midi, after_dense_pressure_midi,
           after_mpe_note,
           after_mpe_pressure, after_mpe_release, after_released_mpe_pressure,
           eps16_probe_machine_illegal_instructions());
    if (after_note != before + 2 || after_pressure != after_note ||
        pressure_midi_after != pressure_midi_before + 3 ||
        after_release != after_pressure + 2 ||
        after_released_pressure != after_release ||
        after_dense_release != before_dense_pressure + 2 ||
        after_dense_pressure_midi != before_dense_pressure_midi + 64 * 3 ||
        after_mpe_note != after_dense_release ||
        after_mpe_pressure != after_mpe_note ||
        after_mpe_release != after_mpe_note + 4 ||
        after_released_mpe_pressure != after_mpe_release ||
        !expect_analog(0, 511) || eps16_probe_machine_illegal_instructions())
        failed = 1;

    eps16_probe_machine_destroy(machine);
    return failed ? 1 : 0;
}
