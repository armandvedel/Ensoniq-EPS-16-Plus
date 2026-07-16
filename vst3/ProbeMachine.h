#ifndef EPS16_VST3_PROBE_MACHINE_H
#define EPS16_VST3_PROBE_MACHINE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int eps16_probe_machine_initialize(const char *rom_path, const char *kpc_path,
                                   const char *os_disk_path,
                                   char *error, size_t error_size);
int eps16_probe_machine_is_initialized(void);
void eps16_probe_machine_run_until(uint64_t cpu_cycle);
typedef struct {
    uint64_t cpu_cycle;
    uint32_t clock_divider;
    float left;
    float right;
} Eps16ProbeAudioFrame;
size_t eps16_probe_machine_drain_audio(Eps16ProbeAudioFrame *frames,
                                       size_t capacity);
void eps16_probe_machine_midi(uint8_t status, uint8_t data1, uint8_t data2);
void eps16_probe_machine_panel_byte(uint8_t value);
void eps16_probe_machine_analog(unsigned int channel, uint16_t value);
void eps16_probe_machine_sampling_input(float left, float right);
void eps16_probe_machine_stereo_output(float *left, float *right);
void eps16_probe_machine_display(char display[23]);
uint32_t eps16_probe_machine_decimal_mask(void);
uint16_t eps16_probe_machine_indicator_on(unsigned int bank);
uint16_t eps16_probe_machine_indicator_flash(unsigned int bank);
void eps16_probe_machine_cursor(int *start, int *end);
uint64_t eps16_probe_machine_cycles(void);
size_t eps16_probe_machine_illegal_instructions(void);
uint64_t eps16_probe_machine_sample_ram_write_bytes(void);
uint64_t eps16_probe_machine_sampling_input_conversions(void);
uint64_t eps16_probe_machine_midi_input_bytes(void);
size_t eps16_probe_machine_state_size(void);
int eps16_probe_machine_save_state(void *data, size_t size);
int eps16_probe_machine_load_state(const void *data, size_t size,
                                   char *error, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
