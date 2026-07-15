#include "m68hc11_core.h"

#include <string.h>

static uint16_t read16(M68hc11Core *core, uint16_t address) {
    uint8_t high = core->read8(core->memory_context, address);
    uint8_t low = core->read8(core->memory_context, (uint16_t)(address + 1));
    return (uint16_t)(((uint16_t)high << 8) | low);
}

static void write8(M68hc11Core *core, uint16_t address, uint8_t value) {
    core->write8(core->memory_context, address, value);
}

static uint8_t fetch8(M68hc11Core *core) {
    return core->read8(core->memory_context, core->pc++);
}

static uint16_t fetch16(M68hc11Core *core) {
    uint16_t value = read16(core, core->pc);
    core->pc = (uint16_t)(core->pc + 2);
    return value;
}

static void set_nzv8(M68hc11Core *core, uint8_t value) {
    core->ccr &= (uint8_t)~(M68HC11_CCR_N | M68HC11_CCR_Z | M68HC11_CCR_V);
    if (!value) core->ccr |= M68HC11_CCR_Z;
    if (value & 0x80) core->ccr |= M68HC11_CCR_N;
}

static void set_nzv16(M68hc11Core *core, uint16_t value) {
    core->ccr &= (uint8_t)~(M68HC11_CCR_N | M68HC11_CCR_Z | M68HC11_CCR_V);
    if (!value) core->ccr |= M68HC11_CCR_Z;
    if (value & 0x8000) core->ccr |= M68HC11_CCR_N;
}

static void set_sub8(M68hc11Core *core, uint8_t left, uint8_t right,
                     uint8_t result) {
    core->ccr &= (uint8_t)~(M68HC11_CCR_N | M68HC11_CCR_Z |
                            M68HC11_CCR_V | M68HC11_CCR_C);
    if (!result) core->ccr |= M68HC11_CCR_Z;
    if (result & 0x80) core->ccr |= M68HC11_CCR_N;
    if (((left ^ right) & (left ^ result) & 0x80) != 0)
        core->ccr |= M68HC11_CCR_V;
    if (left < right) core->ccr |= M68HC11_CCR_C;
}

static void set_sub16(M68hc11Core *core, uint16_t left, uint16_t right,
                      uint16_t result) {
    core->ccr &= (uint8_t)~(M68HC11_CCR_N | M68HC11_CCR_Z |
                            M68HC11_CCR_V | M68HC11_CCR_C);
    if (!result) core->ccr |= M68HC11_CCR_Z;
    if (result & 0x8000) core->ccr |= M68HC11_CCR_N;
    if (((left ^ right) & (left ^ result) & 0x8000) != 0)
        core->ccr |= M68HC11_CCR_V;
    if (left < right) core->ccr |= M68HC11_CCR_C;
}

static void set_add16(M68hc11Core *core, uint16_t left, uint16_t right,
                      uint16_t result) {
    uint32_t wide = (uint32_t)left + right;
    core->ccr &= (uint8_t)~(M68HC11_CCR_N | M68HC11_CCR_Z |
                            M68HC11_CCR_V | M68HC11_CCR_C);
    if (!result) core->ccr |= M68HC11_CCR_Z;
    if (result & 0x8000) core->ccr |= M68HC11_CCR_N;
    if ((~(left ^ right) & (left ^ result) & 0x8000) != 0)
        core->ccr |= M68HC11_CCR_V;
    if (wide > 0xffff) core->ccr |= M68HC11_CCR_C;
}

static void set_add8(M68hc11Core *core, uint8_t left, uint8_t right,
                     uint8_t result) {
    uint16_t wide = (uint16_t)left + right;
    core->ccr &= (uint8_t)~(M68HC11_CCR_H | M68HC11_CCR_N |
                            M68HC11_CCR_Z | M68HC11_CCR_V |
                            M68HC11_CCR_C);
    if (((left & 0x0f) + (right & 0x0f)) > 0x0f)
        core->ccr |= M68HC11_CCR_H;
    if (!result) core->ccr |= M68HC11_CCR_Z;
    if (result & 0x80) core->ccr |= M68HC11_CCR_N;
    if ((~(left ^ right) & (left ^ result) & 0x80) != 0)
        core->ccr |= M68HC11_CCR_V;
    if (wide > 0xff) core->ccr |= M68HC11_CCR_C;
}

