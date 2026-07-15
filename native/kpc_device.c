#include "kpc_device.h"

#include <string.h>

enum {
    SCI_STATUS = 0x102e,
    SCI_DATA = 0x102f,
    SCI_BAUD = 0x102b,
    SCI_CONTROL_2 = 0x102d,
    SCI_TIE = 0x80,
    SCI_TCIE = 0x40,
    SCI_TDRE = 0x80,
    SCI_TC = 0x40,
    SCI_RDRF = 0x20,
    SCI_RIE = 0x20,
    SCI_VECTOR = 0xffd6,
    SPI_CONTROL = 0x1028,
    SPI_STATUS = 0x1029,
    SPI_DATA = 0x102a,
    SPI_INTERRUPT_ENABLE = 0x80,
    SPI_TRANSFER_COMPLETE = 0x80,
    SPI_VECTOR = 0xffd8,
    CONFIG_REGISTER = 0x103f,
    KPC_CONFIG = 0x0d,
    TIMER_MASK_1 = 0x1022,
    TIMER_FLAG_1 = 0x1023,
    TIMER_COUNTER = 0x100e,
    TIMER_CAPTURE_2 = 0x1012,
    TIMER_CAPTURE_3 = 0x1014,
    TIMER_COMPARE_4 = 0x101c,
    TIMER_COMPARE_5 = 0x101e,
    OC4_MASK = 0x10,
    OC5_MASK = 0x08,
    IC2_MASK = 0x02,
    IC3_MASK = 0x01,
    IC2_VECTOR = 0xffec,
    IC3_VECTOR = 0xffea,
    PORT_A = 0x1000,
    OC4_VECTOR = 0xffe2,
    OC5_VECTOR = 0xffe0
};

static uint16_t device_read16_raw(const KpcDevice *device, uint16_t address) {
    return (uint16_t)(((uint16_t)device->writable[address] << 8) |
                      device->writable[address + 1]);
}

static unsigned int sci_character_cycles(const KpcDevice *device) {
    static const unsigned int prescalers[4] = {1, 3, 4, 13};
    uint8_t baud = device->writable[SCI_BAUD];
    unsigned int prescaler = prescalers[(baud >> 4) & 3];
    unsigned int divider = 1U << (baud & 7);
    return 11U * 16U * prescaler * divider;
}

static uint8_t sci_status_value(const KpcDevice *device) {
    return (uint8_t)((device->sci_tx_pending ? 0 : SCI_TDRE) |
                     (device->sci_tx_cycles_remaining ? 0 : SCI_TC) |
                     (device->rx_count ? SCI_RDRF : 0));
}

static void sci_start_transmit(KpcDevice *device, uint8_t value) {
    if (device->tx_count == KPC_DEVICE_QUEUE_SIZE) return;
    device->sci_recent_tx[device->sci_tx_total & 0xff] = value;
    ++device->sci_tx_total;
    device->tx[device->tx_write] = value;
    device->tx_write = (device->tx_write + 1) % KPC_DEVICE_QUEUE_SIZE;
    ++device->tx_count;
    device->sci_tx_cycles_remaining = sci_character_cycles(device);
    if (value == 0xc9 && device->capture_mode == 2)
        device->capture_period = 0;
}

static uint8_t device_read8(void *context, uint16_t address) {
    KpcDevice *device = context;
    ++device->reads;
    if (address == SCI_STATUS)
        return sci_status_value(device);
    if (address == SCI_DATA) {
        if (!device->rx_count) return 0;
        uint8_t value = device->rx[device->rx_read];
        device->rx_read = (device->rx_read + 1) % KPC_DEVICE_QUEUE_SIZE;
        --device->rx_count;
        return value;
    }
    if (address == SPI_STATUS) {
        device->spi_status_read = 1;
        return device->writable[SPI_STATUS];
    }
    if (address == SPI_DATA) {
        uint8_t value = device->writable[SPI_DATA];
        if (device->spi_status_read)
            device->writable[SPI_STATUS] &=
                (uint8_t)~SPI_TRANSFER_COMPLETE;
        device->spi_status_read = 0;
        return value;
    }
    if (address == TIMER_COUNTER) return (uint8_t)(device->timer_counter >> 8);
    if (address == TIMER_COUNTER + 1) return (uint8_t)device->timer_counter;
    if (address >= 0xe000)
        return kpc_firmware_read(&device->firmware, address);
    return device->writable[address];
}

