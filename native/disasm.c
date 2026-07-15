#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m68k.h"

enum { ADDRESS_SPACE = 1 << 24 };
static uint8_t *memory;

unsigned int m68k_read_memory_8(unsigned int address) {
    return memory[address & 0x00ffffffu];
}

unsigned int m68k_read_memory_16(unsigned int address) {
    return (m68k_read_memory_8(address) << 8) | m68k_read_memory_8(address + 1);
}

unsigned int m68k_read_memory_32(unsigned int address) {
    return (m68k_read_memory_16(address) << 16) | m68k_read_memory_16(address + 2);
}

unsigned int m68k_read_disassembler_16(unsigned int address) {
    return m68k_read_memory_16(address);
}

unsigned int m68k_read_disassembler_32(unsigned int address) {
    return m68k_read_memory_32(address);
}

void m68k_write_memory_8(unsigned int address, unsigned int value) {
    memory[address & 0x00ffffffu] = (uint8_t)value;
}

void m68k_write_memory_16(unsigned int address, unsigned int value) {
    m68k_write_memory_8(address, value >> 8);
    m68k_write_memory_8(address + 1, value);
}

void m68k_write_memory_32(unsigned int address, unsigned int value) {
    m68k_write_memory_16(address, value >> 16);
    m68k_write_memory_16(address + 2, value);
}

static unsigned long parse_number(const char *text) {
    char *end = NULL;
    errno = 0;
    unsigned long value = strtoul(text, &end, 0);
    if (errno || end == text || *end != '\0') {
        fprintf(stderr, "invalid number: %s\n", text);
        exit(2);
    }
    return value;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 5) {
        fprintf(stderr, "usage: %s FILE [FILE_OFFSET=0x204] [COUNT=64] [LOAD_ADDRESS=0]\n", argv[0]);
        return 2;
    }
    unsigned long file_offset = argc >= 3 ? parse_number(argv[2]) : 0x204;
    unsigned long count = argc >= 4 ? parse_number(argv[3]) : 64;
    unsigned long load_address = argc >= 5 ? parse_number(argv[4]) : 0;
    FILE *input = fopen(argv[1], "rb");
    if (!input) {
        perror(argv[1]);
        return 1;
    }
    if (fseek(input, 0, SEEK_END) || ftell(input) < 0) {
        perror("seek");
        fclose(input);
        return 1;
    }
    long file_size = ftell(input);
    if (file_offset >= (unsigned long)file_size || load_address + (unsigned long)file_size > ADDRESS_SPACE ||
        fseek(input, 0, SEEK_SET)) {
        fprintf(stderr, "file offset is outside input\n");
        fclose(input);
        return 1;
    }
    memory = calloc(ADDRESS_SPACE, 1);
    if (!memory) {
        perror("calloc");
        fclose(input);
        return 1;
    }
    size_t available = (size_t)file_size;
    if (fread(memory + load_address, 1, available, input) != available) {
        perror("read");
        free(memory);
        fclose(input);
        return 1;
    }
    fclose(input);

    unsigned int pc = (unsigned int)(load_address + file_offset);
    for (unsigned long index = 0; index < count; ++index) {
        char text[256];
        unsigned int length = m68k_disassemble(text, pc, M68K_CPU_TYPE_68000);
        printf("%06x  ", pc);
        for (unsigned int byte = 0; byte < 10; ++byte) {
            if (byte < length) printf("%02x", m68k_read_memory_8(pc + byte));
            else printf("  ");
        }
        printf("  %s\n", text);
        if (!length) break;
        pc += length;
    }
    free(memory);
    return 0;
}
