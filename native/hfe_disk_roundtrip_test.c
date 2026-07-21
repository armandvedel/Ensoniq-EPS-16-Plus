#include "hfe_disk.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    uint8_t source[EPS16_LOGICAL_DISK_SIZE];
    uint8_t decoded[EPS16_LOGICAL_DISK_SIZE];
    if (!eps16_disk_create_blank(decoded, sizeof(decoded)) ||
        memcmp(decoded + 512 + 38, "ID", 2) ||
        memcmp(decoded + 2 * 512 + 28, "OS", 2) ||
        memcmp(decoded + 5 * 512 - 2, "DR", 2) ||
        memcmp(decoded + 6 * 512 - 2, "FB", 2) ||
        decoded[2 * 512 + 2] != 0x06 || decoded[2 * 512 + 3] != 0x31) {
        fputs("blank EPS disk structure mismatch\n", stderr);
        return 1;
    }
    for (size_t index = 0; index < sizeof(source); ++index)
        source[index] = (uint8_t)((index * 37U + index / 512U) & 0xffU);

    char path[128];
    snprintf(path, sizeof(path), "/tmp/eps16-hfe-roundtrip-%ld.hfe",
             (long)getpid());
    char error[256] = {0};
    if (!eps16_disk_save(path, source, sizeof(source), EPS16_DISK_HFE,
                         error, sizeof(error))) {
        fprintf(stderr, "HFE save failed: %s\n", error);
        return 1;
    }
    Eps16DiskFormat format;
    const int loaded = eps16_disk_load(path, decoded, sizeof(decoded), &format,
                                       error, sizeof(error));
    remove(path);
    if (!loaded || format != EPS16_DISK_HFE) {
        fprintf(stderr, "generated HFE load failed: %s\n", error);
        return 1;
    }
    if (memcmp(source, decoded, sizeof(source))) {
        fputs("IMG -> HFE -> IMG roundtrip mismatch\n", stderr);
        return 1;
    }
    puts("IMG -> HFE -> IMG roundtrip: 1600 sectors, 819200 bytes");
    return 0;
}
