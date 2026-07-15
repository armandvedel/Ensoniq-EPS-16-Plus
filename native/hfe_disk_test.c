#include "hfe_disk.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s KNOWN_GOOD.HFE REFERENCE.IMG\n", argv[0]);
        return 2;
    }
    uint8_t hfe[EPS16_LOGICAL_DISK_SIZE], img[EPS16_LOGICAL_DISK_SIZE];
    Eps16DiskFormat hfe_format, img_format;
    char error[256];
    if (!eps16_disk_load(argv[1], hfe, sizeof(hfe), &hfe_format, error, sizeof(error)) ||
        hfe_format != EPS16_DISK_HFE) {
        fprintf(stderr, "HFE decode failed: %s\n", error);
        return 1;
    }
    if (!eps16_disk_load(argv[2], img, sizeof(img), &img_format, error, sizeof(error)) ||
        img_format != EPS16_DISK_IMG) {
        fprintf(stderr, "IMG load failed: %s\n", error);
        return 1;
    }
    if (memcmp(hfe, img, sizeof(hfe))) {
        for (size_t index = 0; index < sizeof(hfe); ++index) {
            if (hfe[index] != img[index]) {
                fprintf(stderr, "first mismatch at byte %zu: HFE=%02x IMG=%02x\n",
                        index, hfe[index], img[index]);
                break;
            }
        }
        return 1;
    }
    puts("native HFE decode matches IMG: 1600 sectors, 819200 bytes");
    return 0;
}
