/*
 * $Id$
 *
 * debug print 
 *
 */
 
#include "dprint.h"
#include "globals.h"
 
#include <stdarg.h>
#include <stdio.h>

void dprint(char * format, ...)
{
	va_list ap;

	fprintf(stderr, "%2d(%d) ", process_no, pids?pids[process_no]:0);
	va_start(ap, format);
	vfprintf(stderr,format,ap);
	fflush(stderr);
	va_end(ap);
}
