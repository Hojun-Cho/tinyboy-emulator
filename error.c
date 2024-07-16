#include "gb.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void
panic(const char* fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  fprintf(stderr, "panic:");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

void
error(const char* fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  fprintf(stderr, "panic:");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
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
