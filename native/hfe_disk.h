#ifndef EPS16_HFE_DISK_H
#define EPS16_HFE_DISK_H

#include <stddef.h>
#include <stdint.h>

enum { EPS16_LOGICAL_DISK_SIZE = 80 * 2 * 10 * 512 };

typedef enum {
    EPS16_DISK_IMG,
    EPS16_DISK_HFE
} Eps16DiskFormat;

int eps16_disk_load(const char *path, uint8_t *logical, size_t logical_size,
                    Eps16DiskFormat *format, char *error, size_t error_size);

#endif
