#include "gb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int savereq, loadreq;
int cpuhalt;
int backup;
int savefd = -1;
int saveframes;
const char *romname;
u8 mbc, feat, mode;

void
writeback(void)
{
	if(saveframes == 0)
		saveframes = 15;
}

void
flushback(void)
{
	if(savefd >= 0)
		pwrite(savefd, back, nback, 0);
	saveframes = 0;
}

static void
_flushback(void)
{
	flushback();
	close(savefd);
}

static void
loadsave(const char *file)
{
	u8 *buf, *p;

	buf = xalloc(strlen(file) + 4);
	strcpy(buf, file);
	p = strrchr(buf, '.');
	if(p == nil)
		p = buf + strlen(buf);
	strcpy(p, ".sav");
	savefd = open(buf, O_RDWR);
	if(savefd == -1){
		savefd = open(buf, O_RDWR|O_CREAT, 0664);
		if(savefd == -1)
			error("Can't load save file '%s'", file);
		back = xalloc(nback);
		if(write(savefd, back, nback)!= nback)
			error("Can't Write %d byte to savefile", nback);
		atexit(_flushback);
		free(buf);
		return;
	}
	back = xalloc(nback);
	if(read(savefd, back, nback) != nback)
		error("savefile size is not matched\n");
	atexit(_flushback);
	free(buf);
}

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
	if((feat & FEATBAT) != 0)
		loadsave(file);
}

void
flush()
{
	static char *savestatename;
	if (savestatename == 0){
		int len = strlen(romname);
		savestatename = xalloc(len + 4);
		strncpy(savestatename, romname, len);
		strcpy(savestatename + len, "-state.save");
	}

  render();
	if(saveframes > 0 && -- saveframes == 0)
		flushback();
	if(savereq){
		savestate(savestatename);
		savereq = 0;
	}
	if(loadreq){
		loadstate(savestatename);
		loadreq = 0;
	}
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

int
main(int argc, char* argv[])
{
  loadrom(romname = argv[1]);
  colinit();
  initwindow(5);
  initevent();
  meminit();
  reset();
	ppuinit();

  for (;;) {
    int t = step();
    clock += t;
    if (elist && (elist->time -= t) <= 0) {
      popevent();
    }
  }
}