static uint16_t get_d(const M68hc11Core *core) {
    return (uint16_t)(((uint16_t)core->a << 8) | core->b);
}

static void set_d(M68hc11Core *core, uint16_t value) {
    core->a = (uint8_t)(value >> 8);
    core->b = (uint8_t)value;
}

static void push8(M68hc11Core *core, uint8_t value) {
    write8(core, core->sp, value);
    --core->sp;
}

static uint8_t pull8(M68hc11Core *core) {
    ++core->sp;
    return core->read8(core->memory_context, core->sp);
}

static void push16(M68hc11Core *core, uint16_t value) {
    push8(core, (uint8_t)value);
    push8(core, (uint8_t)(value >> 8));
}

static uint16_t pull16(M68hc11Core *core) {
    uint8_t high = pull8(core);
    uint8_t low = pull8(core);
    return (uint16_t)(((uint16_t)high << 8) | low);
}

static unsigned int finish(M68hc11Core *core, unsigned int cycles) {
    core->cycles += cycles;
    return cycles;
}

static unsigned int load_a(M68hc11Core *core, uint8_t value,
                           unsigned int cycles) {
    core->a = value;
    set_nzv8(core, value);
    return finish(core, cycles);
}

static unsigned int load_b(M68hc11Core *core, uint8_t value,
                           unsigned int cycles) {
    core->b = value;
    set_nzv8(core, value);
    return finish(core, cycles);
}

static int branch_condition(const M68hc11Core *core, uint8_t opcode) {
    int c = (core->ccr & M68HC11_CCR_C) != 0;
    int v = (core->ccr & M68HC11_CCR_V) != 0;
    int z = (core->ccr & M68HC11_CCR_Z) != 0;
    int n = (core->ccr & M68HC11_CCR_N) != 0;
    switch (opcode & 0x0f) {
        case 0x0: return 1;             /* BRA */
        case 0x1: return 0;             /* BRN */
        case 0x2: return !c && !z;      /* BHI */
        case 0x3: return c || z;        /* BLS */
        case 0x4: return !c;            /* BCC */
        case 0x5: return c;             /* BCS */
        case 0x6: return !z;            /* BNE */
        case 0x7: return z;             /* BEQ */
        case 0x8: return !v;            /* BVC */
        case 0x9: return v;             /* BVS */
        case 0xa: return !n;            /* BPL */
        case 0xb: return n;             /* BMI */
        case 0xc: return n == v;        /* BGE */
        case 0xd: return n != v;        /* BLT */
        case 0xe: return !z && n == v;  /* BGT */
        default: return z || n != v;    /* BLE */
    }
}

void m68hc11_init(M68hc11Core *core, void *memory_context,
                  M68hc11Read8 read8, M68hc11Write8 write8) {
    memset(core, 0, sizeof(*core));
    core->memory_context = memory_context;
    core->read8 = read8;
    core->write8 = write8;
}

void m68hc11_reset(M68hc11Core *core) {
    core->a = 0;
    core->b = 0;
    core->x = 0;
    core->y = 0;
    core->sp = 0;
    core->ccr = M68HC11_CCR_S | M68HC11_CCR_X | M68HC11_CCR_I;
    core->pc = read16(core, 0xfffe);
    core->cycles = 3;
    core->last_opcode = 0;
    core->illegal = 0;
}

unsigned int m68hc11_interrupt(M68hc11Core *core, uint16_t vector) {
    if (core->illegal || (core->ccr & M68HC11_CCR_I)) return 0;
    push16(core, core->pc);
    push16(core, core->y);
    push16(core, core->x);
    push8(core, core->a);
    push8(core, core->b);
    push8(core, core->ccr);
    core->ccr |= M68HC11_CCR_I;
    core->pc = read16(core, vector);
    return finish(core, 12);
}

