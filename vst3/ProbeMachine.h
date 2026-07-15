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
void eps16_probe_machine_midi(uint8_t status, uint8_t data1, uint8_t data2);
void eps16_probe_machine_panel_byte(uint8_t value);
void eps16_probe_machine_analog(unsigned int channel, uint16_t value);
void eps16_probe_machine_sampling_input(float left, float right);
void eps16_probe_machine_stereo_output(float *left, float *right);
void eps16_probe_machine_display(char display[23]);
uint64_t eps16_probe_machine_cycles(void);
size_t eps16_probe_machine_illegal_instructions(void);
uint64_t eps16_probe_machine_sample_ram_write_bytes(void);
uint64_t eps16_probe_machine_sampling_input_conversions(void);

#ifdef __cplusplus
}
#endif

#endif
