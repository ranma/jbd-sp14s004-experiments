#include <stdarg.h>
#include "printf.h"

static int print_int(unsigned int i, int npad, char cpad, int neg)
{
	char buf[10];
	int nwritten = 0;
	int nchars = 0;
	if (i > 0) {
		while (i > 0) {
			int digit = i % 10;
			i = i / 10;
			buf[nchars++] = '0' + digit;
		}
	} else {
		buf[0] = '0';
		nchars = 1;
	}
	if (neg && npad) {
		npad--;
		if (cpad == '0') {
			putchar('-');
			nwritten++;
			neg = 0;
		}
	}
	while (npad > nchars) {
		putchar(cpad);
		nwritten++;
		npad--;
	}
	if (neg) {
		putchar('-');
		nwritten++;
	}
	nwritten += nchars;
	while (nchars > 0) {
		putchar(buf[--nchars]);
	}
	return nwritten;
}

static void putnibble(int x)
{
	if (x < 10) {
		putchar('0' + x);
	} else {
		putchar('a' + x - 10);
	}
}

static int hex_chars(unsigned int i)
{
	if (i == 0) {
		return 1;
	}

	unsigned int res = 0;
	unsigned int tmp = i >> 16;
	if (tmp) {
		i = tmp;
		res = 4;
	}
	tmp = i >> 8;
	if (tmp) {
		i = tmp;
		res += 2;
	}
	tmp = i >> 4;
	if (tmp) {
		i = tmp;
		res++;
	}
	if (i) {
		res++;
	}
	return res;
}

static int print_hex(unsigned int i, int npad, char cpad)
{
	int nwritten = 0;
	int nchars = hex_chars(i);
	unsigned int shift = 4 * (nchars - 1);
	unsigned int mask = 0xf << shift;
	while (npad > nchars) {
		putchar(cpad);
		nwritten++;
		npad--;
	}
	while (mask > 0) {
		putnibble((i & mask) >> shift);
		shift -= 4;
		mask >>= 4;
	}
	return nwritten;
}

/*
 * Minimal printf implementation, only a subset of formats and
 * specifiers are supported, in particular:
 * d int
 * x hexadecimal int
 * s string
 * p pointer
 */
static int minivprintf(const char *format, va_list ap, int (*putchar)(int))
{
	int nwritten = 0;
	while (*format) {
		if (*format != '%') {
			putchar(*format++);
			nwritten++;
			continue;
		}
		if (*format == '%' && *(format + 1) == '%') {
			putchar('%');
			nwritten++;
			format += 2;
			continue;
		}

		char cpad = ' ';
		int npad = 0;
		if (*++format == '0') {
			cpad = '0';
		}
		while (*format >= '0' && *format <= '9') {
			npad = 10 * npad + *format++ - '0';
		}
		switch (*format++) {
		case 'd': {
			int i = va_arg(ap, int);
			if (i >= 0) {
				nwritten += print_int(i, npad, cpad, 0);
			} else {
				nwritten += print_int(-i, npad, cpad, 1);
			}
			break; }
		case 's': {
			const char *s = va_arg(ap, const char*);
			while (*s) {
				putchar(*s++);
				nwritten++;
			}
			break; }
		case 'p': {
			cpad = '0';
			npad = '8';
			// fall-through
			}
		case 'x': {
			unsigned int i = va_arg(ap, unsigned int);
			nwritten += print_hex(i, npad, cpad);
			break; }
		}
	}
	return nwritten;
}

int printf(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	int ret = minivprintf(format, ap, putchar);
	va_end(ap);
	return ret;
}

int puts(const char *s)
{
	return printf("%s\n", s);
}
