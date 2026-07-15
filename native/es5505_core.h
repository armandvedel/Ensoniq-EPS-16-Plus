#ifndef EPS16_ES5505_CORE_H
#define EPS16_ES5505_CORE_H

#include <stddef.h>
#include <stdint.h>

enum {
    ES5505_VOICES = 32,
    ES5505_STEREO_BUSES = 4,
    ES5505_STOP_MASK = 0x0003,
    ES5505_LOOP_ENABLE = 0x0008,
    ES5505_BIDIRECTIONAL = 0x0010,
    ES5505_IRQ_ENABLE = 0x0020,
    ES5505_REVERSE = 0x0040,
    ES5505_IRQ = 0x0080
};

typedef struct {
    uint16_t control;
    uint32_t frequency;
    uint32_t start;
    uint32_t end;
    uint32_t accumulator;
    uint16_t k1;
    uint16_t k2;
    uint8_t left_volume;
    uint8_t right_volume;
    int32_t pole1;
    int32_t pole2;
    int32_t pole2_previous;
    int32_t pole3;
    int32_t pole3_previous;
    int32_t pole4;
} Es5505Voice;

typedef uint16_t (*Es5505SampleReader)(void *context, unsigned int bank, uint32_t address);
typedef uint16_t (*Es5505PortReader)(void *context);

typedef struct {
    Es5505Voice voices[ES5505_VOICES];
    uint32_t volume_table[256];
    uint8_t page;
    uint8_t active_voice;
    uint8_t irq_vector;
    uint16_t mode;
    Es5505SampleReader sample_reader;
    void *sample_context;
    Es5505PortReader port_reader;
    void *port_context;
} Es5505Core;

void es5505_core_init(Es5505Core *core, Es5505SampleReader reader, void *context);
void es5505_core_set_port_reader(Es5505Core *core, Es5505PortReader reader, void *context);
uint16_t es5505_core_read(Es5505Core *core, unsigned int register_index);
void es5505_core_write(Es5505Core *core, unsigned int register_index, uint16_t value);
void es5505_core_render_buses(Es5505Core *core,
                              int32_t *outputs[ES5505_STEREO_BUSES * 2],
                              size_t frames);
void es5505_core_render(Es5505Core *core, int32_t *left, int32_t *right, size_t frames);
int es5505_core_irq_pending(const Es5505Core *core);

#endif
