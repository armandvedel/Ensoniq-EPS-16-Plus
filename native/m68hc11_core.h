#ifndef EPS16_M68HC11_CORE_H
#define EPS16_M68HC11_CORE_H

#include <stdint.h>

typedef uint8_t (*M68hc11Read8)(void *context, uint16_t address);
typedef void (*M68hc11Write8)(void *context, uint16_t address, uint8_t value);

enum {
    M68HC11_CCR_C = 0x01,
    M68HC11_CCR_V = 0x02,
    M68HC11_CCR_Z = 0x04,
    M68HC11_CCR_N = 0x08,
    M68HC11_CCR_I = 0x10,
    M68HC11_CCR_H = 0x20,
    M68HC11_CCR_X = 0x40,
    M68HC11_CCR_S = 0x80
};

typedef struct {
    uint8_t a;
    uint8_t b;
    uint8_t ccr;
    uint16_t x;
    uint16_t y;
    uint16_t sp;
    uint16_t pc;
    uint64_t cycles;
    uint8_t last_opcode;
    int illegal;
    void *memory_context;
    M68hc11Read8 read8;
    M68hc11Write8 write8;
} M68hc11Core;

void m68hc11_init(M68hc11Core *core, void *memory_context,
                  M68hc11Read8 read8, M68hc11Write8 write8);
void m68hc11_reset(M68hc11Core *core);
unsigned int m68hc11_step(M68hc11Core *core);
unsigned int m68hc11_interrupt(M68hc11Core *core, uint16_t vector);

#endif
