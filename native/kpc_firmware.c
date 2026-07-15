#include "kpc_firmware.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int fail(char *error, size_t error_size, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(error, error_size, format, arguments);
    va_end(arguments);
    return 0;
}

int kpc_firmware_load(KpcFirmware *firmware, const char *path,
                      char *error, size_t error_size) {
    memset(firmware, 0, sizeof(*firmware));
    FILE *input = fopen(path, "rb");
    if (!input) return fail(error, error_size, "cannot open KPC ROM: %s", path);

    size_t count = fread(firmware->eprom, 1, sizeof(firmware->eprom), input);
    int extra = fgetc(input);
    int read_error = ferror(input);
    fclose(input);
    if (read_error)
        return fail(error, error_size, "cannot read KPC ROM: %s", path);
    if (count != KPC_EPROM_SIZE || extra != EOF)
        return fail(error, error_size,
                    "KPC ROM must be exactly %u bytes (got %zu%s)",
                    KPC_EPROM_SIZE, count, extra == EOF ? "" : " or more");

    for (size_t index = 0; index < KPC_PROGRAM_OFFSET; ++index) {
        if (firmware->eprom[index] != 0xff)
            return fail(error, error_size,
                        "KPC ROM has data outside the 0xe000-0xffff window");
    }
    firmware->reset_vector =
        ((uint16_t)firmware->eprom[KPC_EPROM_SIZE - 2] << 8) |
        firmware->eprom[KPC_EPROM_SIZE - 1];
    if (firmware->reset_vector < 0xe000)
        return fail(error, error_size, "invalid KPC reset vector: %04x",
                    firmware->reset_vector);
    firmware->loaded = 1;
    if (error_size) error[0] = '\0';
    return 1;
}

uint8_t kpc_firmware_read(const KpcFirmware *firmware, uint16_t address) {
    if (!firmware->loaded || address < 0xe000) return 0xff;
    return firmware->eprom[KPC_PROGRAM_OFFSET + (address - 0xe000)];
}
