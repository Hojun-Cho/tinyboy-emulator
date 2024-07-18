#include "gb.h"
#include <stdio.h>

extern Var cpuvars[], ppuvars[], memvars[], evvars[];
static FILE *fp;

void
putevents(void)
{
	u8 i, j;
	Event *e;

	for(i = 0; i < NEVENT; ++i)
		if(elist == events[i]) /* find head */
			break;
	if(i == NEVENT && elist != nil)
		error("Unkown event in chain [%p]", elist);
	fputc(i, fp);
	for(i = 0; i < NEVENT; ++i){
		e = events[i];
		fputc(e->time & 0x000000ff, fp);
		fputc(e->time & 0x0000ff00, fp);
		fputc(e->time & 0x00ff0000, fp);
		fputc(e->time & 0xff000000, fp);
		for(j = 0; j < NEVENT; ++j)
			if(e->next == events[j]) /* find next */
				break;
		if(j == NEVENT && e->next != nil)
			error("Unkown event in chain [%p]", e->next);
		fputc(j, fp);
	}
}

void
putvars(Var *v)
{
	int n;
	u16 *p;
	u32 *q;

	for(; v->a != nil; v++){
		switch(v->s){
		case 1:
			fwrite(v->a, 1, v->n, fp);
			break;
		case 2:
			n = v->n;
			p = v->a;
			while(n--){
				fputc(*p & 0xff, fp);
				fputc(*p++ >> 8, fp);
			}
			break;
		case 4:
			n = v->n;
			q = v->a;
			while(n--){
				fputc(*q, fp);
				fputc(*q >> 8, fp);
				fputc(*q >> 16, fp);
				fputc(*q >> 24, fp);
			}
			break;
		}
	}
}

void
getvars(Var *v)
{
	int n;
	u16 *p, w;
	u32 *q, l;

	for(; v->a != nil; v++){
		switch(v->s){
		case 1: fread(v->a, v->n, 1, fp); break;
		case 2:
			n = v->n;
			p = v->a;
			while(n--){
				w = fgetc(fp);
				w |= fgetc(fp) << 8;
				*p++ = w;
			}
			break;
		case 4:
			n = v->n;
			q = v->a;
			while(n--){
				l |= fgetc(fp);
				l |= fgetc(fp) << 8;
				l |= fgetc(fp) << 16;
				l |= fgetc(fp) << 24;
				*q++ = l;
			}
			break;
		}
	}
}

int
savestate(const char *fname)
{
	fp = fopen(fname, "w");
	if(fp == 0){
		error("Can't open '%s'", fname);
		return -1;
	}
	putvars(cpuvars);
	putvars(ppuvars);
	putvars(memvars);
	putvars(evvars);
	putevents();
	mapper(SAVE, 0);
	fclose(fp);
	fp = 0;
	return 0;
}

static void
getevents(void)
{
	int i, j;
	Event *e;

	i = fgetc(fp);
	if(i >= NEVENT)
		elist = nil;
	else
		elist = events[i];
	for(i = 0; i < NEVENT; ++i){
		e = events[i];
		e->time = fgetc(fp);
		e->time |= fgetc(fp) << 8;
		e->time |= fgetc(fp) << 16;
		e->time |= fgetc(fp) << 24;
		j = fgetc(fp);
		if(j >= NEVENT)
			e->next = nil;
		else
			e->next = events[j];
	}
}

int
loadstate(const char *fname)
{
	fp = fopen(fname, "r");
	if(fp == 0){
		error("Can't load '%s'", fname);
		return -1;
	}
	getvars(cpuvars);
	getvars(ppuvars);
	getvars(memvars);
	getvars(evvars);
	getevents();
	mapper(RSTR, 0);
	fclose(fp);
	fp = 0;
	return 0;
}
