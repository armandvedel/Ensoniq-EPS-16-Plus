#include "ProbeMachine.h"

/* Milestone bridge: compile the verified probe unchanged into a private VST
   translation unit, rename its CLI entry point, and expose only a DAW-clocked
   interface below.  This keeps live_host.c (HTTP, AudioQueue and CoreMIDI)
   out of the plug-in while the static probe state is made instance-owned in
   the next extraction step. */
#define main eps16_probe_cli_main
#include "../native/rom_probe.c"
#undef main

#include "m68kcpu.h"

#include <math.h>

static int plugin_initialized;
static uint64_t plugin_executed;
static uint64_t plugin_audio_scheduled_cycle;
static uint64_t plugin_audio_cycle_accumulator;
static unsigned int plugin_timer_irqs;
static int16_t plugin_input_sample;
static int plugin_input_valid;
static float plugin_output_left;
static float plugin_output_right;
enum { PLUGIN_AUDIO_QUEUE_CAPACITY = 1024 };
static Eps16ProbeAudioFrame plugin_audio_queue[PLUGIN_AUDIO_QUEUE_CAPACITY];
static size_t plugin_audio_queue_read;
static size_t plugin_audio_queue_write;
static uint8_t plugin_panel_pair[2];
static size_t plugin_panel_pair_count;

#define PLUGIN_SNAPSHOT_FIELDS(X) \
    X(low_ram) X(sample_ram) X(sample_ram_write_bytes) X(os_ram) \
    X(es5505_writes) X(es5510) X(es5510_gpr_latch) \
    X(es5510_instruction_latch) X(es5510_dil_latch) X(es5510_dol_latch) \
    X(es5510_dadr_latch) X(es5510_ram_read) X(es5510_dram_reads) \
    X(es5510_dram_writes) X(es5510_gpr_writes) X(es5510_instruction_writes) \
    X(es5510_host_serial) X(es5510_host_serial_writes) \
    X(es5510_host_upload_active) X(es5510_host_access_until) X(live_mode) \
    X(deterministic_host_input) X(es5510_input_next_cycle) \
    X(es5510_input_next_time_ns) X(es5510_input_last_poll_time_ns) \
    X(es5510_input_last_poll_cycle) X(es5510_input_polls) \
    X(es5510_input_valid) X(es5510_input_bypass) \
    X(sample_record_input_valid_start) X(sample_record_write_start) \
    X(function_code) X(duart_registers) X(panel_rx) X(panel_rx_read) \
    X(panel_rx_write) X(panel_rx_count) X(panel_rx_consumed) X(panel_wire) \
    X(panel_wire_read) X(panel_wire_write) X(panel_wire_count) \
    X(panel_wire_tail_cycle) X(panel_tx) X(panel_tx_count) X(current_cycle) \
    X(panel_display) X(panel_display_decimal_mask) X(panel_display_dirty) \
    X(panel_display_last_change_cycle) X(panel_cursor) X(panel_cursor_start) \
    X(panel_cursor_end) X(panel_cursor_active) X(panel_cursor_width_pending) \
    X(panel_cursor_width_known) X(panel_noncell_parameter_pending) \
    X(panel_last_tx) X(panel_pick_instrument_seen) X(panel_file_loaded_seen) \
    X(disk_image) X(disk_loaded) X(disk_change_pending) X(fdc_track) \
    X(fdc_physical_track) X(fdc_sector) X(fdc_data_register) \
    X(fdc_last_command) X(fdc_step_direction) X(fdc_remaining) \
    X(fdc_data_reads) X(duart_output) X(duart_tx_a_enabled) \
    X(duart_tx_a_ready) X(duart_tx_a_ready_cycle) X(duart_tx_b_enabled) \
    X(duart_tx_b_ready) X(duart_tx_b_ready_cycle) X(midi_tx) \
    X(midi_tx_count) X(analog_values) X(analog_reads) \
    X(duart_timer_pending) X(duart_timer_running) X(duart_timer_next_cycle) \
    X(fdc_reads) X(dmac_registers) X(dmac_irq_channel) X(dmac_transfers) \
    X(dmac_pcl_level) X(kpc) X(kpc_firmware_execution) \
    X(kpc_firmware_failure_reported) X(kpc_physical_queue) \
    X(kpc_physical_queue_read) X(kpc_physical_queue_write) \
    X(kpc_physical_queue_count) X(kpc_physical_expected) \
    X(kpc_physical_active) X(kpc_physical_saw_code) \
    X(kpc_physical_next_cycle) X(display_trace_bytes) \
    X(display_trace_byte_count) X(illegal_instructions) \
    X(illegal_instruction_count) X(live_press_cycle) \
    X(sampling_enter_pending) X(sampling_enter_release_cycle) \
    X(sampling_recording_active) X(plugin_executed) \
    X(plugin_audio_scheduled_cycle) X(plugin_audio_cycle_accumulator) \
    X(plugin_timer_irqs) X(plugin_input_sample) X(plugin_input_valid) \
    X(plugin_output_left) X(plugin_output_right) X(plugin_panel_pair) \
    X(plugin_panel_pair_count)