static void device_write8(void *context, uint16_t address, uint8_t value) {
    KpcDevice *device = context;
    ++device->writes;
    if (address == SCI_DATA) {
        if (!device->sci_tx_cycles_remaining)
            sci_start_transmit(device, value);
        else if (!device->sci_tx_pending) {
            device->sci_tx_pending_value = value;
            device->sci_tx_pending = 1;
        }
        return;
    }
    if (address == SPI_DATA) {
        if (device->spi_tx_count < sizeof(device->spi_tx_trace))
            device->spi_tx_trace[device->spi_tx_count] = value;
        ++device->spi_tx_count;
        device->spi_recent_tx[device->spi_recent_count & 0xff] = value;
        device->spi_transfer_receive_value = device->spi_receive_value;
        if (device->panel_transition_phase == 6) {
            /* Report the release on the matching matrix scan exactly once. */
            if (!device->panel_scan_command_valid) {
                device->panel_scan_command = value;
                device->panel_scan_command_valid = 1;
            }
            device->spi_transfer_receive_value =
                device->panel_scan_command_valid &&
                value == device->panel_scan_command
                    ? device->panel_event_receive_value : 0;
            if (device->panel_scan_command_valid &&
                value == device->panel_scan_command) {
                --device->panel_transfers_remaining;
                if (!device->panel_transfers_remaining) {
                    device->panel_transition_phase = 7;
                }
            }
        } else if (device->panel_transition_phase == 7) {
            device->spi_transfer_receive_value = 0;
            if (value == 0xff) {
                device->panel_transition_phase = 0;
                device->spi_receive_value = 0;
            }
        } else if (device->panel_transition_phase == 1 &&
            device->panel_transfers_remaining) {
            if (!device->panel_scan_command && value != 0xff && value != 0)
                device->panel_scan_command = value;
            if (value == device->panel_scan_command) {
                --device->panel_transfers_remaining;
                if (!device->panel_transfers_remaining) {
                    device->panel_transition_phase = 2;
                    device->panel_transfers_remaining = 1;
                }
            }
        } else if (device->panel_transition_phase == 2) {
            /* The matching matrix scan has already reported the transition.
               Other scan commands and the following FF fetch see the idle
               return line; otherwise one physical edge is queued repeatedly
               by the original KPC firmware. */
            device->spi_transfer_receive_value = 0;
            if (value == 0xff) {
                --device->panel_transfers_remaining;
                if (!device->panel_transfers_remaining) {
                    device->panel_transition_phase = 0;
                    device->spi_receive_value = 0;
                }
            }
        } else if (device->panel_transition_phase == 3 &&
                   device->panel_transfers_remaining) {
            --device->panel_transfers_remaining;
            if (!device->panel_transfers_remaining) {
                device->panel_transition_phase = 4;
                device->spi_receive_value =
                    device->panel_tail_receive_value;
            }
        } else if (device->panel_transition_phase == 4) {
            device->panel_transition_phase = 0;
            device->spi_receive_value = 0;
        } else if (device->panel_transition_phase == 5) {
            if (device->panel_transfers_remaining)
                --device->panel_transfers_remaining;
            if (!device->panel_transfers_remaining) {
                device->panel_transition_phase = 0;
                device->spi_receive_value = 0;
            }
        }
        device->writable[SPI_DATA] = value;
        device->writable[SPI_STATUS] &=
            (uint8_t)~SPI_TRANSFER_COMPLETE;
        device->spi_status_read = 0;
        device->spi_cycles_remaining = 16;
        return;
    }
    if (address == TIMER_FLAG_1) {
        device->writable[TIMER_FLAG_1] &= (uint8_t)~value;
        return;
    }
    if (address < 0xe000) device->writable[address] = value;
}

int kpc_device_load(KpcDevice *device, const char *path,
                    char *error, size_t error_size) {
    memset(device, 0, sizeof(*device));
    if (!kpc_firmware_load(&device->firmware, path, error, error_size))
        return 0;
    kpc_device_reset(device);
    return 1;
}

void kpc_device_reset(KpcDevice *device) {
    memset(device->writable, 0, sizeof(device->writable));
    /* EEPROM-programmed CONFIG value selected by the KPC 2.33 normal reset
       path. Other values deliberately enter its serial bootstrap loader. */
    device->writable[CONFIG_REGISTER] = KPC_CONFIG;
    device->rx_read = device->rx_write = device->rx_count = 0;
    device->tx_read = device->tx_write = device->tx_count = 0;
    device->sci_rx_total = device->sci_tx_total = 0;
    device->sci_tx_cycles_remaining = 0;
    device->sci_tx_pending_value = 0;
    device->sci_tx_pending = 0;
    device->reads = device->writes = 0;
    device->timer_counter = 0;
    device->spi_cycles_remaining = 0;
    device->spi_status_read = 0;
    /* The keypad/display board drives the synchronous return line low while
       no control is active. KPC firmware complements the received byte and
       treats only values with the original high bit set as matrix events. */
    device->spi_receive_value = 0x00;
    device->spi_transfer_receive_value = 0x00;
    device->panel_transition_phase = 0;
    device->panel_transfers_remaining = 1;
    device->panel_scan_command = 0;
    device->panel_scan_command_valid = 0;
    device->panel_tail_receive_value = 0;
    device->panel_event_receive_value = 0;
    device->panel_tail_transfers = 0;
    device->spi_tx_count = 0;
    device->spi_recent_count = 0;
    device->capture_period = 0;
    device->capture_next_cycle = 0;
    device->capture_mode = 0;
    device->capture_count = 0;
    m68hc11_init(&device->cpu, device, device_read8, device_write8);
    m68hc11_reset(&device->cpu);
}

