#include <stdint.h>

#define nelem(x) (sizeof(x)/sizeof(x[0]))

#define VAR(a) {&a, sizeof(a), 1}
#define ARR(a) {a, sizeof(*a), nelem(a)}
#define nil ((void*)0)
#define JOYPAD_CYCLE (30)

enum
{
  JOYP = 0x00,
  SB = 0x01,
  SC = 0x02,
  DIV = 0x04,
  TIMA = 0x05,
  TMA = 0x06,
  TAC = 0x07,
  IF = 0x0F,
  NR10 = 0x10,
  NR11 = 0x11,
  NR12 = 0x12,
  NR13 = 0x13,
  NR14 = 0x14,
  NR21 = 0x16,
  NR22 = 0x17,
  NR23 = 0x18,
  NR24 = 0x19,
  NR30 = 0x1A,
  NR31 = 0x1B,
  NR32 = 0x1C,
  NR33 = 0x1D,
  NR34 = 0x1E,
  NR41 = 0x20,
  NR42 = 0x21,
  NR43 = 0x22,
  NR44 = 0x23,
  NR50 = 0x24,
  NR51 = 0x25,
  NR52 = 0x26,
  WAVE = 0x30,
  LCDC = 0x40,
  STAT = 0x41,
  SCY = 0x42,
  SCX = 0x43,
  LY = 0x44,
  LYC = 0x45,
  DMA = 0x46,
  BGP = 0x47,
  OBP0 = 0x48,
  OBP1 = 0x49,
  WY = 0x4A,
  WX = 0x4B,
  KEY1 = 0x4D,
  VBK = 0x4F,
  RP = 0x56,
  HDMASH = 0x51,
  HDMASL = 0x52,
  HDMADH = 0x53,
  HDMADL = 0x54,
  HDMAC = 0x55,

  BCPS = 0x68,
  BCPD = 0x69,
  OCPS = 0x6A,
  OCPD = 0x6B,
  SVBK = 0x70,
  IE = 0xFF
};

enum
{
  LCDEN = 0x80,
  WINMAP = 0x40,
  WINEN = 0x20,
  BGTILE = 0x10,
  BGMAP = 0x08,
  SPR16 = 0x04,
  SPREN = 0x02,
  BGEN = 0x01,
  BGPRI = 0x01,

  IRQLYC = 0x40,
  IRQM2 = 0x20,
  IRQM1 = 0x10,
  IRQM0 = 0x08,

  IRQVBL = 1,
  IRQLCDS = 2,
  IRQTIM = 4,
  IRQSER = 8,
  IRQJOY = 16,
};

enum
{
  CGB = 1,
  COL = 2,
  TURBO = 4,
  FORCEDMG = 8,

  FEATRAM = 1,
  FEATBAT = 2,
  FEATTIM = 4,

  DMAREADY = 1,
  DMAHBLANK = 2,

  INIT = -1,
  SAVE = -2,
  RSTR = -3,
  READ = -4,
};

enum
{
  TIMERSIZ = 18,
  PICW = 160,
  PICH = 144,
  FREQ = 1 << 23
};

enum
{
  GB_KEY_RIGHT = 0x01,
  GB_KEY_LEFT = 0x02,
  GB_KEY_UP = 0x04,
  GB_KEY_DOWN = 0x08,
  GB_KEY_A = 0x10,
  GB_KEY_B = 0x20,
  GB_KEY_SELECT = 0x40,
  GB_KEY_START = 0x80
};

enum { NEVENT = 2 };

enum
{
	REG_RIP = 7,
	REG_RSP = 6,
};

typedef uint8_t u8;
typedef uint16_t u16;
typedef int8_t i8;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint64_t u64;
typedef int64_t i64;
typedef struct Event Event;
typedef struct Var Var;

struct Event
{
  int time;
  void (*f)(void*);
  Event* next;
  void* aux;
};

struct Var
{
	void *a;
	int s, n;
};

#define VAR(a) {&a, sizeof(a), 1}
#define ARR(a) {a, sizeof(*a), nelem(a)}

extern u8 *rom, *back, reg[256], oam[256];
extern u8 vram[16384];
extern int nrom, nback, nbackbank;
extern u32 pal[64];
extern u8 dma;
extern u32 divclock;
extern u64 clock;
extern u8 ppuy, ppustate;
extern u8 keys;
extern int prish;
extern u8 mode;
extern u8 mbc;
extern Event* elist;
extern Event evhblank, evjoypad;
extern u32 moncols[4];
extern u8* pic;
extern int (*mapper)(int, int);
extern Event *events[NEVENT];
extern int savereq, loadreq;

/* joypad */
void
joypadevent(void*);

/* memory */
void
memwrite(u16 a, u8 v);
u8
memread(u16 a);
void
meminit(void);

/* events */
void
initevent(void);
void
addevent(Event*, int);
void
delevent(Event*);
void
popevent(void);

/* ppu */
void
hblanktick(void*);
void
ppusync(void);
void
ppuinit(void);

/* cpu */
int
step(void);
void
reset(void);

/* graphic */
void
initwindow(int scale);
void
render();

/* save */
void
putvars(Var *v);
void
getvars(Var *v);
void
flush();
int
savestate(const char *fname);
int
loadstate(const char *fname);
void
writeback(void);

/* error */
void
error(const char*, ...);
void
panic(const char*, ...);
void*
xalloc(long);