#define PLUGIN_SNAPSHOT_V2_FIELDS(X) \
    X(panel_indicator_on) X(panel_indicator_flash) \
    X(panel_indicator_command_pending)

typedef struct {
    uint8_t magic[8];
    uint32_t version;
    uint32_t header_size;
    uint64_t total_size;
    uint64_t checksum;
    uint32_t m68k_context_size;
    uint32_t reserved;
    int64_t fdc_data_offset;
} PluginSnapshotHeader;

typedef struct {
    uint8_t *current;
    uint8_t *end;
    int valid;
} PluginSnapshotWriter;

typedef struct {
    const uint8_t *current;
    const uint8_t *end;
    int valid;
} PluginSnapshotReader;

static void snapshot_write(PluginSnapshotWriter *writer,
                           const void *source, size_t size) {
    if (!writer->valid || size > (size_t)(writer->end - writer->current)) {
        writer->valid = 0;
        return;
    }
    memcpy(writer->current, source, size);
    writer->current += size;
}

static void snapshot_read(PluginSnapshotReader *reader,
                          void *destination, size_t size) {
    if (!reader->valid || size > (size_t)(reader->end - reader->current)) {
        reader->valid = 0;
        return;
    }
    memcpy(destination, reader->current, size);
    reader->current += size;
}

