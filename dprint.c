/*
 * $Id$
 *
 * debug print 
 *
 */
 
#include "dprint.h"
 
#include <stdarg.h>
#include <stdio.h>

void dprint(char * format, ...)
{
	va_list ap;

	va_start(ap, format);
	vfprintf(stderr,format,ap);
	fflush(stderr);
	va_end(ap);
}
