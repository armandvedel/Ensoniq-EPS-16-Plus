#include "ProbeMachine.h"

/* Milestone bridge: compile the verified probe unchanged into a private VST
   translation unit, rename its CLI entry point, and expose only a DAW-clocked
   interface below.  This keeps live_host.c (HTTP, AudioQueue and CoreMIDI)
   out of the plug-in while the static probe state is made instance-owned in
   the next extraction step. */
#define main eps16_probe_cli_main
#include "../native/rom_probe.c"
#undef main

#include <math.h>

enum { PLUGIN_AUDIO_RATE = CPU_CLOCK_RATE / (16 * 21) };

static int plugin_initialized;
static uint64_t plugin_executed;
static uint64_t plugin_audio_scheduled_cycle;
static uint64_t plugin_audio_cycle_accumulator;
static unsigned int plugin_timer_irqs;
static int16_t plugin_input_sample;
static int plugin_input_valid;
static float plugin_output_left;
static float plugin_output_right;
static uint8_t plugin_panel_pair[2];
static size_t plugin_panel_pair_count;

static void plugin_error(char *error, size_t error_size, const char *message) {
    if (error && error_size) snprintf(error, error_size, "%s", message);
}

static void plugin_render_audio(uint64_t elapsed_cycles) {
    plugin_audio_cycle_accumulator += elapsed_cycles * PLUGIN_AUDIO_RATE;
    uint64_t frames_due = plugin_audio_cycle_accumulator / CPU_CLOCK_RATE;
    plugin_audio_cycle_accumulator %= CPU_CLOCK_RATE;
    while (frames_due) {
        int32_t buses[ES5505_STEREO_BUSES * 2][64];
        int32_t *bus_outputs[ES5505_STEREO_BUSES * 2];
        size_t chunk = frames_due > 64 ? 64 : (size_t)frames_due;
        for (unsigned int output = 0;
             output < ES5505_STEREO_BUSES * 2; ++output)
            bus_outputs[output] = buses[output];
        es5505_core_render_buses(&es5505, bus_outputs, chunk);
        audio_frames += chunk;
        frames_due -= chunk;
        for (size_t frame = 0; frame < chunk; ++frame) {
            const int16_t esp_inputs[8] = {
                audio_to_pcm16(buses[0][frame]),
                audio_to_pcm16(buses[1][frame]),
                0, 0,
                audio_to_pcm16(buses[2][frame]),
                audio_to_pcm16(buses[3][frame]),
                audio_to_pcm16(buses[4][frame]),
                audio_to_pcm16(buses[5][frame])
            };
            int16_t esp_outputs[2];
            if (es5510_host_upload_active &&
                (uint64_t)current_cycle >= es5510_host_access_until) {
                es5510_host_upload_active = 0;
                es5510_core_set_halted(&es5510, 0);
            }
            es5510_core_process(&es5510, esp_inputs, esp_outputs);
            const int32_t output_left =
                apply_master_volume((int32_t)esp_outputs[0] << 4);
            const int32_t output_right =
                apply_master_volume((int32_t)esp_outputs[1] << 4);
            plugin_output_left = (float)audio_to_pcm16(output_left) / 32768.0f;
            plugin_output_right = (float)audio_to_pcm16(output_right) / 32768.0f;
        }
    }
}

int eps16_probe_machine_initialize(const char *rom_path, const char *kpc_path,
                                   const char *os_disk_path,
                                   char *error, size_t error_size) {
    if (plugin_initialized) return 1;
    if (!rom_path || !*rom_path || !kpc_path || !*kpc_path ||
        !os_disk_path || !*os_disk_path) {
        plugin_error(error, error_size, "ROM, KPC ROM and OS disk are required");
        return 0;
    }
    if (!load_rom(rom_path)) {
        plugin_error(error, error_size, "combined EPS ROM must be exactly 128 KiB");
        return 0;
    }
    if (!load_disk(os_disk_path)) {
        plugin_error(error, error_size, "EPS OS disk could not be decoded");
        return 0;
    }
    kpc_legacy_init(&kpc);
    char kpc_error[256];
    if (!kpc_device_load(&kpc_device, kpc_path, kpc_error, sizeof(kpc_error))) {
        plugin_error(error, error_size, kpc_error);
        return 0;
    }
    kpc_firmware_execution = 1;
    kpc_device_set_capture_clock(&kpc_device, 2, 40000);
    deterministic_host_input = 1;
    live_mode = 0;

    m68k_init();
    es5505_core_init(&es5505, es5505_sample_read, NULL);
    es5510_core_init(&es5510);
    es5505_core_set_port_reader(&es5505, es5505_port_read, NULL);
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    m68k_set_fc_callback(set_function_code);
    m68k_set_int_ack_callback(interrupt_acknowledge);
    m68k_set_illg_instr_callback(illegal_instruction);
    m68k_set_instr_hook_callback(instruction_hook);
    m68k_pulse_reset();
    plugin_initialized = 1;
    if (error && error_size) error[0] = '\0';
    return 1;
}

int eps16_probe_machine_is_initialized(void) {
    return plugin_initialized;
}

