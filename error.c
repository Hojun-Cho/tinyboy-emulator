#include "gb.h"
#include "print.h"
#include <stdlib.h>
#include <string.h>

void
panic(char* fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  fprint(2, "panic:");
  vfprint(2, fmt, ap);
  fprint(2, "\n");
  exit(1);
}

void
error(char* fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  fprint(2, "panic:");
  vfprint(2, fmt, ap);
  fprint(2, "\n");
}

void*
xalloc(long sz)
{
  void* x = malloc(sz);
  if (x == 0)
    panic("can't alloc %z", sz);
  memset(x, 0, sz);
  return x;
}
