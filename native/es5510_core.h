#ifndef EPS16_ES5510_CORE_H
#define EPS16_ES5510_CORE_H

#include <stdint.h>

enum { ES5510_INSTRUCTIONS = 160, ES5510_DRAM_WORDS = 1 << 20 };

typedef struct {
    uint8_t a_reg, b_reg, op, src, dst;
    int32_t a_value, b_value, result;
    int update_ccr, write_result;
} Es5510Alu;

typedef struct {
    uint8_t c_reg, d_reg, src, dst;
    int accumulate, write_result;
    int32_t c_value, d_value;
    int64_t product, result;
} Es5510Mac;

typedef struct {
    int32_t address;
    uint8_t io, cycle;
} Es5510RamCycle;

typedef struct {
    uint32_t gpr[256];
    uint64_t instruction[ES5510_INSTRUCTIONS];
    int16_t dram[ES5510_DRAM_WORDS];
    int16_t serial[8];
    int64_t machl;
    int mac_overflow;
    int16_t dil;
    uint32_t memsiz, memmask, memincrement;
    int memshift;
    uint32_t dlength, abase, bbase, dbase, sigreg;
    uint8_t ccr, cmr;
    uint8_t host_serial;
    int16_t dol[2];
    unsigned int dol_count;
    Es5510Alu alu;
    Es5510Mac mac;
    Es5510RamCycle ram, ram_p, ram_pp;
    int halted;
    uint64_t frames;
    uint64_t instructions_executed;
} Es5510Core;

void es5510_core_init(Es5510Core *core);
uint32_t es5510_core_read_reg(Es5510Core *core, uint8_t reg);
void es5510_core_write_reg(Es5510Core *core, uint8_t reg, uint32_t value);
void es5510_core_set_halted(Es5510Core *core, int halted);
void es5510_core_set_host_serial(Es5510Core *core, uint8_t value);
unsigned int es5510_core_dram_address(const Es5510Core *core,
                                      uint32_t address);
void es5510_core_process(Es5510Core *core, const int16_t inputs[8],
                         int16_t outputs[2]);

#endif
