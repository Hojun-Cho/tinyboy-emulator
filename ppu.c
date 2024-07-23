#include "gb.h"
#include <string.h>

#define ryield() {if(psetjmp(renderjmp) == 0) plongjmp(mainjmp, 1);}
#define myield() {if(psetjmp(mainjmp) == 0) plongjmp(renderjmp, 1);}
#define eat(nc)                                                                \
  if (cyc <= nc) {                                                             \
    for (i = 0; i < nc; i++)                                                   \
      if (--cyc == 0)                                                          \
        ryield();                                                           \
  } else                                                                       \
    cyc -= nc;

typedef void *jmp_buf[10];
typedef struct sprite sprite;
struct sprite
{
  u8 dy, x, t;
  u8 pal;
  u16 chr;
};

enum
{
  SPRPRI = 0x80,
  SPRYFL = 0x40,
  SPRXFL = 0x20,
  SPRPAL = 0x10,
  SPRBANK = 0x08,

  TILCOL0 = 0x01,
  TILPRI = 0x02,
  TILSPR = 0x04,
};

jmp_buf mainjmp,renderjmp;
u8 ppustate, ppuy, ppuw;
u64 hblclock, rendclock;
static int cyc, ppux, ppux0;
sprite spr[10], *sprm;

Var ppuvars[] = {VAR(ppustate), VAR(ppuy), VAR(hblclock), VAR(rendclock),
									{nil, 0, 0}};

extern int psetjmp(jmp_buf buf);
extern void plongjmp(jmp_buf buf, int v);

static void
ppurender(void)
{
  int x, y, i, n, m, win;
  u16 ta, ca, chr;
  u8 tile;
  u32 sr[8], *picp;

	ryield();

  for (;;) {
    eat(6 * 2);
    m = 168 + (reg[SCX] & 7);
    win = 0;
    if (reg[LCDC] & WINEN && ppuy >= reg[WY] && reg[WX] <= 166) {
      if (reg[WX] == 0)
        m = 7;
      else if (reg[WX] == 166)
        m = 0;
      else {
        m = reg[WX] + (reg[SCX] & 7) + 1;
        win = -1;
      }
    }
    ppux0 = ppux = 0;
    picp = (u32*)pic + ppuy * PICW;
    y = ppuy + reg[SCY] << 1 & 14;
    ta = 0x1800 | reg[LCDC] << 7 & 0x400 | ppuy + reg[SCY] << 2 & 0x3e0 |
         reg[SCX] >> 3;
    x = -(reg[SCX] & 7);
  restart:
    do {
      tile = vram[ta];
      if (reg[LCDC] & BGTILE)
        ca = ((u16)tile << 4) + y;
      else
        ca = 0x1000 + (((i32)tile << 24) >> 20) + y;
      chr = (u16)vram[ca] << 8 | vram[ca + 1];
      for (i = 0; i < 8; ++i) {
        sr[i] = pal[chr >> 15 | chr >> 6 & 2] | ((chr & 0x8080) == 0) << prish;
        chr <<= 1;
      }
      if (cyc <= 2 * 8) {
        for (i = 0; i < 2 * 8; ++i)
          if (--cyc == 0)
            ryield();
        y = ppuy + reg[SCY] << 1 & 14;
        ta = 0x1800 | reg[LCDC] << 7 & 0x400 | ppuy + reg[SCY] << 2 & 0x3E0 |
             ta & 0x1F;
      } else
        cyc -= 2 * 8;
      m -= 8;
      n = m < 8 ? m : 8;
      if ((ta & 0x1F) == 0x1F)
        ta &= ~0x1F;
      else
        ta++;
      for (i = 0; i < n; ++i, ++x) {
        if (x >= 0)
          picp[x] = sr[i];
      }
      ppux = x;
    } while (m > 8);
    if (win == -1) {
      win = 1;
      ta = 0x1800 | reg[LCDC] << 4 & 0x400 | ppuw - reg[WY] << 2 & 0x3E0;
      y = ppuw - reg[WY] << 1 & 14;
      cyc += 12;
      m = 175 - reg[WX];
      goto restart;
    }
    ryield();
  }
}

