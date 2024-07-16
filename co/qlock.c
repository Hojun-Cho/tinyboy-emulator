#include "taskimpl.h"

int
_qlock(QLock* l, int block)
{
  if (l->owner == nil) {
    l->owner = taskrunning;
    return 1;
  }
  if (!block)
    return 0;
  addtask(&l->waiting, taskrunning);
  taskswitch(); /* wait until own lock */
  if (l->owner != taskrunning) {
    /* TODO: */
    exit(1);
  }
  return 1;
}

void
qlock(QLock* l)
{
  _qlock(l, 1);
}

int
canqlock(QLock* l)
{
  return _qlock(l, 0);
}

void
qunlock(QLock* l)
{
  Task* ready;

  if (l->owner == nil) {
    /* TODO: */
    exit(1);
  }
  l->owner = ready = l->waiting.head;
  if (l->owner != nil) {
    deltask(&l->waiting, ready);
    taskready(ready);
  }
}

QLock*
newqlock()
{
  QLock* l;

  l = malloc(sizeof *l);
  if (l == nil)
    exit(1);
  l->owner = 0;
  l->waiting = (Tasklist){ 0 };
  return l;
}
