#include "m68hc11_core.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t bytes[65536];
    unsigned int reads;
    unsigned int writes;
} TestBus;

static uint8_t read8(void *context, uint16_t address) {
    TestBus *bus = context;
    ++bus->reads;
    return bus->bytes[address];
}

static void write8(void *context, uint16_t address, uint8_t value) {
    TestBus *bus = context;
    ++bus->writes;
    bus->bytes[address] = value;
}

static int require(int condition, const char *message) {
    if (condition) return 1;
    fprintf(stderr, "M68HC11 core test failed: %s\n", message);
    return 0;
}

int main(void) {
    TestBus bus;
    memset(&bus, 0, sizeof(bus));
    bus.bytes[0xfffe] = 0xe0;
    bus.bytes[0xffff] = 0x05;
    bus.bytes[0xe005] = 0x01; /* NOP */

    M68hc11Core core;
    m68hc11_init(&core, &bus, read8, write8);
    m68hc11_reset(&core);
    if (!require(core.pc == 0xe005, "reset vector")) return 1;
    if (!require(core.ccr == (M68HC11_CCR_S | M68HC11_CCR_X | M68HC11_CCR_I),
                 "reset CCR S/X/I bits")) return 1;
    if (!require(core.cycles == 3 && bus.reads == 2, "reset fetch timing")) return 1;

    if (!require(m68hc11_step(&core) == 2, "NOP cycle count")) return 1;
    if (!require(core.pc == 0xe006 && core.cycles == 5, "NOP state")) return 1;
    if (!require(!core.illegal && bus.writes == 0, "NOP bus behavior")) return 1;

    size_t pc = 0xe006;
    bus.bytes[pc++] = 0x86; bus.bytes[pc++] = 0x80;       /* LDAA #$80 */
    bus.bytes[pc++] = 0x97; bus.bytes[pc++] = 0x40;       /* STAA $40 */
    bus.bytes[pc++] = 0x86; bus.bytes[pc++] = 0x00;       /* LDAA #0 */
    bus.bytes[pc++] = 0x26; bus.bytes[pc++] = 0x02;       /* BNE not taken */
    bus.bytes[pc++] = 0x27; bus.bytes[pc++] = 0x02;       /* BEQ taken */
    bus.bytes[pc++] = 0x00; bus.bytes[pc++] = 0x00;       /* skipped */
    bus.bytes[pc++] = 0xce; bus.bytes[pc++] = 0x20; bus.bytes[pc++] = 0x00;
    bus.bytes[pc++] = 0xa6; bus.bytes[pc++] = 0x03;       /* LDAA 3,X */
    bus.bytes[pc++] = 0xb7; bus.bytes[pc++] = 0x30; bus.bytes[pc++] = 0x00;
    bus.bytes[pc++] = 0x00;                               /* illegal */
    bus.bytes[0x2003] = 0x55;

    if (!require(m68hc11_step(&core) == 2 && core.a == 0x80 &&
                 (core.ccr & M68HC11_CCR_N), "LDAA immediate flags")) return 1;
    if (!require(m68hc11_step(&core) == 3 && bus.bytes[0x40] == 0x80,
                 "STAA direct")) return 1;
    if (!require(m68hc11_step(&core) == 2 &&
                 (core.ccr & M68HC11_CCR_Z), "LDAA zero flag")) return 1;
    if (!require(m68hc11_step(&core) == 3 && core.pc == 0xe00e,
                 "BNE not taken")) return 1;
    if (!require(m68hc11_step(&core) == 3 && core.pc == 0xe012,
                 "BEQ taken")) return 1;
    if (!require(m68hc11_step(&core) == 3 && core.x == 0x2000,
                 "LDX immediate")) return 1;
    if (!require(m68hc11_step(&core) == 4 && core.a == 0x55,
                 "LDAA indexed")) return 1;
    if (!require(m68hc11_step(&core) == 4 && bus.bytes[0x3000] == 0x55,
                 "STAA extended")) return 1;
    if (!require(m68hc11_step(&core) == 0 && core.illegal,
                 "unknown opcode stops core")) return 1;

    memset(&bus.bytes[0xe100], 0, 0x40);
    bus.bytes[0xe100] = 0x8e; bus.bytes[0xe101] = 0x01; bus.bytes[0xe102] = 0xff;
    bus.bytes[0xe103] = 0xcc; bus.bytes[0xe104] = 0x12; bus.bytes[0xe105] = 0x34;
    bus.bytes[0xe106] = 0xfd; bus.bytes[0xe107] = 0x20; bus.bytes[0xe108] = 0x00;
    bus.bytes[0xe109] = 0xbd; bus.bytes[0xe10a] = 0xe1; bus.bytes[0xe10b] = 0x10;
    bus.bytes[0xe10c] = 0x7f; bus.bytes[0xe10d] = 0x20; bus.bytes[0xe10e] = 0x02;
    bus.bytes[0xe10f] = 0x00;
    bus.bytes[0xe110] = 0x4f;
    bus.bytes[0xe111] = 0x39;
    core.pc = 0xe100;
    core.illegal = 0;
    if (!require(m68hc11_step(&core) == 3 && core.sp == 0x01ff,
                 "LDS immediate")) return 1;
    if (!require(m68hc11_step(&core) == 3 && core.a == 0x12 && core.b == 0x34,
                 "LDD immediate")) return 1;
    if (!require(m68hc11_step(&core) == 5 && bus.bytes[0x2000] == 0x12 &&
                 bus.bytes[0x2001] == 0x34, "STD extended")) return 1;
    if (!require(m68hc11_step(&core) == 6 && core.pc == 0xe110 &&
                 core.sp == 0x01fd, "JSR stack")) return 1;
    if (!require(m68hc11_step(&core) == 2 && core.a == 0,
                 "CLRA")) return 1;
    if (!require(m68hc11_step(&core) == 5 && core.pc == 0xe10c &&
                 core.sp == 0x01ff, "RTS stack")) return 1;
    bus.bytes[0x2002] = 0x77;
    if (!require(m68hc11_step(&core) == 5 && bus.bytes[0x2002] == 0,
                 "CLR extended")) return 1;

    bus.bytes[0xe120] = 0x84; bus.bytes[0xe121] = 0x0f;
    core.pc = 0xe120;
    core.a = 0xa5;
    core.ccr |= M68HC11_CCR_C;
    if (!require(m68hc11_step(&core) == 2 && core.a == 0x05 &&
                 (core.ccr & M68HC11_CCR_C), "ANDA immediate")) return 1;

    bus.bytes[0xe130] = 0x37;
    bus.bytes[0xe131] = 0x5a;
    bus.bytes[0xe132] = 0x33;
    core.pc = 0xe130;
    core.sp = 0x01ff;
    core.b = 0x80;
    if (!require(m68hc11_step(&core) == 3 && core.sp == 0x01fe &&
                 bus.bytes[0x01ff] == 0x80, "PSHB")) return 1;
    if (!require(m68hc11_step(&core) == 2 && core.b == 0x7f &&
                 (core.ccr & M68HC11_CCR_V), "DECB")) return 1;
    if (!require(m68hc11_step(&core) == 4 && core.b == 0x80 &&
                 core.sp == 0x01ff, "PULB")) return 1;

    bus.bytes[0xe140] = 0x18; bus.bytes[0xe141] = 0xce;
    bus.bytes[0xe142] = 0x20; bus.bytes[0xe143] = 0x04;
    bus.bytes[0xe144] = 0x18; bus.bytes[0xe145] = 0x09;
    bus.bytes[0xe146] = 0x18; bus.bytes[0xe147] = 0xe6;
    bus.bytes[0xe148] = 0x01;
    bus.bytes[0xe149] = 0x3a;
    bus.bytes[0xe14a] = 0x18; bus.bytes[0xe14b] = 0x8c;
    bus.bytes[0xe14c] = 0x20; bus.bytes[0xe14d] = 0x03;
    bus.bytes[0xe14e] = 0x8f;
    bus.bytes[0x2004] = 0x07;
    core.pc = 0xe140;
    core.x = 0x1000;
    core.a = 0x12;
    core.b = 0x34;
    if (!require(m68hc11_step(&core) == 4 && core.y == 0x2004,
                 "LDY immediate")) return 1;
    if (!require(m68hc11_step(&core) == 4 && core.y == 0x2003,
                 "DEY")) return 1;
    if (!require(m68hc11_step(&core) == 5 && core.b == 0x07,
                 "LDAB indexed Y")) return 1;
    if (!require(m68hc11_step(&core) == 3 && core.x == 0x1007,
                 "ABX")) return 1;
    if (!require(m68hc11_step(&core) == 5 &&
                 (core.ccr & M68HC11_CCR_Z), "CPY immediate")) return 1;
    if (!require(m68hc11_step(&core) == 3 && core.x == 0x1207 &&
                 core.a == 0x10 && core.b == 0x07, "XGDX")) return 1;

    bus.bytes[0xe150] = 0xf3; bus.bytes[0xe151] = 0x21; bus.bytes[0xe152] = 0x00;
    bus.bytes[0x2100] = 0x00; bus.bytes[0x2101] = 0x02;
    core.pc = 0xe150;
    core.a = 0xff;
    core.b = 0xff;
    if (!require(m68hc11_step(&core) == 6 && core.a == 0x00 &&
                 core.b == 0x01 && (core.ccr & M68HC11_CCR_C),
                 "ADDD extended")) return 1;

    bus.bytes[0xe160] = 0x08;
    bus.bytes[0xe161] = 0x09;
    bus.bytes[0xe162] = 0x8c; bus.bytes[0xe163] = 0x22; bus.bytes[0xe164] = 0x00;
    core.pc = 0xe160;
    core.x = 0x21ff;
    if (!require(m68hc11_step(&core) == 3 && core.x == 0x2200,
                 "INX")) return 1;
    if (!require(m68hc11_step(&core) == 3 && core.x == 0x21ff,
                 "DEX")) return 1;
    core.x = 0x2200;
    if (!require(m68hc11_step(&core) == 4 &&
                 (core.ccr & M68HC11_CCR_Z), "CPX immediate")) return 1;

    bus.bytes[0xe170] = 0xde; bus.bytes[0xe171] = 0x70;
    bus.bytes[0xe172] = 0xdf; bus.bytes[0xe173] = 0x72;
    bus.bytes[0x70] = 0xab; bus.bytes[0x71] = 0xcd;
    core.pc = 0xe170;
    if (!require(m68hc11_step(&core) == 4 && core.x == 0xabcd,
                 "LDX direct")) return 1;
    if (!require(m68hc11_step(&core) == 4 && bus.bytes[0x72] == 0xab &&
                 bus.bytes[0x73] == 0xcd, "STX direct")) return 1;

    bus.bytes[0xe180] = 0x12; bus.bytes[0xe181] = 0x75;
    bus.bytes[0xe182] = 0x20; bus.bytes[0xe183] = 0x02;
    bus.bytes[0xe184] = 0x00; bus.bytes[0xe185] = 0x00;
    bus.bytes[0xe186] = 0x13; bus.bytes[0xe187] = 0x75;
    bus.bytes[0xe188] = 0x40; bus.bytes[0xe189] = 0x02;
    bus.bytes[0x75] = 0x20;
    core.pc = 0xe180;
    if (!require(m68hc11_step(&core) == 6 && core.pc == 0xe186,
                 "BRSET direct")) return 1;
    if (!require(m68hc11_step(&core) == 6 && core.pc == 0xe18c,
                 "BRCLR direct")) return 1;

    core.pc = 0xe200;
    core.sp = 0x01ff;
    core.y = 0x3456;
    core.x = 0x789a;
    core.a = 0xbc;
    core.b = 0xde;
    core.ccr = M68HC11_CCR_X | M68HC11_CCR_C;
    bus.bytes[0xffea] = 0xe3;
    bus.bytes[0xffeb] = 0x00;
    bus.bytes[0xe300] = 0x3b;
    if (!require(m68hc11_interrupt(&core, 0xffea) == 12 &&
                 core.pc == 0xe300 && core.sp == 0x01f6 &&
                 (core.ccr & M68HC11_CCR_I), "interrupt entry")) return 1;
    if (!require(bus.bytes[0x01ff] == 0x00 && bus.bytes[0x01fe] == 0xe2 &&
                 bus.bytes[0x01fd] == 0x56 && bus.bytes[0x01fc] == 0x34 &&
                 bus.bytes[0x01fb] == 0x9a && bus.bytes[0x01fa] == 0x78 &&
                 bus.bytes[0x01f9] == 0xbc && bus.bytes[0x01f8] == 0xde &&
                 bus.bytes[0x01f7] == (M68HC11_CCR_X | M68HC11_CCR_C),
                 "interrupt stack order")) return 1;
    if (!require(m68hc11_step(&core) == 12 && core.pc == 0xe200 &&
                 core.sp == 0x01ff && core.y == 0x3456 && core.x == 0x789a &&
                 core.a == 0xbc && core.b == 0xde &&
                 core.ccr == (M68HC11_CCR_X | M68HC11_CCR_C),
                 "RTI restore")) return 1;

    bus.bytes[0xe210] = 0xc4; bus.bytes[0xe211] = 0x0f;
    bus.bytes[0xe212] = 0x5f;
    core.pc = 0xe210;
    core.b = 0xa5;
    if (!require(m68hc11_step(&core) == 2 && core.b == 0x05,
                 "ANDB immediate")) return 1;
    if (!require(m68hc11_step(&core) == 2 && core.b == 0 &&
                 (core.ccr & M68HC11_CCR_Z), "CLRB")) return 1;

    bus.bytes[0xe220] = 0x18; bus.bytes[0xe221] = 0x8f;
    bus.bytes[0xe222] = 0x16;
    bus.bytes[0xe223] = 0x17;
    bus.bytes[0xe224] = 0x5c;
    bus.bytes[0xe225] = 0xd1; bus.bytes[0xe226] = 0x76;
    bus.bytes[0x76] = 0x35;
    core.pc = 0xe220;
    core.y = 0x1234;
    core.a = 0xab;
    core.b = 0xcd;
    if (!require(m68hc11_step(&core) == 4 && core.y == 0xabcd &&
                 core.a == 0x12 && core.b == 0x34, "XGDY")) return 1;
    if (!require(m68hc11_step(&core) == 2 && core.b == 0x12,
                 "TAB")) return 1;
    core.b = 0x34;
    if (!require(m68hc11_step(&core) == 2 && core.a == 0x34,
                 "TBA")) return 1;
    if (!require(m68hc11_step(&core) == 2 && core.b == 0x35,
                 "INCB")) return 1;
    if (!require(m68hc11_step(&core) == 3 &&
                 (core.ccr & M68HC11_CCR_Z), "CMPB direct")) return 1;

    bus.bytes[0xe230] = 0x43;
    bus.bytes[0xe231] = 0x53;
    bus.bytes[0xe232] = 0x5d;
    bus.bytes[0xe233] = 0xf1; bus.bytes[0xe234] = 0x22; bus.bytes[0xe235] = 0x10;
    bus.bytes[0x2210] = 0x00;
    core.pc = 0xe230;
    core.a = 0x00;
    core.b = 0xff;
    if (!require(m68hc11_step(&core) == 2 && core.a == 0xff &&
                 (core.ccr & M68HC11_CCR_C), "COMA")) return 1;
    if (!require(m68hc11_step(&core) == 2 && core.b == 0x00 &&
                 (core.ccr & M68HC11_CCR_C), "COMB")) return 1;
    if (!require(m68hc11_step(&core) == 2 &&
                 (core.ccr & M68HC11_CCR_Z) &&
                 !(core.ccr & M68HC11_CCR_C), "TSTB")) return 1;
    if (!require(m68hc11_step(&core) == 4 &&
                 (core.ccr & M68HC11_CCR_Z), "CMPB extended")) return 1;

    bus.bytes[0xe240] = 0x48;
    bus.bytes[0xe241] = 0x1b;
    bus.bytes[0xe242] = 0x8b; bus.bytes[0xe243] = 0x01;
    bus.bytes[0xe244] = 0x58;
    core.pc = 0xe240;
    core.a = 0x80;
    core.b = 0x01;
    if (!require(m68hc11_step(&core) == 2 && core.a == 0 &&
                 (core.ccr & M68HC11_CCR_C) &&
                 (core.ccr & M68HC11_CCR_V), "ASLA")) return 1;
    if (!require(m68hc11_step(&core) == 2 && core.a == 1,
                 "ABA")) return 1;
    if (!require(m68hc11_step(&core) == 2 && core.a == 2,
                 "ADDA immediate")) return 1;
    core.b = 0x80;
    if (!require(m68hc11_step(&core) == 2 && core.b == 0 &&
                 (core.ccr & M68HC11_CCR_C) &&
                 (core.ccr & M68HC11_CCR_V), "ASLB")) return 1;

    bus.bytes[0xe250] = 0xee; bus.bytes[0xe251] = 0x04;
    bus.bytes[0x2004] = 0x12; bus.bytes[0x2005] = 0x34;
    core.pc = 0xe250;
    core.x = 0x2000;
    if (!require(m68hc11_step(&core) == 5 && core.x == 0x1234,
                 "LDX indexed")) return 1;

    bus.bytes[0xe260] = 0x8d; bus.bytes[0xe261] = 0x04;
    core.pc = 0xe260;
    core.sp = 0x01ff;
    if (!require(m68hc11_step(&core) == 6 && core.pc == 0xe266 &&
                 core.sp == 0x01fd && bus.bytes[0x01ff] == 0x62 &&
                 bus.bytes[0x01fe] == 0xe2, "BSR")) return 1;

    bus.bytes[0xe270] = 0x36; bus.bytes[0xe271] = 0x32;
    core.pc = 0xe270;
    core.sp = 0x01ff;
    core.a = 0xa5;
    if (!require(m68hc11_step(&core) == 3 && core.sp == 0x01fe &&
                 bus.bytes[0x01ff] == 0xa5, "PSHA")) return 1;
    core.a = 0;
    if (!require(m68hc11_step(&core) == 4 && core.a == 0xa5 &&
                 core.sp == 0x01ff, "PULA")) return 1;

    bus.bytes[0xe280] = 0xc8; bus.bytes[0xe281] = 0x0f;
    bus.bytes[0xe282] = 0xc1; bus.bytes[0xe283] = 0xa5;
    core.pc = 0xe280;
    core.b = 0xaa;
    if (!require(m68hc11_step(&core) == 2 && core.b == 0xa5,
                 "EORB immediate")) return 1;
    if (!require(m68hc11_step(&core) == 2 &&
                 (core.ccr & M68HC11_CCR_Z), "CMPB immediate")) return 1;

    bus.bytes[0xe290] = 0xa1; bus.bytes[0xe291] = 0x05;
    bus.bytes[0x3005] = 0x42;
    core.pc = 0xe290;
    core.x = 0x3000;
    core.a = 0x42;
    if (!require(m68hc11_step(&core) == 4 &&
                 (core.ccr & M68HC11_CCR_Z), "CMPA indexed")) return 1;

    bus.bytes[0xe2a0] = 0x18; bus.bytes[0xe2a1] = 0xa7;
    bus.bytes[0xe2a2] = 0x06;
    core.pc = 0xe2a0;
    core.y = 0x3100;
    core.a = 0x66;
    if (!require(m68hc11_step(&core) == 5 && bus.bytes[0x3106] == 0x66,
                 "STAA indexed Y")) return 1;

    bus.bytes[0xe2a3] = 0x18; bus.bytes[0xe2a4] = 0xa6;
    bus.bytes[0xe2a5] = 0x07;
    bus.bytes[0xe2a6] = 0x18; bus.bytes[0xe2a7] = 0xe7;
    bus.bytes[0xe2a8] = 0x08;
    bus.bytes[0x3107] = 0x77;
    core.pc = 0xe2a3;
    core.b = 0x88;
    if (!require(m68hc11_step(&core) == 5 && core.a == 0x77,
                 "LDAA indexed Y")) return 1;
    if (!require(m68hc11_step(&core) == 5 && bus.bytes[0x3108] == 0x88,
                 "STAB indexed Y")) return 1;

    bus.bytes[0xe2b0] = 0x4d;
    core.pc = 0xe2b0;
    core.a = 0;
    core.ccr |= M68HC11_CCR_C;
    if (!require(m68hc11_step(&core) == 2 &&
                 (core.ccr & M68HC11_CCR_Z) &&
                 !(core.ccr & M68HC11_CCR_C), "TSTA")) return 1;

    bus.bytes[0xe2c0] = 0x4c;
    core.pc = 0xe2c0;
    core.a = 0x7f;
    if (!require(m68hc11_step(&core) == 2 && core.a == 0x80 &&
                 (core.ccr & M68HC11_CCR_N) &&
                 (core.ccr & M68HC11_CCR_V), "INCA")) return 1;

    bus.bytes[0xe2d0] = 0x10;
    core.pc = 0xe2d0;
    core.a = 0x40;
    core.b = 0x01;
    if (!require(m68hc11_step(&core) == 2 && core.a == 0x3f &&
                 !(core.ccr & M68HC11_CCR_C), "SBA")) return 1;

    bus.bytes[0xe2d1] = 0x30;
    core.pc = 0xe2d1;
    core.sp = 0x12ff;
    core.x = 0;
    if (!require(m68hc11_step(&core) == 3 && core.x == 0x1300 &&
                 core.sp == 0x12ff, "TSX")) return 1;

    bus.bytes[0xe2d2] = 0xed; bus.bytes[0xe2d3] = 0x07;
    core.pc = 0xe2d2;
    core.x = 0x1300;
    core.a = 0xab;
    core.b = 0xcd;
    if (!require(m68hc11_step(&core) == 5 &&
                 bus.bytes[0x1307] == 0xab && bus.bytes[0x1308] == 0xcd,
                 "STD indexed X")) return 1;

    bus.bytes[0xe2e0] = 0x3d;
    core.pc = 0xe2e0;
    core.a = 0x10;
    core.b = 0x08;
    if (!require(m68hc11_step(&core) == 10 && core.a == 0 &&
                 core.b == 0x80 && (core.ccr & M68HC11_CCR_C),
                 "MUL")) return 1;

    bus.bytes[0xe2f0] = 0xfb; bus.bytes[0xe2f1] = 0x32;
    bus.bytes[0xe2f2] = 0x00; bus.bytes[0x3200] = 0x03;
    core.pc = 0xe2f0;
    core.b = 0x7e;
    if (!require(m68hc11_step(&core) == 4 && core.b == 0x81 &&
                 (core.ccr & M68HC11_CCR_V), "ADDB extended")) return 1;

    bus.bytes[0xe300] = 0x89; bus.bytes[0xe301] = 0x00;
    core.pc = 0xe300;
    core.a = 0x7f;
    core.ccr |= M68HC11_CCR_C;
    if (!require(m68hc11_step(&core) == 2 && core.a == 0x80 &&
                 (core.ccr & M68HC11_CCR_V) &&
                 !(core.ccr & M68HC11_CCR_C), "ADCA immediate")) return 1;

    bus.bytes[0xe310] = 0x02;
    core.pc = 0xe310;
    core.a = 0x01;
    core.b = 0x01;
    core.x = 0x0010;
    if (!require(m68hc11_step(&core) == 41 && core.x == 0x0010 &&
                 core.a == 0 && core.b == 1, "IDIV")) return 1;

    bus.bytes[0xe320] = 0x1a; bus.bytes[0xe321] = 0x83;
    bus.bytes[0xe322] = 0x12; bus.bytes[0xe323] = 0x34;
    core.pc = 0xe320;
    core.a = 0x12;
    core.b = 0x34;
    if (!require(m68hc11_step(&core) == 5 &&
                 (core.ccr & M68HC11_CCR_Z), "CPD immediate")) return 1;

    puts("M68HC11 core instruction tests OK");
    return 0;
}
