/*
 * $Id$
 *
 * debug print 
 *
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

 
#include "dprint.h"
#include "globals.h"
#include "pt.h"
 
#include <stdarg.h>
#include <stdio.h>
#include <strings.h>

static char* str_fac[]={"LOG_AUTH","LOG_CRON","LOG_DAEMON",
					"LOG_KERN","LOG_LOCAL0","LOG_LOCAL1",
					"LOG_LOCAL2","LOG_LOCAL3","LOG_LOCAL4","LOG_LOCAL5",
					"LOG_LOCAL6","LOG_LOCAL7","LOG_LPR","LOG_MAIL",
					"LOG_NEWS","LOG_USER","LOG_UUCP",
#ifndef __OS_solaris
					"LOG_AUTHPRIV","LOG_FTP","LOG_SYSLOG",
#endif
					0};
static int int_fac[]={LOG_AUTH ,  LOG_CRON , LOG_DAEMON ,
					LOG_KERN , LOG_LOCAL0 , LOG_LOCAL1 ,
					LOG_LOCAL2 , LOG_LOCAL3 , LOG_LOCAL4 , LOG_LOCAL5 ,
					LOG_LOCAL6 , LOG_LOCAL7 , LOG_LPR , LOG_MAIL ,
					LOG_NEWS , LOG_USER , LOG_UUCP
#ifndef __OS_solaris
					,LOG_AUTHPRIV,LOG_FTP,LOG_SYSLOG
#endif
					};


void dprint(char * format, ...)
{
	va_list ap;

	fprintf(stderr, "%2d(%d) ", process_no, my_pid());
	va_start(ap, format);
	vfprintf(stderr,format,ap);
	fflush(stderr);
	va_end(ap);
}


int str2facility(char *s)
{
	int i;

	for( i=0; str_fac[i] ; i++) {
		if (!strcasecmp(s,str_fac[i]))
			return int_fac[i];
	}
	return -1;
}
