#include "es5505_core.h"

#include <assert.h>
#include <stdio.h>

static uint16_t sample_data[] = {0, 1000, 2000, 3000, 4000, 5000};
static unsigned int last_bank;

static uint16_t read_sample(void *context, unsigned int bank, uint32_t address) {
    (void)context;
    last_bank = bank;
    return address < sizeof(sample_data) / sizeof(sample_data[0]) ? sample_data[address] : 0;
}

static uint16_t read_port(void *context) {
    return *(const uint16_t *)context;
}

int main(void) {
    Es5505Core core;
    es5505_core_init(&core, read_sample, NULL);
    uint16_t port_value = 0x7fc0;
    es5505_core_set_port_reader(&core, read_port, &port_value);
    es5505_core_write(&core, 15, 0x40);
    assert(es5505_core_read(&core, 9) == port_value);
    es5505_core_write(&core, 15, 0);
    assert(es5505_core_read(&core, 13) == 31);
    assert(es5505_core_read(&core, 14) == 0x80);
    assert((es5505_core_read(&core, 0) & 3) == 3);
    es5505_core_write(&core, 13, 7);
    assert(es5505_core_output_divider(&core) == 128);
    assert(es5505_core_output_rate(&core, 10000000) == 78125);
    es5505_core_write(&core, 13, 13);
    assert(es5505_core_output_divider(&core) == 224);
    assert(es5505_core_output_rate(&core, 10000000) == 44642);
    es5505_core_write(&core, 13, 20);
    assert(es5505_core_output_divider(&core) == 336);
    assert(es5505_core_output_rate(&core, 10000000) == 29761);
    for (unsigned int value = 1; value < 256; ++value)
        assert(core.volume_table[value] >= core.volume_table[value - 1]);

    es5505_core_write(&core, 15, 0x20);
    es5505_core_write(&core, 1, 0x1234);
    assert((uint16_t)core.voices[0].pole4 == 0x1234);
    es5505_core_write(&core, 15, 0);

    core.active_voice = 0;
    Es5505Voice *voice = &core.voices[0];
    voice->control = 0x0800; /* four low-pass poles, running forward */
    voice->k1 = voice->k2 = 0xffff;
    voice->left_volume = voice->right_volume = 0xff;
    voice->start = 0;
    voice->end = 4U << 11;
    voice->accumulator = 1U << 11;
    voice->frequency = 1U << 11;
    int32_t left[3], right[3];
    es5505_core_render(&core, left, right, 3);
    assert(left[0] != 0 && left[1] != left[0]);
    assert(left[0] == right[0]);
    assert(voice->accumulator == (4U << 11));

    voice->control = 0x0800 | 0x0004; /* running in sample bank 1 */
    voice->accumulator = 1U << 11;
    es5505_core_render(&core, left, right, 1);
    assert(last_bank == 1);

    voice->control = ES5505_LOOP_ENABLE | ES5505_BIDIRECTIONAL;
    voice->start = 0;
    voice->end = 2U << 11;
    voice->accumulator = voice->end;
    voice->frequency = 1U << 11;
    es5505_core_render(&core, left, right, 1);
    assert(voice->control & ES5505_REVERSE);
    assert(voice->accumulator == (1U << 11));

    voice->control = ES5505_STOP_MASK;
    es5505_core_render(&core, left, right, 1);
    assert(left[0] == 0 && right[0] == 0);

    voice->control = ES5505_IRQ_ENABLE;
    voice->start = 0;
    voice->end = 0;
    voice->accumulator = 0;
    voice->frequency = 1U << 11;
    es5505_core_render(&core, left, right, 1);
    assert(es5505_core_irq_pending(&core));
    assert(es5505_core_read(&core, 14) == 0);
    assert(!es5505_core_irq_pending(&core));
    assert(es5505_core_read(&core, 14) == 0x80);

    voice->control = 0x0200; /* ES5505 channel assignment 2 */
    voice->start = 0;
    voice->end = 4U << 11;
    voice->accumulator = 1U << 11;
    voice->frequency = 0;
    int32_t bus_samples[8][1];
    int32_t *bus_outputs[8];
    for (unsigned int output = 0; output < 8; ++output)
        bus_outputs[output] = bus_samples[output];
    es5505_core_render_buses(&core, bus_outputs, 1);
    assert(bus_samples[4][0] != 0 && bus_samples[5][0] != 0);
    for (unsigned int output = 0; output < 8; ++output)
        if (output != 4 && output != 5) assert(bus_samples[output][0] == 0);
    puts("ES5505 core OK");
    return 0;
}
