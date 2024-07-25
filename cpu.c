#include "gb.h"
#include "print.h"

#define BC() ((u16)r[rB] << 8 | r[rC])
#define DE() ((u16)r[rD] << 8 | r[rE])
#define HL() ((u16)r[rH] << 8 | r[rL])

enum
{
  FLAGC = 0x10,
  FLAGH = 0x20,
  FLAGN = 0x40,
  FLAGZ = 0x80,
};

enum
{
  rB,
  rC,
  rD,
  rE,
  rH,
  rL,
  rHL,
  rA,
  rF = rHL,
};

u8 r[8], ime;
u16 pc, sp, curpc;
int halt;

Var cpuvars[] = {ARR(r), VAR(ime), VAR(pc), VAR(curpc), VAR(sp), VAR(halt),
									{nil, 0, 0}};

void
state(void)
{
  print("A:%02X F:%02X B:%02X C:%02X D:%02X E:%02X H:%02X L:%02X SP:%04X "
         "PC:%04X PCMEM:%02X,%02X,%02X,%02X\n",
         r[rA],
         r[rF],
         r[rB],
         r[rC],
         r[rD],
         r[rE],
         r[rH],
         r[rL],
         sp,
         pc - 1,
         memread(pc - 1),
         memread(pc),
         memread(pc + 1),
         memread(pc + 2));
}

static u8
fetch8(void)
{
  return memread(pc++);
}

static u16
fetch16(void)
{
  u16 u = memread(pc++);
  return u | memread(pc++) << 8;
}

static void
push8(u8 u)
{
  memwrite(--sp, u);
}

static void
push16(u16 u)
{
  memwrite(--sp, u >> 8);
  memwrite(--sp, u);
}

static u8
pop8(void)
{
  return memread(sp++);
}

static u16
pop16(void)
{
  u16 v = memread(sp++);
  return v | memread(sp++) << 8;
}

static void
write16(u16 n, u16 v)
{
  memwrite(n++, v);
  memwrite(n, v >> 8);
}

static int
move(u8 dst, u8 src)
{
  if (dst == rHL) {
    if (src == rHL) {
      halt = 1;
      return 4;
    }
    memwrite(HL(), r[src]);
    return 8;
  }
  if (src == rHL) {
    r[dst] = memread(HL());
    return 8;
  }
  r[dst] = r[src];
  return 4;
}

static int
alu(u8 op, u8 n)
{
  u8 v4, u;
  u16 v = 0;
  int t;

  switch (n) {
  case 8:
    u = fetch8();
    t = 8;
    break;
  case rHL:
    u = memread(HL());
    t = 8;
    break;
  default:
    u = r[n];
    t = 4;
    break;
  }
  switch (op) {
  default:
    v4 = (r[rA] & 0x0F) + (u & 0x0F);
    v = r[rA] + u;
    break;
  case 1:
    v4 = (r[rA] & 0x0F) + (u & 0x0F) + (r[rF] >> 4 & 1);
    v = r[rA] + u + (r[rF] >> 4 & 1);
    break;
  case 2:
  case 7:
    v4 = (r[rA] & 0x0F) + (~u & 0x0F) + 1;
    v = r[rA] + (u ^ 0xFF) + 1;
    break;
  case 3:
    v4 = (r[rA] & 0x0F) + (~u & 0x0F) + (~r[rF] >> 4 & 1);
    v = r[rA] + (u ^ 0xFF) + (~r[rF] >> 4 & 1);
    break;
  case 4:
    v = r[rA] & u;
    break;
  case 5:
    v = r[rA] ^ u;
    break;
  case 6:
    v = r[rA] | u;
    break;
  }
  r[rF] = 0;
  if ((u8)v == 0)
    r[rF] |= FLAGZ;
  if (op < 2) {
    if ((v & 0x100) != 0)
      r[rF] |= FLAGC;
    if ((v4 & 0x10) != 0)
      r[rF] |= FLAGH;
  } else if (op < 4 || op == 7) {
    r[rF] |= FLAGN;
    if ((v & 0x100) == 0)
      r[rF] |= FLAGC;
    if ((v4 & 0x10) == 0)
      r[rF] |= FLAGH;
  } else if (op == 4) {
    r[rF] |= FLAGH;
  }
  if (op != 7)
    r[rA] = v;
  return t;
}

static int
branch(int cc, int t)
{
  u16 v = (i8)fetch8();
  if (cc == 0)
    return t + 8;
  pc += v;
  return t + 12;
}

static u8
inc(u8 v)
{
  r[rF] &= FLAGC;
  ++v;
  if (v == 0)
    r[rF] |= FLAGZ;
  if ((v & 0xF) == 0)
    r[rF] |= FLAGH;
  return v;
}

