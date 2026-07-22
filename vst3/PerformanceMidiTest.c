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

    const size_t before = eps16_probe_machine_panel_rx_consumed();
    eps16_probe_machine_midi(0x90, 60, 100);
    run_for(20000000);
    const size_t after_note = eps16_probe_machine_panel_rx_consumed();
    eps16_probe_machine_midi(0xa0, 60, 50);
    run_for(20000000);
    const size_t after_pressure = eps16_probe_machine_panel_rx_consumed();
    eps16_probe_machine_midi(0x80, 60, 0);
    run_for(20000000);
    const size_t after_release = eps16_probe_machine_panel_rx_consumed();
    eps16_probe_machine_midi(0xa0, 60, 0);
    run_for(20000000);
    const size_t after_released_pressure =
        eps16_probe_machine_panel_rx_consumed();

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

    printf("keyboard_bytes=%zu/%zu/%zu/%zu/%zu mpe=%zu/%zu/%zu/%zu "
           "illegal=%zu\n", before, after_note, after_pressure, after_release,
           after_released_pressure, after_mpe_note, after_mpe_pressure,
           after_mpe_release, after_released_mpe_pressure,
           eps16_probe_machine_illegal_instructions());
    if (after_note != before + 2 || after_pressure != after_note ||
        after_release != after_note + 2 ||
        after_released_pressure != after_release ||
        after_mpe_note != after_released_pressure ||
        after_mpe_pressure != after_mpe_note ||
        after_mpe_release != after_mpe_note + 4 ||
        after_released_mpe_pressure != after_mpe_release ||
        !expect_analog(0, 511) || eps16_probe_machine_illegal_instructions())
        failed = 1;

    eps16_probe_machine_destroy(machine);
    return failed ? 1 : 0;
}
