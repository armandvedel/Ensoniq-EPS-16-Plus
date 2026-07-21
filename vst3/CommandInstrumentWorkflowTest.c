#include "ProbeMachine.h"

#include <stdint.h>
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
    run_for(50000000);
}

static int expect_display(const char *expected) {
    char display[23];
    eps16_probe_machine_display(display);
    printf("display=|%s| expected=|%s| illegal=%zu\n", display, expected,
           eps16_probe_machine_illegal_instructions());
    return !strncmp(display, expected, strlen(expected)) &&
           !eps16_probe_machine_illegal_instructions();
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
    if (!expect_display("NO INSTRUMENTS")) return 1;

    /* Create the complete hierarchy through original OS/KPC panel events. */
    click(0x06); /* COMMAND */
    click(0x0f); /* INSTRUMENT */
    if (!expect_display("CREATE NEW INSTRUMENT")) return 1;
    click(0x23); /* CREATE NEW INSTRUMENT */
    if (!expect_display("SELECT UNUSED INST=1")) return 1;
    click(0x23); /* SELECT UNUSED INST=1 */

    click(0x25); /* LAYER */
    if (!expect_display("CREATE NEW LAYER")) return 1;
    click(0x23); /* CREATE NEW LAYER */
    if (!expect_display("CREATE NEW LAYER")) return 1;

    click(0x24); /* WAVE */
    if (!expect_display("CREATE NEW WAVESAMPLE")) return 1;
    click(0x23); /* CREATE NEW WAVESAMPLE */
    if (!expect_display("CREATE NEW WAVESAMPLE")) return 1;

    /* Re-enter COMMAND and select the physical INSTRUMENT page key. */
    click(0x06);
    click(0x0f);
    if (!expect_display("CREATE NEW INSTRUMENT")) return 1;

    click(0x1a);
    if (!expect_display("NO INSTRUMENTS")) return 1;

    eps16_probe_machine_destroy(machine);
    return 0;
}
