#include "taskimpl.h"

Channel*
newchan(int elemsize, int bufsize)
{
  Channel* c;

  c = malloc(sizeof *c + bufsize * elemsize);
  if (c == 0) {
    exit(1);
  }
  *c = (Channel){
    .elemsize = elemsize,
    .bufsize = bufsize,
    .nbuf = 0,
    .buf = (uchar*)&c[1],
  };
  return c;
}

void
deletechan(Channel* c)
{
  if (c == 0)
    return;
  free(c->name);
  free(c->asend.a);
  free(c->arecv.a);
  free(c);
}

static void
addarray(Altarray* a, Alt* alt)
{
  if (a->n == a->m) {
    a->m += 16;
    a->a = realloc(a->a, a->m * sizeof a->a[0]);
    if (a->a == nil) {
      exit(1);
    }
  }
  a->a[a->n++] = alt;
}

static void
amove(void* dst, void* src, uint n)
{
  if (dst) {
    if (src == nil)
      memset(dst, 0, n);
    else
      memmove(dst, src, n);
  }
}

static void
delarray(Altarray* a, int i)
{
  --a->n;
  a->a[i] = a->a[a->n];
}

static enum chan_op
otherop(enum chan_op op)
{
  return (CHANSND + CHANRCV) - op;
}

static Altarray*
chanarray(Channel* c, enum chan_op op)
{
  switch (op) {
    default:
      return nil;
    case CHANSND:
      return &c->asend;
    case CHANRCV:
      return &c->arecv;
  }
}

/*
 * 3 players: sender, reciver, 'channel itself'
 * if chann.empty
 *			send(send to reciver);
 */
static void
altcopy(Alt* s, Alt* r)
{
  Channel* c;
  uchar* cp;

  if (s == nil && r == nil)
    return;
  assert(s != nil); /* sender must not nil */
  c = s->c;
  if (s->op == CHANRCV) {
    Alt* t = s;
    s = r;
    r = t;
  }
  assert(s == nil || s->op == CHANSND); /* sender */
  assert(r == nil || r->op == CHANRCV); /* reciver */

  /* Channel is unbufferd */
  if (s && r && c->nbuf == 0) {
    amove(r->v, s->v, c->elemsize);
    return;
  }
  if (r) {
    /* recive buffred data first */
    cp = c->buf + c->off * c->elemsize;
    amove(r->v, cp, c->elemsize);
    --c->nbuf;
    if (++c->off == c->bufsize)
      c->off = 0;
  }
  if (s) {
    cp = c->buf + (c->off + c->nbuf) % c->bufsize * c->elemsize;
    amove(cp, s->v, c->elemsize);
    ++c->nbuf;
  }
}

static void
altdeque(Alt* a)
{
  Altarray* ar;

  ar = chanarray(a->c, a->op);
  if (ar == nil) {
    exit(1);
  }

  for (uint i = 0; i < ar->n; ++i) {
    if (ar->a[i] == a) {
      delarray(ar, i);
      return;
    }
  }
  /* can't find self in altdq */
  exit(1);
}

static void
altqueue(Alt* a)
{
  Altarray* ar;

  ar = chanarray(a->c, a->op);
  addarray(ar, a);
}

static void
altalldeque(Alt* a)
{
  for (int i = 0; a[i].op != CHANEND && a[i].op != CHANNOBLK; i++) {
    if (a[i].op != CHANNOP)
      altdeque(&a[i]);
  }
}

static void
altexec(Alt* a)
{
  Altarray* ar;
  Channel* c;

  c = a->c;
  ar = chanarray(c, otherop(a->op));
  if (ar && ar->n) {
    int i = rand() % ar->n; /* find any other palyer */
    Alt* other = ar->a[i];
    altcopy(a, other);        /* copy data to other's buffer */
    altalldeque(other->xalt); /* delete because now request is completed */
    other->xalt[0].xalt = other;
    taskready(other->task);
  } else
    altcopy(a, nil); /* if channel is unbufferd then do nothing */
}

static int
altcanexec(Alt* a)
{
  Channel* c;

  if (a->op == CHANNOP)
    return 0;
  c = a->c;
  if (c->bufsize == 0) {
    Altarray* ar = chanarray(c, otherop(a->op));
    return ar && ar->n;
  }
  switch (a->op) {
    default:
      return 0;
    case CHANSND:
      return c->nbuf < c->bufsize;
    case CHANRCV:
      return c->nbuf > 0;
  }
}

int
chanalt(Alt* a)
{
  int i, j, ncan, n, canblock;
  Task* t;

  assertstack(512);
  for (i = 0; a[i].op != CHANEND && a[i].op != CHANNOBLK; ++i)
    ;
  n = i;
  canblock = (a[i].op == CHANEND);
  t = taskrunning;
  for (i = 0; i < n; ++i) {
    a[i].task = t;
    a[i].xalt = a;
  }
  ncan = 0;
  for (i = 0; i < n; i++) {
    if (altcanexec(&a[i])) {
      ncan++;
    }
  }
  if (ncan) {
    j = rand() % ncan;
    for (i = 0; i < n; ++i) {
      if (altcanexec(&a[i])) {
        if (j-- == 0) {
          altexec(&a[i]);
          return i;
        }
      }
    }
  }
  if (canblock == 0)
    return -1;
  for (i = 0; i < n; ++i) {
    if (a[i].op != CHANNOP)
      altqueue(&a[i]);
  }
  taskswitch();
  return a[0].xalt - a; /* calculate offset */
}

static int
_chanop(Channel* c, int op, void* p, int canblock)
{
  Alt ar[2];

  ar[0] = (Alt){
    .c = c,
    .op = op,
    .v = p,
  };
  ar[1] = (Alt){
    .op = canblock ? CHANEND : CHANNOBLK,
  };
  if (chanalt(ar) < 0)
    return -1;
  return 1;
}

int
chansend(Channel* c, void* v)
{
  return _chanop(c, CHANSND, v, 1);
}

int
chanbsend(Channel* c, void* v)
{
  return _chanop(c, CHANSND, v, 0);
}

int
chanrecv(Channel* c, void* v)
{
  return _chanop(c, CHANRCV, v, 1);
}

int
chanbrecv(Channel* c, void* v)
{
  return _chanop(c, CHANSND, v, 0);
}

int
chansendp(Channel* c, void* v)
{
  return _chanop(c, CHANSND, &v, 1);
}

void*
chanrecvp(Channel* c)
{
  void* v;

  _chanop(c, CHANRCV, &v, 1);
  return v;
}

int
channbsendp(Channel* c, void* v)
{
  return _chanop(c, CHANSND, (void*)&v, 0);
}

void*
channbrecvp(Channel* c)
{
  void* v;

  _chanop(c, CHANRCV, &v, 0);
  return v;
}

int
chansendul(Channel* c, ulong v)
{
  return _chanop(c, CHANSND, &v, 1);
}

ulong
chanrecvul(Channel* c)
{
  ulong v;

  _chanop(c, CHANRCV, &v, 1);
  return v;
}

int
channbsendul(Channel* c, ulong v)
{
  return _chanop(c, CHANSND, &v, 0);
}

int
channbrecvul(Channel* c, ulong v)
{
  return _chanop(c, CHANRCV, &v, 0);
}

int
channbsend(Channel* c, void* v)
{
  return _chanop(c, CHANSND, v, 0);
}

int
channbrecv(Channel* c, void* v)
{
  return _chanop(c, CHANRCV, v, 0);
}
