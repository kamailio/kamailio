/*
 * $Id: fmt.h 4518 2008-07-28 15:39:28Z henningw $
 *
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *
 * History:
 * ---------
 *  2006-09-08  first version (bogdan)
 */

/*!
 * \file 
 * \brief MI :: Format handling
 * \ingroup mi
 */



#ifndef _MI_FMT_H_
#define _MI_FMT_H_

#include <stdarg.h>
#include <errno.h>

/*! \brief size of the buffer used for printing the FMT */
#define DEFAULT_MI_FMT_BUF_SIZE 2048

extern char *mi_fmt_buf;
extern int  mi_fmt_buf_len;

int mi_fmt_init( unsigned int size );

static inline char* mi_print_fmt(char *fmt, va_list ap, int *len)
{
	int n;

	if (mi_fmt_buf==NULL) {
		if (mi_fmt_init(DEFAULT_MI_FMT_BUF_SIZE)!=0) {
			LM_ERR("failed to init\n");
			return 0;
		}
	}

	n = vsnprintf( mi_fmt_buf, mi_fmt_buf_len, fmt, ap);
	if (n<0 || n>=mi_fmt_buf_len) {
		LM_ERR("formatting failed with n=%d, %s\n",n,strerror(errno));
		return 0;
	}

	*len = n;
	return mi_fmt_buf;
}

#endif