void eps16_probe_machine_run_until(uint64_t target_cycle) {
    if (!plugin_initialized) return;
    while (plugin_executed < target_cycle) {
        current_cycle = (long long)plugin_executed;
        duart_service_time(plugin_executed);
        kpc_execution_service(plugin_executed);
        uint64_t remaining = target_cycle - plugin_executed;
        int budget = remaining > 50000 ? 50000 : (int)remaining;
        uint64_t next_cycle = plugin_executed + (uint64_t)budget;
        if (duart_timer_running && duart_timer_next_cycle < next_cycle)
            next_cycle = duart_timer_next_cycle;
        if (panel_wire_count && panel_rx_count < PANEL_RX_SIZE &&
            panel_wire[panel_wire_read].cycle < next_cycle)
            next_cycle = panel_wire[panel_wire_read].cycle;
        if (!duart_tx_a_ready && duart_tx_a_ready_cycle < next_cycle)
            next_cycle = duart_tx_a_ready_cycle;
        if (!duart_tx_b_ready && duart_tx_b_ready_cycle < next_cycle)
            next_cycle = duart_tx_b_ready_cycle;
        budget = next_cycle > plugin_executed
                     ? (int)(next_cycle - plugin_executed) : 1;
        const int slice = m68k_execute(budget);
        if (slice <= 0) break;
        plugin_executed += (uint64_t)slice;
        current_cycle = (long long)plugin_executed;
        duart_service_time(plugin_executed);
        kpc_execution_service(plugin_executed);
        dmac_service();
        if (dmac_irq_channel >= 0) {
            m68k_set_irq(2);
            plugin_executed += (uint64_t)m68k_execute(128);
            current_cycle = (long long)plugin_executed;
            duart_service_time(plugin_executed);
            m68k_set_irq(0);
        }
        if (duart_interrupt_status() & duart_registers[5]) {
            m68k_set_irq(3);
            plugin_executed += (uint64_t)m68k_execute(128);
            current_cycle = (long long)plugin_executed;
            duart_service_time(plugin_executed);
            m68k_set_irq(0);
            ++plugin_timer_irqs;
        }
        const uint64_t elapsed = plugin_executed - plugin_audio_scheduled_cycle;
        plugin_audio_scheduled_cycle = plugin_executed;
        plugin_render_audio(elapsed);
        if (es5505_core_irq_pending(&es5505)) {
            m68k_set_irq(1);
            plugin_executed += (uint64_t)m68k_execute(128);
            current_cycle = (long long)plugin_executed;
            duart_service_time(plugin_executed);
            m68k_set_irq(0);
            ++es5505_irqs;
        }
    }
}

void eps16_probe_machine_midi(uint8_t status, uint8_t data1, uint8_t data2) {
    if (!plugin_initialized) return;
    const unsigned int kind = status & 0xf0;
    if (kind == 0x90 && data2)
        live_note(data1, data2, 1);
    else if (kind == 0x80 || (kind == 0x90 && !data2))
        live_note(data1, data2, 0);
}

void eps16_probe_machine_panel_byte(uint8_t value) {
    if (!plugin_initialized) return;
    plugin_panel_pair[plugin_panel_pair_count++] = value;
    if (plugin_panel_pair_count == 2) {
        kpc_execution_panel_packet(plugin_panel_pair, 2, plugin_executed);
        plugin_panel_pair_count = 0;
    }
}

void eps16_probe_machine_analog(unsigned int channel, uint16_t value) {
    if (!plugin_initialized || channel >= 8) return;
    analog_values[channel] = (uint16_t)((value > 1023 ? 1023 : value) << 6);
}

void eps16_probe_machine_sampling_input(float left, float right) {
    float mono = 0.5f * (left + right);
    if (mono > 1.0f) mono = 1.0f;
    if (mono < -1.0f) mono = -1.0f;
    plugin_input_sample = (int16_t)lrintf(mono * 32767.0f);
    plugin_input_valid = 1;
}

void eps16_probe_machine_stereo_output(float *left, float *right) {
    if (left) *left = plugin_initialized ? plugin_output_left : 0.0f;
    if (right) *right = plugin_initialized ? plugin_output_right : 0.0f;
}

void eps16_probe_machine_display(char display[23]) {
    if (!display) return;
    memcpy(display, panel_display, 22);
    display[22] = '\0';
}

void eps16_probe_machine_cursor(int *start, int *end) {
    if (start) *start = panel_cursor_start;
    if (end) *end = panel_cursor_end;
}

uint64_t eps16_probe_machine_cycles(void) {
    return plugin_executed;
}

size_t eps16_probe_machine_illegal_instructions(void) {
    return illegal_instruction_count;
}

uint64_t eps16_probe_machine_sample_ram_write_bytes(void) {
    return sample_ram_write_bytes;
}

uint64_t eps16_probe_machine_sampling_input_conversions(void) {
    return es5510_input_valid;
}

/* live_host replacements used by the included probe.  No host services,
   sockets, CoreMIDI or AudioQueue enter the plug-in. */
int live_host_start(uint32_t sample_rate) { (void)sample_rate; return 1; }
void live_host_stop(void) {}
void live_host_write(const int16_t *samples, size_t frames) {
    (void)samples; (void)frames;
}
int live_host_poll_line(char *line, size_t size) {
    (void)line; (void)size; return 0;
}
int live_host_poll_midi(LiveMidiEvent *event) { (void)event; return 0; }
int live_host_audio_input_sample(uint32_t target_rate, int16_t *sample) {
    (void)target_rate;
    if (!plugin_input_valid || !sample) return 0;
    *sample = plugin_input_sample;
    return 1;
}
void live_host_audio_input_prepare_recording(void) {}
void live_host_clear_display_hold(void) {}
void live_host_display(const char display[23], uint32_t decimal_mask,
                       int cursor_start, int cursor_end) {
    (void)display; (void)decimal_mask; (void)cursor_start; (void)cursor_end;
}
void live_host_panel_tx(uint8_t value) { (void)value; }
void live_host_adc_state(const uint16_t values[8], const unsigned int reads[8],
                         unsigned int duart_opr, unsigned int es5505_page) {
    (void)values; (void)reads; (void)duart_opr; (void)es5505_page;
}
