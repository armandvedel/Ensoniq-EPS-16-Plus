#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "m68k.h"

enum { MEMORY_SIZE = 1 << 20 };
static uint8_t memory[MEMORY_SIZE];

static unsigned int mask_address(unsigned int address) {
    return address & (MEMORY_SIZE - 1);
}

unsigned int m68k_read_memory_8(unsigned int address) {
    return memory[mask_address(address)];
}

unsigned int m68k_read_memory_16(unsigned int address) {
    address = mask_address(address);
    return ((unsigned int)memory[address] << 8) |
           memory[mask_address(address + 1)];
}

unsigned int m68k_read_memory_32(unsigned int address) {
    return (m68k_read_memory_16(address) << 16) |
           m68k_read_memory_16(address + 2);
}

unsigned int m68k_read_disassembler_16(unsigned int address) {
    return m68k_read_memory_16(address);
}

unsigned int m68k_read_disassembler_32(unsigned int address) {
    return m68k_read_memory_32(address);
}

void m68k_write_memory_8(unsigned int address, unsigned int value) {
    memory[mask_address(address)] = (uint8_t)value;
}

void m68k_write_memory_16(unsigned int address, unsigned int value) {
    address = mask_address(address);
    memory[address] = (uint8_t)(value >> 8);
    memory[mask_address(address + 1)] = (uint8_t)value;
}

void m68k_write_memory_32(unsigned int address, unsigned int value) {
    m68k_write_memory_16(address, value >> 16);
    m68k_write_memory_16(address + 2, value);
}

static void write_long(unsigned int address, uint32_t value) {
    m68k_write_memory_32(address, value);
}

int main(void) {
    /* Reset vectors followed by: MOVEQ #42,D0; STOP #$2700. */
    static const uint8_t program[] = {0x70, 0x2a, 0x4e, 0x72, 0x27, 0x00};
    memset(memory, 0, sizeof(memory));
    write_long(0, 0x00001000);
    write_long(4, 0x00000100);
    memcpy(&memory[0x100], program, sizeof(program));

    m68k_init();
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    m68k_pulse_reset();
    (void)m68k_execute(100);

    if (m68k_get_reg(NULL, M68K_REG_D0) != 42) {
        fprintf(stderr, "68000 smoke test failed: D0=%u\n", m68k_get_reg(NULL, M68K_REG_D0));
        return 1;
    }
    printf("68000 core OK: D0=%u PC=0x%06x\n",
           m68k_get_reg(NULL, M68K_REG_D0),
           m68k_get_reg(NULL, M68K_REG_PC));
    return 0;
}