static uint64_t snapshot_checksum(const uint8_t *data, size_t size) {
    uint64_t hash = UINT64_C(1469598103934665603);
    for (size_t index = 0; index < size; ++index) {
        hash ^= data[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static void plugin_error(char *error, size_t error_size, const char *message) {
    if (error && error_size) snprintf(error, error_size, "%s", message);
}

static void plugin_queue_audio(uint64_t cycle, uint32_t divider,
                               float left, float right) {
    const size_t next = (plugin_audio_queue_write + 1) % PLUGIN_AUDIO_QUEUE_CAPACITY;
    if (next == plugin_audio_queue_read)
        plugin_audio_queue_read = (plugin_audio_queue_read + 1) % PLUGIN_AUDIO_QUEUE_CAPACITY;
    plugin_audio_queue[plugin_audio_queue_write] =
        (Eps16ProbeAudioFrame){cycle, divider, left, right};
    plugin_audio_queue_write = next;
}

static void plugin_render_audio(uint64_t elapsed_cycles, uint64_t end_cycle) {
    const uint32_t output_divider = es5505_core_output_divider(&es5505);
    plugin_audio_cycle_accumulator += elapsed_cycles;
    uint64_t frames_due = plugin_audio_cycle_accumulator / output_divider;
    plugin_audio_cycle_accumulator %= output_divider;
    uint64_t frame_cycle = frames_due
        ? end_cycle - plugin_audio_cycle_accumulator -
              (frames_due - 1) * output_divider
        : 0;
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
            plugin_queue_audio(frame_cycle, output_divider,
                               plugin_output_left, plugin_output_right);
            frame_cycle += output_divider;
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
        plugin_render_audio(elapsed, plugin_executed);
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

size_t eps16_probe_machine_drain_audio(Eps16ProbeAudioFrame *frames,
                                       size_t capacity) {
    if (!frames || !capacity) return 0;
    size_t count = 0;
    while (count < capacity && plugin_audio_queue_read != plugin_audio_queue_write) {
        frames[count++] = plugin_audio_queue[plugin_audio_queue_read];
        plugin_audio_queue_read =
            (plugin_audio_queue_read + 1) % PLUGIN_AUDIO_QUEUE_CAPACITY;
    }
    return count;
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

uint32_t eps16_probe_machine_decimal_mask(void) {
    return panel_display_decimal_mask & 0x3fffffU;
}

uint16_t eps16_probe_machine_indicator_on(unsigned int bank) {
    return bank < 3 ? panel_indicator_on[bank] : 0;
}

uint16_t eps16_probe_machine_indicator_flash(unsigned int bank) {
    return bank < 3 ? panel_indicator_flash[bank] : 0;
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

size_t eps16_probe_machine_state_size(void) {
    if (!plugin_initialized) return 0;
#define SNAPSHOT_FIELD_SIZE(name) + sizeof(name)
    return sizeof(PluginSnapshotHeader) + sizeof(Es5505Core) +
           sizeof(KpcDevice) + m68k_context_size()
           PLUGIN_SNAPSHOT_FIELDS(SNAPSHOT_FIELD_SIZE)
           PLUGIN_SNAPSHOT_V2_FIELDS(SNAPSHOT_FIELD_SIZE);
#undef SNAPSHOT_FIELD_SIZE
}

int eps16_probe_machine_save_state(void *data, size_t size) {
    const size_t required = eps16_probe_machine_state_size();
    if (!required || !data || size != required) return 0;
    memset(data, 0, size);
    PluginSnapshotHeader *header = (PluginSnapshotHeader *)data;
    memcpy(header->magic, "EPS16ST\0", 8);
    header->version = 2;
    header->header_size = sizeof(*header);
    header->total_size = size;
    header->m68k_context_size = m68k_context_size();
    header->fdc_data_offset = -1;
    if (fdc_data) {
        const uintptr_t pointer = (uintptr_t)fdc_data;
        const uintptr_t beginning = (uintptr_t)disk_image;
        const uintptr_t end = beginning + sizeof(disk_image);
        if (pointer >= beginning && pointer <= end)
            header->fdc_data_offset = (int64_t)(pointer - beginning);
    }

    PluginSnapshotWriter writer = {
        (uint8_t *)data + sizeof(*header), (uint8_t *)data + size, 1
    };
#define SNAPSHOT_SAVE_FIELD(name) snapshot_write(&writer, &(name), sizeof(name));
    PLUGIN_SNAPSHOT_FIELDS(SNAPSHOT_SAVE_FIELD)
    PLUGIN_SNAPSHOT_V2_FIELDS(SNAPSHOT_SAVE_FIELD)
#undef SNAPSHOT_SAVE_FIELD

    Es5505Core saved_es5505 = es5505;
    saved_es5505.sample_reader = NULL;
    saved_es5505.sample_context = NULL;
    saved_es5505.port_reader = NULL;
    saved_es5505.port_context = NULL;
    snapshot_write(&writer, &saved_es5505, sizeof(saved_es5505));

    KpcDevice saved_kpc_device = kpc_device;
    memset(&saved_kpc_device.firmware, 0, sizeof(saved_kpc_device.firmware));
    saved_kpc_device.cpu.memory_context = NULL;
    saved_kpc_device.cpu.read8 = NULL;
    saved_kpc_device.cpu.write8 = NULL;
    snapshot_write(&writer, &saved_kpc_device, sizeof(saved_kpc_device));

    m68ki_cpu_core saved_cpu;
    m68k_get_context(&saved_cpu);
    saved_cpu.cyc_instruction = NULL;
    saved_cpu.cyc_exception = NULL;
    saved_cpu.int_ack_callback = NULL;
    saved_cpu.bkpt_ack_callback = NULL;
    saved_cpu.reset_instr_callback = NULL;
    saved_cpu.cmpild_instr_callback = NULL;
    saved_cpu.rte_instr_callback = NULL;
    saved_cpu.tas_instr_callback = NULL;
    saved_cpu.illg_instr_callback = NULL;
    saved_cpu.trap_instr_callback = NULL;
    saved_cpu.pc_changed_callback = NULL;
    saved_cpu.set_fc_callback = NULL;
    saved_cpu.instr_hook_callback = NULL;
    snapshot_write(&writer, &saved_cpu, sizeof(saved_cpu));
    if (!writer.valid || writer.current != writer.end) return 0;
    header->checksum = snapshot_checksum((const uint8_t *)data + sizeof(*header),
                                         size - sizeof(*header));
    return 1;
}

int eps16_probe_machine_load_state(const void *data, size_t size,
                                   char *error, size_t error_size) {
    if (!plugin_initialized) {
        plugin_error(error, error_size, "machine must be initialized before restore");
        return 0;
    }
    if (!data || size < sizeof(PluginSnapshotHeader)) {
        plugin_error(error, error_size, "machine snapshot is truncated");
        return 0;
    }
    PluginSnapshotHeader header;
    memcpy(&header, data, sizeof(header));
    const size_t expected_v2 = eps16_probe_machine_state_size();
#define SNAPSHOT_V2_FIELD_SIZE(name) - sizeof(name)
    const size_t expected_v1 = expected_v2
        PLUGIN_SNAPSHOT_V2_FIELDS(SNAPSHOT_V2_FIELD_SIZE);
#undef SNAPSHOT_V2_FIELD_SIZE
    const size_t expected = header.version == 1 ? expected_v1 : expected_v2;
    if (memcmp(header.magic, "EPS16ST\0", 8) ||
        (header.version != 1 && header.version != 2) ||
        header.header_size != sizeof(header) || header.total_size != size ||
        size != expected || header.m68k_context_size != m68k_context_size()) {
        plugin_error(error, error_size, "machine snapshot format is incompatible");
        return 0;
    }
    const uint8_t *payload = (const uint8_t *)data + sizeof(header);
    if (header.checksum != snapshot_checksum(payload, size - sizeof(header))) {
        plugin_error(error, error_size, "machine snapshot checksum failed");
        return 0;
    }
    if (header.fdc_data_offset < -1 ||
        header.fdc_data_offset > (int64_t)sizeof(disk_image)) {
        plugin_error(error, error_size, "machine snapshot has invalid disk position");
        return 0;
    }

    const Es5505SampleReader sample_reader = es5505.sample_reader;
    void *const sample_context = es5505.sample_context;
    const Es5505PortReader port_reader = es5505.port_reader;
    void *const port_context = es5505.port_context;
    const KpcFirmware device_firmware = kpc_device.firmware;
    void *const kpc_memory_context = kpc_device.cpu.memory_context;
    const M68hc11Read8 kpc_read8 = kpc_device.cpu.read8;
    const M68hc11Write8 kpc_write8 = kpc_device.cpu.write8;

    PluginSnapshotReader reader = {payload, (const uint8_t *)data + size, 1};
#define SNAPSHOT_LOAD_FIELD(name) snapshot_read(&reader, &(name), sizeof(name));
    PLUGIN_SNAPSHOT_FIELDS(SNAPSHOT_LOAD_FIELD)
    if (header.version >= 2) {
        PLUGIN_SNAPSHOT_V2_FIELDS(SNAPSHOT_LOAD_FIELD)
    } else {
        memset(panel_indicator_on, 0, sizeof(panel_indicator_on));
        memset(panel_indicator_flash, 0, sizeof(panel_indicator_flash));
        panel_indicator_command_pending = 0;
    }
#undef SNAPSHOT_LOAD_FIELD
    snapshot_read(&reader, &es5505, sizeof(es5505));
    snapshot_read(&reader, &kpc_device, sizeof(kpc_device));
    if (!reader.valid ||
        (size_t)(reader.end - reader.current) != header.m68k_context_size) {
        plugin_error(error, error_size, "machine snapshot payload is invalid");
        return 0;
    }
    m68ki_cpu_core current_cpu;
    m68ki_cpu_core restored_cpu;
    m68k_get_context(&current_cpu);
    snapshot_read(&reader, &restored_cpu, sizeof(restored_cpu));
    restored_cpu.cyc_instruction = current_cpu.cyc_instruction;
    restored_cpu.cyc_exception = current_cpu.cyc_exception;
    restored_cpu.int_ack_callback = current_cpu.int_ack_callback;
    restored_cpu.bkpt_ack_callback = current_cpu.bkpt_ack_callback;
    restored_cpu.reset_instr_callback = current_cpu.reset_instr_callback;
    restored_cpu.cmpild_instr_callback = current_cpu.cmpild_instr_callback;
    restored_cpu.rte_instr_callback = current_cpu.rte_instr_callback;
    restored_cpu.tas_instr_callback = current_cpu.tas_instr_callback;
    restored_cpu.illg_instr_callback = current_cpu.illg_instr_callback;
    restored_cpu.trap_instr_callback = current_cpu.trap_instr_callback;
    restored_cpu.pc_changed_callback = current_cpu.pc_changed_callback;
    restored_cpu.set_fc_callback = current_cpu.set_fc_callback;
    restored_cpu.instr_hook_callback = current_cpu.instr_hook_callback;
    m68k_set_context(&restored_cpu);

    es5505.sample_reader = sample_reader;
    es5505.sample_context = sample_context;
    es5505.port_reader = port_reader;
    es5505.port_context = port_context;
    kpc_device.firmware = device_firmware;
    kpc_device.cpu.memory_context = kpc_memory_context;
    kpc_device.cpu.read8 = kpc_read8;
    kpc_device.cpu.write8 = kpc_write8;
    fdc_data = header.fdc_data_offset >= 0
                   ? disk_image + header.fdc_data_offset : NULL;
    plugin_audio_queue_read = 0;
    plugin_audio_queue_write = 0;
    if (error && error_size) error[0] = '\0';
    return reader.current == reader.end;
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
