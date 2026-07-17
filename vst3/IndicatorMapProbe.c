#include "ProbeMachine.h"

#include <stdint.h>
#include <stdio.h>

static void run_for(uint64_t cycles) {
    eps16_probe_machine_run_until(eps16_probe_machine_cycles() + cycles);
}

static void click(uint8_t code) {
    eps16_probe_machine_panel_byte((uint8_t)(code | 0x80));
    eps16_probe_machine_panel_byte(0);
    run_for(1000000);
    eps16_probe_machine_panel_byte(code);
    eps16_probe_machine_panel_byte(0);
    run_for(9000000);
}

static void show(const char *label) {
    char display[23];
    eps16_probe_machine_display(display);
    printf("%-12s |%s| %04x/%04x %04x/%04x %04x/%04x\n", label,
           display,
           eps16_probe_machine_indicator_on(0),
           eps16_probe_machine_indicator_flash(0),
           eps16_probe_machine_indicator_on(1),
           eps16_probe_machine_indicator_flash(1),
           eps16_probe_machine_indicator_on(2),
           eps16_probe_machine_indicator_flash(2));
}

int main(int argc, char **argv) {
    if (argc != 4) return 2;
    Eps16ProbeMachine *machine = eps16_probe_machine_create();
    if (!machine || !eps16_probe_machine_begin(machine)) return 1;
    char error[256];
    if (!eps16_probe_machine_initialize(argv[1], argv[2], argv[3],
                                        error, sizeof(error))) {
        fprintf(stderr, "%s\n", error);
        return 1;
    }
    eps16_probe_machine_run_until(220000000);
    const struct { const char *name; uint8_t code; } pages[] = {
        {"INSTRUMENT", 0x1a}, {"SEQ-SONG", 0x15},
        {"SYSTEM-MIDI", 0x1b}, {"EFFECTS", 0x09},
        {"ENV", 0x0d}, {"PITCH", 0x18}, {"FILTER", 0x19},
        {"AMP", 0x1e}, {"LFO", 0x1f}, {"WAVE", 0x24},
        {"LAYER", 0x25}, {"TRACK", 0x0c}
    };
    click(0x05);
    for (unsigned int index = 0;
         index < sizeof(pages) / sizeof(pages[0]); ++index) {
        click(pages[index].code);
        /* 3f is outside the verified panel matrix and is ignored by the OS;
           it supplies the next physical KPC transaction without selecting a
           different parameter or manufacturing display state. */
        click(0x3f);
        click(0x3f);
        show(pages[index].name);
    }
    const int result = eps16_probe_machine_illegal_instructions() ? 1 : 0;
    eps16_probe_machine_end(machine);
    eps16_probe_machine_destroy(machine);
    return result;
}
