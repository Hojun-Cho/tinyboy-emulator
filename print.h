#include <stdarg.h>

char* vseprint(char *dst, char *edst, char *fmt, va_list arg);
char* vsnprint(char *dst, unsigned int n, char *fmt, va_list arg);
char* snprint(char *dst, unsigned int n, char *fmt, ...);
char* seprint(char* dst, char* edst, char* fmt, ...);
int vfprint(int fd, char *fmt, va_list arg);
int vprint(char *fmt, va_list arg);
int fprint(int fd, char *fmt, ...);
int print(char *fmt, ...);
char* strecpy(char *dst, char *edst, char *src);
