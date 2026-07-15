#ifndef EPS16_KPC_FIRMWARE_H
#define EPS16_KPC_FIRMWARE_H

#include <stddef.h>
#include <stdint.h>

enum {
    KPC_EPROM_SIZE = 32768,
    KPC_PROGRAM_OFFSET = 0x6000,
    KPC_PROGRAM_SIZE = 0x2000
};

typedef struct {
    uint8_t eprom[KPC_EPROM_SIZE];
    uint16_t reset_vector;
    int loaded;
} KpcFirmware;

int kpc_firmware_load(KpcFirmware *firmware, const char *path,
                      char *error, size_t error_size);
uint8_t kpc_firmware_read(const KpcFirmware *firmware, uint16_t address);

#endif