static void
oamsearch(void)
{
  u8* p;
  sprite* q;
  int y0, sy;
  u8 t, tn;
  u8* chrp;

  y0 = ppuy + 16;
  sy = (reg[LCDC] & SPR16) != 0 ? 16 : 8;
  sprm = spr;
  if ((reg[LCDC] & SPREN) == 0)
    return;
  for (p = oam; p < oam + 160; p += 4) {
    if ((u8)(y0 - p[0]) >= sy)
      continue;
    for (q = spr; q < sprm; q++) {
      if (q->x > p[1]) {
        if (sprm != spr + 10) {
          memmove(q + 1, q, (sprm - q) * sizeof(sprite));
          ++sprm;
        } else
          memmove(q + 1, q, (sprm - 1 - q) * sizeof(sprite));
        goto found;
      }
    }
    if (q == spr + 10)
      continue;
    ++sprm;
  found:
    q->dy = y0 - p[0];
    q->x = p[1];
    q->t = t = p[3];
    if ((t & SPRYFL) != 0)
      q->dy ^= sy - 1;
    tn = p[2];
    if (sy == 16)
      tn = tn & ~1 | q->dy >> 3;
    chrp = vram + ((u16)tn << 4 | q->dy << 1 & 14);
    q->pal = 4 + (t >> 2 & 4);
    q->chr = (u16)chrp[0] << 8 | chrp[1];
    if (p[1] < 8) {
      if ((t & SPRXFL) != 0)
        q->chr >>= 8 - p[1];
      else
        q->chr <<= 8 - p[1];
    }
  }
}

static void
sprites(void)
{
  sprite* q;
  u8 attr;
  u32* picp;
  int x, x1;
  u16 chr;

  picp = (u32*)pic + ppuy * PICW;
  for (q = spr; q < sprm; q++) {
    if (q->x <= ppux0 || q->x >= ppux + 8)
      continue;
    x = q->x - 8;
    if (x < ppux0)
      x = ppux0;
    x1 = q->x;
    if (x1 > ppux)
      x1 = ppux;
    for (; x < x1; ++x) {
      attr = picp[x] >> prish;
      chr = q->chr;
      if ((chr & ((q->t & SPRXFL) != 0 ? 0x0101 : 0x8080)) != 0 &&
          (attr & TILSPR) == 0 &&
          ((mode & COL) != 0 && (reg[LCDC] & BGPRI) == 0 ||
           (attr & TILCOL0) != 0 ||
           (attr & TILPRI) == 0 && (q->t & SPRPRI) == 0)) {
        if ((q->t & SPRXFL) == 0)
          picp[x] = pal[q->pal | chr >> 15 | chr >> 6 & 2] | TILSPR << prish;
        else
          picp[x] = pal[q->pal | chr << 1 & 2 | chr >> 8 & 1] | TILSPR << prish;
			}
      if ((q->t & SPRXFL) != 0)
        q->chr >>= 1;
      else
        q->chr <<= 1;
    }
  }
  ppux0 = ppux;
}

void
ppusync(void)
{
  if (ppustate != 3)
    return;
  cyc = clock - rendclock;
  if (cyc != 0)
    myield();
  sprites();
  rendclock = clock;
}

static int
linelen(void)
{
  int t = 174 + (reg[SCX] & 7);
  if ((reg[LCDC] & WINEN) && ppuy >= reg[WY] && reg[WX] < 166) {
    if (reg[WX] == 0)
      t += 7;
    else
      t += 6;
  }
  return t * 2;
}

void
hblanktick(void* _)
{
  int t;

  switch (ppustate) {
  case 0:
    hblclock = clock + evhblank.time;
    if (reg[WX] <= 166 && reg[WY] <= 143)
      ppuw++;
    if (++ppuy == 144) {
      ppustate = 1;
      if (reg[STAT] & IRQM1)
        reg[IF] |= IRQLCDS;
      addevent(&evhblank, 456 * 2);
      reg[IF] |= IRQVBL;
      flush();
    } else {
      ppustate = 2;
      if ((reg[STAT] & IRQM2) != 0)
        reg[IF] |= IRQLCDS;
      addevent(&evhblank, 80 * 2);
    }
    if ((reg[STAT] & IRQLYC) != 0 && ppuy == reg[LYC])
      reg[IF] |= IRQLCDS;
    break;
  case 1:
    hblclock = clock + evhblank.time;
    if (reg[WX] <= 166 && reg[WY] <= 143)
      ppuw++;
    if (++ppuy == 154) {
      ppuy = 0;
      ppuw = 0;
      ppustate = 2;
      if (reg[STAT] & IRQM2)
        reg[IF] |= IRQLCDS;
      addevent(&evhblank, 80 * 2);
    } else
      addevent(&evhblank, 456 * 2);
    if ((reg[STAT] & IRQLYC) && ppuy == reg[LYC])
      reg[IF] |= IRQLCDS;
    break;
  case 2:
    oamsearch();
    rendclock = clock + evhblank.time;
    ppustate = 3;
    addevent(&evhblank, linelen());
    break;
  case 3:
    ppusync();
    ppustate = 0;
    if ((reg[STAT] & IRQM0) != 0)
      reg[IF] |= IRQLCDS;
    t = hblclock + 456 * 2 - clock;
    addevent(&evhblank, t < 0 ? 456 * 2 : t);
    break;
  }
}

void
ppuinit(void)
{
	static u8 stk[8192];

	renderjmp[REG_RSP] = stk + sizeof(stk) - 64;
	renderjmp[REG_RIP] = ppurender;
	myield();
}