int kpc_device_receive(KpcDevice *device, uint8_t value) {
    if (device->rx_count == KPC_DEVICE_QUEUE_SIZE) return 0;
    device->sci_recent_rx[device->sci_rx_total & 0xff] = value;
    ++device->sci_rx_total;
    device->rx[device->rx_write] = value;
    device->rx_write = (device->rx_write + 1) % KPC_DEVICE_QUEUE_SIZE;
    ++device->rx_count;
    return 1;
}

int kpc_device_transmit(KpcDevice *device, uint8_t *value) {
    if (!device->tx_count) return 0;
    *value = device->tx[device->tx_read];
    device->tx_read = (device->tx_read + 1) % KPC_DEVICE_QUEUE_SIZE;
    --device->tx_count;
    return 1;
}

int kpc_device_panel_transition(KpcDevice *device, uint8_t matrix_code,
                                int pressed) {
    if (matrix_code > 0x3f || device->panel_transition_phase) return 0;
    uint8_t release_value = (uint8_t)(1U - matrix_code);
    device->panel_event_receive_value =
        pressed ? (uint8_t)(release_value ^ 0x40) : release_value;
    device->spi_receive_value = device->panel_event_receive_value;
    device->panel_transition_phase = 1;
    device->panel_transfers_remaining = 3;
    device->panel_scan_command = 0;
    device->panel_scan_command_valid = 0;
    return 1;
}

void kpc_device_panel_transition_complete(KpcDevice *device) {
    device->spi_receive_value = 0;
    device->panel_transition_phase = 0;
    device->panel_transfers_remaining = 0;
    device->panel_scan_command = 0;
    device->panel_scan_command_valid = 0;
}

int kpc_device_input_capture(KpcDevice *device, unsigned int channel,
                             uint16_t timer_value) {
    uint16_t capture_address;
    uint16_t vector;
    uint8_t mask;
    if (channel == 2) {
        capture_address = TIMER_CAPTURE_2;
        vector = IC2_VECTOR;
        mask = IC2_MASK;
    } else if (channel == 3) {
        capture_address = TIMER_CAPTURE_3;
        vector = IC3_VECTOR;
        mask = IC3_MASK;
    } else {
        return 0;
    }
    device->writable[capture_address] = (uint8_t)(timer_value >> 8);
    device->writable[capture_address + 1] = (uint8_t)timer_value;
    device->writable[PORT_A] ^= mask;
    device->writable[TIMER_FLAG_1] |= mask;
    (void)vector;
    return 1;
}

void kpc_device_set_capture_clock(KpcDevice *device, unsigned int mode,
                                  uint64_t period) {
    device->capture_mode = mode;
    device->capture_period = period;
    device->capture_next_cycle = device->cpu.cycles + period;
    device->capture_count = 0;
}

static void timer_set_compare_flags(KpcDevice *device) {
    if (device->timer_counter ==
        device_read16_raw(device, TIMER_COMPARE_4))
        device->writable[TIMER_FLAG_1] |= OC4_MASK;
    if (device->timer_counter ==
        device_read16_raw(device, TIMER_COMPARE_5))
        device->writable[TIMER_FLAG_1] |= OC5_MASK;
}

static void timer_tick(KpcDevice *device, unsigned int cycles) {
    for (unsigned int cycle = 0; cycle < cycles; ++cycle) {
        ++device->timer_counter;
        timer_set_compare_flags(device);
    }
}

static void spi_tick(KpcDevice *device, unsigned int cycles) {
    if (!device->spi_cycles_remaining) return;
    if (cycles < device->spi_cycles_remaining) {
        device->spi_cycles_remaining -= cycles;
        return;
    }
    device->spi_cycles_remaining = 0;
    device->spi_recent_rx[device->spi_recent_count & 0xff] =
        device->spi_transfer_receive_value;
    ++device->spi_recent_count;
    device->writable[SPI_DATA] = device->spi_transfer_receive_value;
    device->writable[SPI_STATUS] |= SPI_TRANSFER_COMPLETE;
}