static u8
dec(u8 v)
{
  r[rF] = (r[rF] & FLAGC) | FLAGN;
  --v;
  if (v == 0)
    r[rF] |= FLAGZ;
  if ((v & 0xF) == 0xF)
    r[rF] |= FLAGH;
  return v;
}

static u8
addhl(u16 u)
{
  r[rF] &= ~(FLAGN | FLAGC | FLAGH);
  u32 v = HL() + u;
  if ((v & 0x10000) != 0)
    r[rF] |= FLAGC;
  if ((HL() & 0xFFF) + (u & 0xFFF) >= 0x1000)
    r[rF] |= FLAGH;
  r[rL] = v;
  r[rH] = v >> 8;
  return 8;
}

static int
jump(int cc)
{
  u16 v = fetch16();
  if (cc == 0)
    return 12;
  pc = v;
  return 16;
}

static int
call(u16 a, int cc)
{
  if (cc == 0)
    return 12;
  push16(pc);
  pc = a;
  if (cc < 0)
    return 16;
  return 24;
}

static int
bits(void)
{
  u8 op = fetch8();
  u8 n = op & 7;
  u8 m = op >> 3 & 7;
  u16 a = HL();
  u8 v;
  int t;

  if (n == 6) {
    v = memread(a);
    t = 16;
  } else {
    v = r[n];
    t = 8;
  }
  u8 c;
  switch (op >> 6) {
    case 0:
      c = r[rF] >> 4 & 1;
      switch (m) {
        default:
          r[rF] = v >> 3 & 0x10;
          v = v << 1 | v >> 7;
          break;
        case 1:
          r[rF] = v << 4 & 0x10;
          v = v >> 1 | v << 7;
          break;
        case 2:
          r[rF] = v >> 3 & 0x10;
          v = v << 1 | c;
          break;
        case 3:
          r[rF] = v << 4 & 0x10;
          v = v >> 1 | c << 7;
          break;
        case 4:
          r[rF] = v >> 3 & 0x10;
          v = v << 1;
          break;
        case 5:
          r[rF] = v << 4 & 0x10;
          v = (v & 0x80) | v >> 1;
          break;
        case 6:
          r[rF] = 0;
          v = v << 4 | v >> 4;
          break;
        case 7:
          r[rF] = v << 4 & 0x10;
          v >>= 1;
          break;
      }
      if (v == 0)
        r[rF] |= FLAGZ;
      break;
    case 1:
      r[rF] = (r[rF] & ~(FLAGN | FLAGZ)) | FLAGH;
      if ((v & 1 << m) == 0)
        r[rF] |= FLAGZ;
      if (n == 6)
        t = 12;
      return t;
    case 2:
      v &= ~(1 << m);
      break;
    case 3:
      v |= (1 << m);
  }
  if (n == 6) {
    memwrite(a, v);
  } else {
    r[n] = v;
  }
  return t;
}

void
reset(void)
{
  r[rA] = 0x01;
  r[rF] = 0xb0;
  r[rC] = 0x13;
  r[rE] = 0xd8;
  r[rL] = 0x4d;
  r[rH] = 0x01;
  sp = 0xfffe;
  pc = 0x100;
}

