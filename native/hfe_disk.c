#include "hfe_disk.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    EPS_TRACKS = 80,
    EPS_SIDES = 2,
    EPS_SECTORS = 10,
    EPS_SECTOR_SIZE = 512
};

static void fail(char *error, size_t size, const char *format, ...) {
    if (!error || !size) return;
    va_list args;
    va_start(args, format);
    vsnprintf(error, size, format, args);
    va_end(args);
}

static uint16_t le16(const uint8_t *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint16_t be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint16_t crc16(const uint8_t *data, size_t size, uint16_t crc) {
    while (size--) {
        crc ^= (uint16_t)*data++ << 8;
        for (unsigned int bit = 0; bit < 8; ++bit)
            crc = crc & 0x8000 ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

static uint8_t reverse_bits(uint8_t value) {
    value = (uint8_t)(((value & 0x55) << 1) | ((value >> 1) & 0x55));
    value = (uint8_t)(((value & 0x33) << 2) | ((value >> 2) & 0x33));
    return (uint8_t)((value << 4) | (value >> 4));
}

static uint8_t decode_word(const uint8_t *encoded) {
    uint16_t word = be16(encoded);
    uint8_t value = 0;
    for (int bit = 14; bit >= 0; bit -= 2)
        value = (uint8_t)((value << 1) | ((word >> bit) & 1));
    return value;
}

static int decode_bytes(const uint8_t *stream, size_t stream_size, size_t offset,
                        uint8_t *output, size_t count) {
    if (offset > stream_size || count > (stream_size - offset) / 2) return 0;
    for (size_t index = 0; index < count; ++index)
        output[index] = decode_word(stream + offset + index * 2);
    return 1;
}

static int decode_hfe(const uint8_t *image, size_t image_size, uint8_t *logical,
                      size_t logical_size, char *error, size_t error_size) {
    if (logical_size != EPS16_LOGICAL_DISK_SIZE) {
        fail(error, error_size, "logical buffer must be %u bytes", EPS16_LOGICAL_DISK_SIZE);
        return 0;
    }
    if (image_size < 1024 || memcmp(image, "HXCPICFE", 8)) {
        fail(error, error_size, "invalid HFE v1 signature or truncated header");
        return 0;
    }
    unsigned int tracks = image[9], sides = image[10], encoding = image[11];
    unsigned int bitrate = le16(image + 12), rpm = le16(image + 14);
    size_t table = (size_t)le16(image + 18) * 512;
    if (image[8] != 0 || tracks != EPS_TRACKS || sides != EPS_SIDES ||
        encoding != 0 || bitrate != 250 || (rpm != 0 && rpm != 300)) {
        fail(error, error_size,
             "unsupported EPS HFE geometry: rev=%u tracks=%u sides=%u encoding=%u bitrate=%u rpm=%u",
             image[8], tracks, sides, encoding, bitrate, rpm);
        return 0;
    }
    if (table < 512 || table > image_size || tracks * 4 > image_size - table) {
        fail(error, error_size, "invalid HFE track lookup table");
        return 0;
    }

    memset(logical, 0, logical_size);
    unsigned int total = 0;
    for (unsigned int track_index = 0; track_index < tracks; ++track_index) {
        const uint8_t *entry = image + table + track_index * 4;
        size_t track_start = (size_t)le16(entry) * 512;
        size_t track_size = le16(entry + 2);
        if (!track_size || track_start > image_size || track_size > image_size - track_start) {
            fail(error, error_size, "track %u is empty or outside the HFE file", track_index);
            return 0;
        }
        size_t side_capacity = ((track_size + 511) / 512) * 256;
        uint8_t *stream = malloc(side_capacity);
        if (!stream) {
            fail(error, error_size, "out of memory decoding HFE track %u", track_index);
            return 0;
        }
        for (unsigned int side = 0; side < sides; ++side) {
            size_t stream_size = 0;
            for (size_t block = 0; block < track_size; block += 512) {
                size_t start = block + side * 256;
                if (start >= track_size) continue;
                size_t count = track_size - start;
                if (count > 256) count = 256;
                for (size_t index = 0; index < count; ++index)
                    stream[stream_size++] = reverse_bits(image[track_start + start + index]);
            }

            unsigned int seen = 0;
            int pending_sector = -1;
            for (size_t position = 0; position + 6 <= stream_size; ++position) {
                static const uint8_t sync[6] = {0x44, 0x89, 0x44, 0x89, 0x44, 0x89};
                if (memcmp(stream + position, sync, sizeof(sync))) continue;
                size_t field = position + sizeof(sync);
                uint8_t mark;
                if (!decode_bytes(stream, stream_size, field, &mark, 1)) break;
                if (mark == 0xfe) {
                    uint8_t id[7];
                    if (!decode_bytes(stream, stream_size, field, id, sizeof(id))) {
                        free(stream);
                        fail(error, error_size, "truncated ID field at track %u side %u", track_index, side);
                        return 0;
                    }
                    uint16_t actual = crc16((const uint8_t *)"\xa1\xa1\xa1", 3, 0xffff);
                    actual = crc16(id, 5, actual);
                    if (actual != be16(id + 5) || id[1] != track_index || id[2] != side ||
                        id[3] >= EPS_SECTORS || id[4] != 2) {
                        free(stream);
                        fail(error, error_size,
                             "invalid ID/CRC at track %u side %u: C=%u H=%u R=%u N=%u",
                             track_index, side, id[1], id[2], id[3], id[4]);
                        return 0;
                    }
                    pending_sector = id[3];
                } else if ((mark == 0xf8 || mark == 0xfb) && pending_sector >= 0) {
                    uint8_t data[EPS_SECTOR_SIZE + 3];
                    if (!decode_bytes(stream, stream_size, field, data, sizeof(data))) {
                        free(stream);
                        fail(error, error_size, "truncated data field at track %u side %u sector %d",
                             track_index, side, pending_sector);
                        return 0;
                    }
                    uint16_t actual = crc16((const uint8_t *)"\xa1\xa1\xa1", 3, 0xffff);
                    actual = crc16(data, EPS_SECTOR_SIZE + 1, actual);
                    if (actual != be16(data + EPS_SECTOR_SIZE + 1) ||
                        (seen & (1u << pending_sector))) {
                        free(stream);
                        fail(error, error_size, "bad data CRC or duplicate at track %u side %u sector %d",
                             track_index, side, pending_sector);
                        return 0;
                    }
                    size_t block = ((track_index * EPS_SIDES + side) * EPS_SECTORS) +
                                   (unsigned int)pending_sector;
                    memcpy(logical + block * EPS_SECTOR_SIZE, data + 1, EPS_SECTOR_SIZE);
                    seen |= 1u << pending_sector;
                    ++total;
                    pending_sector = -1;
                }
            }
            if (seen != 0x03ff) {
                free(stream);
                fail(error, error_size, "track %u side %u has %u of 10 sectors",
                     track_index, side, total);
                return 0;
            }
        }
        free(stream);
    }
    if (total != EPS_TRACKS * EPS_SIDES * EPS_SECTORS) {
        fail(error, error_size, "decoded %u sectors, expected 1600", total);
        return 0;
    }
    return 1;
}

int eps16_disk_load(const char *path, uint8_t *logical, size_t logical_size,
                    Eps16DiskFormat *format, char *error, size_t error_size) {
    FILE *input = fopen(path, "rb");
    if (!input) {
        fail(error, error_size, "cannot open disk image: %s", path);
        return 0;
    }
    if (fseek(input, 0, SEEK_END) || ftell(input) < 0) {
        fclose(input);
        fail(error, error_size, "cannot determine disk image size: %s", path);
        return 0;
    }
    long length = ftell(input);
    rewind(input);
    uint8_t *image = malloc(length ? (size_t)length : 1);
    if (!image || fread(image, 1, (size_t)length, input) != (size_t)length) {
        free(image);
        fclose(input);
        fail(error, error_size, "cannot read complete disk image: %s", path);
        return 0;
    }
    fclose(input);

    int ok;
    if (length >= 8 && !memcmp(image, "HXCPICFE", 8)) {
        uint8_t *decoded = malloc(logical_size);
        if (!decoded) {
            fail(error, error_size, "out of memory allocating validated HFE disk");
            ok = 0;
        } else {
            ok = decode_hfe(image, (size_t)length, decoded, logical_size, error, error_size);
            if (ok) {
                memcpy(logical, decoded, logical_size);
                if (format) *format = EPS16_DISK_HFE;
            }
            free(decoded);
        }
    } else if ((size_t)length == logical_size) {
        memcpy(logical, image, logical_size);
        ok = 1;
        if (format) *format = EPS16_DISK_IMG;
    } else {
        fail(error, error_size, "expected an HFE v1 file or exactly %zu IMG bytes, got %ld",
             logical_size, length);
        ok = 0;
    }
    free(image);
    return ok;
}