static void sci_tick(KpcDevice *device, unsigned int cycles) {
    while (device->sci_tx_cycles_remaining && cycles) {
        if (cycles < device->sci_tx_cycles_remaining) {
            device->sci_tx_cycles_remaining -= cycles;
            return;
        }
        cycles -= device->sci_tx_cycles_remaining;
        device->sci_tx_cycles_remaining = 0;
        if (device->sci_tx_pending) {
            uint8_t value = device->sci_tx_pending_value;
            device->sci_tx_pending = 0;
            sci_start_transmit(device, value);
        }
    }
}

static void timer_resolve_interrupt(KpcDevice *device) {
    if (device->cpu.ccr & M68HC11_CCR_I) return;
    uint8_t priority = device->writable[0x103c] & 0x0f;
    uint8_t sci_status = sci_status_value(device);
    uint8_t sci_control = device->writable[SCI_CONTROL_2];
    int sci_pending =
        ((sci_control & SCI_RIE) && (sci_status & SCI_RDRF)) ||
        ((sci_control & SCI_TIE) && (sci_status & SCI_TDRE)) ||
        ((sci_control & SCI_TCIE) && (sci_status & SCI_TC));
    int spi_pending =
        (device->writable[SPI_CONTROL] & SPI_INTERRUPT_ENABLE) &&
        (device->writable[SPI_STATUS] & SPI_TRANSFER_COMPLETE);
    uint8_t pending = (uint8_t)(device->writable[TIMER_MASK_1] &
                                device->writable[TIMER_FLAG_1]);
    unsigned int entry_cycles = 0;
    /* HPRIO PSEL=4 promotes SCI over every other I-bit interrupt.  KPC 2.33
       programs exactly that value; PSEL=3 similarly promotes SPI. */
    if (priority == 4 && sci_pending)
        entry_cycles = m68hc11_interrupt(&device->cpu, SCI_VECTOR);
    else if (priority == 3 && spi_pending)
        entry_cycles = m68hc11_interrupt(&device->cpu, SPI_VECTOR);
    else if (priority == 9 && (pending & IC2_MASK))
        entry_cycles = m68hc11_interrupt(&device->cpu, IC2_VECTOR);
    else if (priority == 10 && (pending & IC3_MASK))
        entry_cycles = m68hc11_interrupt(&device->cpu, IC3_VECTOR);
    else if (priority == 14 && (pending & OC4_MASK))
        entry_cycles = m68hc11_interrupt(&device->cpu, OC4_VECTOR);
    else if (priority == 15 && (pending & OC5_MASK))
        entry_cycles = m68hc11_interrupt(&device->cpu, OC5_VECTOR);
    else if (pending & IC2_MASK)
        entry_cycles = m68hc11_interrupt(&device->cpu, IC2_VECTOR);
    else if (pending & IC3_MASK)
        entry_cycles = m68hc11_interrupt(&device->cpu, IC3_VECTOR);
    else if (pending & OC4_MASK)
        entry_cycles = m68hc11_interrupt(&device->cpu, OC4_VECTOR);
    else if (pending & OC5_MASK)
        entry_cycles = m68hc11_interrupt(&device->cpu, OC5_VECTOR);
    else if (spi_pending)
        entry_cycles = m68hc11_interrupt(&device->cpu, SPI_VECTOR);
    else if (sci_pending)
        entry_cycles = m68hc11_interrupt(&device->cpu, SCI_VECTOR);
    if (entry_cycles) {
        timer_tick(device, entry_cycles);
        spi_tick(device, entry_cycles);
        sci_tick(device, entry_cycles);
    }
}

unsigned int kpc_device_step(KpcDevice *device) {
    unsigned int instruction_cycles = m68hc11_step(&device->cpu);
    timer_tick(device, instruction_cycles);
    spi_tick(device, instruction_cycles);
    sci_tick(device, instruction_cycles);
    if (device->capture_period && device->capture_mode &&
        device->cpu.cycles >= device->capture_next_cycle) {
        unsigned int channel = device->capture_mode == 23
                                   ? ((device->capture_count & 1) ? 3 : 2)
                                   : device->capture_mode;
        kpc_device_input_capture(device, channel, device->timer_counter);
        ++device->capture_count;
        do device->capture_next_cycle += device->capture_period;
        while (device->cpu.cycles >= device->capture_next_cycle);
    }
    timer_resolve_interrupt(device);
    return instruction_cycles;
}

unsigned int kpc_device_run(KpcDevice *device, unsigned int step_limit) {
    unsigned int steps = 0;
    while (steps < step_limit && !device->cpu.illegal) {
        kpc_device_step(device);
        ++steps;
    }
    return steps;
}
