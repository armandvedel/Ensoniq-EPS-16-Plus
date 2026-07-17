#include "ProbeMachine.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    Eps16ProbeMachine *machine;
    uint64_t advance;
    uint64_t resulting_cycle;
    char display[23];
    int ok;
} ThreadRun;

static void click(uint8_t code) {
    eps16_probe_machine_panel_byte((uint8_t)(code | 0x80));
    eps16_probe_machine_panel_byte(0);
    eps16_probe_machine_run_until(eps16_probe_machine_cycles() + 1000000);
    eps16_probe_machine_panel_byte(code);
    eps16_probe_machine_panel_byte(0);
    eps16_probe_machine_run_until(eps16_probe_machine_cycles() + 30000000);
}

static int display_starts_with(const char *expected) {
    char display[23];
    eps16_probe_machine_display(display);
    return !strncmp(display, expected, strlen(expected));
}

static void *run_on_thread(void *context) {
    ThreadRun *run = context;
    if (!eps16_probe_machine_begin(run->machine)) return NULL;
    const uint64_t target = eps16_probe_machine_cycles() + run->advance;
    eps16_probe_machine_run_until(target);
    run->resulting_cycle = eps16_probe_machine_cycles();
    eps16_probe_machine_display(run->display);
    run->ok = !eps16_probe_machine_illegal_instructions();
    eps16_probe_machine_end(run->machine);
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s COMBINED_ROM KPC_ROM OS_DISK\n", argv[0]);
        return 2;
    }
    Eps16ProbeMachine *first = eps16_probe_machine_create();
    Eps16ProbeMachine *second = eps16_probe_machine_create();
    if (!first || !second) return 1;

    char error[256] = {0};
    if (!eps16_probe_machine_begin(first)) return 1;
    if (!eps16_probe_machine_initialize(argv[1], argv[2], argv[3],
                                        error, sizeof(error))) {
        fprintf(stderr, "first init failed: %s\n", error);
        return 1;
    }
    eps16_probe_machine_run_until(220000000);
    if (!display_starts_with("NO INSTRUMENTS")) return 1;
    const uint64_t first_boot_cycle = eps16_probe_machine_cycles();
    eps16_probe_machine_end(first);

    if (!eps16_probe_machine_begin(second)) return 1;
    if (!eps16_probe_machine_initialize(argv[1], argv[2], argv[3],
                                        error, sizeof(error))) {
        fprintf(stderr, "second init failed: %s\n", error);
        return 1;
    }
    eps16_probe_machine_run_until(220000000);
    if (!display_starts_with("NO INSTRUMENTS")) return 1;
    const uint64_t second_boot_cycle = eps16_probe_machine_cycles();
    eps16_probe_machine_end(second);

    if (!eps16_probe_machine_begin(first)) return 1;
    click(0x20);
    if (!display_starts_with("PICK SAMPLE INSTRUMENT")) return 1;
    const uint64_t first_sample_cycle = eps16_probe_machine_cycles();
    eps16_probe_machine_end(first);

    if (!eps16_probe_machine_begin(second)) return 1;
    if (eps16_probe_machine_cycles() != second_boot_cycle ||
        !display_starts_with("NO INSTRUMENTS"))
        return 1;
    eps16_probe_machine_end(second);

    if (!eps16_probe_machine_begin(first)) return 1;
    if (eps16_probe_machine_cycles() != first_sample_cycle ||
        !display_starts_with("PICK SAMPLE INSTRUMENT"))
        return 1;
    eps16_probe_machine_end(first);

    ThreadRun first_run = {first, 5000000, 0, {0}, 0};
    ThreadRun second_run = {second, 7000000, 0, {0}, 0};
    pthread_t first_thread;
    pthread_t second_thread;
    if (pthread_create(&first_thread, NULL, run_on_thread, &first_run) ||
        pthread_create(&second_thread, NULL, run_on_thread, &second_run))
        return 1;
    pthread_join(first_thread, NULL);
    pthread_join(second_thread, NULL);

    printf("first boot=%llu final=%llu display=|%s|\n"
           "second boot=%llu final=%llu display=|%s|\n",
           (unsigned long long)first_boot_cycle,
           (unsigned long long)first_run.resulting_cycle, first_run.display,
           (unsigned long long)second_boot_cycle,
           (unsigned long long)second_run.resulting_cycle, second_run.display);
    const int ok = first_run.ok && second_run.ok &&
        first_run.resulting_cycle >= first_sample_cycle + first_run.advance &&
        second_run.resulting_cycle >= second_boot_cycle + second_run.advance &&
        !strncmp(first_run.display, "PICK SAMPLE INSTRUMENT", 22) &&
        !strncmp(second_run.display, "NO INSTRUMENTS", 14);

    eps16_probe_machine_destroy(first);
    eps16_probe_machine_destroy(second);
    return ok ? 0 : 1;
}
