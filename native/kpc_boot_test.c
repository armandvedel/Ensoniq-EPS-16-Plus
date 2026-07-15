#include "kpc_device.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : getenv("EPS16_KPC_ROM");
    if (!path || !*path) {
        fprintf(stderr, "usage: %s KPC-ROM.BIN [SCI-BYTE ...]\n", argv[0]);
        return 2;
    }

    KpcDevice device;
    char error[256];
    if (!kpc_device_load(&device, path, error, sizeof(error))) {
        fprintf(stderr, "KPC boot test: %s\n", error);
        return 2;
    }
    const char *spi_receive_text = getenv("EPS16_KPC_SPI_RECEIVE");
    if (spi_receive_text && *spi_receive_text) {
        char *end = NULL;
        unsigned long value = strtoul(spi_receive_text, &end, 16);
        if (!end || *end || value > 0xff) {
            fprintf(stderr, "invalid hexadecimal SPI receive byte: %s\n",
                    spi_receive_text);
            return 2;
        }
        device.spi_receive_value = (uint8_t)value;
    }
    for (int argument = 2; argument < argc; ++argument) {
        char *end = NULL;
        unsigned long value = strtoul(argv[argument], &end, 16);
        if (!end || *end || value > 0xff) {
            fprintf(stderr, "invalid hexadecimal SCI byte: %s\n",
                    argv[argument]);
            return 2;
        }
        if (!kpc_device_receive(&device, (uint8_t)value)) {
            fprintf(stderr, "too many SCI bytes\n");
            return 2;
        }
    }

    printf("KPC reset: vector=%04x pc=%04x\n",
           device.firmware.reset_vector, device.cpu.pc);
    const char *step_limit_text = getenv("EPS16_KPC_STEP_LIMIT");
    const unsigned int step_limit = step_limit_text && *step_limit_text
                                        ? (unsigned int)strtoul(step_limit_text,
                                                                NULL, 0)
                                        : 1000000;
    const char *capture_text = getenv("EPS16_KPC_CAPTURE");
    const char *capture_period_text = getenv("EPS16_KPC_CAPTURE_PERIOD");
    unsigned int capture_channel = capture_text && *capture_text
                                       ? (unsigned int)strtoul(capture_text, NULL, 0)
                                       : 0;
    uint64_t capture_period = capture_period_text && *capture_period_text
                                  ? strtoull(capture_period_text, NULL, 0)
                                  : 0;
    uint64_t next_capture_cycle = 0;
    unsigned int capture_count = 0;
    size_t pulse_start_spi_count = 0;
    size_t pulse_end_spi_count = 0;
    const char *spi_pulse_text = getenv("EPS16_KPC_SPI_PULSE");
    unsigned int spi_pulse_value = spi_pulse_text && *spi_pulse_text
                                       ? (unsigned int)strtoul(spi_pulse_text,
                                                               NULL, 16)
                                       : 0;
    int spi_pulse_enabled = spi_pulse_text && *spi_pulse_text &&
                            spi_pulse_value <= 0xff;
    const char *panel_event_text = getenv("EPS16_KPC_PANEL_EVENT");
    unsigned int panel_event_code = panel_event_text && *panel_event_text
                                        ? (unsigned int)strtoul(panel_event_text,
                                                                NULL, 16)
                                        : 0;
    int panel_event_pressed = panel_event_text &&
                              (strchr(panel_event_text, 'p') ||
                               strchr(panel_event_text, 'x'));
    int panel_event_click = panel_event_text &&
                            strchr(panel_event_text, 'x');
    const char *panel_transfers_text = getenv("EPS16_KPC_PANEL_TRANSFERS");
    const char *panel_tail_text = getenv("EPS16_KPC_PANEL_TAIL");
    if (panel_tail_text && *panel_tail_text)
        device.panel_tail_receive_value =
            (uint8_t)strtoul(panel_tail_text, NULL, 16);
    const char *spi_pulse_steps_text = getenv("EPS16_KPC_SPI_PULSE_STEPS");
    unsigned int spi_pulse_steps = spi_pulse_steps_text &&
                                           *spi_pulse_steps_text
                                       ? (unsigned int)strtoul(
                                             spi_pulse_steps_text, NULL, 0)
                                       : 20000;
    uint32_t pc_hits[65536];
    memset(pc_hits, 0, sizeof(pc_hits));
    unsigned int steps;
    for (steps = 0; steps < step_limit && !device.cpu.illegal; ++steps) {
        if (steps == 200000 && panel_event_text && *panel_event_text &&
            !kpc_device_panel_transition(&device,
                                         (uint8_t)panel_event_code,
                                         panel_event_pressed)) {
            fprintf(stderr, "KPC panel transition rejected: %s\n",
                    panel_event_text);
            return 2;
        }
        if (steps == 200000 && panel_event_text && *panel_event_text)
            device.spi_tx_count = 0;
        if (steps == 300000 && panel_event_click &&
            !kpc_device_panel_transition(&device,
                                         (uint8_t)panel_event_code, 0)) {
            fprintf(stderr, "KPC panel release rejected: %s\n",
                    panel_event_text);
            return 2;
        }
        if (steps == 200000 && panel_event_text && *panel_event_text &&
            panel_transfers_text && *panel_transfers_text)
            device.panel_transfers_remaining =
                (unsigned int)strtoul(panel_transfers_text, NULL, 0);
        if (spi_pulse_enabled) {
            if (steps == 200000)
                device.spi_receive_value = (uint8_t)spi_pulse_value,
                device.spi_tx_count = 0,
                pulse_start_spi_count = 0;
            else if (steps == 200000 + spi_pulse_steps) {
                device.spi_receive_value = 0;
                pulse_end_spi_count = device.spi_tx_count;
            }
        }
        ++pc_hits[device.cpu.pc];
        kpc_device_step(&device);
        int first_capture = !capture_count && capture_channel &&
                            device.cpu.pc == 0xe20f;
        int periodic_capture = capture_count && capture_period &&
                               device.cpu.cycles >= next_capture_cycle;
        if (first_capture || periodic_capture) {
            unsigned int active_channel = capture_channel == 23
                                              ? (capture_count & 1 ? 3 : 2)
                                              : capture_channel;
            if (!kpc_device_input_capture(&device, active_channel,
                                          device.timer_counter)) {
                fprintf(stderr, "invalid capture channel: %u\n", capture_channel);
                return 2;
            }
            ++capture_count;
            next_capture_cycle = device.cpu.cycles + capture_period;
        }
    }

    if (device.cpu.illegal) {
        uint16_t opcode_address = (uint16_t)(device.cpu.pc - 1);
        fprintf(stderr,
                "KPC stopped: unsupported opcode %02x at %04x after %u steps "
                "(%llu cycles, A=%02x B=%02x X=%04x Y=%04x SP=%04x CCR=%02x)\n",
                device.cpu.last_opcode, opcode_address, steps,
                (unsigned long long)device.cpu.cycles,
                device.cpu.a, device.cpu.b, device.cpu.x, device.cpu.y,
                device.cpu.sp, device.cpu.ccr);
        return 1;
    }

    printf("KPC ran %u steps (%llu cycles, reads=%llu writes=%llu)\n",
           steps, (unsigned long long)device.cpu.cycles,
           (unsigned long long)device.reads,
           (unsigned long long)device.writes);
    printf("KPC SCI transmit:");
    uint8_t value;
    int transmitted = 0;
    size_t transmit_count = 0;
    while (kpc_device_transmit(&device, &value)) {
        if (transmit_count < 64) printf(" %02x", value);
        ++transmit_count;
        transmitted = 1;
    }
    if (!transmitted) printf(" (none)");
    if (transmit_count > 64) printf(" ...");
    printf(" (count=%zu)", transmit_count);
    putchar('\n');
    printf("KPC SPI transmit:");
    size_t spi_trace_count = device.spi_tx_count;
    if (spi_trace_count > sizeof(device.spi_tx_trace))
        spi_trace_count = sizeof(device.spi_tx_trace);
    for (size_t index = 0; index < spi_trace_count && index < 64; ++index)
        printf(" %02x", device.spi_tx_trace[index]);
    if (!spi_trace_count) printf(" (none)");
    if (device.spi_tx_count > 64) printf(" ...");
    printf(" (count=%zu)\n", device.spi_tx_count);
    if (spi_pulse_enabled)
        printf("KPC SPI pulse transfers: %zu\n",
               pulse_end_spi_count - pulse_start_spi_count);
    if (capture_channel)
        printf("KPC input capture: channel=%u count=%u period=%llu\n",
               capture_channel, capture_count,
               (unsigned long long)capture_period);
    printf("KPC state: e4=%02x e5=%02x porta=%02x tctl2=%02x "
           "tmsk1=%02x tflg1=%02x timer=%04x spi_rx=%02x panel_phase=%u\n",
           device.writable[0x00e4], device.writable[0x00e5],
           device.writable[0x1000], device.writable[0x1021],
           device.writable[0x1022], device.writable[0x1023],
           device.timer_counter, device.spi_receive_value,
           device.panel_transition_phase);

    printf("KPC hottest PCs:");
    for (unsigned int rank = 0; rank < 8; ++rank) {
        uint32_t best_hits = 0;
        uint16_t best_pc = 0;
        for (unsigned int address = 0; address < 65536; ++address) {
            if (pc_hits[address] > best_hits) {
                best_hits = pc_hits[address];
                best_pc = (uint16_t)address;
            }
        }
        if (!best_hits) break;
        printf(" %04x=%u", best_pc, best_hits);
        pc_hits[best_pc] = 0;
    }
    putchar('\n');
    return 0;
}
