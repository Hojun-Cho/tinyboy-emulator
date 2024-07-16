#include "gb.h"
#include "co/task.h"
#include <stdio.h>

int cpuhalt;
int backup;
int savefd = -1;
u64 clock;
u8 mbc, feat, mode;

static void
loadrom(const char* file)
{
  FILE* f;
  long sz;
  static u8 mbctab[31] = { 0, 1, 1, 1, -1, 2, 2, -1, 0,  0, -1, 6, 6, 6, -1, 3,
                           3, 3, 3, 3, -1, 4, 4, 4,  -1, 5, 5,  5, 5, 5, 5 };
	static u8 feattab[31] = {
		0, 0, FEATRAM, FEATRAM|FEATBAT, 0, FEATRAM, FEATRAM|FEATBAT, 0,
		FEATRAM, FEATRAM|FEATBAT, 0, 0, FEATRAM, FEATRAM|FEATBAT, 0, FEATTIM|FEATBAT,
		FEATTIM|FEATRAM|FEATBAT, 0, FEATRAM, FEATRAM|FEATBAT, 0, 0, FEATRAM, FEATRAM|FEATBAT,
		0, 0, FEATRAM, FEATRAM|FEATBAT, 0, FEATRAM, FEATRAM|FEATBAT,
	};
	
  f = fopen(file, "r");
  if (f == nil)
    panic("can't open %s", file);
  fseek(f, 0, SEEK_END);
  sz = ftell(f);
  if (sz < 0 || sz > 32 * 1024 * 1024)
    panic("bad size %d", sz);
  fseek(f, 0, SEEK_SET);
  nrom = sz;
  rom = xalloc(nrom);
  if (fread(rom, 1, nrom, f) != nrom)
    panic("siz is different %z", nrom);
  fclose(f);
  if (rom[0x147] > 0x1F)
    panic("bad cartidge type %d\n", rom[0x147]);
  mbc = mbctab[rom[0x147]];
	feat = feattab[rom[0x147]];
	if((feat & FEATRAM) != 0){
		switch(rom[0x149]){
		case 0: feat &= ~FEATRAM; break;
		case 1: nback = 2048; break;
		case 2: nback = 8192; break;
		case 3: nback = 32768; break;
		default: panic("Unkown Ram size %d\n", rom[0x149]);
		}
	}
	back = xalloc(nback);
	if(nback == 0)
		nbackbank = 1;
	else
		nbackbank = (nback + 8191) / 8192; 
  switch (mbc) {
		case 0: case 1:
      break;
    default: panic("unsupported mbc %d", mbc);
  }
  if ((rom[0x143] & 0x80) != 0 && (mode & FORCEDMG) == 0)
    mode = CGB | COL;
}

static void
colinit(void)
{
  union
  {
    u8 c[4];
    u32 l;
  } c;

  c.c[3] = 0;
  for (int i = 0; i < 4; i++) {
    c.c[0] = c.c[1] = c.c[2] = i * 0x55;
    moncols[i] = c.l;
  }
}

void
taskmain(int argc, char* argv[])
{
  colinit();
  loadrom(argv[1]);
  initwindow(5);
  initevent();
  meminit();
  reset();
  taskcreate(pputask, 0, 32768);

  for (;;) {
    int t = step();
    clock += t;
    if (elist && (elist->time -= t) <= 0) {
      popevent();
    }
  }
}
