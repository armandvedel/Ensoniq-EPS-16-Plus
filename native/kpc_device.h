#ifndef EPS16_KPC_DEVICE_H
#define EPS16_KPC_DEVICE_H

#include "kpc_firmware.h"
#include "m68hc11_core.h"

#include <stddef.h>
#include <stdint.h>

enum { KPC_DEVICE_QUEUE_SIZE = 4096 };

typedef struct {
    KpcFirmware firmware;
    M68hc11Core cpu;
    uint8_t writable[0xe000];
    uint8_t rx[KPC_DEVICE_QUEUE_SIZE];
    size_t rx_read;
    size_t rx_write;
    size_t rx_count;
    uint8_t tx[KPC_DEVICE_QUEUE_SIZE];
    size_t tx_read;
    size_t tx_write;
    size_t tx_count;
    uint8_t sci_recent_rx[256];
    uint8_t sci_recent_tx[256];
    size_t sci_rx_total;
    size_t sci_tx_total;
    uint64_t reads;
    uint64_t writes;
    unsigned int sci_tx_cycles_remaining;
    uint8_t sci_tx_pending_value;
    int sci_tx_pending;
    uint16_t timer_counter;
    unsigned int spi_cycles_remaining;
    int spi_status_read;
    uint8_t spi_receive_value;
    uint8_t spi_transfer_receive_value;
    unsigned int panel_transition_phase;
    unsigned int panel_transfers_remaining;
    uint8_t panel_scan_command;
    int panel_scan_command_valid;
    uint8_t panel_tail_receive_value;
    uint8_t panel_event_receive_value;
    unsigned int panel_tail_transfers;
    uint8_t spi_tx_trace[256];
    size_t spi_tx_count;
    uint8_t spi_recent_tx[256];
    uint8_t spi_recent_rx[256];
    size_t spi_recent_count;
    uint64_t capture_period;
    uint64_t capture_next_cycle;
    unsigned int capture_mode;
    unsigned int capture_count;
} KpcDevice;

int kpc_device_load(KpcDevice *device, const char *path,
                    char *error, size_t error_size);
void kpc_device_reset(KpcDevice *device);
int kpc_device_receive(KpcDevice *device, uint8_t value);
int kpc_device_transmit(KpcDevice *device, uint8_t *value);
int kpc_device_panel_transition(KpcDevice *device, uint8_t matrix_code,
                                int pressed);
void kpc_device_panel_transition_complete(KpcDevice *device);
int kpc_device_input_capture(KpcDevice *device, unsigned int channel,
                             uint16_t timer_value);
void kpc_device_set_capture_clock(KpcDevice *device, unsigned int mode,
                                  uint64_t period);
unsigned int kpc_device_step(KpcDevice *device);
unsigned int kpc_device_run(KpcDevice *device, unsigned int step_limit);

#endif
