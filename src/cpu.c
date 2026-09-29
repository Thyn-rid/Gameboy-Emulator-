/* cpu.c - Sharp LR35902: fetch, decode, execute, interrupts */
#include <stdio.h>
#include <stdlib.h>
#include "gb.h"

/* r[]: 0=B 1=C 2=D 3=E 4=H 5=L 6=(HL) 7=A */
static u8 r[8], f;
static u16 sp, pc;
static int ime, ei_delay, halted;

#define BC ((u16)((r[0] << 8) | r[1]))
#define DE ((u16)((r[2] << 8) | r[3]))
#define HL ((u16)((r[4] << 8) | r[5]))

static void setHL(u16 v)
{
    r[4] = v >> 8;
    r[5] = v & 0xFF;
}
static u8 R(int i) { return i == 6 ? rb(HL) : r[i]; }
static void W(int i, u8 v)
{
    if (i == 6)
        wb(HL, v);
    else
        r[i] = v;
}

static u8 fetch(void) { return rb(pc++); }
static u16 fetch16(void)
{
    u16 l = fetch();
    u16 h = fetch();
    return l | (h << 8);
}
static void push(u16 v)
{
    wb(--sp, v >> 8);
    wb(--sp, v & 0xFF);
}
static u16 pop(void)
{
    u16 l = rb(sp++);
    u16 h = rb(sp++);
    return l | (h << 8);
}

static u16 RR(int i)
{
    switch (i)
    {
    case 0:
        return BC;
    case 1:
        return DE;
    case 2:
        return HL;
    default:
        return sp;
    }
}
static void setRR(int i, u16 v)
{
    switch (i)
    {
    case 0:
        r[0] = v >> 8;
        r[1] = v & 0xFF;
        break;
    case 1:
        r[2] = v >> 8;
        r[3] = v & 0xFF;
        break;
    case 2:
        setHL(v);
        break;
    default:
        sp = v;
    }
}
static int cond(int cc)
{
    switch (cc)
    {
    case 0:
        return !(f & FZ);
    case 1:
        return f & FZ;
    case 2:
        return !(f & FC);
    default:
        return f & FC;
    }
}

static void alu(int k, u8 v)
{
    u8 a = r[7];
    int c = (f & FC) ? 1 : 0, res;
    switch (k)
    {
    case 0:
    case 1:
    { /* ADD / ADC */
        int cc = k ? c : 0;
        res = a + v + cc;
        f = ((res & 0xFF) == 0 ? FZ : 0) | (((a & 0xF) + (v & 0xF) + cc > 0xF) ? FH : 0) | (res > 0xFF ? FC : 0);
        r[7] = res;
        break;
    }
    case 2:
    case 3:
    case 7:
    { /* SUB / SBC / CP */
        int cc = (k == 3) ? c : 0;
        res = a - v - cc;
        f = FN | ((res & 0xFF) == 0 ? FZ : 0) | (((a & 0xF) - (v & 0xF) - cc < 0) ? FH : 0) | (res < 0 ? FC : 0);
        if (k != 7)
            r[7] = res;
        break;
    }
    case 4:
        r[7] = a & v;
        f = (r[7] ? 0 : FZ) | FH;
        break;
    case 5:
        r[7] = a ^ v;
        f = r[7] ? 0 : FZ;
        break;
    case 6:
        r[7] = a | v;
        f = r[7] ? 0 : FZ;
        break;
    }
}

static int cb_exec(void)
{
    u8 cb = fetch();
    int i = cb & 7, b = (cb >> 3) & 7;
    u8 v = R(i), c;
    switch (cb >> 6)
    {
    case 0:
        switch (b)
        {
        case 0:
            c = v >> 7;
            v = (v << 1) | c;
            break; /* RLC  */
        case 1:
            c = v & 1;
            v = (v >> 1) | (c << 7);
            break; /* RRC  */
        case 2:
            c = v >> 7;
            v = (v << 1) | ((f & FC) ? 1 : 0);
            break; /* RL   */
        case 3:
            c = v & 1;
            v = (v >> 1) | ((f & FC) ? 0x80 : 0);
            break; /* RR   */
        case 4:
            c = v >> 7;
            v <<= 1;
            break; /* SLA  */
        case 5:
            c = v & 1;
            v = (v >> 1) | (v & 0x80);
            break; /* SRA  */
        case 6:
            c = 0;
            v = (v << 4) | (v >> 4);
            break; /* SWAP */
        default:
            c = v & 1;
            v >>= 1;
            break; /* SRL  */
        }
        f = (v ? 0 : FZ) | (c ? FC : 0);
        W(i, v);
        return i == 6 ? 16 : 8;
    case 1: /* BIT  */
        f = (f & FC) | FH | ((v & (1 << b)) ? 0 : FZ);
        return i == 6 ? 12 : 8;
    case 2:
        W(i, v & ~(1 << b));
        return i == 6 ? 16 : 8; /* RES  */
    default:
        W(i, v | (1 << b));
        return i == 6 ? 16 : 8; /* SET  */
    }
}

