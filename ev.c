#include "gb.h"

Event evhblank, evjoypad;
Event* elist;

void
addevent(Event* ev, int time)
{
  Event **p, *e;
  int t = time;

  for (p = &elist; (e = *p) != nil; p = &e->next) {
    if (t < e->time) {
      e->time -= t;
      break;
    }
    t -= e->time;
  }
  ev->next = e;
  ev->time = t;
  *p = ev;
}

void
delevent(Event* ev)
{
  Event **p, *e;

  for (p = &elist; (e = *p) != nil; p = &e->next) {
    if (e == ev) {
      *p = e->next;
      if (e->next != nil)
        e->next->time += e->time;
      return;
    }
  }
}

void
popevent(void)
{
  Event* e;
  int t;

  do {
    e = elist;
    t = e->time;
    elist = e->next;
    e->f(e->aux);
  } while ((elist->time += t) <= 0);
}

void
initevent(void)
{
  evjoypad.f = joypadevent;
  addevent(&evjoypad, JOYPAD_CYCLE);
  evhblank.f = hblanktick;
  addevent(&evhblank, 240 * 4);
}