unsigned int m68hc11_step(M68hc11Core *core) {
    if (core->illegal) return 0;
    uint8_t opcode = fetch8(core);
    core->last_opcode = opcode;
    if (opcode == 0x1a) {
        uint8_t page_opcode = fetch8(core);
        core->last_opcode = page_opcode;
        switch (page_opcode) {
            case 0x83: { /* CPD immediate */
                uint16_t value = fetch16(core);
                uint16_t d = get_d(core);
                set_sub16(core, d, value, (uint16_t)(d - value));
                return finish(core, 5);
            }
            default:
                core->illegal = 1;
                return 0;
        }
    }
    if (opcode == 0x18) {
        uint8_t page_opcode = fetch8(core);
        core->last_opcode = page_opcode;
        switch (page_opcode) {
            case 0x09: { /* DEY */
                uint16_t previous = core->y;
                --core->y;
                core->ccr &= (uint8_t)~(M68HC11_CCR_N | M68HC11_CCR_Z |
                                        M68HC11_CCR_V);
                if (!core->y) core->ccr |= M68HC11_CCR_Z;
                if (core->y & 0x8000) core->ccr |= M68HC11_CCR_N;
                if (previous == 0x8000) core->ccr |= M68HC11_CCR_V;
                return finish(core, 4);
            }
            case 0x8c: { /* CPY immediate */
                uint16_t value = fetch16(core);
                set_sub16(core, core->y, value,
                          (uint16_t)(core->y - value));
                return finish(core, 5);
            }
            case 0x8f: { /* XGDY */
                uint16_t d = get_d(core);
                set_d(core, core->y);
                core->y = d;
                return finish(core, 4);
            }
            case 0xa7: { /* STAA indexed by Y */
                uint16_t address = (uint16_t)(core->y + fetch8(core));
                write8(core, address, core->a);
                set_nzv8(core, core->a);
                return finish(core, 5);
            }
            case 0xa6: { /* LDAA indexed by Y */
                uint16_t address = (uint16_t)(core->y + fetch8(core));
                return load_a(core,
                              core->read8(core->memory_context, address), 5);
            }
            case 0xce: /* LDY immediate */
                core->y = fetch16(core);
                set_nzv16(core, core->y);
                return finish(core, 4);
            case 0xe6: { /* LDAB indexed by Y */
                uint16_t address = (uint16_t)(core->y + fetch8(core));
                return load_b(core,
                              core->read8(core->memory_context, address), 5);
            }
            case 0xe7: { /* STAB indexed by Y */
                uint16_t address = (uint16_t)(core->y + fetch8(core));
                write8(core, address, core->b);
                set_nzv8(core, core->b);
                return finish(core, 5);
            }
            default:
                core->illegal = 1;
                return 0;
        }
    }
    if ((opcode & 0xf0) == 0x20) {
        int8_t offset = (int8_t)fetch8(core);
        if (branch_condition(core, opcode))
            core->pc = (uint16_t)(core->pc + offset);
        return finish(core, 3);
    }
    switch (opcode) {
        case 0x01: /* NOP */
            return finish(core, 2);
        case 0x02: { /* IDIV */
            uint16_t dividend = get_d(core);
            uint16_t divisor = core->x;
            core->ccr &= (uint8_t)~(M68HC11_CCR_Z | M68HC11_CCR_V |
                                    M68HC11_CCR_C);
            if (!divisor) {
                core->x = 0xffff;
                core->ccr |= M68HC11_CCR_V | M68HC11_CCR_C;
            } else {
                core->x = (uint16_t)(dividend / divisor);
                set_d(core, (uint16_t)(dividend % divisor));
                if (!core->x) core->ccr |= M68HC11_CCR_Z;
            }
            return finish(core, 41);
        }
        case 0x06: /* TAP */
            core->ccr = core->a;
            return finish(core, 2);
        case 0x07: /* TPA */
            core->a = core->ccr;
            return finish(core, 2);
        case 0x08: /* INX */
            ++core->x;
            if (core->x) core->ccr &= (uint8_t)~M68HC11_CCR_Z;
            else core->ccr |= M68HC11_CCR_Z;
            return finish(core, 3);
        case 0x09: /* DEX */
            --core->x;
            if (core->x) core->ccr &= (uint8_t)~M68HC11_CCR_Z;
            else core->ccr |= M68HC11_CCR_Z;
            return finish(core, 3);
        case 0x0a: /* CLV */
            core->ccr &= (uint8_t)~M68HC11_CCR_V;
            return finish(core, 2);
        case 0x0b: /* SEV */
            core->ccr |= M68HC11_CCR_V;
            return finish(core, 2);
        case 0x0c: /* CLC */
            core->ccr &= (uint8_t)~M68HC11_CCR_C;
            return finish(core, 2);
        case 0x0d: /* SEC */
            core->ccr |= M68HC11_CCR_C;
            return finish(core, 2);
        case 0x0e: /* CLI */
            core->ccr &= (uint8_t)~M68HC11_CCR_I;
            return finish(core, 2);
        case 0x0f: /* SEI */
            core->ccr |= M68HC11_CCR_I;
            return finish(core, 2);
        case 0x10: { /* SBA */
            uint8_t left = core->a;
            uint8_t result = (uint8_t)(left - core->b);
            core->a = result;
            set_sub8(core, left, core->b, result);
            return finish(core, 2);
        }
        case 0x12: /* BRSET direct */
        case 0x13: { /* BRCLR direct */
            uint16_t address = fetch8(core);
            uint8_t mask = fetch8(core);
            int8_t offset = (int8_t)fetch8(core);
            uint8_t value = core->read8(core->memory_context, address);
            int bits_set = (value & mask) == mask;
            if ((opcode == 0x12 && bits_set) ||
                (opcode == 0x13 && !bits_set))
                core->pc = (uint16_t)(core->pc + offset);
            return finish(core, 6);
        }
        case 0x14: { /* BSET direct */
            uint16_t address = fetch8(core);
            uint8_t result = (uint8_t)(core->read8(core->memory_context,
                                                   address) | fetch8(core));
            write8(core, address, result);
            set_nzv8(core, result);
            return finish(core, 6);
        }
        case 0x15: { /* BCLR direct */
            uint16_t address = fetch8(core);
            uint8_t result = (uint8_t)(core->read8(core->memory_context,
                                                   address) & ~fetch8(core));
            write8(core, address, result);
            set_nzv8(core, result);
            return finish(core, 6);
        }
        case 0x16: /* TAB */
            core->b = core->a;
            set_nzv8(core, core->b);
            return finish(core, 2);
        case 0x17: /* TBA */
            core->a = core->b;
            set_nzv8(core, core->a);
            return finish(core, 2);
        case 0x1b: { /* ABA */
            uint8_t left = core->a;
            uint8_t result = (uint8_t)(left + core->b);
            core->a = result;
            set_add8(core, left, core->b, result);
            return finish(core, 2);
        }
        case 0x30: /* TSX */
            core->x = (uint16_t)(core->sp + 1);
            return finish(core, 3);
        case 0x33: /* PULB */
            core->b = pull8(core);
            return finish(core, 4);
        case 0x32: /* PULA */
            core->a = pull8(core);
            return finish(core, 4);
        case 0x36: /* PSHA */
            push8(core, core->a);
            return finish(core, 3);
        case 0x37: /* PSHB */
            push8(core, core->b);
            return finish(core, 3);
        case 0x39: /* RTS */
            core->pc = pull16(core);
            return finish(core, 5);
        case 0x3a: /* ABX */
            core->x = (uint16_t)(core->x + core->b);
            return finish(core, 3);
        case 0x3b: { /* RTI */
            int x_was_clear = !(core->ccr & M68HC11_CCR_X);
            core->ccr = pull8(core);
            if (x_was_clear) core->ccr &= (uint8_t)~M68HC11_CCR_X;
            core->b = pull8(core);
            core->a = pull8(core);
            core->x = pull16(core);
            core->y = pull16(core);
            core->pc = pull16(core);
            return finish(core, 12);
        }
        case 0x3d: { /* MUL */
            uint16_t result = (uint16_t)core->a * (uint16_t)core->b;
            set_d(core, result);
            core->ccr &= (uint8_t)~M68HC11_CCR_C;
            if (result & 0x0080) core->ccr |= M68HC11_CCR_C;
            return finish(core, 10);
        }
        case 0x43: /* COMA */
            core->a = (uint8_t)~core->a;
            set_nzv8(core, core->a);
            core->ccr |= M68HC11_CCR_C;
            return finish(core, 2);
        case 0x48: { /* ASLA */
            uint8_t previous = core->a;
            core->a = (uint8_t)(core->a << 1);
            core->ccr &= (uint8_t)~(M68HC11_CCR_N | M68HC11_CCR_Z |
                                    M68HC11_CCR_V | M68HC11_CCR_C);
            if (!core->a) core->ccr |= M68HC11_CCR_Z;
            if (core->a & 0x80) core->ccr |= M68HC11_CCR_N;
            if (previous & 0x80) core->ccr |= M68HC11_CCR_C;
            if (((core->ccr & M68HC11_CCR_N) != 0) !=
                ((core->ccr & M68HC11_CCR_C) != 0))
                core->ccr |= M68HC11_CCR_V;
            return finish(core, 2);
        }
        case 0x4c: /* INCA */ {
            uint8_t previous = core->a;
            ++core->a;
            core->ccr &= (uint8_t)~(M68HC11_CCR_N | M68HC11_CCR_Z |
                                    M68HC11_CCR_V);
            if (!core->a) core->ccr |= M68HC11_CCR_Z;
            if (core->a & 0x80) core->ccr |= M68HC11_CCR_N;
            if (previous == 0x7f) core->ccr |= M68HC11_CCR_V;
            return finish(core, 2);
        }
        case 0x4d: /* TSTA */
            set_nzv8(core, core->a);
            core->ccr &= (uint8_t)~M68HC11_CCR_C;
            return finish(core, 2);
        case 0x4f: /* CLRA */
            core->a = 0;
            core->ccr &= (uint8_t)~(M68HC11_CCR_N | M68HC11_CCR_V |
                                    M68HC11_CCR_C);
            core->ccr |= M68HC11_CCR_Z;
            return finish(core, 2);
        case 0x5a: /* DECB */ {
            uint8_t previous = core->b;
            core->b = (uint8_t)(core->b - 1);
            core->ccr &= (uint8_t)~(M68HC11_CCR_N | M68HC11_CCR_Z |
                                    M68HC11_CCR_V);
            if (!core->b) core->ccr |= M68HC11_CCR_Z;
            if (core->b & 0x80) core->ccr |= M68HC11_CCR_N;
            if (previous == 0x80) core->ccr |= M68HC11_CCR_V;
            return finish(core, 2);
        }
        case 0x5c: /* INCB */ {
            uint8_t previous = core->b;
            ++core->b;
            core->ccr &= (uint8_t)~(M68HC11_CCR_N | M68HC11_CCR_Z |
                                    M68HC11_CCR_V);
            if (!core->b) core->ccr |= M68HC11_CCR_Z;
            if (core->b & 0x80) core->ccr |= M68HC11_CCR_N;
            if (previous == 0x7f) core->ccr |= M68HC11_CCR_V;
            return finish(core, 2);
        }
        case 0x53: /* COMB */
            core->b = (uint8_t)~core->b;
            set_nzv8(core, core->b);
            core->ccr |= M68HC11_CCR_C;
            return finish(core, 2);
        case 0x58: { /* ASLB */
            uint8_t previous = core->b;
            core->b = (uint8_t)(core->b << 1);
            core->ccr &= (uint8_t)~(M68HC11_CCR_N | M68HC11_CCR_Z |
                                    M68HC11_CCR_V | M68HC11_CCR_C);
            if (!core->b) core->ccr |= M68HC11_CCR_Z;
            if (core->b & 0x80) core->ccr |= M68HC11_CCR_N;
            if (previous & 0x80) core->ccr |= M68HC11_CCR_C;
            if (((core->ccr & M68HC11_CCR_N) != 0) !=
                ((core->ccr & M68HC11_CCR_C) != 0))
                core->ccr |= M68HC11_CCR_V;
            return finish(core, 2);
        }
        case 0x5d: /* TSTB */
            set_nzv8(core, core->b);
            core->ccr &= (uint8_t)~M68HC11_CCR_C;
            return finish(core, 2);
        case 0x5f: /* CLRB */
            core->b = 0;
            core->ccr &= (uint8_t)~(M68HC11_CCR_N | M68HC11_CCR_V |
                                    M68HC11_CCR_C);
            core->ccr |= M68HC11_CCR_Z;
            return finish(core, 2);
        case 0x6e: /* JMP indexed by X */
            core->pc = (uint16_t)(core->x + fetch8(core));
            return finish(core, 3);
        case 0x7e: { /* JMP extended */
            uint16_t target = fetch16(core);
            core->pc = target;
            return finish(core, 3);
        }
        case 0x7f: { /* CLR extended */
            uint16_t address = fetch16(core);
            write8(core, address, 0);
            core->ccr &= (uint8_t)~(M68HC11_CCR_N | M68HC11_CCR_V |
                                    M68HC11_CCR_C);
            core->ccr |= M68HC11_CCR_Z;
            return finish(core, 5);
        }
        case 0x81: { /* CMPA immediate */
            uint8_t value = fetch8(core);
            set_sub8(core, core->a, value, (uint8_t)(core->a - value));
            return finish(core, 2);
        }
        case 0x84: /* ANDA immediate */
            core->a = (uint8_t)(core->a & fetch8(core));
            set_nzv8(core, core->a);
            return finish(core, 2);
        case 0x8a: /* ORAA immediate */
            core->a = (uint8_t)(core->a | fetch8(core));
            set_nzv8(core, core->a);
            return finish(core, 2);
        case 0x8b: { /* ADDA immediate */
            uint8_t left = core->a;
            uint8_t right = fetch8(core);
            uint8_t result = (uint8_t)(left + right);
            core->a = result;
            set_add8(core, left, right, result);
            return finish(core, 2);
        }
        case 0x8d: { /* BSR */
            int8_t offset = (int8_t)fetch8(core);
            push16(core, core->pc);
            core->pc = (uint16_t)(core->pc + offset);
            return finish(core, 6);
        }
        case 0x8e: /* LDS immediate */
            core->sp = fetch16(core);
            set_nzv16(core, core->sp);
            return finish(core, 3);
        case 0x8c: { /* CPX immediate */
            uint16_t value = fetch16(core);
            set_sub16(core, core->x, value,
                      (uint16_t)(core->x - value));
            return finish(core, 4);
        }
        case 0x8f: { /* XGDX */
            uint16_t d = get_d(core);
            set_d(core, core->x);
            core->x = d;
            return finish(core, 3);
        }
        case 0x86: return load_a(core, fetch8(core), 2); /* LDAA immediate */
        case 0x89: { /* ADCA immediate */
            uint8_t left = core->a;
            uint8_t right = fetch8(core);
            uint8_t carry = (core->ccr & M68HC11_CCR_C) ? 1 : 0;
            uint16_t sum = (uint16_t)left + (uint16_t)right + carry;
            uint8_t result = (uint8_t)sum;
            core->a = result;
            core->ccr &= (uint8_t)~(M68HC11_CCR_H | M68HC11_CCR_N |
                                    M68HC11_CCR_Z | M68HC11_CCR_V |
                                    M68HC11_CCR_C);
            if (((left & 0x0f) + (right & 0x0f) + carry) & 0x10)
                core->ccr |= M68HC11_CCR_H;
            if (!result) core->ccr |= M68HC11_CCR_Z;
            if (result & 0x80) core->ccr |= M68HC11_CCR_N;
            if ((~(left ^ right) & (left ^ result)) & 0x80)
                core->ccr |= M68HC11_CCR_V;
            if (sum & 0x100) core->ccr |= M68HC11_CCR_C;
            return finish(core, 2);
        }
        case 0x96: return load_a(core, core->read8(core->memory_context,
                                                   fetch8(core)), 3);
        case 0xa1: { /* CMPA indexed by X */
            uint16_t address = (uint16_t)(core->x + fetch8(core));
            uint8_t value = core->read8(core->memory_context, address);
            set_sub8(core, core->a, value, (uint8_t)(core->a - value));
            return finish(core, 4);
        }
        case 0xa6: return load_a(core, core->read8(core->memory_context,
                                                   (uint16_t)(core->x + fetch8(core))), 4);
        case 0xb6: return load_a(core, core->read8(core->memory_context,
                                                   fetch16(core)), 4);
        case 0x97: { /* STAA direct */
            uint16_t address = fetch8(core);
            write8(core, address, core->a);
            set_nzv8(core, core->a);
            return finish(core, 3);
        }
        case 0xa7: { /* STAA indexed by X */
            uint16_t address = (uint16_t)(core->x + fetch8(core));
            write8(core, address, core->a);
            set_nzv8(core, core->a);
            return finish(core, 4);
        }
        case 0xb7: { /* STAA extended */
            uint16_t address = fetch16(core);
            write8(core, address, core->a);
            set_nzv8(core, core->a);
            return finish(core, 4);
        }
        case 0xbd: { /* JSR extended */
            uint16_t target = fetch16(core);
            push16(core, core->pc);
            core->pc = target;
            return finish(core, 6);
        }
        case 0xcc: { /* LDD immediate */
            uint16_t value = fetch16(core);
            set_d(core, value);
            set_nzv16(core, value);
            return finish(core, 3);
        }
        case 0xc3: { /* ADDD immediate */
            uint16_t left = get_d(core);
            uint16_t right = fetch16(core);
            uint16_t result = (uint16_t)(left + right);
            set_d(core, result);
            set_add16(core, left, right, result);
            return finish(core, 4);
        }
        case 0xc4: /* ANDB immediate */
            core->b = (uint8_t)(core->b & fetch8(core));
            set_nzv8(core, core->b);
            return finish(core, 2);
        case 0xc1: { /* CMPB immediate */
            uint8_t value = fetch8(core);
            set_sub8(core, core->b, value, (uint8_t)(core->b - value));
            return finish(core, 2);
        }
        case 0xc6: return load_b(core, fetch8(core), 2); /* LDAB immediate */
        case 0xc8: /* EORB immediate */
            core->b = (uint8_t)(core->b ^ fetch8(core));
            set_nzv8(core, core->b);
            return finish(core, 2);
        case 0xd6: return load_b(core, core->read8(core->memory_context,
                                                   fetch8(core)), 3);
        case 0xe6: return load_b(core, core->read8(core->memory_context,
                                                   (uint16_t)(core->x + fetch8(core))), 4);
        case 0xf6: return load_b(core, core->read8(core->memory_context,
                                                   fetch16(core)), 4);
        case 0xd7: { /* STAB direct */
            uint16_t address = fetch8(core);
            write8(core, address, core->b);
            set_nzv8(core, core->b);
            return finish(core, 3);
        }
        case 0xd1: { /* CMPB direct */
            uint8_t value = core->read8(core->memory_context, fetch8(core));
            set_sub8(core, core->b, value, (uint8_t)(core->b - value));
            return finish(core, 3);
        }
        case 0xde: { /* LDX direct */
            uint16_t address = fetch8(core);
            core->x = read16(core, address);
            set_nzv16(core, core->x);
            return finish(core, 4);
        }
        case 0xdf: { /* STX direct */
            uint16_t address = fetch8(core);
            write8(core, address, (uint8_t)(core->x >> 8));
            write8(core, (uint16_t)(address + 1), (uint8_t)core->x);
            set_nzv16(core, core->x);
            return finish(core, 4);
        }
        case 0xe7: { /* STAB indexed by X */
            uint16_t address = (uint16_t)(core->x + fetch8(core));
            write8(core, address, core->b);
            set_nzv8(core, core->b);
            return finish(core, 4);
        }
        case 0xed: { /* STD indexed by X */
            uint16_t address = (uint16_t)(core->x + fetch8(core));
            write8(core, address, core->a);
            write8(core, (uint16_t)(address + 1), core->b);
            set_nzv16(core, get_d(core));
            return finish(core, 5);
        }
        case 0xee: { /* LDX indexed by X */
            uint16_t address = (uint16_t)(core->x + fetch8(core));
            core->x = read16(core, address);
            set_nzv16(core, core->x);
            return finish(core, 5);
        }
        case 0xf7: { /* STAB extended */
            uint16_t address = fetch16(core);
            write8(core, address, core->b);
            set_nzv8(core, core->b);
            return finish(core, 4);
        }
        case 0xfb: { /* ADDB extended */
            uint16_t address = fetch16(core);
            uint8_t left = core->b;
            uint8_t right = core->read8(core->memory_context, address);
            uint8_t result = (uint8_t)(left + right);
            core->b = result;
            set_add8(core, left, right, result);
            return finish(core, 4);
        }
        case 0xfc: { /* LDD extended */
            uint16_t value = read16(core, fetch16(core));
            set_d(core, value);
            set_nzv16(core, value);
            return finish(core, 5);
        }
        case 0xf3: { /* ADDD extended */
            uint16_t address = fetch16(core);
            uint16_t left = get_d(core);
            uint16_t right = read16(core, address);
            uint16_t result = (uint16_t)(left + right);
            set_d(core, result);
            set_add16(core, left, right, result);
            return finish(core, 6);
        }
        case 0xf1: { /* CMPB extended */
            uint8_t value = core->read8(core->memory_context, fetch16(core));
            set_sub8(core, core->b, value, (uint8_t)(core->b - value));
            return finish(core, 4);
        }
        case 0xfd: { /* STD extended */
            uint16_t address = fetch16(core);
            write8(core, address, core->a);
            write8(core, (uint16_t)(address + 1), core->b);
            set_nzv16(core, get_d(core));
            return finish(core, 5);
        }
        case 0xce: /* LDX immediate */
            core->x = fetch16(core);
            set_nzv16(core, core->x);
            return finish(core, 3);
        default:
            core->illegal = 1;
            return 0;
    }
}
