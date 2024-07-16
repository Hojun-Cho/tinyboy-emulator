#include "gb.h"

static int
mbc0(int a, int _);

u8* rom;
u8 *romb, *vramb, *wramb, *eramb;
u8 wram[32768], vram[16384], oam[256], reg[256];
u8* back;
u8 palm[128];
u32 pal[64];
int nrom, nback, nbackbank;
u32 divclock;
int prish;
u8 dma;
u32 moncols[4];
u32 white;
int (*mappers[])(int, int) = { mbc0 };
int (*mapper)(int, int);

static u8
regread(u16 a)
{
  u8 v;

  switch (a) {
    case JOYP:
      v = 0xff;
      if ((reg[a] & 0x10) == 0)
        v &= 0xf0 | ~keys;
      if ((reg[a] & 0x20) == 0)
        v &= 0xf0 | ~(keys >> 4);
      return v;
    case DIV:
      return reg[DIV] + (clock - divclock >> 7 - ((mode & TURBO) != 0));
    case STAT:
      return reg[a] & 0xf8 | (reg[LYC] == ppuy) << 2 | ppustate;
    case LY:
      return ppuy;
    default:
      return reg[a];
  }
}

static void
regwrite(u16 a, u8 v)
{
  int i;

  switch (a) {
    case JOYP:
			v |= 0xCF;/* all disabled */
      break;
    case DIV:
      divclock = clock;
      v = 0;
      break;
    case STAT:
      v |= 0x80;
      if ((v & IRQLYC) != 0 && ppuy == reg[LYC])
        reg[IF] |= IRQLCDS;
      break;
    case LYC:
      if ((reg[STAT] & IRQLYC) != 0 && ppuy == v)
        reg[IF] |= IRQLCDS;
      break;
    case LCDC:
      ppusync();
      if ((~v & reg[a] & LCDEN) != 0) {
        ppuy = 0;
        ppuw = 0;
        ppustate = 0;
        delevent(&evhblank);
      }
      if ((v & ~reg[a] & LCDEN) != 0)
        addevent(&evhblank, 456 * 2);
      break;
    case SCY:
    case SCX:
    case WY:
    case WX:
      ppusync();
      break;
    case BGP:
    case OBP0:
    case OBP1:
      ppusync();
      i = a - BGP << 2;
      pal[i] = moncols[~v & 3];
      pal[i + 1] = moncols[~v >> 2 & 3];
      pal[i + 2] = moncols[~v >> 4 & 3];
      pal[i + 3] = moncols[~v >> 6 & 3];
      break;
    case IE:
      v &= 0x1f;
      break;
    case DMA:
      for (i = 0; i < 160; ++i)
        oam[i] = memread((u16)v << 8 | i);
      break;
    default:
      if (a >= 0x80)
        break;
      v = 0xff;
  }
  reg[a] = v;
}

u8
memread(u16 a)
{
  switch (a >> 12) {
    case 0:
    case 1:
    case 2:
    case 3:
      return rom[a];
    case 4:
    case 5:
    case 6:
    case 7:
      return romb[a - 0x4000];
    case 8:
    case 9:
      return vramb[a - 0x8000];
    case 10:
    case 11:
      if (eramb != nil)
        return eramb[a - 0xa000];
      return mapper(READ, a);
    case 12:
    case 14:
      return wram[a & 0xFFF];
    case 13:
      return wramb[a & 0xFFF];
    case 15:
      if (a >= 0xFF00)
        return regread(a - 0xFF00);
      else if (a >= 0xFE00)
        return oam[a - 0xFE00];
  }
  return 0xFF;
}

void
memwrite(u16 a, u8 v)
{
  switch (a >> 12) {
    case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
      mapper(a, v);
      return;
    case 8: case 9:
      vramb[a - 0x8000] = v;
      return;
    case 10: case 11:
      if (eramb != nil)
        eramb[a - 0xa000] = v;
      else
        mapper(a, v);
      return;
    case 12: case 14:
      wram[a & 0xFFF] = v;
      return;
    case 13:
      wramb[a & 0xFFF] = v;
      return;
    case 15:
      if (a >= 0xFF00)
        regwrite(a - 0xFF00, v);
      else if (a >= 0xFE00)
        oam[a - 0xFE00] = v;
      return;
  }
}

static int
mbc0(int a, int _)
{
  if (a >= 0)
    return 0;
  switch (a) {
    case INIT:
      return 0;
    case SAVE:
    case RSTR:
      return 0;
    case READ:
      return -1;
    default:
      panic("MBC0 does not have function of %d", a);
  }
  return 0;
}

void
meminit(void)
{
  union
  {
    u8 c[4];
    u32 l;
  } c;

  c.c[0] = c.c[1] = c.c[2] = 0;
  c.c[3] = 1;
  for (; c.l != 1; prish++)
    c.l >>= 1;
  c.c[0] = c.c[1] = c.c[2] = 0xff;
  c.c[3] = 0;
  white = c.l;

  romb = rom + 0x4000;
  wramb = wram + 0x1000;
  vramb = vram;
  mapper = mappers[mbc];
  mapper(INIT, 0);

  reg[LCDC] = 0x91;
  reg[IF] = 0xE0;
  reg[SVBK] = 0xff;
  reg[VBK] = 0xff;
}