int
step(void)
{
  u8 op, v4;
  u16 v, w;
  i8 s;

  if (halt) {
    if (reg[IF] & reg[IE])
      halt = 0;
    else
      return 4;
  }
  if ((reg[IF] & reg[IE]) != 0 && ime != 0) {
    push16(pc);
    ime = 0;
    v4 = reg[IF] & reg[IE];
    v4 &= -v4;
    reg[IF] &= ~v4;
    for (pc = 0x40; v4 != 1; pc += 8)
      v4 >>= 1;
    /* 2nop + push + changepc */
    return 20;
  }
  curpc = pc;
  op = fetch8();
  switch (op >> 6) {
    case 1:
      return move(op >> 3 & 7, op & 7);
    case 2:
      return alu(op >> 3 & 7, op & 7);
  }
  switch (op) {
  default: state(); panic("Unkown opcode OPCODE:[%X]", op);
  case 0x00: return 4;
  case 0x10: return 4;
  case 0x20: return branch((r[rF] & FLAGZ) == 0, 0);
  case 0x30: return branch((r[rF] & FLAGC) == 0, 0);
  case 0x01:
    r[rC] = fetch8();
    r[rB] = fetch8();
    return 12;
  case 0x11:
    r[rE] = fetch8();
    r[rD] = fetch8();
    return 12;
  case 0x21:
    r[rL] = fetch8();
    r[rH] = fetch8();
    return 12;
  case 0x31: sp = fetch16(); return 12;
  case 0x02: memwrite(BC(), r[rA]); return 8;
  case 0x12: memwrite(DE(), r[rA]); return 8;
  case 0x22:
    memwrite(HL(), r[rA]);
    if (++r[rL] == 0)
      r[rH]++;
    return 8;
  case 0x32:
    memwrite(HL(), r[rA]);
    if (r[rL]-- == 0)
      r[rH]--;
    return 8;
  case 0x03:
    if (++r[rC] == 0)
      r[rB]++;
    return 8;
  case 0x13:
    if (++r[rE] == 0)
      r[rD]++;
    return 8;
  case 0x23:
    if (++r[rL] == 0)
      r[rH]++;
    return 8;
  case 0x33: ++sp; return 8;
  case 0x04: inc(r[rB]++); return 4;
  case 0x14: inc(r[rD]++); return 4;
  case 0x24: inc(r[rH]++); return 4;
  case 0x34: memwrite(HL(), inc(memread(HL()))); return 12;
  case 0x05: dec(r[rB]--); return 4;
  case 0x15: dec(r[rD]--); return 4;
  case 0x25: dec(r[rH]--); return 4;
  case 0x35: memwrite(HL(), dec(memread(HL()))); return 12;
  case 0x06: r[rB] = fetch8(); return 8;
  case 0x16: r[rD] = fetch8(); return 8;
  case 0x26: r[rH] = fetch8(); return 8;
  case 0x36: memwrite(HL(), fetch8()); return 12;
  case 0x07:
    r[rF] = r[rA] >> 3 & 0x10;
    r[rA] = r[rA] << 1 | r[rA] >> 7;
    return 4;
  case 0x17:
    v = r[rF] >> 4 & 1;
    r[rF] = r[rA] >> 3 & 0x10;
    r[rA] = r[rA] << 1 | v;
    return 4;
  case 0x27:
    if ((r[rA] > 0x99 && (r[rF] & FLAGN) == 0) || (r[rF] & FLAGC) != 0) {
      r[rF] |= FLAGC;
      v = 0x60;
    } else {
      r[rF] &= ~FLAGC;
      v = 0;
    }
    if (((r[rA] & 0xF) > 9 && (r[rF] & FLAGN) == 0) || (r[rF] & FLAGH) != 0) {
      v |= 6;
    }
    if ((r[rF] & FLAGN) != 0) {
      r[rA] -= v;
    } else {
      r[rA] += v;
    }
    r[rF] &= ~(FLAGZ | FLAGH);
    if (r[rA] == 0)
      r[rF] |= FLAGZ;
    return 4;
  case 0x37: r[rF] = (r[rF] & ~(FLAGN | FLAGH)) | FLAGC; return 4;
  case 0x08: write16(fetch16(), sp); return 20;
  case 0x18: return branch(1, 0);
  case 0x28: return branch((r[rF] & FLAGZ) != 0, 0);
  case 0x38: return branch((r[rF] & FLAGC) != 0, 0);
  case 0x09: return addhl(BC());
  case 0x19: return addhl(DE());
  case 0x29: return addhl(HL());
  case 0x39: return addhl(sp);
  case 0x0A: r[rA] = memread(BC()); return 8;
  case 0x1A: r[rA] = memread(DE()); return 8;
  case 0x2A:
    r[rA] = memread(HL());
    if (++r[rL] == 0)
      r[rH]++;
    return 8;
  case 0x3A:
    r[rA] = memread(HL());
    if (r[rL]-- == 0)
      r[rH]--;
    return 8;
  case 0x0b:
    if (r[rC]-- == 0)
      r[rB]--;
    return 8;
  case 0x1b:
    if (r[rE]-- == 0)
      r[rD]--;
    return 8;
  case 0x2b:
    if (r[rL]-- == 0)
      r[rH]--;
    return 8;
  case 0x3b: sp--; return 8;
  case 0x0C: inc(r[rC]++); return 4;
  case 0x1C: inc(r[rE]++); return 4;
  case 0x2C: inc(r[rL]++); return 4;
  case 0x3C: inc(r[rA]++); return 4;
  case 0x0d: dec(r[rC]--); return 4;
  case 0x1d: dec(r[rE]--); return 4;
  case 0x2d: dec(r[rL]--); return 4;
  case 0x3d: dec(r[rA]--); return 4;
	case 0x0e: r[rC] = fetch8(); return 8;
  case 0x1e: r[rE] = fetch8(); return 8;
  case 0x2e: r[rL] = fetch8(); return 8;
  case 0x3e: r[rA] = fetch8(); return 8;
  case 0x0F:
    r[rF] = r[rA] << 4 & 0x10;
    r[rA] = r[rA] >> 1 | r[rA] << 7;
    return 4;
  case 0x1F:
    v = r[rF] << 3 & 0x80;
    r[rF] = r[rA] << 4 & 0x10;
    r[rA] = r[rA] >> 1 | v;
    return 4;
  case 0x2F:
    r[rF] |= FLAGN | FLAGH;
    r[rA] ^= 0xFF;
    return 4;
  case 0x3F:
    r[rF] = (r[rF] & ~(FLAGN | FLAGH)) ^ FLAGC;
    return 4;
  case 0xc0:
    if ((r[rF] & FLAGZ) == 0) {
      pc = pop16();
      return 20;
    }
    return 8;
  case 0xD0:
    if ((r[rF] & FLAGC) == 0) {
      pc = pop16();
      return 20;
    }
    return 8;
  case 0xE0: memwrite(0xFF00 | fetch8(), r[rA]); return 12;
  case 0xF0: r[rA] = memread(0xFF00 | fetch8()); return 12;
  case 0xC1:
    r[rC] = pop8();
    r[rB] = pop8();
    return 12;
  case 0xd1:
    r[rE] = pop8();
    r[rD] = pop8();
    return 12;
  case 0xe1:
    r[rL] = pop8();
    r[rH] = pop8();
    return 12;
  case 0xf1:
    r[rF] = pop8() & 0xF0;
    r[rA] = pop8();
    return 12;
  case 0xC2: return jump((r[rF] & FLAGZ) == 0);
  case 0xD2: return jump((r[rF] & FLAGC) == 0);
  case 0xE2: memwrite(0xFF00 | r[rC], r[rA]); return 8;
  case 0xF2: r[rA] = memread(0xFF00 | r[rC]); return 8;
  case 0xC3: return jump(1);
  case 0xF3: ime = 0; return 4;
  case 0xC4: return call(fetch16(), (r[rF] & FLAGZ) == 0);
	case 0xD4: return call(fetch16(), (r[rF] & FLAGC) == 0);
  case 0xC5:
    push8(r[rB]);
    push8(r[rC]);
    return 16;
  case 0xD5:
    push8(r[rD]);
    push8(r[rE]);
    return 16;
  case 0xE5:
    push8(r[rH]);
    push8(r[rL]);
    return 16;
  case 0xF5:
    push8(r[rA]);
    push8(r[rF]);
    return 16;
  case 0xC6: return alu(0, 8);
  case 0xD6: return alu(2, 8);
  case 0xE6: return alu(4, 8);
  case 0xF6: return alu(6, 8);
  case 0xC7: return call(0x00, -1);
  case 0xD7: return call(0x10, -1);
  case 0xE7: return call(0x20, -1);
  case 0xF7: return call(0x30, -1);
  case 0xC8:
    if ((r[rF] & FLAGZ) != 0) {
      pc = pop16();
      return 20;
    }
    return 8;
  case 0xD8:
    if ((r[rF] & FLAGC) != 0) {
      pc = pop16();
      return 20;
    }
    return 8;
  case 0xE8: case 0xF8:
    s = fetch8();
    v = sp + s;
    v4 = (sp & 0xF) + (s & 0xF);
    w = (sp & 0xFF) + (s & 0xFF);
    r[rF] = 0;
    if (v4 >= 0x10)
      r[rF] |= FLAGH;
    if (w >= 0x100)
      r[rF] |= FLAGC;
    if (op == 0xE8) {
      sp = v;
      return 16;
    } else {
      r[rL] = v;
      r[rH] = v >> 8;
      return 12;
    }
  case 0xC9: pc = pop16(); return 16;
  case 0xD9:
    pc = pop16();
    ime = 1;
    return 16;
  case 0xE9: pc = HL(); return 4;
  case 0xF9: sp = HL(); return 8;
  case 0xCA: return jump((r[rF] & FLAGZ) != 0);
  case 0xDA: return jump((r[rF] & FLAGC) != 0);
  case 0xEA: memwrite(fetch16(), r[rA]); return 16;
  case 0xFA: r[rA] = memread(fetch16()); return 16;
  case 0xCB: return bits();
  case 0xFB: ime = 1; return 4;
  case 0xCC: return call(fetch16(), (r[rF] & FLAGZ) != 0);
  case 0xDC: return call(fetch16(), (r[rF] & FLAGC) != 0);
  case 0xCD: return call(fetch16(), 1);
  case 0xCE: return alu(1, 8);
  case 0xDE: return alu(3, 8);
  case 0xEE: return alu(5, 8);
  case 0xFE: return alu(7, 8);
  case 0xCF: return call(0x08, -1);
  case 0xDF: return call(0x18, -1);
  case 0xEF: return call(0x28, -1);
  case 0xFF: return call(0x38, -1);
  }
}
