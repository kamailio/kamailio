/* 
 * $Id$
 *
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of a module for Kamailio, a free SIP server.
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
 * History:
 * --------
 * 2006-09-25: first version (bogdan)
 */

/*!
 * \file
 * \brief MI Fifo :: Functions
 * \ingroup mi
 */

#ifndef _FIFO_FNC_H_
#define _FIFO_FNC_H_

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>


/*! \brief how patient is openser with FIFO clients not awaiting a reply? 
   default = 4 x 80ms = 0.32 sec
*/
#define FIFO_REPLY_RETRIES  4
#define FIFO_REPLY_WAIT     80000

FILE* mi_init_fifo_server(char *fifo_name, int mode, int uid, int gid,
		char* fifo_reply_dir);

void  mi_fifo_server(FILE *fifostream);

int   mi_read_line( char *b, int max, FILE *stream, int *read);

static inline int mi_fifo_reply( FILE *stream, char *reply_fmt, ... )
{
	int r;
	va_list ap;

retry:
	va_start(ap, reply_fmt);
	r = vfprintf( stream, reply_fmt, ap);
	va_end(ap);
	if (r<=0) {
		if ((errno==EINTR)||(errno==EAGAIN)||(errno==EWOULDBLOCK)) {
			goto retry;
		}
		LM_ERR("fifo_error: write error: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}


#endif /* _FIFO_FNC_H */

