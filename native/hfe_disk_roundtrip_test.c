#include "hfe_disk.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

enum {
    HFE_HEADER_SIZE = 1024,
    HFE_TRACK_LENGTH = 49 * 512,
    HFE_SIDE_LENGTH = 49 * 256
};

static unsigned int le16(const uint8_t *value) {
    return value[0] | ((unsigned int)value[1] << 8);
}

static uint8_t reverse_bits(uint8_t value) {
    value = (uint8_t)(((value & 0x55) << 1) | ((value >> 1) & 0x55));
    value = (uint8_t)(((value & 0x33) << 2) | ((value >> 2) & 0x33));
    return (uint8_t)((value << 4) | (value >> 4));
}

static uint8_t decode_word(const uint8_t *encoded) {
    const unsigned int word = ((unsigned int)encoded[0] << 8) | encoded[1];
    uint8_t value = 0;
    for (int bit = 14; bit >= 0; bit -= 2)
        value = (uint8_t)((value << 1) | ((word >> bit) & 1));
    return value;
}

static int verify_hardware_track(FILE *input, unsigned int track) {
    uint8_t interleaved[HFE_TRACK_LENGTH];
    uint8_t side_stream[HFE_SIDE_LENGTH];
    if (fseek(input, HFE_HEADER_SIZE + (long)track * HFE_TRACK_LENGTH,
              SEEK_SET) ||
        fread(interleaved, 1, sizeof(interleaved), input) !=
            sizeof(interleaved))
        return 0;

    for (unsigned int side = 0; side < 2; ++side) {
        size_t output = 0;
        for (size_t block = 0; block < 49; ++block)
            for (size_t index = 0; index < 256; ++index)
                side_stream[output++] = reverse_bits(
                    interleaved[block * 512 + side * 256 + index]);

        unsigned int sectors[10];
        unsigned int count = 0;
        for (size_t position = 0;
             position + 16 <= sizeof(side_stream) && count < 10;
             ++position) {
            if (side_stream[position] != 0x44 ||
                side_stream[position + 1] != 0x89 ||
                side_stream[position + 2] != 0x44 ||
                side_stream[position + 3] != 0x89 ||
                side_stream[position + 4] != 0x44 ||
                side_stream[position + 5] != 0x89 ||
                decode_word(side_stream + position + 6) != 0xfe)
                continue;
            sectors[count++] = decode_word(side_stream + position + 12);
            position += 15;
        }
        if (count != 10) return 0;
        const unsigned int first = (track * 6U + side * 8U) % 10U;
        for (unsigned int index = 0; index < 10; ++index)
            if (sectors[index] != (first + index) % 10U) return 0;
    }
    return 1;
}

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
    FILE *encoded = fopen(path, "rb");
    uint8_t header[HFE_HEADER_SIZE];
    if (!encoded || fread(header, 1, sizeof(header), encoded) != sizeof(header) ||
        le16(header + 512 + 2) != HFE_TRACK_LENGTH ||
        le16(header + 512 + 4) != 51 ||
        le16(header + 512 + 6) != HFE_TRACK_LENGTH ||
        !verify_hardware_track(encoded, 0) ||
        !verify_hardware_track(encoded, 1)) {
        if (encoded) fclose(encoded);
        remove(path);
        fputs("generated HFE hardware track layout mismatch\n", stderr);
        return 1;
    }
    fclose(encoded);
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
