#include "taskimpl.h"
#include <stdio.h>

int taskcount;
int taskidgen;
int tasknswitch;
int taskexitval;

Task* taskrunning;
ucontext_t taskschedcontext;
Tasklist taskrunqueue;
Task** alltask;
int nalltask;

static void
contextswitch(ucontext_t* from, ucontext_t* to);
void
assertstack(uint n);

static void
taskinfo(int s)
{
  int i;
  Task* t;
  char* extra;

  for (i = 0; i < nalltask; ++i) {
    t = alltask[i];
    if (t == taskrunning)
      extra = " (running)";
    else if (t->ready)
      extra = " (ready)";
    else
      extra = "";
    printf("%s\n", extra);
  }
}

static void
taskstart(uint x, uint y)
{
  Task* t;
  ulong z;

  z = x << 16;
  z <<= 16;
  z |= y;
  t = (Task*)z;
  t->startfn(t->startarg);
  taskexit(0);
}

static Task*
taskalloc(void (*fn)(void*), void* arg, uint stk)
{
  Task* t;
  sigset_t zero;
  uint x, y;
  ulong z;

  if ((t = malloc(sizeof *t + stk)) == nil) {
    exit(1);
  }
  *t = (Task){
    .stk = (uchar*)(&t[1]),
    .stksize = stk,
    .id = ++taskidgen,
    .startfn = fn,
    .startarg = arg,
  };
  sigemptyset(&zero);
  sigprocmask(SIG_BLOCK, &zero, &t->uc.uc_sigmask);
  if (getcontext(&t->uc)) {
    exit(1);
  }
  t->uc.uc_stack.ss_sp = t->stk + 8;
  t->uc.uc_stack.ss_size = t->stksize - 64;

  z = (ulong)t;
  y = z;
  z >>= 16;
  x = z >> 16;
  makecontext(&t->uc, (void (*)())taskstart, 2, x, y);
  return t;
}

int
taskcreate(void (*fn)(void*), void* arg, uint stk)
{
  int id;
  Task* t;

  t = taskalloc(fn, arg, stk);
  taskcount++;
  id = t->id;
  if (nalltask % 64 == 0) {
    alltask = realloc(alltask, (nalltask + 64) * sizeof(alltask[0]));
    if (alltask == 0) {
      exit(1);
    }
  }
  t->alltaskslot = nalltask;
  alltask[nalltask++] = t;
  taskready(t);
  return id;
}

void
taskready(Task* t)
{
  t->ready = 1;
  addtask(&taskrunqueue, t);
}

void
taskswitch(void)
{
  assertstack(0);
  contextswitch(&taskrunning->uc, &taskschedcontext);
}

int
taskyield(void)
{
  int n;

  n = tasknswitch;
  taskready(taskrunning);
  taskswitch();
  return tasknswitch - n - 1;
}

void
taskexit(int val)
{
  taskexitval = val;
  taskrunning->exiting = 1;
  taskswitch();
}

void
taskexitall(int val)
{
  exit(val);
}

void
deltask(Tasklist* l, Task* t)
{
  if (t->prev)
    t->prev->next = t->next;
  else
    l->head = t->next;
  if (t->next)
    t->next->prev = t->prev;
  else
    l->tail = t->prev;
}

void
addtask(Tasklist* l, Task* t)
{
  if (l->tail) {
    l->tail->next = t;
    t->prev = l->tail;
  } else {
    l->head = t;
    t->prev = nil;
  }
  l->tail = t;
  t->next = nil;
}

static void
contextswitch(ucontext_t* from, ucontext_t* to)
{
  if (swapcontext(from, to) < 0) {
    printf("swapcontext is fail\n");
    exit(1);
  }
}

void
assertstack(uint n)
{
  Task* t;

  t = taskrunning;
  if ((uchar*)&t <= (uchar*)t->stk || (uchar*)&t - (uchar*)t->stk < 256 + n) {
    /* satck over flow */
    exit(1);
  }
}

static void
taskscheduler(void)
{
  int i;
  Task* t;

  for (;;) {
    if (taskcount == 0)
      exit(taskexitval);
    t = taskrunqueue.head;
    if (t == nil) {
      /* nothing to do */
      exit(1);
    }
    deltask(&taskrunqueue, t); /* delete from runqueue */
    t->ready = 0;
    taskrunning = t;
    tasknswitch++;
    contextswitch(&taskschedcontext, &t->uc);
    taskrunning = nil; /* ready for next task */
    if (t->exiting) {
      taskcount--;
      i = t->alltaskslot;
      alltask[i] = alltask[--nalltask];
      alltask[i]->alltaskslot = i;
      free(t);
    }
  }
}

static char* argv0;
static int taskargc;
static char** taskargv;
int mainstacksize;

static void
taskmainstart(void* v)
{
  taskmain(taskargc, taskargv);
}

int
main(int argc, char** argv)
{
  struct sigaction sa, osa;

  memset(&sa, 0, sizeof(sigaction));
  sa.sa_handler = taskinfo;
  sa.sa_flags = SA_RESTART;
  sigaction(SIGQUIT, &sa, &osa);
  /*sigaction(SIGINFO, &sa, &osa);*/
  argv0 = argv[0];
  taskargc = argc;
  taskargv = argv;
  mainstacksize = 256 * 1024;
  taskcreate(taskmainstart, nil, mainstacksize);
  taskscheduler();
  exit(0);
}
