#include "es5510_core.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static uint64_t instruction(uint8_t d, uint8_t c, uint8_t b, uint8_t a,
                            uint8_t alu_op, uint8_t select) {
    return ((uint64_t)d << 40) | ((uint64_t)c << 32) |
           ((uint64_t)b << 24) | ((uint64_t)a << 16) |
           ((uint64_t)alu_op << 12) | ((uint64_t)select << 8);
}

int main(void) {
    Es5510Core core;
    es5510_core_init(&core);

    /* MOV SER2L -> SER3L; MOV SER2R -> SER3R; END.  ALU results are
       committed one instruction later, exactly like the real ES5510. */
    core.instruction[0] = instruction(0xff, 0xff, 0xef, 0xf1, 9, 0);
    core.instruction[1] = instruction(0xff, 0xff, 0xee, 0xf0, 9, 0);
    core.instruction[2] = instruction(0xff, 0xff, 0xff, 0xff, 15, 0);
    es5510_core_set_halted(&core, 0);
    es5510_core_set_host_serial(&core, 0x20);

    const int16_t input[8] = {111, -222, 333, -444, 12345, -23456, 0, 0};
    int16_t output[2] = {0, 0};
    es5510_core_process(&core, input, output);
    assert(output[0] == input[4]);
    assert(output[1] == input[5]);
    assert(core.frames == 1);
    assert(core.instructions_executed == 3);

    /* Saturating positive overflow produces MAX with V set, but it must not
       retain the wrapped result's N flag. Conditional distortion programs
       use these flags in their following skippable instructions. */
    es5510_core_init(&core);
    core.gpr[0] = 0x7fffff;
    core.gpr[1] = 1;
    core.instruction[0] = instruction(0xff, 0xff, 1, 0, 0, 0);
    core.instruction[1] = instruction(0xff, 0xff, 0xff, 0xff, 15, 0);
    es5510_core_set_halted(&core, 0);
    es5510_core_process(&core, input, output);
    assert(core.gpr[0] == 0x7fffff);
    assert((core.ccr & (0x80 | 0x20 | 0x08)) == 0x20);

    /* External DRAM addresses are left-justified in the 24-bit DADR and
       base-register words. MEMSIZ selects their implemented address bits. */
    es5510_core_write_reg(&core, 0xf4, 0x0000ff);
    assert(es5510_core_dram_address(&core, 0x3dfe00) == 0x3dfe);
    assert(es5510_core_dram_address(&core, 0x3eff00) == 0x3eff);

    puts("ES5510 core OK");
    return 0;
}
