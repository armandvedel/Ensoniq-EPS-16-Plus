#include "ProbeMachine.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void run_for(uint64_t cycles) {
    eps16_probe_machine_run_until(eps16_probe_machine_cycles() + cycles);
}

static void click(uint8_t code) {
    eps16_probe_machine_panel_byte((uint8_t)(code | 0x80));
    eps16_probe_machine_panel_byte(0);
    run_for(1000000);
    eps16_probe_machine_panel_byte(code);
    eps16_probe_machine_panel_byte(0);
    run_for(30000000);
}

static int expect_display(const char *expected) {
    char display[23];
    eps16_probe_machine_display(display);
    printf("display=|%s|\n", display);
    return !strncmp(display, expected, strlen(expected));
}

int main(int argc, char **argv) {
    if (argc != 4) return 2;
    char error[256];
    if (!eps16_probe_machine_initialize(argv[1], argv[2], argv[3],
                                        error, sizeof(error))) {
        fprintf(stderr, "%s\n", error);
        return 1;
    }
    eps16_probe_machine_run_until(220000000);
    if (!expect_display("NO INSTRUMENTS")) return 1;

    click(0x06);
    /* Temporary native-panel compatibility for the currently incomplete KPC
       electrical scanner: the real rack commits INSTRUMENT on click/release. */
    eps16_probe_machine_panel_byte(0x1a);
    eps16_probe_machine_panel_byte(0);
    run_for(20000000);
    if (!expect_display("CREATE NEW INSTRUMENT")) return 1;

    click(0x23);
    run_for(30000000);
    if (!expect_display("SELECT UNUSED INST")) return 1;
    return eps16_probe_machine_illegal_instructions() ? 1 : 0;
}
