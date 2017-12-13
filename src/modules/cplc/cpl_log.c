/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *
 * History:
 * -------
 * 2003-09-22: created (bogdan)
 *
 */

#include <stdlib.h>
#include <string.h>
#include "cpl_log.h"
#include "../../core/mem/mem.h"
#include "../../core/dprint.h"


static str  cpl_logs[MAX_LOG_NR];
static int  nr_logs;


void reset_logs(void)
{
	nr_logs = 0;
}



void append_log( int nr, ...)
{
	va_list ap;
	int     i;


	if ( nr_logs+nr>MAX_LOG_NR ) {
		LM_ERR("no more space for logging\n");
		return;
	}

	va_start(ap, nr);

	for(i=0;i<nr;i++,nr_logs++) {
		cpl_logs[nr_logs].s   = va_arg(ap, char *);
		cpl_logs[nr_logs].len = va_arg(ap, int );
	}

	va_end(ap);
}



void compile_logs( str *log)
{
	int i;
	char *p;

	log->s = 0;
	log->len = 0;

	if (nr_logs==0)
		/* no logs */
		return;

	/* compile the total len */
	for(i=0;i<nr_logs;i++)
		log->len += cpl_logs[i].len;

	/* get a buffer */
	log->s = (char*)pkg_malloc(log->len+1);
	if (log->s==0) {
		LM_ERR("no more pkg mem\n");
		log->len = 0;
		return;
	}

	/*copy all logs into buffer */
	p = log->s;
	for(i=0;i<nr_logs;i++) {
		memcpy( p, cpl_logs[i].s, cpl_logs[i].len);
		p += cpl_logs[i].len;
	}
	log->s[log->len] = '\0';

	return;
}

