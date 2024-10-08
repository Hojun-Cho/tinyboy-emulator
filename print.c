#include "print.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

enum
{
	FlagLong = 1<<0,
	FlagLongLong = 1<<1,
	FlagUnsigned = 1<<2,
};

static char*
printstr(char *dst, char *edst, char *s, int sz)
{
	int l, n, isneg;

	isneg = 0;
	if(sz < 0){
		sz = -sz;
		isneg = 1;
	}
	if(dst >= edst)
		return dst;
	n = l = strlen(s);
	if(n < sz)
		n = sz;
	if(n >= edst - dst)
		n = (edst - dst) - 1;
	if(l > n)
		l = n;
	if(isneg){
		memmove(dst, s, l);
		if(n - l)
			memset(dst + l, ' ', n - l);
	}else{
		if(n - l)
			memset(dst, ' ', n - l);
		memmove(dst + n - l, s, l);
	}
	return dst + n;
}

char*
vseprint(char *dst, char *edst, char *fmt, va_list arg)
{
	int fl, sz, sign, base;
	char *p, *w;
	char cbuf[2];

	w = dst;
	for(p = fmt; *p && w < edst - 1; p++){
		switch(*p){
		default:
			*w++ = *p;
			break;
		case '%':
			sign = 1;
			fl = sz = 0;
			for(p++; *p; p++){
				switch(*p){
				case '-': sign = -1; break;
				case '0': case '1': case '2': case '3': case '4':
				case '5': case '6': case '7': case '8': case '9':
					sz = sz * 10  + *p - '0';
					break;
				case 'l': if(fl & FlagLong)fl |= FlagLongLong;break;
				case 'u': fl |= FlagUnsigned; break;
				case 'i': case 'd': 
					base = 10; goto num;
				case 'o': base = 8; goto num;
				case 'p': case 'x': case 'X':
					base = 16;
					goto num;
				num:
				{
					static char digits[] = "0123456789abcdef";
					char buf[30], *p;
					int neg, zero;
					unsigned long long luv;

					if(fl & FlagLongLong){
						if(fl & FlagUnsigned)
							luv = va_arg(arg,  unsigned long long);
						else
							luv = va_arg(arg, long long);
					}else{
						if(fl & FlagLong){
							if(fl & FlagUnsigned)
								luv = va_arg(arg, unsigned long);
							else
								luv = va_arg(arg, long);
						}else{
							if(fl & FlagUnsigned)
								luv = va_arg(arg, unsigned int);
							else
								luv = va_arg(arg, int);
						}
					}
					p = buf + sizeof(buf);
					neg = zero = 0;
					if((fl & FlagUnsigned) == 0 && (long long)luv < 0){
						neg = 1;
						luv = -luv;
					}
					if(luv == 0)
						zero  = 1;
					*--p = 0;
					while(luv){
						*--p = digits[luv % base];
						luv /= base;
					}
					if(base == 16){
						*--p = 'x';
						*--p = '0';
					}
					if(base == 8 || zero)
						*--p = '0';
					w = printstr(w, edst,p, sz * sign);	
					goto break2;
				}
				case 'c':
					cbuf[0] = va_arg(arg, int);
					cbuf[1] = 0;
					w = printstr(w, edst, cbuf, sz * sign);
					goto break2;
				case 's':
					w = printstr(w, edst, va_arg(arg, char*), sz*sign);
					goto break2;
				case 'r':
					w = printstr(w, edst, strerror(errno), sz*sign);
					goto break2;
				default:
					p = "error";
					goto break2;
				}
			}
			break2:
				break;
		}
	}
	assert(w < edst);
	*w = 0;
	return dst;
}

char*
vsnprint(char *dst, unsigned int n, char *fmt, va_list arg)
{
	return vseprint(dst, dst + n, fmt, arg);
}

char*
snprint(char *dst, unsigned int n, char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	vsnprint(dst, n, fmt, arg);
	va_end(arg);
	return dst;
}

char*
seprint(char* dst, char* edst, char* fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	vseprint(dst, edst, fmt, arg);
	va_end(arg);
	return dst;
}

int
vfprint(int fd, char *fmt, va_list arg)
{
	char buf[1024];

	vseprint(buf, buf + sizeof buf, fmt, arg);
	return write(fd, buf, strlen(buf));
}


int
vprint(char *fmt, va_list arg)
{
	return vfprint(1, fmt, arg);
}

int
fprint(int fd, char *fmt, ...)
{
	int n;
	va_list arg;

	va_start(arg, fmt);
	n = vfprint(fd, fmt, arg);
	va_end(arg);
	return n;
}

int
print(char *fmt, ...)
{
	int n;
	va_list arg;

	va_start(arg, fmt);
	n = vprint(fmt, arg);
	va_end(arg);
	return n;
}

char*
strecpy(char *dst, char *edst, char *src)
{
	*printstr(dst, edst, src, 0) = 0;
	return dst;
}
