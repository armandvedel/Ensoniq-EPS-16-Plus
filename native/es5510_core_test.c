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

    puts("ES5510 core OK");
    return 0;
}