static int illegal(u8 op)
{
    fprintf(stderr, "Illegal opcode 0x%02X at 0x%04X\n", op, (u16)(pc - 1));
    exit(1);
    return 0;
}

static u16 add_sp_e8(void)
{
    u8 e = fetch();
    u16 res = sp + (s8)e;
    f = (((sp & 0xF) + (e & 0xF)) > 0xF ? FH : 0) | (((sp & 0xFF) + e) > 0xFF ? FC : 0);
    return res;
}

static int exec(void)
{
    u8 op = fetch();
    switch (op >> 6)
    {
    case 1: /* LD r,r / HALT */
        if (op == 0x76)
        {
            halted = 1;
            return 4;
        }
        W((op >> 3) & 7, R(op & 7));
        return (((op & 7) == 6) || (((op >> 3) & 7) == 6)) ? 8 : 4;
    case 2: /* ALU A,r */
        alu((op >> 3) & 7, R(op & 7));
        return (op & 7) == 6 ? 8 : 4;

    case 0:
        switch (op & 7)
        {
        case 0:
            switch (op)
            {
            case 0x00:
                return 4;
            case 0x08:
            {
                u16 a = fetch16();
                wb(a, sp & 0xFF);
                wb(a + 1, sp >> 8);
                return 20;
            }
            case 0x10:
                fetch();
                return 4; /* STOP */
            case 0x18:
            {
                s8 o = fetch();
                pc += o;
                return 12;
            }
            default:
            {
                s8 o = fetch();
                if (cond((op >> 3) & 3))
                {
                    pc += o;
                    return 12;
                }
                return 8;
            }
            }
        case 1:
        {
            int i = (op >> 4) & 3;
            if (!(op & 8))
            {
                setRR(i, fetch16());
                return 12;
            }
            u16 hl = HL, v = RR(i);
            u32 res = hl + v; /* ADD HL,rr */
            f = (f & FZ) | (((hl & 0xFFF) + (v & 0xFFF)) > 0xFFF ? FH : 0) | (res > 0xFFFF ? FC : 0);
            setHL(res);
            return 8;
        }
        case 2:
        {
            int i = (op >> 4) & 3;
            u16 a;
            if (i == 0)
                a = BC;
            else if (i == 1)
                a = DE;
            else
            {
                a = HL;
                setHL(a + (i == 2 ? 1 : -1));
            }
            if (op & 8)
                r[7] = rb(a);
            else
                wb(a, r[7]);
            return 8;
        }
        case 3:
        {
            int i = (op >> 4) & 3;
            setRR(i, RR(i) + ((op & 8) ? -1 : 1));
            return 8;
        }
        case 4:
        { /* INC r */
            int i = (op >> 3) & 7;
            u8 v = R(i), res = v + 1;
            f = (f & FC) | (res ? 0 : FZ) | ((v & 0xF) == 0xF ? FH : 0);
            W(i, res);
            return i == 6 ? 12 : 4;
        }
        case 5:
        { /* DEC r */
            int i = (op >> 3) & 7;
            u8 v = R(i), res = v - 1;
            f = (f & FC) | FN | (res ? 0 : FZ) | ((v & 0xF) == 0 ? FH : 0);
            W(i, res);
            return i == 6 ? 12 : 4;
        }
        case 6:
        { /* LD r,d8 */
            int i = (op >> 3) & 7;
            W(i, fetch());
            return i == 6 ? 12 : 8;
        }
        default:
            switch (op)
            {
            case 0x07:
            {
                u8 c = r[7] >> 7;
                r[7] = (r[7] << 1) | c;
                f = c ? FC : 0;
                return 4;
            }
            case 0x0F:
            {
                u8 c = r[7] & 1;
                r[7] = (r[7] >> 1) | (c << 7);
                f = c ? FC : 0;
                return 4;
            }
            case 0x17:
            {
                u8 c = r[7] >> 7;
                r[7] = (r[7] << 1) | ((f & FC) ? 1 : 0);
                f = c ? FC : 0;
                return 4;
            }
            case 0x1F:
            {
                u8 c = r[7] & 1;
                r[7] = (r[7] >> 1) | ((f & FC) ? 0x80 : 0);
                f = c ? FC : 0;
                return 4;
            }
            case 0x27:
            { /* DAA */
                int a = r[7];
                if (!(f & FN))
                {
                    if ((f & FC) || a > 0x99)
                    {
                        a += 0x60;
                        f |= FC;
                    }
                    if ((f & FH) || (a & 0xF) > 9)
                        a += 6;
                }
                else
                {
                    if (f & FC)
                        a -= 0x60;
                    if (f & FH)
                        a -= 6;
                }
                f &= ~(FZ | FH);
                if ((a & 0xFF) == 0)
                    f |= FZ;
                r[7] = a;
                return 4;
            }
            case 0x2F:
                r[7] = ~r[7];
                f |= FN | FH;
                return 4; /* CPL */
            case 0x37:
                f = (f & FZ) | FC;
                return 4; /* SCF */
            default:
                f = (f & FZ) | ((f & FC) ? 0 : FC);
                return 4; /* CCF */
            }
        }

    default: /* case 3: 0xC0 - 0xFF */
        switch (op & 7)
        {
        case 0:
            if (op < 0xE0)
            {
                if (cond((op >> 3) & 3))
                {
                    pc = pop();
                    return 20;
                }
                return 8;
            }
            if (op == 0xE0)
            {
                wb(0xFF00 + fetch(), r[7]);
                return 12;
            }
            if (op == 0xF0)
            {
                r[7] = rb(0xFF00 + fetch());
                return 12;
            }
            if (op == 0xE8)
            {
                sp = add_sp_e8();
                return 16;
            }
            setHL(add_sp_e8());
            return 12; /* 0xF8 LD HL,SP+e8 */
        case 1:
            if (!(op & 8))
            { /* POP */
                u16 v = pop();
                switch ((op >> 4) & 3)
                {
                case 0:
                    r[0] = v >> 8;
                    r[1] = v & 0xFF;
                    break;
                case 1:
                    r[2] = v >> 8;
                    r[3] = v & 0xFF;
                    break;
                case 2:
                    setHL(v);
                    break;
                default:
                    r[7] = v >> 8;
                    f = v & 0xF0;
                }
                return 12;
            }
            switch (op)
            {
            case 0xC9:
                pc = pop();
                return 16;
            case 0xD9:
                pc = pop();
                ime = 1;
                return 16;
            case 0xE9:
                pc = HL;
                return 4;
            default:
                sp = HL;
                return 8;
            }
        case 2:
            if (op < 0xE0)
            {
                u16 a = fetch16();
                if (cond((op >> 3) & 3))
                {
                    pc = a;
                    return 16;
                }
                return 12;
            }
            switch (op)
            {
            case 0xE2:
                wb(0xFF00 + r[1], r[7]);
                return 8;
            case 0xEA:
                wb(fetch16(), r[7]);
                return 16;
            case 0xF2:
                r[7] = rb(0xFF00 + r[1]);
                return 8;
            case 0xFA:
                r[7] = rb(fetch16());
                return 16;
            default:
                return illegal(op);
            }
        case 3:
            switch (op)
            {
            case 0xC3:
                pc = fetch16();
                return 16;
            case 0xCB:
                return cb_exec();
            case 0xF3:
                ime = 0;
                ei_delay = 0;
                return 4;
            case 0xFB:
                ei_delay = 2;
                return 4;
            default:
                return illegal(op);
            }
        case 4:
            if (op < 0xE0)
            {
                u16 a = fetch16();
                if (cond((op >> 3) & 3))
                {
                    push(pc);
                    pc = a;
                    return 24;
                }
                return 12;
            }
            return illegal(op);
        case 5:
            if (!(op & 8))
            { /* PUSH */
                u16 v;
                switch ((op >> 4) & 3)
                {
                case 0:
                    v = BC;
                    break;
                case 1:
                    v = DE;
                    break;
                case 2:
                    v = HL;
                    break;
                default:
                    v = (r[7] << 8) | f;
                }
                push(v);
                return 16;
            }
            if (op == 0xCD)
            {
                u16 a = fetch16();
                push(pc);
                pc = a;
                return 24;
            }
            return illegal(op);
        case 6:
            alu((op >> 3) & 7, fetch());
            return 8;
        default:
            push(pc);
            pc = op & 0x38;
            return 16; /* RST */
        }
    }
}

int cpu_step(void)
{
    int c;
    u8 pend = io[0x0F] & ie & 0x1F;
    if (pend)
        halted = 0;
    if (pend && ime)
    {
        int i = 0;
        while (!(pend & (1 << i)))
            i++;
        ime = 0;
        io[0x0F] &= ~(1 << i);
        push(pc);
        pc = 0x40 + i * 8;
        c = 20;
    }
    else if (halted)
        c = 4;
    else
        c = exec();
    if (ei_delay && --ei_delay == 0)
        ime = 1;
    timer_tick(c);
    ppu_tick(c);
    return c;
}

void cpu_reset(void)
{ /* post-boot register values */
    r[7] = 0x01;
    f = 0xB0;
    r[0] = 0x00;
    r[1] = 0x13;
    r[2] = 0x00;
    r[3] = 0xD8;
    r[4] = 0x01;
    r[5] = 0x4D;
    sp = 0xFFFE;
    pc = 0x0100;
}
