#include <stdarg.h>
#include <stdio.h>
//#include <ctype.h>
#include <compiler.h>

/*
 * very simple printf -- just understands a few format features
 * does c,s,u,d,x
 */

#define ULONG unsigned long
#define LONG long

//unsigned char __ctype[];

static void putcharstr(char **ptr, int val)
{
    char *ptr1 = *ptr;
    *ptr1++ = val;
    *ptr = ptr1;
}

static int
putcw(char **ptr, int c, int width) {
	int put = 0;

	putcharstr(ptr, c); put++;
	while (--width > 0) {
		putcharstr(ptr, ' ');
		put++;
	}
	return put;
}

static int
putsw(char **ptr, const char *s, int width) {
	int put = 0;

	while (*s) {
	  putcharstr(ptr, *s++); put++;
	  width--;
	}
	while (width-- > 0) {
	  putcharstr(ptr, ' '); put++;
	}
	return put;
}

static int d2a(int val)
{
    if (val < 10)
        val += '0';
    else
        val += 'A' - 10;
    return val;
}

static int ISDIGIT(int val)
{
    if (val >= 0 && val <= 9) return 1;
    return 0;
}

static int
putlw(char **ptr, ULONG u, int base, int width, int fill_char)
{
	int put = 0;
	char obuf[24]; /* 64 bits -> 22 digits maximum in octal */ 
	char *t;

	t = obuf;

	do {
		*t++ = d2a(u % base);
		u /= base;
		width--;
	} while (u > 0);

	while (width-- > 0) {
	  putcharstr(ptr, fill_char); put++;
	}
	while (t != obuf) {
	  putcharstr(ptr, *--t); put++;
	}
	return put;
}

static int
_doprnt(char *ptr, const char *fmt, va_list args )
{
   char c, fill_char;
   char *s_arg;
   unsigned int i_arg;
   ULONG l_arg;
   int width, long_flag;
   int outbytes = 0;
   int base;

   while( (c = *fmt++) != 0 ) {
     if (c != '%') {
       outbytes += putcw(&ptr, c, 1);
       continue;
     }
     c = *fmt++;
     width = 0;
     long_flag = 0;
     fill_char = ' ';
     if (c == '0') fill_char = '0';
     while (c && ISDIGIT(c)) {
       width = 10*width + (c-'0');
       c = *fmt++;
     }
     /* for us "long int" and "int" are the same size, so
	we can ignore one 'l' flag; use long long if two
    'l flags are seen */
     while (c == 'l' || c == 'L') {
       long_flag++;
       c = *fmt++;
     }
     if (!c) break;

     switch (c) {
     case '%':
       outbytes += putcw(&ptr, c, width);
       break;
     case 'c':
       i_arg = va_arg(args, unsigned int);
       outbytes += putcw(&ptr, i_arg, width);
       break;
     case 's':
       s_arg = va_arg(args, char *);
       outbytes += putsw(&ptr, s_arg, width);
       break;
     case 'd':
     case 'x':
     case 'u':
       base = (c == 'x') ? 16 : 10;
       l_arg = va_arg(args, ULONG);
       if (c == 'd') {
	 if (((LONG)l_arg) < 0) {
           outbytes += putcw(&ptr, '-', 1);
           width--;
           l_arg = (ULONG)(-((LONG)l_arg));
         }
       }
       outbytes += putlw(&ptr, l_arg, base, width, fill_char);
       break;
     }
   }
   putcharstr(&ptr, 0);
   return outbytes;
}

int sprintf(char *ptr, const char *fmt, ...)
{
    va_list args;
    int r;
    va_start(args, fmt);
    r = _doprnt(ptr, fmt, args);
    va_end(args);
    return r;
}
