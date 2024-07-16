#ifndef _TASK_H_
#define _TASK_H_

typedef struct Task Task;
typedef struct Tasklist Tasklist;

struct Tasklist
{
  Task* head;
  Task* tail;
};

typedef struct QLock
{
  Task* owner;
  Tasklist waiting;
} QLock;

void
taskmain(int argc, char** argv);
int
taskyield(void);
void
taskexitall(int val);
int
taskcreate(void (*fn)(void*), void* arg, unsigned int stk);
void
taskready(Task* t);
void
taskexit(int val);
void
taskswitch(void);
void
assertstack(unsigned int n);

void
qlock(QLock*);
int
canqlock(QLock*);
void
qunlock(QLock*);
QLock*
newqlock();

typedef struct Rendez Rendez;
struct Rendez
{
  QLock* l;
  Tasklist waiting;
};

void
tasksleep(Rendez*);
int
taskwakeup(Rendez*);
int
taskwakeupall(Rendez*);
Rendez*
newrendez(QLock* l);

extern Task* taskrunning;
extern int taskcount;

typedef struct Alt Alt;
typedef struct Altarray Altarray;
typedef struct Channel Channel;

enum chan_op
{
  CHANEND,
  CHANSND,
  CHANRCV,
  CHANNOP,
  CHANNOBLK,
};

struct Alt
{
  Channel* c;
  void* v;
  enum chan_op op;
  Task* task;
  Alt* xalt;
};

struct Altarray
{
  Alt** a;
  unsigned int n;
  unsigned int m;
};

struct Channel
{
  unsigned int bufsize;
  unsigned int elemsize;
  unsigned char* buf;
  unsigned int nbuf;
  unsigned int off;
  Altarray asend;
  Altarray arecv;
  char* name;
};

Channel*
newchan(int elemsize, int bufsize);
void
deletechan(Channel* c);
int
chansend(Channel* c, void* v);
int
chanbsend(Channel* c, void* v);
int
chanrecv(Channel* c, void* v);
int
chanbrecv(Channel* c, void* v);
int
chansendp(Channel* c, void* v);
void*
chanrecvp(Channel* c);
int
channbsend(Channel* c, void* v);
int
channbsendp(Channel* c, void* v);
void*
channbrecvp(Channel* c);
int
channbrecv(Channel* c, void* v);
int
chansendul(Channel* c, unsigned long v);
unsigned long
chanrecvul(Channel* c);
int
channbsendul(Channel* c, unsigned long v);
int
channbrecvul(Channel* c, unsigned long v);

#endif
